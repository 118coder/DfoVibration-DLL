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
#include <stdarg.h>
#include <process.h>

#include "inject_x86.c"   /* inject_by_name / launch_and_inject */

/* ★2026-09-24 装载器自身日志(与 loader 同目录 DfoVibration_OLD_Loader.log):
 * 缘由: 之前 loader 只在控制台打印, 出问题时我读不到它的输出 → 只能靠推断,
 * 多花了整场战斗。现在同时落盘(文本文件不受那个"清扫新建 exe"的监控影响)。 */
static FILE *g_lg = NULL;

static void lg_open(void)
{
    char path[MAX_PATH];
    DWORD n = GetModuleFileNameA(NULL, path, MAX_PATH);
    char *slash;
    if (n == 0 || n >= MAX_PATH) return;
    slash = strrchr(path, '\\');
    if (!slash) return;
    *(slash + 1) = '\0';
    strncat(path, "DfoVibration_OLD_Loader.log", MAX_PATH - strlen(path) - 1);
    g_lg = fopen(path, "a");
}

static void lg(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    vprintf(fmt, ap);
    va_end(ap);
    fflush(stdout);
    if (g_lg) {
        va_start(ap, fmt);
        vfprintf(g_lg, fmt, ap);
        va_end(ap);
        fflush(g_lg);
    }
}

/* 目标游戏进程名列表（各版本客户端进程名不同, 轮询按名全匹配）:
 *   DNF.exe=ACT1 / DNFACT5.exe=ACT5 / DFO.exe+DFO_fixed_v3.exe=60US(壳版/脱壳版) */
static const wchar_t *GAME_EXES[] = {
    L"DNF.exe", L"DNFACT5.exe", L"DFO.exe", L"DFO_fixed_v3.exe", NULL
};
#define GAME_EXE    "DNF.exe/DNFACT5.exe/DFO.exe/DFO_fixed_v3.exe"
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

/* ★2026-09-24: 目标判定改为【标记文件制】:
 *   注入条件 = 目标 exe 与 loader 同目录, 或目标 exe 目录存在 .dfovib_target 标记。
 * 缘由有两层:
 *  1) 事故: 常驻 loader 按进程名匹配 DNF.exe, 把 60US DLL 注进了
 *     E:\Game\DNF-ACT4\ACT4\DNF.exe(另一个装有各自震动 DLL 的客户端)。
 *     签名自检全部拒装未造成伤害, 但跨客户端注入是不该冒的险。
 *  2) 部署: 本机存在按内容清扫新建 exe 的监控(非 Defender, 日志无记录),
 *     游戏目录/TEMP 新写的装载器 exe 数十秒内被删, 而工作区存活。
 *     故 loader 从工作区运行(注入工作区旁的 DLL), 用标记文件圈定目标客户端。
 * 标记文件: <目标exe目录>\.dfovib_target (ASCII 文本, 如 "60US") */
static int proc_is_target(DWORD pid)
{
    HANDLE h;
    WCHAR exePath[MAX_PATH];
    WCHAR myPath[MAX_PATH];
    WCHAR probe[MAX_PATH];
    DWORD sz = MAX_PATH, myLen, dirLen;
    WCHAR *slash;

    h = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (!h) return 0;
    if (!QueryFullProcessImageNameW(h, 0, exePath, &sz)) {
        CloseHandle(h);
        return 0;
    }
    CloseHandle(h);

    /* 条件1: 目标 exe 与 loader 同目录 */
    myLen = GetModuleFileNameW(NULL, myPath, MAX_PATH);
    slash = myLen > 0 ? wcsrchr(myPath, L'\\') : NULL;
    if (slash) {
        *slash = L'\0';
        dirLen = (DWORD)(slash - myPath);
        if (sz > dirLen + 1 &&
            _wcsnicmp(exePath, myPath, dirLen) == 0 &&
            exePath[dirLen] == L'\\')
            return 1;
    }

    /* 条件2: 目标 exe 目录存在标记文件 .dfovib_target */
    if (sz < MAX_PATH) {
        lstrcpyW(probe, exePath);
        slash = wcsrchr(probe, L'\\');
        if (slash) {
            *(slash + 1) = L'\0';
            lstrcatW(probe, L".dfovib_target");
            if (GetFileAttributesW(probe) != INVALID_FILE_ATTRIBUTES)
                return 1;
        }
    }
    return 0;
}

static void console_title_set(const char *t)
{
    SetConsoleTitleA(t);
}

/* ★2026-09-24 进程存活判定（事故驱动）:
 * 游戏崩溃后会留下"所有线程已退出、但进程对象未被回收"的僵尸 —— 名字仍是
 * DFO_fixed_v3.exe。旧轮询扫到【第一个名字匹配就 break】, 一旦扫到僵尸就把它
 * 当成命中记为 lastPid, 此后真正的游戏进程被 `pid != lastPid` 永久跳过 →
 * DLL 永不注入, 玩家那一场没有任何日志(实机踩到: 日志为空而 loader 明明在跑)。
 * → 现在每个候选先做存活检查, 僵尸跳过后【继续扫】。 */
