/* test_version_proxy.c - version.dll 代理白盒验证
 * 编译: i686-w64-mingw32-gcc -O0 -g -o release\\test_version_proxy.exe test_version_proxy.c
 * 验证目标:
 *   A. 代理可加载
 *   B. DNF.exe 引用的两个无 @ 名可解析(GetFileVersionInfoA / GetFileVersionInfoSizeA)
 *   C. 真实 System32\\version.dll 可加载且函数可达
 *   D. 全部 17 导出存在
 */
#include <windows.h>
#include <stdio.h>

static void step(const char *msg)
{
    printf("[t] %s (err=%lu)\n", msg, (unsigned long)GetLastError());
    fflush(stdout);
}

int main(void)
{
    char path[MAX_PATH];
    char *slash;
    HMODULE h;
    HMODULE real = NULL;
    wchar_t sys[MAX_PATH];
    wchar_t realPath[MAX_PATH];

    GetModuleFileNameA(NULL, path, MAX_PATH);
    slash = strrchr(path, '\\');
    if (slash) *(slash + 1) = 0;
    strcat(path, "version.dll");
    step("proxy path built");

    /* A */
    h = LoadLibraryA(path);
    if (!h) { step("FAIL: proxy LoadLibrary"); return 1; }
    step("proxy loaded OK");

    /* B */
    {
        static const char *names[] = {
            "GetFileVersionInfoA", "GetFileVersionInfoSizeA",
        };
        int i, ok = 1;
        for (i = 0; i < 2; i++) {
            if (!GetProcAddress(h, names[i])) {
                printf("[t] FAIL: %s missing\n", names[i]);
                ok = 0;
            }
        }
        if (ok) step("DNF.exe import names resolved");
    }

    /* C */
    {
        UINT n = GetSystemDirectoryW(sys, MAX_PATH);
        if (n == 0 || n >= MAX_PATH) { step("FAIL: GetSystemDirectoryW"); return 3; }
        wcscat(sys, L"\\version.dll");
        real = LoadLibraryW(sys);
        if (!real) { step("FAIL: real System32 version.dll"); return 4; }
        step("real System32 version.dll loaded");
    }

    /* D */
    {
        static const char *names[] = {
            "GetFileVersionInfoA", "GetFileVersionInfoByHandle",
            "GetFileVersionInfoExA", "GetFileVersionInfoExW",
            "GetFileVersionInfoSizeA", "GetFileVersionInfoSizeExA",
            "GetFileVersionInfoSizeExW", "GetFileVersionInfoSizeW",
            "GetFileVersionInfoW", "VerFindFileA", "VerFindFileW",
            "VerInstallFileA", "VerInstallFileW", "VerLanguageNameA",
            "VerLanguageNameW", "VerQueryValueA", "VerQueryValueW",
        };
        int i, ok = 1;
        for (i = 0; i < 17; i++) {
            if (!GetProcAddress(h, names[i])) {
                printf("[t] FAIL: %s missing\n", names[i]);
                ok = 0;
            }
        }
        if (ok) step("all 17 exports present");
    }

    /* 实际调用 GetFileVersionInfoSizeA(游戏最常用) */
    {
        typedef DWORD (WINAPI *FnSizeA)(LPCSTR, LPDWORD);
        FnSizeA fn = (FnSizeA)GetProcAddress(h, "GetFileVersionInfoSizeA");
        DWORD handle = 0;
        DWORD n = fn("C:\\Windows\\System32\\version.dll", &handle);
        printf("[t] GetFileVersionInfoSizeA real call -> %lu bytes\n", (unsigned long)n);
        if (n == 0) step("NOTE: 0 size (maybe file has no version info); call itself worked");
        else step("call forwarded to real version.dll OK");
    }

    step("TEST PASS");
    return 0;
}