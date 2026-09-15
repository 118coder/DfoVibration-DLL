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

#ifndef GAME_EXE
#define GAME_EXE    "DNF.exe"
#endif
#define DLL_NAME    "DfoVibration_OLD.dll"
#define POLL_MS     2000

/* ★40JP: 目标进程名候选表 —— 同一份 Loader 同时认 老客户端 DNF.exe 与 40JP 的 ARAD.exe。
 * 这样宿主(SorahkDFO)的 auto_inject(硬编码 DNF.exe) 不需要改：
 * 由本 Loader 负责把 DfoVibration_OLD.dll 送进 ARAD.exe，宿主照常从
 * Local\DfoVibrationShm 读事件(宿主的 process_alive 只校验 PID 存活, 不校验进程名)。 */
static const char *g_targets[] = { GAME_EXE, "ARAD.exe", "DNF.exe" };
#define N_TARGETS ((int)(sizeof(g_targets) / sizeof(g_targets[0])))

static DWORD find_target_pid(void)
{
    HANDLE hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    PROCESSENTRY32W pe;
    DWORD pid = 0;
    int t;
    if (hSnap == INVALID_HANDLE_VALUE) return 0;
    pe.dwSize = sizeof(pe);
    if (Process32FirstW(hSnap, &pe)) {
        do {
            for (t = 0; t < N_TARGETS; t++) {
                wchar_t w[64];
                MultiByteToWideChar(CP_ACP, 0, g_targets[t], -1, w, 64);
                if (_wcsicmp(pe.szExeFile, w) == 0) { pid = pe.th32ProcessID; break; }
            }
            if (pid) break;
        } while (Process32NextW(hSnap, &pe));
    }
    CloseHandle(hSnap);
    return pid;
}

static char g_dllStore[MAX_PATH];
static const char *g_dllPath = NULL;
static volatile int g_running = 1;

static int build_dll_path(char *buf, size_t cap);   /* 定义在后面 */

/* 在"目标进程 exe 所在目录"里找 DLL（Loader 未与 DLL 同目录时的兜底） */
static int build_dll_path_for_pid(DWORD pid, char *buf, size_t cap)
{
    HANDLE h = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    char img[MAX_PATH];
    DWORD sz = sizeof(img);
    char *slash;
    if (!h) return 0;
    if (!QueryFullProcessImageNameA(h, 0, img, &sz)) { CloseHandle(h); return 0; }
    CloseHandle(h);
    slash = strrchr(img, '\\');
    if (slash) *slash = 0;
    _snprintf(buf, cap, "%s\\%s", img, DLL_NAME);
    return GetFileAttributesA(buf) != INVALID_FILE_ATTRIBUTES;
}

/* 解析 DLL 路径：Loader 同目录优先，其次目标进程所在目录（游戏目录）。
 * 成功则写入 g_dllStore 并让 g_dllPath 指向它（避免悬垂指针）。 */
static int resolve_dll_path(DWORD pid)
{
    if (g_dllPath) return 1;
    if (build_dll_path(g_dllStore, sizeof(g_dllStore))) {
        g_dllPath = g_dllStore;
        return 1;
    }
    if (build_dll_path_for_pid(pid, g_dllStore, sizeof(g_dllStore))) {
        printf("[loader] DLL 取自游戏目录: %s\n", g_dllStore);
        g_dllPath = g_dllStore;
        return 1;
    }
    return 0;
}

/* ★收集所有同名候选 PID（可能有多个：正常实例 + 崩溃残留的僵尸）。
 * 崩溃残留进程会拒绝 OpenProcess(err=5)，必须跳过它去试下一个。 */
