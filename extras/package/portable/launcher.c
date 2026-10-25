/* SPDX-License-Identifier: GPL-2.0-or-later
 * Thin Unicode bootstrap only. No playback, decoding or configuration logic. */
#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif
#include <windows.h>
#include <wchar.h>
#include <wincrypt.h>
int WINAPI wWinMain(HINSTANCE instance, HINSTANCE previous, PWSTR args, int show)
{
    wchar_t root[32768], system[32768], command[32768];
    STARTUPINFOW startup = {0};
    PROCESS_INFORMATION child = {0};
    (void)instance; (void)previous; (void)show;
    wchar_t encoded[24576];
    DWORD encodedCount = sizeof(encoded) / sizeof(encoded[0]);
    size_t argumentBytes = wcslen(args) * sizeof(wchar_t);
    encoded[0] = 0;
    if (!argumentBytes) encodedCount = 1;
    if (argumentBytes > 16000 || (argumentBytes && !CryptBinaryToStringW((const BYTE *)args,
            (DWORD)argumentBytes, CRYPT_STRING_BASE64 | CRYPT_STRING_NOCRLF,
            encoded, &encodedCount))) goto error;
    DWORD count = GetModuleFileNameW(NULL, root, 32768);
    if (!count || count >= 32768) goto error;
    wchar_t *slash = wcsrchr(root, L'\\');
    if (!slash) goto error;
    *slash = 0;
    if (!GetSystemDirectoryW(system, 32768)) goto error;
    if (wcslen(root) + wcslen(system) + encodedCount + 200 >= 32768) goto error;
    /* Paths come from Windows; double quotes cannot occur in file names.
       Transport the original UTF-16 command line as base64, never PowerShell code.
       Never execute through cmd.exe/PATH. */
    wcscpy(command, root); wcscat(command, L"\\portable\\Start-Portable.ps1");
    DWORD attributes = GetFileAttributesW(command);
    if (attributes == INVALID_FILE_ATTRIBUTES || (attributes & FILE_ATTRIBUTE_DIRECTORY)) goto error;
    wcscat(system, L"\\WindowsPowerShell\\v1.0\\powershell.exe");
    wcscpy(command, L"\""); wcscat(command, system);
    wcscat(command, L"\" -NoLogo -NoProfile -NonInteractive -ExecutionPolicy Bypass -File \"");
    wcscat(command, root); wcscat(command, L"\\portable\\Start-Portable.ps1\"");
    if (argumentBytes) {
        wcscat(command, L" -ExplorerArgumentsBase64 ");
        wcscat(command, encoded);
    }
    startup.cb = sizeof(startup);
    startup.dwFlags = STARTF_USESHOWWINDOW;
    startup.wShowWindow = SW_HIDE;
    if (!CreateProcessW(system, command, NULL, NULL, FALSE, CREATE_NO_WINDOW,
                        NULL, root, &startup, &child)) goto error;
    CloseHandle(child.hThread); CloseHandle(child.hProcess);
    return 0;
error:
    MessageBoxW(NULL, L"Cannot start VLC Timeline Preview. Extract the complete ZIP to a writable folder. Windows PowerShell 5.1 is required.",
                L"VLC Timeline Preview", MB_OK | MB_ICONERROR);
    return 1;
}