static int proc_alive(DWORD pid)
{
    HANDLE h = OpenProcess(SYNCHRONIZE | PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    DWORD w;
    if (!h) return 0;                       /* 打不开(死进程常拒绝访问) → 当死 */
    w = WaitForSingleObject(h, 0);
    CloseHandle(h);
    return (w == WAIT_TIMEOUT);             /* 仍运行才为真 */
}

/* 轮询线程(后台) */
static unsigned __stdcall poll_thread(void *arg)
{
    (void)arg;
    int lastPid = 0;
    int lastOk = 0;          /* ★上次注入是否成功(失败允许重试) */
    int skipCount = 0;
    DWORD deadLogged[4] = {0, 0, 0, 0};   /* ★已通报过的尸体(多具: 只报一次不刷屏) */
    int nDeadLogged = 0;
    while (g_running) {
        DWORD pid = 0;
        const wchar_t *hitName = NULL;
        /* 按名扫描: 只认【存活】的名字匹配进程(僵尸跳过继续扫) */
        {
            HANDLE hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
            PROCESSENTRY32W pe;
            pe.dwSize = sizeof(pe);
            if (hSnap != INVALID_HANDLE_VALUE) {
                if (Process32FirstW(hSnap, &pe)) {
                    do {
                        int gi;
                        for (gi = 0; GAME_EXES[gi]; gi++) {
                            if (_wcsicmp(pe.szExeFile, GAME_EXES[gi]) == 0) break;
                        }
                        if (!GAME_EXES[gi]) continue;               /* 名字不匹配 */
                        if (!proc_alive(pe.th32ProcessID)) {        /* ★僵尸: 跳过并继续 */
                            int d, seen = 0;
                            for (d = 0; d < nDeadLogged; d++)
                                if (deadLogged[d] == pe.th32ProcessID) { seen = 1; break; }
                            if (!seen && nDeadLogged < 4) {
                                deadLogged[nDeadLogged++] = pe.th32ProcessID;
                                lg("[loader] game-named pid=%lu is a corpse (all threads exited) -> skip\n",
                                   (unsigned long)pe.th32ProcessID);
                            }
                            continue;
                        }
                        pid = pe.th32ProcessID;
                        hitName = GAME_EXES[gi];
                        break;
                    } while (Process32NextW(hSnap, &pe));
                }
                CloseHandle(hSnap);
            }
        }
        /* 同一个 pid: 上次注入成功则不重复; 失败则每 4 轮重试一次(最多 5 次) */
        if (pid && (int)pid == lastPid) {
            if (lastOk || skipCount >= 5) { Sleep(POLL_MS); continue; }
            if (++skipCount < 4) { Sleep(POLL_MS); continue; }
            skipCount = 0;
        }
        if (pid && (int)pid != lastPid) skipCount = 0;
        if (pid) {
            char nameA[64];
            int k;
            for (k = 0; k < 63 && hitName[k]; k++) nameA[k] = (char)hitName[k];
            nameA[k] = '\0';
            /* ★目标判定: 同目录直通 或 .dfovib_target 标记文件, 其余跳过 */
            if (!proc_is_target(pid)) {
                lg("[loader] %s pid=%lu not a marked target -> SKIP\n",
                       nameA, (unsigned long)pid);
                lastPid = (int)pid;
                lastOk = 1;               /* 有意跳过, 不再重试 */
                Sleep(POLL_MS);
                continue;
            }
            lg("[loader] %s pid=%lu found, injecting...\n", nameA, (unsigned long)pid);
            lastOk = inject_process(pid, g_dllPath);
            if (lastOk)
                lg("[loader] injected OK (pid %lu)\n", (unsigned long)pid);
            else
                lg("[loader] inject failed (pid %lu) - will retry\n", (unsigned long)pid);
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
        lg_open();
        lg("[loader] launcher mode: %s\n", argv[1]);
        if (!build_dll_path(dllPath, sizeof(dllPath))) {
            fprintf(stderr, "[loader] DLL not found next to loader: %s\n", DLL_NAME);
            return 1;
        }
        g_dllPath = dllPath;
        if (launch_and_inject(argv[1], g_dllPath, NULL))
            lg("[loader] launched + injected OK\n");
        else
            lg("[loader] launched, inject failed (game may still start ok)\n");
    } else {
        /* 常驻轮询模式 */
        if (!build_dll_path(dllPath, sizeof(dllPath))) {
            fprintf(stderr, "[loader] DLL not found next to loader: %s\n", DLL_NAME);
            fprintf(stderr, "用法: DfoVibration_OLD_Loader.exe [DNF.exe路径]\n");
            return 1;
        }
        g_dllPath = dllPath;
        lg_open();
        console_title_set("DfoVibration OLD Loader - watching game exe...");
        lg("[loader] watching for %s ...  (resident mode, dll=%s)\n", GAME_EXE, dllPath);
        _beginthreadex(NULL, 0, poll_thread, NULL, 0, NULL);
        while (g_running) Sleep(1000);
    }
    return 0;
}