#define MAX_CAND 16
static int find_target_pids(DWORD *out, int cap)
{
    HANDLE hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    PROCESSENTRY32W pe;
    int n = 0;
    if (hSnap == INVALID_HANDLE_VALUE) return 0;
    pe.dwSize = sizeof(pe);
    if (Process32FirstW(hSnap, &pe)) {
        do {
            int t;
            for (t = 0; t < N_TARGETS; t++) {
                wchar_t w[64];
                MultiByteToWideChar(CP_ACP, 0, g_targets[t], -1, w, 64);
                if (_wcsicmp(pe.szExeFile, w) == 0) {
                    if (n < cap) out[n++] = pe.th32ProcessID;
                    break;
                }
            }
        } while (Process32NextW(hSnap, &pe));
    }
    CloseHandle(hSnap);
    return n;
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

/* 轮询线程(后台)：遍历所有同名进程，跳过打不开的（崩溃残留僵尸），注入第一个成功的 */
static unsigned __stdcall poll_thread(void *arg)
{
    static DWORD reported[MAX_CAND];   /* 已报告过"不可注入"的 PID，避免刷屏 */
    static int nreported = 0;
    (void)arg;
    int lastPid = 0;
    while (g_running) {
        DWORD cand[MAX_CAND];
        int np = find_target_pids(cand, MAX_CAND);
        int i, injected = 0;
        for (i = 0; i < np && !injected; i++) {
            int seen = 0, k;
            if ((int)cand[i] == lastPid) { injected = 1; break; }   /* 该实例已注入过 */
            for (k = 0; k < nreported; k++) if (reported[k] == cand[i]) { seen = 1; break; }
            if (seen) continue;                                     /* 已知不可注入，静默跳过 */
            if (!resolve_dll_path(cand[i])) {
                static int warned = 0;
                if (!warned) { warned = 1;
                    printf("[loader] 未找到 %s（Loader 同目录 / 游戏目录）\n", DLL_NAME); }
                continue;
            }
            printf("[loader] candidate pid=%lu, injecting...\n", (unsigned long)cand[i]);
            if (inject_process(cand[i], g_dllPath)) {
                printf("[loader] injected OK (pid %lu)\n", (unsigned long)cand[i]);
                lastPid = (int)cand[i];
                injected = 1;
            } else {
                printf("[loader] pid %lu 不可注入(可能是崩溃残留进程); 继续等待健康实例\n",
                       (unsigned long)cand[i]);
                if (nreported < MAX_CAND) reported[nreported++] = cand[i];
            }
        }
        Sleep(POLL_MS);
    }
    return 0;
}

int main(int argc, char **argv)
{
    try_enable_debug_priv();
    {   /* 打印当前权限，便于判断 OpenProcess 拒绝访问(错误 5)的原因 */
        HANDLE hTok = NULL;
        BOOL admin = FALSE;
        if (OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &hTok)) {
            TOKEN_ELEVATION te;
            DWORD cb = sizeof(te);
            if (GetTokenInformation(hTok, TokenElevation, &te, sizeof(te), &cb))
                admin = te.TokenIsElevated;
            CloseHandle(hTok);
        }
        printf("[loader] privilege: %s\n", admin
               ? "Administrator"
               : "NORMAL USER  (若报 OpenProcess failed: 5 请改用『以管理员身份运行』)");
    }

    if (argc >= 2) {
        /* 启动器模式: 参数 = 游戏 exe 全路径。由本工具自己拉起游戏 → 句柄在手, 不依赖 OpenProcess */
        printf("[loader] launcher mode: %s\n", argv[1]);
        /* DLL: Loader 同目录优先, 其次游戏 exe 同目录 */
        if (!build_dll_path(g_dllStore, sizeof(g_dllStore))) {
            char *slash;
            char img[MAX_PATH];
            _snprintf(img, sizeof(img) - 1, "%s", argv[1]);
            slash = strrchr(img, '\\');
            if (slash) *slash = 0;
            _snprintf(g_dllStore, sizeof(g_dllStore), "%s\\%s", img, DLL_NAME);
        }
        if (GetFileAttributesA(g_dllStore) == INVALID_FILE_ATTRIBUTES) {
            fprintf(stderr, "[loader] DLL not found: %s (同目录或游戏目录)\n", DLL_NAME);
            return 1;
        }
        g_dllPath = g_dllStore;
        printf("[loader] using dll: %s\n", g_dllPath);
        if (launch_and_inject(argv[1], g_dllPath, NULL))
            printf("[loader] launched + injected OK\n");
        else
            printf("[loader] launched, inject failed (游戏可能仍正常启动)\n");
    } else {
        /* 常驻轮询模式：DLL 路径延迟解析（Loader 同目录 → 目标游戏目录） */
        if (build_dll_path(g_dllStore, sizeof(g_dllStore))) {
            g_dllPath = g_dllStore;
            printf("[loader] using dll: %s\n", g_dllPath);
        } else {
            printf("[loader] %s 不在 Loader 同目录, 将在发现游戏后从其目录查找\n", DLL_NAME);
        }
        console_title_set("DfoVibration OLD Loader - waiting for ARAD.exe / DNF.exe ...");
        printf("[loader] watching for %s / ARAD.exe ... (Ctrl+C 退出)\n", GAME_EXE);
        _beginthreadex(NULL, 0, poll_thread, NULL, 0, NULL);
        while (g_running) Sleep(1000);
    }
    return 0;
}