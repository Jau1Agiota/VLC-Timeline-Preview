/* Stage 2.4.6A standalone experiment. Independent code, GPL-2.0-or-later.
 * Public libmpv APIs only; no MPC-QT implementation copied. No VLC handles.
 */
#define WIN32_LEAN_AND_MEAN
#define _WIN32_WINNT 0x0601
#include <windows.h>
#include <psapi.h>
#include <GL/gl.h>
#include <mpv/client.h>
#include <mpv/render.h>
#include <mpv/render_gl.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <wchar.h>
#include <io.h>
#include <fcntl.h>
#include <math.h>
#include <string.h>
#ifndef GL_BGRA_EXT
#define GL_BGRA_EXT 0x80E1
#endif
#include "request_policy.h"
#define W 320
#define H 180
#define WAKE (WM_APP+1)
#define MAX_REQUESTS 10000u
static HWND window, image_window;
static HDC dc;
static HGLRC gl;
static mpv_handle *mpv;
static mpv_render_context *render;
static FILE *journal;
static HANDLE stdin_thread, watchdog_thread, shutdown_event;
static CRITICAL_SECTION lock;
static Request latest;
static LONG notified, quit_input, bad_input;
static volatile LONGLONG expiration;
static void arm(unsigned ms) { InterlockedExchange64(&expiration,ms?(LONGLONG)GetTickCount64()+ms:0); }
static LARGE_INTEGER frequency;
static Policy policy;
static double start_ms, load_ms, duration_ms, command_ms, restart_ms, render_ms, display_ms;
static double cpu_start, deadline, initial_render_ms, initial_display_ms, initialize_ms;
static int software, ready, file_loaded, load_restart, initial_frame, pts_done, hw_done, rc;
static const char *hardware="no", *seek_mode="absolute+keyframes";
static char *media;
static wchar_t result_dir[32768];
static double observed_pts=-1;
static char hw_current[128]="unobserved";
static unsigned char pixels[W*H*4];
static unsigned captures;
static double now_ms(void) { LARGE_INTEGER t; QueryPerformanceCounter(&t); return 1000.0*(double)t.QuadPart/(double)frequency.QuadPart; }
static double cpu_ms(void) {
    FILETIME a,b,k,u; ULARGE_INTEGER x,y;
    if(!GetProcessTimes(GetCurrentProcess(),&a,&b,&k,&u))return -1;
    x.LowPart=k.dwLowDateTime;x.HighPart=k.dwHighDateTime;
    y.LowPart=u.dwLowDateTime;y.HighPart=u.dwHighDateTime;
    return (double)(x.QuadPart+y.QuadPart)/10000.0;
}
static void json_string(FILE *f,const char *s) {
    fputc('"',f);
    for(const unsigned char *p=(const unsigned char *)s;*p;++p) {
        if(*p=='"'||*p=='\\') {fputc('\\',f);fputc(*p,f);}
        else if(*p<32)fprintf(f,"\\u%04x",(unsigned)*p);
        else fputc(*p,f);
    }
    fputc('"',f);
}
static void line(const char *type,const char *message) {
    FILE *streams[]={stdout,journal};
    for(unsigned i=0;i<2;++i)if(streams[i]) {
        fputs("{\"type\":",streams[i]);json_string(streams[i],type);
        fputs(",\"message\":",streams[i]);json_string(streams[i],message);fputs("}\n",streams[i]);fflush(streams[i]);
    }
}
static void fail(const char *message) { if(!rc)rc=3;line("error",message);PostMessageW(window,WM_CLOSE,0,0); }
static void notify(void *unused) { (void)unused;if(InterlockedExchange(&notified,1)==0)PostMessageW(window,WAKE,0,0); }
static DWORD WINAPI watch(void *unused) {
    (void)unused;
    while(WaitForSingleObject(shutdown_event,20)==WAIT_TIMEOUT) {
        LONGLONG value=InterlockedCompareExchange64(&expiration,0,0);
        if(value>0 && (LONGLONG)GetTickCount64()>value)TerminateProcess(GetCurrentProcess(),124);
    }
    return 0;
}
static void request(double target) {
    EnterCriticalSection(&lock);
    if(latest.id>=MAX_REQUESTS){InterlockedExchange(&bad_input,1);}
    else {latest.id++;latest.target_ms=target;latest.emitted_ms=now_ms();}
    LeaveCriticalSection(&lock);notify(NULL);
}
static DWORD WINAPI read_input(void *unused) {
    (void)unused;char text[128];
    while(fgets(text,sizeof(text),stdin)) {
        if(!strcmp(text,"quit\n")||!strcmp(text,"quit\r\n")){InterlockedExchange(&quit_input,1);notify(NULL);return 0;}
        char *end;double t=strtod(text,&end);
        if(end==text||!isfinite(t)||t<0||t>86400000||(*end!='\n'&&*end!='\r')||
           (*end=='\r' && end[1]!='\n')||(*end=='\n'&&end[1])||(*end=='\r'&&end[2])) {
            InterlockedExchange(&bad_input,1);notify(NULL);return 0;
        }
        request(t);
    }
    return 0; /* EOF is not quit: keep the window alive for manual hovering. */
}
static char *utf8(const wchar_t *wide) {
    int count=WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,wide,-1,NULL,0,NULL,NULL);
    char *s=count?malloc((size_t)count):NULL;
    if(s&&!WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,wide,-1,s,count,NULL,NULL)){free(s);s=NULL;}
    return s;
}
static void put32(unsigned char *p,unsigned n) {for(unsigned i=0;i<4;++i)p[i]=(unsigned char)(n>>(8*i));}
static int save_frame(unsigned id) {
    if(captures>=128)return 0;
    wchar_t path[32768];
    if(wcslen(result_dir)>32000)return 0;
    swprintf(path,32768,L"%ls\\frame-%06u.bmp",result_dir,id);
    HANDLE out=CreateFileW(path,GENERIC_WRITE,0,NULL,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,NULL);
    if(out==INVALID_HANDLE_VALUE)return 0;
    unsigned char header[54]={0};DWORD written;int ok=1;
    header[0]='B';header[1]='M';put32(header+2,54+sizeof(pixels));put32(header+10,54);put32(header+14,40);
    put32(header+18,W);put32(header+22,H);header[26]=1;header[28]=32;
    if(!WriteFile(out,header,54,&written,NULL)||written!=54)ok=0;
    for(int y=H-1;y>=0&&ok;--y)
        if(!WriteFile(out,pixels+(size_t)y*W*4,W*4,&written,NULL)||written!=W*4)ok=0;
    if(!CloseHandle(out))ok=0;
    if(!ok)DeleteFileW(path);else captures++;
    return ok;
}
static void record_frame(void) {
    PROCESS_MEMORY_COUNTERS_EX memory={0};memory.cb=sizeof(memory);
    GetProcessMemoryInfo(GetCurrentProcess(),(PROCESS_MEMORY_COUNTERS *)&memory,sizeof(memory));
    int captured=save_frame(policy.active.id);
    char private_text[32],working_text[32]; /* decimal via double: memory values below 2^53; no legacy %llu. */
    snprintf(private_text,sizeof(private_text),"%.0f",(double)memory.PrivateUsage);
    snprintf(working_text,sizeof(working_text),"%.0f",(double)memory.WorkingSetSize);
    FILE *streams[]={stdout,journal};
    for(unsigned i=0;i<2;++i)if(streams[i]) {
        fprintf(streams[i],"{\"type\":\"displayed\",\"id\":%u,\"requested_ms\":%.3f,\"command_ack_ms\":%.3f,"
            "\"restart_ms\":%.3f,\"render_ms\":%.3f,\"display_handoff_ms\":%.3f,\"cpu_ms\":%.3f,"
            "\"private_bytes\":%s,\"working_set_bytes\":%s,\"playback_position_ms\":%.3f,"
            "\"frame_timestamp_verified\":false,\"physical_display_verified\":false,\"bmp_saved\":%s,\"hwdec_current\":",
            policy.active.id,policy.active.target_ms,command_ms-policy.active.emitted_ms,
            restart_ms-policy.active.emitted_ms,render_ms-policy.active.emitted_ms,
            display_ms-policy.active.emitted_ms,cpu_ms()-cpu_start,private_text,working_text,observed_pts,captured?"true":"false");
        json_string(streams[i],hw_current);fputs("}\n",streams[i]);fflush(streams[i]);
    }
    policy.busy=0;arm(0);
}
static int present_surface(void) {
    if(software) {
        BITMAPINFO info={0};info.bmiHeader.biSize=sizeof(info.bmiHeader);info.bmiHeader.biWidth=W;
        info.bmiHeader.biHeight=-H;info.bmiHeader.biPlanes=1;info.bmiHeader.biBitCount=32;
        int lines=StretchDIBits(dc,0,0,W,H,0,0,W,H,pixels,&info,DIB_RGB_COLORS,SRCCOPY);
        if(lines==0||lines==(int)GDI_ERROR||!GdiFlush()){fail("GDI handoff failed");return 0;}
    } else {if(!SwapBuffers(dc)){fail("OpenGL swap failed");return 0;}mpv_render_context_report_swap(render);}
    return 1;
}
static void finish_candidate(void) {
    if(!candidate_ready(&policy))return;
    EnterCriticalSection(&lock);policy.latest=latest;
    if(!candidate_current(&policy)) {
        LeaveCriticalSection(&lock);line("superseded","decoded obsolete candidate was not displayed");
        policy.busy=0;arm(0);return;
    }
    /* The lock prevents the stdin thread accepting a newer request during handoff. */
    if(!present_surface())return;
    display_ms=now_ms();policy.presented=1;LeaveCriticalSection(&lock);
    observed_pts=-1;pts_done=hw_done=0;
    if(mpv_get_property_async(mpv,100000u+policy.active.id,"time-pos",MPV_FORMAT_DOUBLE)<0||
       mpv_get_property_async(mpv,200000u+policy.active.id,"hwdec-current",MPV_FORMAT_STRING)<0)fail("property query failed");
    wchar_t caption[200];swprintf(caption,200,L"libmpv | %.0f ms | %ls (PTS unverified)",policy.active.target_ms,!strcmp(seek_mode,"absolute+exact")?L"exact":L"keyframes");
    SetWindowTextW(window,caption);
}
static void maybe_ready(void) {
    if(ready||!file_loaded||!load_restart||!initial_frame||duration_ms<=0)return;
    ready=1;arm(0);
    FILE *streams[]={stdout,journal};
    for(unsigned i=0;i<2;++i)if(streams[i]) {
        fprintf(streams[i],"{\"type\":\"ready\",\"open_to_first_render_ms\":%.3f,\"load_to_first_render_ms\":%.3f,\"api_initialize_ms\":%.3f,\"open_to_initial_display_ms\":%.3f,\"duration_ms\":%.3f}\n",
                initial_render_ms-start_ms,initial_render_ms-load_ms,initialize_ms,initial_display_ms-start_ms,duration_ms);fflush(streams[i]);
    }
}
static void render_update(void) {
    uint64_t update=mpv_render_context_update(render);
    if(!(update&MPV_RENDER_UPDATE_FRAME))return;
    mpv_render_frame_info info={0};mpv_render_param get[]={ {MPV_RENDER_PARAM_NEXT_FRAME_INFO,&info},{0,NULL} };
    if(mpv_render_context_get_info(render,get[0])<0)return;
    int actual=(info.flags&MPV_RENDER_FRAME_INFO_PRESENT)&&!(info.flags&(MPV_RENDER_FRAME_INFO_REDRAW|MPV_RENDER_FRAME_INFO_REPEAT));
    int block=0;int result;
    if(software) {
        int size[]={W,H};size_t stride=W*4;
        mpv_render_param params[]={{MPV_RENDER_PARAM_SW_SIZE,size},{MPV_RENDER_PARAM_SW_FORMAT,"bgr0"},
            {MPV_RENDER_PARAM_SW_STRIDE,&stride},{MPV_RENDER_PARAM_SW_POINTER,pixels},
            {MPV_RENDER_PARAM_BLOCK_FOR_TARGET_TIME,&block},{0,NULL}};
        result=mpv_render_context_render(render,params);
    } else {
        mpv_opengl_fbo target={0,W,H,0};int flip=1;
        mpv_render_param params[]={{MPV_RENDER_PARAM_OPENGL_FBO,&target},{MPV_RENDER_PARAM_FLIP_Y,&flip},
            {MPV_RENDER_PARAM_BLOCK_FOR_TARGET_TIME,&block},{0,NULL}};
        result=mpv_render_context_render(render,params);
        if(result>=0) {
            unsigned char bottom[W*H*4];glReadPixels(0,0,W,H,GL_BGRA_EXT,GL_UNSIGNED_BYTE,bottom);
            for(int y=0;y<H;++y)memcpy(pixels+(size_t)y*W*4,bottom+(size_t)(H-1-y)*W*4,W*4);
        }
    }
    if(result<0){fail(mpv_error_string(result));return;}
    if(actual) {
        if(!ready){initial_frame=1;initial_render_ms=now_ms();if(!present_surface())return;initial_display_ms=now_ms();}
        else if(policy.busy&&!policy.presented){policy.frame_seen=1;render_ms=now_ms();}
    }
    maybe_ready();finish_candidate();
}
static void pump(void) {
    InterlockedExchange(&notified,0);
    if(InterlockedCompareExchange(&quit_input,0,0)){PostMessageW(window,WM_CLOSE,0,0);return;}
    if(InterlockedCompareExchange(&bad_input,0,0)){fail("invalid request or request limit");return;}
    for(;;) {
        mpv_event *event=mpv_wait_event(mpv,0);
        if(event->event_id==MPV_EVENT_NONE)break;
        if(event->event_id==MPV_EVENT_FILE_LOADED) {
            file_loaded=1;mpv_get_property_async(mpv,2,"duration",MPV_FORMAT_DOUBLE);
        } else if(event->event_id==MPV_EVENT_COMMAND_REPLY&&policy.busy&&event->reply_userdata==policy.active.id+100u) {
            if(event->error<0){fail(mpv_error_string(event->error));return;}
            policy.command_done=1;command_ms=now_ms();
        } else if(event->event_id==MPV_EVENT_SEEK&&policy.busy)policy.seek_seen=1;
        else if(event->event_id==MPV_EVENT_PLAYBACK_RESTART) {
            if(!ready)load_restart=1;
            else if(policy.busy){policy.restarted=1;restart_ms=now_ms();}
        } else if(event->event_id==MPV_EVENT_GET_PROPERTY_REPLY) {
            mpv_event_property *property=event->data;
            if(event->reply_userdata==2&&event->error>=0&&property->data)duration_ms=*(double *)property->data*1000;
            else if(policy.busy&&policy.presented&&event->reply_userdata==100000u+policy.active.id) {
                if(event->error>=0&&property->data)observed_pts=*(double *)property->data*1000;
                pts_done=1;
            } else if(policy.busy&&policy.presented&&event->reply_userdata==200000u+policy.active.id) {
                snprintf(hw_current,sizeof(hw_current),"%s",event->error>=0&&property->data?*(char **)property->data:"unavailable");hw_done=1;
            }
        } else if(event->event_id==MPV_EVENT_END_FILE) {
            mpv_event_end_file *end=event->data;
            if(end->reason==MPV_END_FILE_REASON_ERROR){fail(mpv_error_string(end->error));return;}
        } else if(event->event_id==MPV_EVENT_SHUTDOWN){fail("mpv shutdown");return;}
    }
    maybe_ready();render_update();finish_candidate();
    if(policy.busy&&policy.presented&&pts_done&&hw_done)record_frame();
    EnterCriticalSection(&lock);policy.latest=latest;LeaveCriticalSection(&lock);
    if(ready&&!policy.busy&&policy.latest.id>policy.active.id) {
        begin_latest(&policy);policy.active.target_ms=fmin(policy.active.target_ms,fmax(0,duration_ms-1));
        char target[64];snprintf(target,sizeof(target),"%.6f",policy.active.target_ms/1000);
        const char *command[]={"seek",target,seek_mode,NULL};
        cpu_start=cpu_ms();deadline=now_ms()+5000;arm(5000);
        int result=mpv_command_async(mpv,policy.active.id+100u,command);
        if(result<0)fail(mpv_error_string(result));
    }
    if(policy.busy&&now_ms()>deadline)fail("seek/render deadline exceeded; no valid candidate claimed");
}
static void *get_proc(void *unused,const char *name) {
    (void)unused; union { PROC proc;void *pointer; } value;
    value.proc=wglGetProcAddress(name);
    if(!value.proc||(uintptr_t)value.pointer<=3||(uintptr_t)value.pointer==UINTPTR_MAX){HMODULE module=GetModuleHandleW(L"opengl32.dll");value.proc=GetProcAddress(module,name);}
    return value.pointer;
}
static LRESULT CALLBACK procedure(HWND hwnd,UINT message,WPARAM wp,LPARAM lp) {
    if(message==WAKE||message==WM_TIMER){if(mpv&&render)pump();return 0;}
    if(message==WM_MOUSEMOVE&&ready) {
        int x=(short)LOWORD(lp),y=(short)HIWORD(lp);
        if(y>=190&&y<=225&&x>=0&&x<W)request(duration_ms*(double)x/(W-1));
        return 0;
    }
    if(message==WM_PAINT) {
        PAINTSTRUCT paint;HDC draw=BeginPaint(hwnd,&paint);RECT area={0,190,W,225};
        FillRect(draw,&area,(HBRUSH)(COLOR_BTNFACE+1));
        DrawTextW(draw,L"Hover this timeline; or type milliseconds",-1,&area,DT_CENTER|DT_VCENTER|DT_SINGLELINE);EndPaint(hwnd,&paint);return 0;
    }
    if(message==WM_CLOSE){PostQuitMessage(0);return 0;}
    if(message==WM_DESTROY){PostQuitMessage(0);return 0;}
    return DefWindowProcW(hwnd,message,wp,lp);
}
static int option(const char *name,const char *value) {
    int result=mpv_set_option_string(mpv,name,value);if(result<0){line("error",name);return 0;}return 1;
}
int wmain(int argc,wchar_t **argv) {
    QueryPerformanceFrequency(&frequency);start_ms=now_ms();
    if(argc<3||argc>6){fputs("usage: mpv-preview.exe LOCAL_VIDEO NEW_RESULTS [gl|sw] [no|auto-copy] [keyframes|exact]\n",stderr);return 2;}
    software=argc>=4&&!wcscmp(argv[3],L"sw");
    if(argc>=4&&wcscmp(argv[3],L"gl")&&wcscmp(argv[3],L"sw"))return 2;
    if(argc>=5){if(!wcscmp(argv[4],L"auto-copy"))hardware="auto-copy";else if(wcscmp(argv[4],L"no"))return 2;}
    if(argc>=6){if(!wcscmp(argv[5],L"exact"))seek_mode="absolute+exact";else if(wcscmp(argv[5],L"keyframes"))return 2;}
    wchar_t path[32768];DWORD n=GetFullPathNameW(argv[1],32768,path,NULL);
    if(!n||n>=32768||path[1]!=L':')return 2;
    wchar_t root[]={path[0],L':',L'\\',0};DWORD attributes=GetFileAttributesW(path);
    if(GetDriveTypeW(root)!=DRIVE_FIXED||attributes==INVALID_FILE_ATTRIBUTES||(attributes&FILE_ATTRIBUTE_DIRECTORY))return 2;
    const wchar_t *ext=wcsrchr(path,L'.');
    if(!ext||(_wcsicmp(ext,L".mp4")&&_wcsicmp(ext,L".mkv")))return 2;
    media=utf8(path);if(!media)return 3;
    n=GetFullPathNameW(argv[2],32768,result_dir,NULL);
    if(result_dir[1]==L':'){root[0]=result_dir[0];}
    if(!n||n>=32000||result_dir[1]!=L':'||GetDriveTypeW(root)!=DRIVE_FIXED||!CreateDirectoryW(result_dir,NULL)){free(media);return 2;}
    swprintf(path,32768,L"%ls\\events.jsonl",result_dir);
    HANDLE output=CreateFileW(path,GENERIC_WRITE,0,NULL,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,NULL);
    if(output==INVALID_HANDLE_VALUE){free(media);return 3;}
    journal=_fdopen(_open_osfhandle((intptr_t)output,_O_BINARY),"wb");if(!journal){CloseHandle(output);free(media);return 3;}
    InitializeCriticalSection(&lock);shutdown_event=CreateEventW(NULL,TRUE,FALSE,NULL);
    watchdog_thread=CreateThread(NULL,0,watch,NULL,0,NULL);arm(15000);
    if(!shutdown_event||!watchdog_thread){rc=3;goto cleanup;}
    WNDCLASSW klass={0};klass.style=CS_OWNDC;klass.lpfnWndProc=procedure;klass.hInstance=GetModuleHandleW(NULL);
    klass.lpszClassName=L"StandaloneLibmpvPreview";klass.hCursor=LoadCursor(NULL,IDC_ARROW);RegisterClassW(&klass);
    RECT bounds={0,0,W,235};DWORD style=WS_CAPTION|WS_SYSMENU|WS_MINIMIZEBOX;AdjustWindowRect(&bounds,style,FALSE);
    window=CreateWindowW(klass.lpszClassName,L"libmpv preview experiment",style,100,100,bounds.right-bounds.left,
                         bounds.bottom-bounds.top,NULL,NULL,klass.hInstance,NULL);
    image_window=CreateWindowW(L"STATIC",L"",WS_CHILD|WS_VISIBLE,0,0,W,H,window,NULL,klass.hInstance,NULL);
    if(!window||!image_window){rc=3;goto cleanup;}dc=GetDC(image_window);
    if(!software) {
        PIXELFORMATDESCRIPTOR pixel={0};pixel.nSize=sizeof(pixel);pixel.nVersion=1;
        pixel.dwFlags=PFD_DRAW_TO_WINDOW|PFD_SUPPORT_OPENGL|PFD_DOUBLEBUFFER;pixel.iPixelType=PFD_TYPE_RGBA;pixel.cColorBits=32;
        int format=ChoosePixelFormat(dc,&pixel);
        if(!format||!SetPixelFormat(dc,format,&pixel)||(gl=wglCreateContext(dc))==NULL||!wglMakeCurrent(dc,gl)){rc=3;goto cleanup;}
        const char *driver=(const char *)glGetString(GL_RENDERER);line("opengl_renderer",driver?driver:"unavailable");
    }
    mpv=mpv_create();if(!mpv){rc=3;goto cleanup;}
    if(!option("config","no")||!option("load-scripts","no")||!option("ytdl","no")||!option("terminal","no")||
       !option("input-default-bindings","no")||!option("input-vo-keyboard","no")||!option("audio","no")||
       !option("sid","no")||!option("sub-visibility","no")||!option("osd-level","0")||!option("pause","yes")||
       !option("keep-open","yes")||!option("keepaspect","yes")||!option("vo","libmpv")||
       !option("hwdec",hardware)||!option("hr-seek",!strcmp(seek_mode,"absolute+exact")?"yes":"no")){rc=3;goto cleanup;}
    double init_start=now_ms();
    if(mpv_initialize(mpv)<0){rc=3;goto cleanup;}
    initialize_ms=now_ms()-init_start;
    for(unsigned i=0;i<3;++i){const char *keys[]={"mpv-version","ffmpeg-version","mpv-configuration"};char *s=mpv_get_property_string(mpv,keys[i]);
        if(s){line(keys[i],s);mpv_free(s);}}
    mpv_opengl_init_params init={get_proc,NULL};
    mpv_render_param params[]={{MPV_RENDER_PARAM_API_TYPE,software?MPV_RENDER_API_TYPE_SW:MPV_RENDER_API_TYPE_OPENGL},
                               {software?MPV_RENDER_PARAM_INVALID:MPV_RENDER_PARAM_OPENGL_INIT_PARAMS,software?NULL:&init},{0,NULL}};
    if(mpv_render_context_create(&render,mpv,params)<0){line("error","render context creation failed");rc=3;goto cleanup;}
    mpv_set_wakeup_callback(mpv,notify,NULL);mpv_render_context_set_update_callback(render,notify,NULL);
    const char *load[]={"loadfile",media,"replace",NULL};load_ms=now_ms();
    if(mpv_command_async(mpv,1,load)<0){rc=3;goto cleanup;}
    line("renderer",software?"software / CPU scaling":"OpenGL / GPU scaling; decode selected separately");
    line("seek_mode",seek_mode);ShowWindow(window,SW_SHOW);SetTimer(window,1,20,NULL);
    stdin_thread=CreateThread(NULL,0,read_input,NULL,0,NULL);
    MSG event;while(GetMessageW(&event,NULL,0,0)>0){TranslateMessage(&event);DispatchMessageW(&event);}
cleanup:
    arm(5001);
    if(stdin_thread){CancelSynchronousIo(stdin_thread);WaitForSingleObject(stdin_thread,1000);CloseHandle(stdin_thread);}
    if(render){mpv_render_context_set_update_callback(render,NULL,NULL);mpv_render_context_free(render);}
    if(mpv){mpv_set_wakeup_callback(mpv,NULL,NULL);mpv_terminate_destroy(mpv);}
    if(gl){wglMakeCurrent(NULL,NULL);wglDeleteContext(gl);}if(dc)ReleaseDC(image_window,dc);
    if(shutdown_event)SetEvent(shutdown_event);
    if(watchdog_thread){WaitForSingleObject(watchdog_thread,1000);CloseHandle(watchdog_thread);}
    if(window)DestroyWindow(window);
    if(shutdown_event)CloseHandle(shutdown_event);
    DeleteCriticalSection(&lock);free(media);
    if(journal)fclose(journal);
    return rc;
}
