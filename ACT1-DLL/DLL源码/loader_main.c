/* ============================================================
 * DFO 老版本手柄震动 - Loader 主程序
 * 用途: 游戏启动前运行(或常驻), 把 DfoVibration_OLD.dll 注入 DNF.exe
 * 模式:
 *   1) 常驻轮询模式: 等待/发现 DNF.exe 进程 -> 注入 -> 继续轮询(支持重启)
 *   2) 启动器模式:   DfoVibration_OLD_Loader.exe <DNF路径> 拉起游戏并注入
 * 32 位编译 (i686-w64-mingw32-gcc -m32)
 * ============================================================ */
#include <windows.h>
#include <stdio.h>
#include <string.h>
#include <process.h>

#include "inject_x86.c"   /* inject_by_name / launch_and_inject */

#define GAME_EXE    "DNF.exe"
#define DLL_NAME    "DfoVibration_OLD.dll"
#define POLL_MS     2000

static const char *g_dllPath = NULL;
static volatile int g_running = 1;

/* 获取 DLL 全路径(与 loader 同目录) */
static int build_dll_path(char *buf, size_t cap)
{
    HMODULE h = NULL;
    char exeDir[MAX_PATH];
    GetModuleFileNameA(h, exeDir, MAX_PATH);
    {
        char *slash = strrchr(exeDir, '\\');
        if (slash) *slash = '\0';
    }
    _snprintf(buf, cap, "%s\\%s", exeDir, DLL_NAME);
    return GetFileAttributesA(buf) != INVALID_FILE_ATTRIBUTES;
}

static void console_title_set(const char *t)
{
    SetConsoleTitleA(t);
}

/* 轮询线程(后台) */
static unsigned __stdcall poll_thread(void *arg)
{
    (void)arg;
    int lastPid = 0;
    while (g_running) {
        DWORD pid = 0;
        /* 简单按名找 PID */
        {
            HANDLE hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
            PROCESSENTRY32W pe;
            pe.dwSize = sizeof(pe);
            if (hSnap != INVALID_HANDLE_VALUE) {
                if (Process32FirstW(hSnap, &pe)) {
                    do {
                        if (_wcsicmp(pe.szExeFile, L"DNF.exe") == 0) { pid = pe.th32ProcessID; break; }
                    } while (Process32NextW(hSnap, &pe));
                }
                CloseHandle(hSnap);
            }
        }
        if (pid && pid != lastPid) {
            printf("[loader] DNF.exe pid=%lu found, injecting...\n", (unsigned long)pid);
            if (inject_process(pid, g_dllPath))
                printf("[loader] injected OK (pid %lu)\n", (unsigned long)pid);
            else
                printf("[loader] inject failed (pid %lu)\n", (unsigned long)pid);
            lastPid = (int)pid;
        }
        Sleep(POLL_MS);
    }
    return 0;
}

int main(int argc, char **argv)
{
    char dllPath[MAX_PATH];

    if (argc >= 2) {
        /* 启动器模式: 参数 = DNF.exe 全路径 */
        printf("[loader] launcher mode: %s\n", argv[1]);
        if (!build_dll_path(dllPath, sizeof(dllPath))) {
            fprintf(stderr, "[loader] DLL not found next to loader: %s\n", DLL_NAME);
            return 1;
        }
        g_dllPath = dllPath;
        if (launch_and_inject(argv[1], g_dllPath, NULL))
            printf("[loader] launched + injected OK\n");
        else
            printf("[loader] launched, inject failed (游戏可能仍正常启动)\n");
    } else {
        /* 常驻轮询模式 */
        if (!build_dll_path(dllPath, sizeof(dllPath))) {
            fprintf(stderr, "[loader] DLL not found next to loader: %s\n", DLL_NAME);
            fprintf(stderr, "用法: DfoVibration_OLD_Loader.exe [DNF.exe路径]\n");
            return 1;
        }
        g_dllPath = dllPath;
        console_title_set("DfoVibration OLD Loader - 等待 DNF.exe 启动...");
        printf("[loader] watching for %s... (Ctrl+C 退出)\n", GAME_EXE);
        _beginthreadex(NULL, 0, poll_thread, NULL, 0, NULL);
        while (g_running) Sleep(1000);
    }
    return 0;
}