/* GPL-2.0-or-later. Test-only Win32 pipe bridge; no extraction implementation. */
#define _WIN32_WINNT 0x0601
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <wchar.h>
#include <io.h>
#include <fcntl.h>
#include <psapi.h>
static int read_exact(HANDLE pipe, void *buffer, DWORD length) {
    DWORD used=0; ULONGLONG deadline=GetTickCount64()+20000;
    while(used<length) {
        DWORD available=0,count=0;
        if(!PeekNamedPipe(pipe,NULL,0,NULL,&available,NULL))return 0;
        if(!available) {if(GetTickCount64()>deadline)return 0;Sleep(1);continue;}
        DWORD take=length-used;if(take>available)take=available;
        if(!ReadFile(pipe,(char*)buffer+used,take,&count,NULL)||!count)return 0;
        used+=count;
    }
    return 1;
}
int wmain(int argc,wchar_t **argv) {
    if(argc<4||wcslen(argv[1])+wcslen(argv[2])>32000||wcschr(argv[1],L'"')||wcschr(argv[2],L'"'))return 2;
    HANDLE inread=NULL,inwrite=NULL,outread=NULL,outwrite=NULL,job=NULL;
    SECURITY_ATTRIBUTES sa={sizeof(sa),NULL,TRUE}; PROCESS_INFORMATION pi={0};
    STARTUPINFOW si={0};si.cb=sizeof(si);int rc=3;
    if(!CreatePipe(&inread,&inwrite,&sa,0)||!CreatePipe(&outread,&outwrite,&sa,0))goto done;
    if(!SetHandleInformation(inwrite,HANDLE_FLAG_INHERIT,0)||!SetHandleInformation(outread,HANDLE_FLAG_INHERIT,0))goto done;
    si.dwFlags=STARTF_USESTDHANDLES;si.hStdInput=inread;si.hStdOutput=outwrite;si.hStdError=GetStdHandle(STD_ERROR_HANDLE);
    wchar_t command[32768];swprintf(command,32768,L"\"%ls\" \"%ls\"",argv[1],argv[2]);
    job=CreateJobObjectW(NULL,NULL);if(!job)goto done;
    JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits={0};limits.BasicLimitInformation.LimitFlags=JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
    if(!SetInformationJobObject(job,JobObjectExtendedLimitInformation,&limits,sizeof(limits)))goto done;
    if(!CreateProcessW(argv[1],command,NULL,NULL,TRUE,CREATE_NO_WINDOW|CREATE_SUSPENDED,NULL,NULL,&si,&pi))goto done;
    if(!AssignProcessToJobObject(job,pi.hProcess)||ResumeThread(pi.hThread)==(DWORD)-1)goto done;
    CloseHandle(inread);inread=NULL;CloseHandle(outwrite);outwrite=NULL;
    _setmode(_fileno(stdout),_O_BINARY);
    for(int i=3;i<argc;++i) {
        wchar_t *end;unsigned long target=wcstoul(argv[i],&end,10);if(*end||target>86400000)goto done;
        char request[64];int size=snprintf(request,sizeof(request),"%u %u 5000\n",(unsigned)(i-2),(unsigned)target);
        DWORD count=0;ULONGLONG start=GetTickCount64();
        if(size<0||size>=(int)sizeof(request)||!WriteFile(inwrite,request,(DWORD)size,&count,NULL)||count!=(DWORD)size)goto done;
        char header[1026];unsigned n=0;
        do {if(n==1025||!read_exact(outread,header+n,1))goto done;}while(header[n++]!='\n');
        unsigned char pixels[230400];if(!read_exact(outread,pixels,sizeof(pixels)))goto done;
        if(i==3) {
            HMODULE modules[256];DWORD bytes=0;
            if(!EnumProcessModules(pi.hProcess,modules,sizeof(modules),&bytes)||bytes>sizeof(modules))goto done;
            for(unsigned m=0;m<bytes/sizeof(HMODULE);++m) {
                char name[256];if(!GetModuleBaseNameA(pi.hProcess,modules[m],name,sizeof(name)))goto done;
                fprintf(stderr,"bridge-module=%s\n",name);
            }
        }
        fprintf(stderr,"bridge request=%u image_ms=%u\n",(unsigned)(i-2),(unsigned)(GetTickCount64()-start));
        if(fwrite(header,1,n,stdout)!=n||fwrite(pixels,1,sizeof(pixels),stdout)!=sizeof(pixels)||fflush(stdout))goto done;
    }
    CloseHandle(inwrite);inwrite=NULL;
    if(WaitForSingleObject(pi.hProcess,8000)!=WAIT_OBJECT_0)goto done;
    DWORD status;if(!GetExitCodeProcess(pi.hProcess,&status)||status)goto done;
    rc=0;
done:
    if(pi.hProcess) {if(WaitForSingleObject(pi.hProcess,0)!=WAIT_OBJECT_0){TerminateProcess(pi.hProcess,124);WaitForSingleObject(pi.hProcess,2000);}CloseHandle(pi.hProcess);}
    if(pi.hThread)CloseHandle(pi.hThread);
    if(job)CloseHandle(job);
    if(inread)CloseHandle(inread);if(inwrite)CloseHandle(inwrite);
    if(outread)CloseHandle(outread);if(outwrite)CloseHandle(outwrite);
    return rc;
}
