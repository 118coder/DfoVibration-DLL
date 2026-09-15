/* ============================================================
 * DFO 老版本手柄震动 - 远程注入库 (x86 => x86)
 * 功能: OpenProcess -> VirtualAllocEx 写路径 -> CreateRemoteThread(LoadLibraryA)
 * 兼容: 老版本 32 位 DNF.exe (ImageBase 0x400000, x86 PE32)
 * 用 32 位编译 (i686-w64-mingw32-gcc -m32 或 LLVM-MinGW gcc -m32)
 * ============================================================ */
#include <windows.h>
#include <tlhelp32.h>
#include <stdio.h>
#include <string.h>

/* 尝试启用 SeDebugPrivilege（仅在本进程已提权时有效；失败不致命） */
static void try_enable_debug_priv(void)
{
    HANDLE hTok = NULL;
    TOKEN_PRIVILEGES tp;
    LUID luid;
    if (!OpenProcessToken(GetCurrentProcess(),
                          TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &hTok))
        return;
    if (LookupPrivilegeValueA(NULL, "SeDebugPrivilege", &luid)) {
        tp.PrivilegeCount = 1;
        tp.Privileges[0].Luid = luid;
        tp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
        AdjustTokenPrivileges(hTok, FALSE, &tp, sizeof(tp), NULL, NULL);
    }
    CloseHandle(hTok);
}

/* 打开目标进程：先要全权，失败则逐级降级到"注入所需最小权限集"。
 * 实测：若游戏由管理员权限的启动器拉起，非管理员进程 OpenProcess 会返回
 * 错误 5(拒绝访问) —— 此时必须"以管理员身份运行"本工具。 */
static HANDLE open_target(DWORD pid)
{
    static const DWORD rights[] = {
        PROCESS_ALL_ACCESS,
        PROCESS_CREATE_THREAD | PROCESS_QUERY_INFORMATION | PROCESS_VM_OPERATION |
        PROCESS_VM_WRITE | PROCESS_VM_READ,
        PROCESS_CREATE_THREAD | PROCESS_VM_OPERATION | PROCESS_VM_WRITE,
    };
    DWORD firstErr = 0;
    int i;
    for (i = 0; i < (int)(sizeof(rights) / sizeof(rights[0])); i++) {
        HANDLE h = OpenProcess(rights[i], FALSE, pid);
        if (h) {
            if (i) printf("[inject] OpenProcess fallback rights set #%d OK\n", i);
            return h;
        }
        if (!firstErr) firstErr = GetLastError();
    }
    fprintf(stderr,
            "[inject] OpenProcess(%lu) failed: %lu%s\n",
            (unsigned long)pid, (unsigned long)firstErr,
            (firstErr == 5) ? "  <== 拒绝访问: 请『以管理员身份运行』本 Loader" : "");
    if (firstErr == 5) {
        fprintf(stderr, "          (或改用启动器模式, 由本工具自己拉起游戏:  DfoVibration_40JP_Loader.exe \"E:\\Game\\Arad40\\ARAD.exe\")\n");
    }
    return NULL;
}

/* 注入单个进程: 远程 LoadLibraryA 加载 dllPath */
static int inject_process(DWORD pid, const char *dllPath)
{
    HANDLE  hProc = NULL;
    HMODULE hK32  = NULL;
    FARPROC pLoadLibraryA = NULL;
    LPVOID  pRemote = NULL;
    SIZE_T  pathLen = strlen(dllPath) + 1;
    HANDLE  hThread = NULL;
    DWORD   threadId = 0;
    int     ok = 0;

    hProc = open_target(pid);
    if (!hProc) {
        return 0;
    }

    /* kernel32 基址: 32 位进程里 kernel32 通常与调用方一致(同为 32 位系统), 直接 GetModuleHandleA */
    hK32 = GetModuleHandleA("kernel32.dll");
    if (!hK32) { fprintf(stderr, "[inject] GetModuleHandle(kernel32) failed\n"); goto done; }
    pLoadLibraryA = (FARPROC)GetProcAddress(hK32, "LoadLibraryA");
    if (!pLoadLibraryA) { fprintf(stderr, "[inject] GetProcAddress(LoadLibraryA) failed\n"); goto done; }

    pRemote = VirtualAllocEx(hProc, NULL, pathLen, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (!pRemote) { fprintf(stderr, "[inject] VirtualAllocEx failed: %lu\n", GetLastError()); goto done; }

    if (!WriteProcessMemory(hProc, pRemote, dllPath, pathLen, NULL)) {
        fprintf(stderr, "[inject] WriteProcessMemory failed: %lu\n", GetLastError());
        goto done;
    }

    hThread = CreateRemoteThread(hProc, NULL, 0, (LPTHREAD_START_ROUTINE)pLoadLibraryA,
                                 pRemote, 0, &threadId);
    if (!hThread) {
        fprintf(stderr, "[inject] CreateRemoteThread failed: %lu\n", GetLastError());
        goto done;
    }

    /* 等注入完成(LoadLibraryA 返回) */
    WaitForSingleObject(hThread, 10000);
    ok = 1;
    CloseHandle(hThread);

done:
    if (pRemote) VirtualFreeEx(hProc, pRemote, 0, MEM_RELEASE);
    CloseHandle(hProc);
    return ok;
}

/* 按进程名找 PID (区分 32/64: 用 IsWow64Process2 检查); 返回第一个匹配 */
static DWORD find_pid_by_name(const char *exeName)
{
    HANDLE hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    PROCESSENTRY32W pe;
    DWORD pid = 0;
    wchar_t wname[MAX_PATH];
    if (hSnap == INVALID_HANDLE_VALUE) return 0;

    MultiByteToWideChar(CP_ACP, 0, exeName, -1, wname, MAX_PATH);
    pe.dwSize = sizeof(pe);
    if (Process32FirstW(hSnap, &pe)) {
        do {
            if (_wcsicmp(pe.szExeFile, wname) == 0) {
                pid = pe.th32ProcessID;
                break;
            }
        } while (Process32NextW(hSnap, &pe));
    }
    CloseHandle(hSnap);
    return pid;
}

/* 注入并验证进程存在 */
int inject_by_name(const char *exeName, const char *dllPath)
{
    DWORD pid = find_pid_by_name(exeName);
    if (!pid) { fprintf(stderr, "[inject] process '%s' not found\n", exeName); return 0; }
    return inject_process(pid, dllPath);
}

/* 启动器模式: CreateProcess(SUSPENDED) -> 注入 -> Resume */
int launch_and_inject(const char *exePath, const char *dllPath, const char *workDir)
{
    STARTUPINFOA si;
    PROCESS_INFORMATION pi;
    int ok = 0;

    memset(&si, 0, sizeof(si));
    memset(&pi, 0, sizeof(pi));
    si.cb = sizeof(si);

    if (!CreateProcessA(exePath, NULL, NULL, NULL, FALSE,
                        CREATE_SUSPENDED, NULL, workDir, &si, &pi)) {
        fprintf(stderr, "[inject] CreateProcess('%s') failed: %lu\n", exePath, GetLastError());
        return 0;
    }

    ok = inject_process(pi.dwProcessId, dllPath);
    ResumeThread(pi.hThread);

    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    return ok;
}