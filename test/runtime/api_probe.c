/* GPL-2.0-or-later. Test-only public ABI load probe. */
#define _WIN32_WINNT 0x0601
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
int wmain(int argc,wchar_t **argv) {
    if(argc!=2)return 2;
    HMODULE dll=LoadLibraryW(argv[1]);if(!dll)return 3;
    typedef unsigned long (__cdecl *api_fn)(void);
    api_fn api=(api_fn)GetProcAddress(dll,"mpv_client_api_version");
    if(!api){FreeLibrary(dll);return 4;}
    unsigned long version=api();printf("{\"client_api_version\":%lu}\n",version);
    FreeLibrary(dll);return version==((2ul<<16)|6ul)?0:5;
}
