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

    hProc = OpenProcess(PROCESS_ALL_ACCESS, FALSE, pid);
    if (!hProc) {
        fprintf(stderr, "[inject] OpenProcess(%lu) failed: %lu\n", pid, GetLastError());
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