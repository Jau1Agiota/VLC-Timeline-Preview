/* Stage 2.4.6A.1 standalone experiment. Independent code, GPL-2.0-or-later.
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
#define MAX_REQUESTS 1000000u
#define MAX_SAMPLES 4096u
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
static double effective_seek, cpu_start, deadline, initial_render_ms, initial_display_ms, initialize_ms;
static int software, ready, file_loaded, load_restart, initial_frame, rc;
static const char *hardware="no", *seek_mode="absolute+keyframes";
static char *media;
static wchar_t result_dir[32768];
static unsigned char pixels[W*H*4];
static unsigned captures, completed, discarded, deduplicated;
/* visual=0: buffered numeric samples only; timing=1: streamed samples; capture=2: readback/BMP. */
static int mode;
static double issued_ms, render_call_ms, readback_ms, swap_ms, last_launch_ms;
static unsigned displayed_id;
static char reported_hw[128]="unobserved";
static double displayed_target;
static int last_mouse_x=-1;
typedef struct {
    unsigned id; double requested, effective, queue, ack, restart, render, handoff;
    double render_call, readback, swap, bmp, emit, cpu, position, input_age;
    SIZE_T private_bytes, working_set; int captured, shown;
    char hw[128];
} FrameSample;
static FrameSample samples[MAX_SAMPLES];
static void repaint_timeline(void) { RECT r={0,H,UI_WIDTH,292};if(window)InvalidateRect(window,&r,FALSE); }

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
static void request(double target,double input_age) {
    EnterCriticalSection(&lock);
    if(latest.id>=MAX_REQUESTS){InterlockedExchange(&bad_input,1);}
    else {latest.id++;latest.target_ms=target;latest.emitted_ms=now_ms();latest.input_age_ms=input_age;}
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
        request(t,0);
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
static void emit_sample(FILE *out,const FrameSample *r) {
    char cpu[40]="null",private_bytes[40]="null",working_set[40]="null";
    if(mode){snprintf(cpu,sizeof(cpu),"%.3f",r->cpu);snprintf(private_bytes,sizeof(private_bytes),"%.0f",(double)r->private_bytes);
        snprintf(working_set,sizeof(working_set),"%.0f",(double)r->working_set);}
    fprintf(out,"{\"type\":\"%s\",\"id\":%u,\"requested_ms\":%.3f,\"effective_seek_ms\":%.3f,"
        "\"ui_message_age_ms\":%.3f,\"queue_ms\":%.3f,\"command_ack_ms\":%.3f,\"restart_ms\":%.3f,\"render_ms\":%.3f,"
        "\"display_handoff_ms\":%.3f,\"render_call_ms\":%.3f,\"readback_ms\":%.3f,\"swap_ms\":%.3f,"
        "\"bmp_ms\":%.3f,\"stream_log_ms\":%.3f,\"cpu_ms\":%s,\"private_bytes\":%s,\"working_set_bytes\":%s,"
        "\"playback_position_ms\":%.3f,\"frame_timestamp_verified\":false,\"physical_display_verified\":false,"
        "\"bmp_saved\":%s,\"hwdec_current\":",r->shown?"displayed":"discarded",r->id,r->requested,r->effective,r->input_age,r->queue,r->ack,r->restart,r->render,
        r->handoff,r->render_call,r->readback,r->swap,r->bmp,r->emit,cpu,private_bytes,
        working_set,r->position,r->captured?"true":"false");
    json_string(out,r->hw);fputs("}\n",out);
}
static void record_frame(int shown) {
    FrameSample *r=&samples[(policy.active.id-1)%MAX_SAMPLES];memset(r,0,sizeof(*r));
    r->shown=shown;r->id=policy.active.id;r->requested=policy.active.target_ms;r->effective=effective_seek;
    r->input_age=policy.active.input_age_ms;
    r->queue=issued_ms-policy.active.emitted_ms;r->ack=command_ms-policy.active.emitted_ms;
    r->restart=restart_ms-policy.active.emitted_ms;r->render=render_ms-policy.active.emitted_ms;
    r->handoff=shown?display_ms-policy.active.emitted_ms:-1;r->render_call=render_call_ms;
    r->readback=readback_ms;r->swap=shown?swap_ms:0;r->position=-1;snprintf(r->hw,sizeof(r->hw),"%s",reported_hw);
    if(mode) {
        PROCESS_MEMORY_COUNTERS_EX memory={0};memory.cb=sizeof(memory);
        GetProcessMemoryInfo(GetCurrentProcess(),(PROCESS_MEMORY_COUNTERS *)&memory,sizeof(memory));
        r->private_bytes=memory.PrivateUsage;r->working_set=memory.WorkingSetSize;r->cpu=cpu_ms()-cpu_start;
        double started=now_ms();if(shown&&mode==2)r->captured=save_frame(r->id);r->bmp=now_ms()-started;
        /* Best-effort diagnostics never gate the next seek; replies are matched by ID. */
        if(shown){mpv_get_property_async(mpv,10000000u+r->id,"time-pos",MPV_FORMAT_DOUBLE);
            mpv_get_property_async(mpv,20000000u+r->id,"hwdec-current",MPV_FORMAT_STRING);}
        started=now_ms();emit_sample(stdout,r);fflush(stdout);r->emit=now_ms()-started;
    }
    if(shown)completed++;
    policy.busy=0;arm(0);repaint_timeline();
}
static int present_surface(void) {
    double started=now_ms();
    if(software) {
        BITMAPINFO info={0};info.bmiHeader.biSize=sizeof(info.bmiHeader);info.bmiHeader.biWidth=W;
        info.bmiHeader.biHeight=-H;info.bmiHeader.biPlanes=1;info.bmiHeader.biBitCount=32;
        int lines=StretchDIBits(dc,0,0,W,H,0,0,W,H,pixels,&info,DIB_RGB_COLORS,SRCCOPY);
        if(lines==0||lines==(int)GDI_ERROR||!GdiFlush()){fail("GDI handoff failed");return 0;}
    } else {if(!SwapBuffers(dc)){fail("OpenGL swap failed");return 0;}mpv_render_context_report_swap(render);}
    swap_ms=now_ms()-started;return 1;
}
static void finish_candidate(void) {
    if(!candidate_ready(&policy))return;
    EnterCriticalSection(&lock);policy.latest=latest;
    if(!candidate_current(&policy)) {
        LeaveCriticalSection(&lock);discarded++;record_frame(0);return;
    }
    /* The lock prevents the stdin thread accepting a newer request during handoff. */
    if(!present_surface()){LeaveCriticalSection(&lock);return;}
    display_ms=now_ms();policy.presented=1;LeaveCriticalSection(&lock);
    displayed_id=policy.active.id;displayed_target=policy.active.target_ms;
    record_frame(1);
}
static void maybe_ready(void) {
    if(ready||!file_loaded||!load_restart||!initial_frame||duration_ms<=0)return;
    ready=1;arm(0);repaint_timeline();
    mpv_get_property_async(mpv,3,"hwdec-current",MPV_FORMAT_STRING);
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
    int block=0;int result;double started=now_ms();readback_ms=0;
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
        render_call_ms=now_ms()-started;
        if(result>=0 && mode==2) {
            double read_started=now_ms();
            unsigned char bottom[W*H*4];glReadPixels(0,0,W,H,GL_BGRA_EXT,GL_UNSIGNED_BYTE,bottom);
            for(int y=0;y<H;++y)memcpy(pixels+(size_t)y*W*4,bottom+(size_t)(H-1-y)*W*4,W*4);
            readback_ms=now_ms()-read_started;
        }
    }
    if(software)render_call_ms=now_ms()-started;
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
    for(unsigned event_count=0;event_count<64;++event_count) {
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
            else if(event->reply_userdata==3) {
                snprintf(reported_hw,sizeof(reported_hw),"%s",event->error>=0&&property->data?*(char **)property->data:"unavailable");
                line("decoder_backend",reported_hw);
            }
            else if(event->reply_userdata>10000000u&&event->reply_userdata<=10000000u+MAX_REQUESTS) {
                unsigned id=(unsigned)(event->reply_userdata-10000000u);FrameSample *r=&samples[(id-1)%MAX_SAMPLES];
                if(r->id==id&&event->error>=0&&property->data)r->position=*(double *)property->data*1000;
            } else if(event->reply_userdata>20000000u&&event->reply_userdata<=20000000u+MAX_REQUESTS) {
                unsigned id=(unsigned)(event->reply_userdata-20000000u);FrameSample *r=&samples[(id-1)%MAX_SAMPLES];
                if(r->id==id)snprintf(r->hw,sizeof(r->hw),"%s",event->error>=0&&property->data?*(char **)property->data:"unavailable");
            }
        } else if(event->event_id==MPV_EVENT_END_FILE) {
            mpv_event_end_file *end=event->data;
            if(end->reason==MPV_END_FILE_REASON_ERROR){fail(mpv_error_string(end->error));return;}
        } else if(event->event_id==MPV_EVENT_SHUTDOWN){fail("mpv shutdown");return;}
    }
    maybe_ready();render_update();finish_candidate();
    EnterCriticalSection(&lock);policy.latest=latest;LeaveCriticalSection(&lock);
    if(ready&&!policy.busy&&policy.latest.id>policy.active.id&&now_ms()-last_launch_ms>=16) {
        begin_latest(&policy);effective_seek=fmin(policy.active.target_ms,fmax(0,duration_ms-1));
        char target[64];snprintf(target,sizeof(target),"%.6f",effective_seek/1000);
        const char *command[]={"seek",target,seek_mode,NULL};
        issued_ms=now_ms();last_launch_ms=issued_ms;cpu_start=mode?cpu_ms():0;deadline=issued_ms+5000;arm(5000);
        repaint_timeline();
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
static void format_time(wchar_t *text,double milliseconds) {
    unsigned value=(unsigned)fmax(0,milliseconds);
    swprintf(text,40,L"%02u:%02u:%02u.%03u",value/3600000,(value/60000)%60,(value/1000)%60,value%1000);
}
static void paint_timeline(HWND hwnd) {
    PAINTSTRUCT ps;HDC out=BeginPaint(hwnd,&ps);HDC draw=CreateCompatibleDC(out);
    HBITMAP bitmap=CreateCompatibleBitmap(out,UI_WIDTH,112);HGDIOBJ old=SelectObject(draw,bitmap);
    RECT area={0,0,UI_WIDTH,112};HBRUSH background=CreateSolidBrush(RGB(25,29,36));FillRect(draw,&area,background);
    SetBkMode(draw,TRANSPARENT);SelectObject(draw,GetStockObject(DEFAULT_GUI_FONT));
    Request target;EnterCriticalSection(&lock);target=latest;LeaveCriticalSection(&lock);
    double position=target.id?target.target_ms:0;
    int marker=TRACK_LEFT+(duration_ms>0?(int)((TRACK_RIGHT-TRACK_LEFT)*fmin(position,duration_ms)/duration_ms):0);
    RECT track={TRACK_LEFT,32,TRACK_RIGHT+1,38};HBRUSH muted=CreateSolidBrush(RGB(70,80,95));FillRect(draw,&track,muted);
    HBRUSH accent=CreateSolidBrush(RGB(66,178,245));RECT progress={TRACK_LEFT,32,marker,38};FillRect(draw,&progress,accent);
    RECT knob={marker-3,27,marker+4,43};FillRect(draw,&knob,accent);
    if(displayed_id&&duration_ms>0){int x=TRACK_LEFT+(int)((TRACK_RIGHT-TRACK_LEFT)*fmin(displayed_target,duration_ms)/duration_ms);
        HBRUSH orange=CreateSolidBrush(RGB(245,178,80));RECT previous={x-1,44,x+2,49};FillRect(draw,&previous,orange);DeleteObject(orange);}
    wchar_t stamp[40],label[180];format_time(stamp,position);
    swprintf(label,180,L"Target  %ls",stamp);SetTextColor(draw,RGB(225,235,247));TextOutW(draw,12,7,label,(int)wcslen(label));
    format_time(stamp,duration_ms);swprintf(label,180,L"%ls",stamp);TextOutW(draw,UI_WIDTH-110,7,label,(int)wcslen(label));
    if(!ready)swprintf(label,180,L"Opening video...");
    else if(target.id!=displayed_id){const wchar_t symbols[]=L"|/-\\";
        swprintf(label,180,L"%lc Seeking latest target (image retained)",symbols[(GetTickCount64()/120)%4]);}
    else swprintf(label,180,L"Ready - move across the blue timeline");
    SetTextColor(draw,RGB(160,205,235));TextOutW(draw,12,57,label,(int)wcslen(label));
    if(displayed_id){format_time(stamp,displayed_target);swprintf(label,180,L"Image request: %ls",stamp);}
    else swprintf(label,180,L"Image: initial frame");
    SetTextColor(draw,RGB(245,178,80));TextOutW(draw,12,77,label,(int)wcslen(label));
    const wchar_t *notice=L"Frame time unverified; target is separate";
    TextOutW(draw,12,94,notice,(int)wcslen(notice));
    RECT upper={0,0,UI_WIDTH,H};FillRect(out,&upper,background);
    BitBlt(out,0,H,UI_WIDTH,112,draw,0,0,SRCCOPY);
    SelectObject(draw,old);DeleteObject(bitmap);DeleteDC(draw);
    DeleteObject(background);DeleteObject(muted);DeleteObject(accent);EndPaint(hwnd,&ps);
}
static LRESULT CALLBACK procedure(HWND hwnd,UINT message,WPARAM wp,LPARAM lp) {
    if(message==WAKE||message==WM_TIMER){
        if(mpv&&render)pump();
        if(message==WM_TIMER && (policy.busy||!ready)) {static ULONGLONG tick;ULONGLONG t=GetTickCount64();if(t-tick>=100){tick=t;repaint_timeline();}}
        return 0;
    }
    if(message==WM_MOUSEMOVE&&ready) {
        int x=(short)LOWORD(lp),y=(short)HIWORD(lp);double target=0;
        if(y>=H+20&&y<=H+52&&hover_target(x,TRACK_LEFT,TRACK_RIGHT,duration_ms,&target)) {
            if(last_mouse_x!=x){last_mouse_x=x;request(target,(double)(DWORD)(GetTickCount()-(DWORD)GetMessageTime()));repaint_timeline();}
            else deduplicated++;
            TRACKMOUSEEVENT tracking={sizeof(tracking),TME_LEAVE,hwnd,0};TrackMouseEvent(&tracking);
        }
        return 0;
    }
    if(message==WM_MOUSELEAVE){last_mouse_x=-1;return 0;}
    if(message==WM_PAINT){paint_timeline(hwnd);return 0;}
    if(message==WM_ERASEBKGND)return 1;
    if(message==WM_CLOSE){PostQuitMessage(0);return 0;}
    if(message==WM_DESTROY){PostQuitMessage(0);return 0;}
    return DefWindowProcW(hwnd,message,wp,lp);
}
static int option(const char *name,const char *value) {
    int result=mpv_set_option_string(mpv,name,value);if(result<0){line("error",name);return 0;}return 1;
}
int wmain(int argc,wchar_t **argv) {
    QueryPerformanceFrequency(&frequency);start_ms=now_ms();
    if(argc<3||argc>7){fputs("usage: mpv-preview.exe LOCAL_VIDEO NEW_RESULTS [gl|sw] [no|auto-copy] [keyframes|exact] [visual|timing|capture]\n",stderr);return 2;}
    software=argc>=4&&!wcscmp(argv[3],L"sw");
    if(argc>=4&&wcscmp(argv[3],L"gl")&&wcscmp(argv[3],L"sw"))return 2;
    if(argc>=5){if(!wcscmp(argv[4],L"auto-copy"))hardware="auto-copy";else if(wcscmp(argv[4],L"no"))return 2;}
    if(argc>=6){if(!wcscmp(argv[5],L"exact"))seek_mode="absolute+exact";else if(wcscmp(argv[5],L"keyframes"))return 2;}
    if(argc>=7){if(!wcscmp(argv[6],L"timing"))mode=1;else if(!wcscmp(argv[6],L"capture"))mode=2;else if(wcscmp(argv[6],L"visual"))return 2;}
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
    RECT bounds={0,0,UI_WIDTH,292};DWORD style=WS_CAPTION|WS_SYSMENU|WS_MINIMIZEBOX|WS_CLIPCHILDREN;AdjustWindowRect(&bounds,style,FALSE);
    window=CreateWindowW(klass.lpszClassName,L"libmpv Stage 2.4.6A.1 visual-performance experiment",style,100,100,bounds.right-bounds.left,
                         bounds.bottom-bounds.top,NULL,NULL,klass.hInstance,NULL);
    image_window=CreateWindowW(L"STATIC",L"",WS_CHILD|WS_VISIBLE,(UI_WIDTH-W)/2,0,W,H,window,NULL,klass.hInstance,NULL);
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
       !option("audio-display","no")||!option("sid","no")||!option("sub-visibility","no")||!option("osd-level","0")||!option("pause","yes")||
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
    line("mode",mode==0?"visual":mode==1?"timing":"capture");
    line("seek_mode",seek_mode);ShowWindow(window,SW_SHOW);SetTimer(window,1,16,NULL);
    stdin_thread=CreateThread(NULL,0,read_input,NULL,0,NULL);
    MSG event;while(GetMessageW(&event,NULL,0,0)>0){TranslateMessage(&event);DispatchMessageW(&event);}
cleanup:
    arm(5001);
    InterlockedExchange(&quit_input,1);
    if(stdin_thread){CancelSynchronousIo(stdin_thread);if(WaitForSingleObject(stdin_thread,1000)!=WAIT_OBJECT_0)TerminateProcess(GetCurrentProcess(),124);CloseHandle(stdin_thread);}
    if(render){mpv_render_context_set_update_callback(render,NULL,NULL);mpv_render_context_free(render);}
    if(mpv){mpv_set_wakeup_callback(mpv,NULL,NULL);mpv_terminate_destroy(mpv);}
    if(gl){wglMakeCurrent(NULL,NULL);wglDeleteContext(gl);}if(dc)ReleaseDC(image_window,dc);
    if(window)DestroyWindow(window);
    DeleteCriticalSection(&lock);free(media);
    if(journal){
        /* Normal hovering performs no journal writes. Flush bounded records only at shutdown. */
        for(unsigned i=0;i<MAX_SAMPLES;++i)if(samples[i].id)emit_sample(journal,&samples[i]);
        PROCESS_MEMORY_COUNTERS_EX memory={0};memory.cb=sizeof(memory);
        GetProcessMemoryInfo(GetCurrentProcess(),(PROCESS_MEMORY_COUNTERS *)&memory,sizeof(memory));
        fprintf(journal,"{\"type\":\"session\",\"mode\":%d,\"completed\":%u,\"discarded\":%u,\"deduplicated\":%u,"
            "\"cpu_ms\":%.3f,\"wall_ms\":%.3f,\"private_bytes\":%.0f,\"working_set_bytes\":%.0f,\"exit_code\":%d}\n",
            mode,completed,discarded,deduplicated,cpu_ms(),now_ms()-start_ms,(double)memory.PrivateUsage,(double)memory.WorkingSetSize,rc);
        fclose(journal);
    }
    if(shutdown_event)SetEvent(shutdown_event);
    if(watchdog_thread){WaitForSingleObject(watchdog_thread,1000);CloseHandle(watchdog_thread);}
    if(shutdown_event)CloseHandle(shutdown_event);
    return rc;
}
