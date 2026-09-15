/* ============================================================
 * DFO 90CN 手柄震动 - Loader 主程序 (v1.2)
 * 结构复用自 DfoVibration_OLD_ACT4 (常驻轮询 + 启动器双模式)
 * 用途: 游戏启动前运行(或常驻), 把 DfoVibration.dll 注入 DNF.exe
 *       v1.2: DLL_NAME 改为 DfoVibration.dll (宿主只认此模块名,
 *             与生产版 DLL 配套; 探针 v1.3 已退役入 release_backup)
 * 模式:
 *   1) 常驻轮询模式: 等待/发现 DNF.exe 进程 -> 注入 -> 继续轮询(支持重启)
 *   2) 启动器模式:   DfoVibration_90CN_Loader.exe <DNF.exe路径> 拉起游戏并注入
 * 32 位编译 (LLVM-MinGW gcc -m32)
 * ============================================================ */
#include <windows.h>
#include <stdio.h>
#include <string.h>
#include <process.h>

#include "inject_x86.c"   /* inject_by_name / launch_and_inject */

#define GAME_EXE    "DNF.exe"
#define DLL_NAME    "DfoVibration.dll"
#define POLL_MS     2000

static const char *g_dllPath = NULL;
static volatile int g_running = 1;

/* 把 stdout/stderr 重定向到 loader 同目录日志文件(追加, 无缓冲):
 * 注入失败必须留档可查, 不能只打在会关掉的控制台上 */
static void log_redirect(void)
{
    char path[MAX_PATH], exePath[MAX_PATH];
    char *slash;
    GetModuleFileNameA(NULL, exePath, MAX_PATH);
    slash = strrchr(exePath, '\\');
    if (slash) *slash = '\0';
    _snprintf(path, sizeof(path), "%s\\DfoVibration_90CN_Loader.log", exePath);
    freopen(path, "a", stdout);
    setvbuf(stdout, NULL, _IONBF, 0);
    freopen(path, "a", stderr);
}

/* 是否以管理员令牌运行 */
static int is_elevated(void)
{
    HANDLE tok = NULL;
    DWORD elevated = 0, ret = 0;
    if (OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &tok)) {
        if (!GetTokenInformation(tok, TokenElevation, &elevated, sizeof(elevated), &ret))
            elevated = 0;
        CloseHandle(tok);
    }
    return elevated != 0;
}

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
            printf("[loader] DNF.exe pid=%lu found, injecting probe...\n", (unsigned long)pid);
            if (inject_process(pid, g_dllPath))
                printf("[loader] probe injected OK (pid %lu)\n", (unsigned long)pid);
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
    SYSTEMTIME st;

    log_redirect();
    GetLocalTime(&st);
    printf("=================================================");
    printf("[loader] v1.2 %04d-%02d-%02d %02d:%02d:%02d pid=%lu elevated=%d",
           st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond,
           (unsigned long)GetCurrentProcessId(), is_elevated());
    fflush(stdout);
    if (!is_elevated()) {
        fprintf(stderr, "[loader] 警告: 未以管理员运行, 注入 highestAvailable 的 DNF.exe 会失败!");
        fprintf(stderr, "[loader] 请右键 -> 以管理员身份运行 (或确认 UAC 已弹并同意)");
    }

    if (argc >= 2) {
        /* 启动器模式: 参数 = DNF.exe 全路径 */
        printf("[loader] launcher mode: %s\n", argv[1]);
        if (!build_dll_path(dllPath, sizeof(dllPath))) {
            fprintf(stderr, "[loader] DLL not found next to loader: %s\n", DLL_NAME);
            return 1;
        }
        g_dllPath = dllPath;
        if (launch_and_inject(argv[1], g_dllPath, NULL))
            printf("[loader] launched + probe injected OK\n");
        else
            printf("[loader] launched, inject failed (游戏可能仍正常启动)\n");
    } else {
        /* 常驻轮询模式 */
        if (!build_dll_path(dllPath, sizeof(dllPath))) {
            fprintf(stderr, "[loader] DLL not found next to loader: %s\n", DLL_NAME);
            fprintf(stderr, "usage: DfoVibration_90CN_Loader.exe [DNF.exe path]\n");
            return 1;
        }
        g_dllPath = dllPath;
        console_title_set("DfoVibration 90CN Probe Loader - waiting DNF.exe...");
        printf("[loader] watching for %s... (Ctrl+C to exit)\n", GAME_EXE);
        _beginthreadex(NULL, 0, poll_thread, NULL, 0, NULL);
        while (g_running) Sleep(1000);
    }
    return 0;
}
