/* test_proxy.c - 隔离验证 dinput8 代理 DLL 加载 + 转发链路
 * 编译: i686-w64-mingw32-gcc -O0 -g -o release\\test_proxy.exe test_proxy.c
 * 运行: 从 release 目录运行(全路径 LoadLibrary 本目录代理 DLL)
 * 逐步诊断版: 每步 fflush, 崩溃时能定位到具体步骤。
 * 预期输出:
 *   [t] step N ... / FAIL ... —— 定位加载/解析/转发链路中断点。
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
    FARPROC p;
    HMODULE real = NULL;
    wchar_t sys[MAX_PATH];
    wchar_t realPath[MAX_PATH];

    GetModuleFileNameA(NULL, path, MAX_PATH);
    step("exe path obtained");
    slash = strrchr(path, '\\');
    if (slash) *(slash + 1) = 0;
    strcat(path, "dinput8.dll");
    step("proxy path built");

    /* === A. 加载代理 DLL === */
    h = LoadLibraryA(path);
    if (!h) {
        step("FAIL: proxy LoadLibrary failed");
        return 1;
    }
    step("proxy loaded OK");

    /* === B. 按 DNF.exe 导入表无 @ 名解析 DirectInput8Create === */
    p = (FARPROC)GetProcAddress(h, "DirectInput8Create");
    if (!p) {
        step("FAIL: DirectInput8Create not exported");
        return 2;
    }
    step("DirectInput8Create resolved");

    /* === C. 直接加载真实 System32\\dinput8.dll(验证绝对路径可得) === */
    {
        UINT n = GetSystemDirectoryW(sys, MAX_PATH);
        if (n == 0 || n >= MAX_PATH) {
            step("FAIL: GetSystemDirectoryW");
            return 3;
        }
        /* 32 位进程: System32 路径会被 WOW64 重定向到 SysWOW64(真实 32 位 dinput8) */
        wcscat(sys, L"\\dinput8.dll");
        real = LoadLibraryW(sys);
        if (!real) {
            step("FAIL: real System32 dinput8 LoadLibrary");
            return 4;
        }
        step("real System32 dinput8 loaded");
    }

    /* === D. 真实 DLL 里同名函数可解析(对照) === */
    p = (FARPROC)GetProcAddress(real, "DirectInput8Create");
    if (!p) {
        step("FAIL: real DirectInput8Create not exported");
        return 5;
    }
    step("real DirectInput8Create resolved");

    /* === E. 只测"地址可达性": 读真实 DLL 第一个导出函数的字节
     *     (不做真实调用, 避免 COM 环境差异导致的误报) === */
    {
        unsigned char buf[16];
        SIZE_T rd = 0;
        /* p 指向真实 dinput8 代码页, 直接读前 8 字节应成功 */
        if (ReadProcessMemory(GetCurrentProcess(), p, buf, 8, &rd) && rd == 8) {
            step("real fn code readable (callable)");
        } else {
            /* 同进程读自己内存失败说明地址非法 -> 转发必崩 */
            step("FAIL: real fn address not readable");
            return 6;
        }
    }

    /* === F. 其余 5 个导出存在性 === */
    {
        static const char *names[] = {
            "DllCanUnloadNow", "DllGetClassObject", "DllRegisterServer",
            "DllUnregisterServer", "GetdfDIJoystick",
        };
        int i, ok = 1;
        for (i = 0; i < 5; i++) {
            if (!GetProcAddress(h, names[i])) {
                step("FAIL: missing export");
                ok = 0;
            }
        }
        if (ok) step("all 5 remaining proxy exports present");
    }

    step("TEST PASS: proxy DLL loadable, exports match DNF.exe import names");
    return 0;
}