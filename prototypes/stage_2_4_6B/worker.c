/* Independent persistent preview subprocess. GPL-2.0-or-later; see COPYING.
 * Public libmpv SW render API. No VLC playback handles, windows, audio or files.
 * stdout: one bounded JSON header + exactly 320*180*4 BGR0 bytes per request.
 * The request label is NOT frame PTS. No timestamp accuracy is implied.
 */
#define _WIN32_WINNT 0x0601
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <mpv/client.h>
#include <mpv/render.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <io.h>
#include <fcntl.h>
#include <math.h>
#include <wchar.h>
#include <psapi.h>
#define WIDTH 320
#define HEIGHT 180
static volatile LONG armed=1;
static ULONGLONG expiry;
static CRITICAL_SECTION lock;
static unsigned char pixels[WIDTH*HEIGHT*4];
static double now(void) { return (double)GetTickCount64(); }
static void arm(unsigned ms) { EnterCriticalSection(&lock);expiry=GetTickCount64()+ms;LeaveCriticalSection(&lock); }
static DWORD WINAPI watchdog(void *unused) {
    (void)unused;
    while(InterlockedCompareExchange(&armed,1,1)) {
        EnterCriticalSection(&lock);ULONGLONG end=expiry;LeaveCriticalSection(&lock);
        if(GetTickCount64()>end)TerminateProcess(GetCurrentProcess(),124);
        Sleep(25);
    }
    return 0;
}
static int opt(mpv_handle *p,const char *key,const char *value) {
    int rc=mpv_set_option_string(p,key,value);
    if(rc<0)fprintf(stderr,"libmpv option %s: %s\n",key,mpv_error_string(rc));
    return rc>=0;
}
int wmain(int argc,wchar_t **argv) {
    if(argc!=2)return 2;
    InitializeCriticalSection(&lock);arm(15000);
    HANDLE watch=CreateThread(NULL,0,watchdog,NULL,0,NULL);
    if(!watch)return 3;
    _setmode(_fileno(stdin),_O_BINARY);_setmode(_fileno(stdout),_O_BINARY);
    double opened=now(),started=opened,render_ms=0;int rc=3;
    mpv_handle *p=mpv_create();mpv_render_context *render=NULL;
    if(!p)goto cleanup;
    if(!opt(p,"config","no")||!opt(p,"load-scripts","no")||!opt(p,"ytdl","no")||
       !opt(p,"terminal","no")||!opt(p,"audio","no")||!opt(p,"audio-display","no")||
       !opt(p,"sid","no")||!opt(p,"sub-visibility","no")||!opt(p,"osd-level","0")||
       !opt(p,"pause","yes")||!opt(p,"keep-open","yes")||!opt(p,"keepaspect","yes")||
       !opt(p,"vo","libmpv")||!opt(p,"hwdec","no")||!opt(p,"hr-seek","no"))goto cleanup;
    if(mpv_initialize(p)<0)goto cleanup;
    mpv_render_param create[]={{MPV_RENDER_PARAM_API_TYPE,MPV_RENDER_API_TYPE_SW},{0,NULL}};
    if(mpv_render_context_create(&render,p,create)<0)goto cleanup;
    int length=WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,argv[1],-1,NULL,0,NULL,NULL);
    if(length<=1||length>32768)goto cleanup;
    char path[32768];
    if(!WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,argv[1],-1,path,length,NULL,NULL))goto cleanup;
    const char *load[]={"loadfile",path,NULL};
    if(mpv_command_async(p,1,load)<0)goto cleanup;
    int loaded=0,initial_restart=0,initial_frame=0,busy=0,ack=0,seek=0,restart=0,frame=0;
    unsigned id=0,last_id=0,target=0;char input[128];size_t used=0;
    double ready_ms=0;int ready=0;
    for(;;) {
        for(unsigned n=0;n<64;++n) {
            mpv_event *e=mpv_wait_event(p,0);
            if(e->event_id==MPV_EVENT_NONE)break;
            if(e->event_id==MPV_EVENT_FILE_LOADED)loaded=1;
            if(e->event_id==MPV_EVENT_COMMAND_REPLY) {
                if(e->error<0)goto cleanup;
                if(busy==1&&e->reply_userdata==id+100u)ack=1;
            }
            if(e->event_id==MPV_EVENT_SEEK&&busy==1)seek=1;
            if(e->event_id==MPV_EVENT_PLAYBACK_RESTART) {if(busy==1)restart=1;else initial_restart=1;}
            if(e->event_id==MPV_EVENT_SHUTDOWN)goto cleanup;
            if(e->event_id==MPV_EVENT_END_FILE) {
                mpv_event_end_file *end=e->data;
                if(end->reason==MPV_END_FILE_REASON_ERROR)goto cleanup;
            }
        }
        if(mpv_render_context_update(render)&MPV_RENDER_UPDATE_FRAME) {
            mpv_render_frame_info info={0};
            if(mpv_render_context_get_info(render,(mpv_render_param){MPV_RENDER_PARAM_NEXT_FRAME_INFO,&info})<0)goto cleanup;
            int actual=(info.flags&MPV_RENDER_FRAME_INFO_PRESENT)&&!(info.flags&(MPV_RENDER_FRAME_INFO_REDRAW|MPV_RENDER_FRAME_INFO_REPEAT));
            int size[]={WIDTH,HEIGHT},block=0;size_t stride=WIDTH*4;
            mpv_render_param params[]={{MPV_RENDER_PARAM_SW_SIZE,size},{MPV_RENDER_PARAM_SW_FORMAT,"bgr0"},
                {MPV_RENDER_PARAM_SW_STRIDE,&stride},{MPV_RENDER_PARAM_SW_POINTER,pixels},
                {MPV_RENDER_PARAM_BLOCK_FOR_TARGET_TIME,&block},{0,NULL}};
            double t=now();if(mpv_render_context_render(render,params)<0)goto cleanup;
            if(busy==1)render_ms+=now()-t;
            if(actual){if(busy==1)frame=1;else initial_frame=1;}
        }
        if(!ready&&loaded&&initial_restart&&initial_frame) {ready=1;ready_ms=now()-opened;arm(86400000);}
        if(busy==1&&ack&&seek&&restart&&frame) {
            /* BGR0's unused alpha is set explicitly for Qt ARGB32 on little endian x64. */
            for(size_t i=3;i<sizeof(pixels);i+=4)pixels[i]=255;
            FILETIME ct,et,kt,ut;ULARGE_INTEGER kernel,user;
            double cpu=-1;PROCESS_MEMORY_COUNTERS_EX memory={0};memory.cb=sizeof(memory);
            if(GetProcessTimes(GetCurrentProcess(),&ct,&et,&kt,&ut)) {
                kernel.LowPart=kt.dwLowDateTime;kernel.HighPart=kt.dwHighDateTime;
                user.LowPart=ut.dwLowDateTime;user.HighPart=ut.dwHighDateTime;
                cpu=(double)(kernel.QuadPart+user.QuadPart)/10000.0;
            }
            if(!GetProcessMemoryInfo(GetCurrentProcess(),(PROCESS_MEMORY_COUNTERS *)&memory,sizeof(memory)))memory.PrivateUsage=0;
            double encode=now();
            if(printf("{\"protocol\":1,\"request_id\":%u,\"requested_ms\":%u,\"bytes\":230400,\"width\":320,\"height\":180,\"frame_timestamp_verified\":false,\"ready_ms\":%.3f,\"seek_render_ms\":%.3f,\"render_ms\":%.3f,\"cpu_total_ms\":%.3f,\"private_bytes\":%.0f}\n",id,target,ready_ms,now()-started,render_ms,cpu,(double)memory.PrivateUsage)<0 ||
               fwrite(pixels,1,sizeof(pixels),stdout)!=sizeof(pixels)||fflush(stdout))goto cleanup;
            /* Includes blocking pipe write: receiver backpressure is watchdog bounded. */
            if(getenv("VLC_TIMELINE_HOVER_DEBUG")&&!strcmp(getenv("VLC_TIMELINE_HOVER_DEBUG"),"1"))fprintf(stderr,"mpv-frame id=%u pipe_write_ms=%.3f\n",id,now()-encode);
            busy=0;arm(86400000);
        }
        DWORD available=0;
        if(!PeekNamedPipe(GetStdHandle(STD_INPUT_HANDLE),NULL,0,NULL,&available,NULL)) {rc=0;break;}
        if(available) {
            if(busy)goto cleanup; /* Controller promises one active request. */
            while(available--) {
                char c;DWORD count=0;
                if(!ReadFile(GetStdHandle(STD_INPUT_HANDLE),&c,1,&count,NULL)||count!=1)goto cleanup;
                if(c=='\n') {
                    input[used]=0;unsigned timeout;char extra;
                    if(sscanf(input,"%u %u %u %c",&id,&target,&timeout,&extra)!=3||id<=last_id||id>1000000||target>86400000||timeout!=5000)goto cleanup;
                    /* Retain startup request until the initial frame is ready. */
                    last_id=id;used=0;busy=2;break;
                }
                if(c=='\r')continue;
                if(used>=sizeof(input)-1)goto cleanup;
                input[used++]=c;
            }
        }
        if(busy==2&&ready) {
            char seconds[40];snprintf(seconds,sizeof(seconds),"%.6f",target/1000.0);
            const char *command[]={"seek",seconds,"absolute+keyframes",NULL};
            busy=1;ack=seek=restart=frame=0;render_ms=0;started=now();arm(5000);
            if(mpv_command_async(p,id+100u,command)<0)goto cleanup;
        }
        Sleep(2);
    }
cleanup:
    arm(5000);
    if(render)mpv_render_context_free(render);
    if(p)mpv_terminate_destroy(p);
    InterlockedExchange(&armed,0);WaitForSingleObject(watch,1000);CloseHandle(watch);
    DeleteCriticalSection(&lock);return rc;
}
