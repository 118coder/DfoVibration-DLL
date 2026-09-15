/* ============================================================
 * DfoVibration 90CN 注入器
 * 用法: vib_injector.exe [DLL完整路径]
 * 默认: 注入器同目录 DfoVibration_90CN_probe.dll
 * 流程: 轮询等待 DNF.exe 进程 (最长 10 分钟) -> 等 8 秒初始化
 *       -> CreateRemoteThread(LoadLibraryA) 注入
 * 若 DNF.exe 已在运行则立即注入。
 * ============================================================ */
#include <windows.h>
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0501
#endif
#include <stdio.h>
#include <string.h>
#include <tlhelp32.h>

static DWORD find_process(const char *name)
{
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    PROCESSENTRY32 pe;
    DWORD pid = 0;
    if (snap == INVALID_HANDLE_VALUE) return 0;
    pe.dwSize = sizeof(pe);
    if (Process32First(snap, &pe)) {
        do {
            if (_stricmp(pe.szExeFile, name) == 0) { pid = pe.th32ProcessID; break; }
        } while (Process32Next(snap, &pe));
    }
    CloseHandle(snap);
    return pid;
}

int main(int argc, char **argv)
{
    char dllPath[MAX_PATH * 2];
    char selfPath[MAX_PATH];
    char *slash;
    DWORD pid = 0;
    DWORD waited = 0;
    HANDLE hProc, hThread;
    LPVOID remoteMem;
    SIZE_T written;
    HMODULE k32;
    FARPROC fnLoadLibraryA;
    DWORD exitCode = 0;

    SetConsoleOutputCP(936);

    if (argc >= 2) {
        _snprintf(dllPath, sizeof(dllPath), "%s", argv[1]);
    } else {
        if (!GetModuleFileNameA(NULL, selfPath, MAX_PATH)) {
            printf("[错误] 无法获取自身路径\n"); system("pause"); return 1;
        }
        slash = strrchr(selfPath, '\\');
        if (!slash) { printf("[错误] 路径异常\n"); system("pause"); return 1; }
        *slash = 0;
        _snprintf(dllPath, sizeof(dllPath), "%s\\DfoVibration_90CN_probe.dll", selfPath);
    }

    if (GetFileAttributesA(dllPath) == INVALID_FILE_ATTRIBUTES) {
        printf("[错误] 找不到 DLL: %s\n", dllPath);
        system("pause");
        return 1;
    }
    /* 全路径反斜杠转双反斜杠显示无必要, 直接显示 */
    printf("==============================================\n");
    printf(" DfoVibration 90CN 注入器\n");
    printf(" DLL: %s\n", dllPath);
    printf("==============================================\n");

    /* 已在运行? */
    pid = find_process("DNF.exe");
    if (pid) {
        printf("[信息] 检测到 DNF.exe 已在运行 (pid=%lu), 等待 8 秒后注入...\n", (unsigned long)pid);
        Sleep(8000);
    } else {
        printf("[信息] 等待 DNF.exe 启动 (先开注入器再开游戏也可以)...\n");
        while (!pid && waited < 600000) {
            Sleep(500);
            waited += 500;
            pid = find_process("DNF.exe");
            if (pid) {
                printf("[信息] 检测到 DNF.exe (pid=%lu), 等待 8 秒初始化...\n", (unsigned long)pid);
                Sleep(8000);
            }
            if (waited % 10000 == 0 && waited > 0 && !pid)
                printf("[信息] 仍在等待... (%lu 秒)\n", (unsigned long)(waited / 1000));
        }
    }
    if (!pid) {
        printf("[失败] 10 分钟内未检测到 DNF.exe\n");
        system("pause");
        return 1;
    }

    /* 进程可能已退出 */
    hProc = OpenProcess(PROCESS_CREATE_THREAD | PROCESS_QUERY_INFORMATION |
                        PROCESS_VM_OPERATION | PROCESS_VM_WRITE | PROCESS_VM_READ,
                        FALSE, pid);
    if (!hProc) {
        printf("[失败] OpenProcess 失败 (%lu)。请尝试右键-以管理员身份运行注入器。\n",
               (unsigned long)GetLastError());
        system("pause");
        return 1;
    }

    remoteMem = VirtualAllocEx(hProc, NULL, strlen(dllPath) + 1,
                               MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (!remoteMem) {
        printf("[失败] VirtualAllocEx 失败 (%lu)\n", (unsigned long)GetLastError());
        CloseHandle(hProc);
        system("pause");
        return 1;
    }
    if (!WriteProcessMemory(hProc, remoteMem, dllPath, strlen(dllPath) + 1, &written)) {
        printf("[失败] WriteProcessMemory 失败 (%lu)\n", (unsigned long)GetLastError());
        VirtualFreeEx(hProc, remoteMem, 0, MEM_RELEASE);
        CloseHandle(hProc);
        system("pause");
        return 1;
    }

    k32 = GetModuleHandleA("kernel32.dll");
    fnLoadLibraryA = GetProcAddress(k32, "LoadLibraryA");
    /* 注意: 目标进程 kernel32 基址与本进程一致 (同一系统会话), 直接传地址安全 */
    hThread = CreateRemoteThread(hProc, NULL, 0,
                                 (LPTHREAD_START_ROUTINE)fnLoadLibraryA,
                                 remoteMem, 0, NULL);
    if (!hThread) {
        printf("[失败] CreateRemoteThread 失败 (%lu)\n", (unsigned long)GetLastError());
        VirtualFreeEx(hProc, remoteMem, 0, MEM_RELEASE);
        CloseHandle(hProc);
        system("pause");
        return 1;
    }
    WaitForSingleObject(hThread, 15000);
    GetExitCodeThread(hThread, &exitCode);
    CloseHandle(hThread);
    VirtualFreeEx(hProc, remoteMem, 0, MEM_RELEASE);
    CloseHandle(hProc);

    if (exitCode) {
        printf("[成功] 注入完成! DLL 模块句柄 0x%08lX\n", (unsigned long)exitCode);
        printf("[提示] 日志文件: 与 DLL 同目录 DfoVibration_90CN_probe_dll.log\n");
        printf("[提示] 进图打怪 5 分钟后即可退出游戏, 把日志发给 AI 分析。\n");
    } else {
        printf("[警告] 注入线程返回 0 (LoadLibrary 失败, 可能被拦截或路径含中文导致编码问题)\n");
        printf("[提示] 若失败: 把 DLL 放到纯英文路径后重试, 或用其它注入器加载后告知。\n");
    }
    system("pause");
    return 0;
}
