#include <string.h>
#include <windows.h>
#include <intsafe.h>
#include "jxr_sys_helpers.h"

uint32_t jxr_get_number_of_processors(void)
{
    SYSTEM_INFO systemInfo;
    GetSystemInfo(&systemInfo);
    return systemInfo.dwNumberOfProcessors;
}

char* jxr_get_error_description(int code)
{
    char* msg;
    static const char defaultMessage[] = "Unidentified error.";
    LPWSTR buffer = NULL;
    DWORD rv = FormatMessageW(
        FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
        NULL,
        (DWORD)code,
        MAKELANGID(LANG_ENGLISH, SUBLANG_ENGLISH_US),
        (LPWSTR)&buffer,
        0,
        NULL);
    if (rv)
    {
        int len = (int)wcslen(buffer);
        int utf8Len = WideCharToMultiByte(CP_UTF8, 0, buffer, len, NULL, 0, NULL, NULL);
        msg = LocalAlloc(LMEM_ZEROINIT, len + 1);
        if(msg)
            WideCharToMultiByte(CP_UTF8, 0, buffer, len, msg, utf8Len, NULL, NULL);
    }
    else
    {
        msg = LocalAlloc(LMEM_ZEROINIT, sizeof(defaultMessage));
        if(msg)
            strcpy_s(msg, sizeof(defaultMessage) - 1, defaultMessage);
    }
    return msg;
}

void jxr_free_error_description(char* desc)
{
    if (desc)
        LocalFree(desc);
}

int jxr_get_command_line(int argc, char* argv[], jxr_command_line* cmdline)
{
    if (!cmdline)
        return E_INVALIDARG;

    ZeroMemory(cmdline, sizeof(jxr_command_line));

    LPWSTR origCmdLine = GetCommandLineW();

    if (!origCmdLine)
        return E_FAIL;

    if(!((cmdline->argv = CommandLineToArgvW(origCmdLine, &cmdline->argc))))
    {
        return HRESULT_FROM_WIN32(GetLastError());
    }

    return S_OK;
}

void jxr_free_command_line(jxr_command_line* cmdline)
{
    if(cmdline && cmdline->argv)
    {
        LocalFree((HLOCAL)cmdline->argv);
        ZeroMemory(cmdline, sizeof(jxr_command_line));
    }
}

int jxr_write_data_to_file(const wchar_t* filename, void* buffer, size_t size)
{
    if (size > UINT32_MAX)
        return E_INVALIDARG;

    HANDLE hFile = CreateFileW(
        filename,
        GENERIC_READ | GENERIC_WRITE,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        NULL,
        CREATE_ALWAYS,
        FILE_ATTRIBUTE_NORMAL,
        NULL);
    if (hFile == INVALID_HANDLE_VALUE)
        return HRESULT_FROM_WIN32(GetLastError());
    BOOL rv = WriteFile(hFile, buffer, (DWORD)size, NULL, NULL);
    if (!rv)
    {
        DWORD err = GetLastError();
        CloseHandle(hFile);
        return HRESULT_FROM_WIN32(err);
    }
    CloseHandle(hFile);
    return S_OK;
}
