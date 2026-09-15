/* inject_test64 : x64 injector (same logic as host auto_inject.rs) -> x86 victim.
 * Verifies: EnumProcessModulesEx(LIST_MODULES_32BIT) finds kernel32 of the
 * 32-bit target, export table resolves a 32-bit LoadLibraryA entry, remote
 * thread loads DfoVibration_OLD.dll into the target, and the collector's
 * DllMain(ATTACH) creates Local\DfoVibrationShm.
 *
 * usage: inject_test64.exe <pid> <dllpath> [expected-shm]
 */
#include <windows.h>
#include <psapi.h>
#include <stdio.h>
#include <wchar.h>

static int read_mem(HANDLE h, DWORD_PTR addr, void *buf, SIZE_T len, SIZE_T *got) {
    return ReadProcessMemory(h, (LPCVOID)addr, buf, len, got) ? 1 : 0;
}

/* Resolve `wanted` export RVA inside the 32-bit module at `base` in process h. */
static DWORD_PTR find_export32(HANDLE h, DWORD_PTR base, const char *wanted) {
    IMAGE_DOS_HEADER dos;
    SIZE_T got = 0;
    if (!read_mem(h, base, &dos, sizeof(dos), &got)) return 0;
    if (dos.e_magic != IMAGE_DOS_SIGNATURE) return 0;
    DWORD_PTR ntaddr = base + dos.e_lfanew;
    IMAGE_NT_HEADERS32 nth;
    if (!read_mem(h, ntaddr, &nth, sizeof(nth), &got)) return 0;
    if (nth.Signature != IMAGE_NT_SIGNATURE) return 0;
    if (nth.OptionalHeader.Magic != IMAGE_NT_OPTIONAL_HDR32_MAGIC) return 0;
    DWORD export_rva = nth.OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT].VirtualAddress;
    if (!export_rva) return 0;
    DWORD_PTR exp_addr = base + export_rva;
    IMAGE_EXPORT_DIRECTORY exp;
    if (!read_mem(h, exp_addr, &exp, sizeof(exp), &got)) return 0;
    DWORD_PTR names = base + exp.AddressOfNames;
    DWORD_PTR ordinals = base + exp.AddressOfNameOrdinals;
    DWORD_PTR funcs = base + exp.AddressOfFunctions;
    for (DWORD i = 0; i < exp.NumberOfNames; i++) {
        DWORD name_rva = 0;
        if (!read_mem(h, names + (DWORD_PTR)i * 4, &name_rva, 4, &got)) break;
        char nm[64];
        if (!read_mem(h, base + name_rva, nm, sizeof(nm), &got)) continue;
        nm[sizeof(nm) - 1] = 0;
        if (_stricmp(nm, wanted) == 0) {
            WORD ord = 0;
            if (!read_mem(h, ordinals + (DWORD_PTR)i * 2, &ord, 2, &got)) return 0;
            DWORD fn_rva = 0;
            if (!read_mem(h, funcs + (DWORD_PTR)ord * 4, &fn_rva, 4, &got)) return 0;
            return base + fn_rva;
        }
    }
    return 0;
}

int main(int argc, char **argv) {
    if (argc < 3) {
        fprintf(stderr, "usage: %s <pid> <dllpath>\n", argv[0]);
        return 2;
    }
    DWORD pid = (DWORD)strtoul(argv[1], NULL, 10);
    HANDLE h = OpenProcess(PROCESS_ALL_ACCESS, FALSE, pid);
    if (!h) { fprintf(stderr, "OpenProcess failed err=%lu\n", GetLastError()); return 1; }

    /* 1) enumerate 32-bit modules of target */
    DWORD needed = 0;
    if (!EnumProcessModulesEx(h, NULL, 0, &needed, LIST_MODULES_32BIT)) {
        fprintf(stderr, "EnumProcessModulesEx(size) failed err=%lu\n", GetLastError());
        return 1;
    }
    DWORD count = needed / sizeof(HMODULE);
    HMODULE *mods = (HMODULE *)malloc(needed);
    if (!EnumProcessModulesEx(h, mods, needed, &needed, LIST_MODULES_32BIT)) {
        fprintf(stderr, "EnumProcessModulesEx failed err=%lu\n", GetLastError());
        return 1;
    }
    DWORD_PTR k32 = 0;
    for (DWORD i = 0; i < count; i++) {
        wchar_t name[MAX_PATH];
        DWORD n = GetModuleBaseNameW(h, mods[i], name, MAX_PATH);
        if (n > 0 && _wcsicmp(name, L"kernel32.dll") == 0) {
            k32 = (DWORD_PTR)mods[i];
            break;
        }
    }
    if (!k32) { fprintf(stderr, "kernel32 not found among 32-bit modules\n"); return 1; }
    printf("[test] 32-bit kernel32 base = 0x%llX\n", (unsigned long long)k32);

    /* 2) resolve LoadLibraryA inside target's kernel32 */
    DWORD_PTR loadlib = find_export32(h, k32, "LoadLibraryA");
    if (!loadlib) { fprintf(stderr, "LoadLibraryA export not found\n"); return 1; }
    printf("[test] target LoadLibraryA = 0x%llX (32-bit)\n", (unsigned long long)loadlib);

    /* 3) write dll path into target */
    SIZE_T len = strlen(argv[2]) + 1;
    PVOID remote = VirtualAllocEx(h, NULL, len, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (!remote) { fprintf(stderr, "VirtualAllocEx failed\n"); return 1; }
    if (!WriteProcessMemory(h, remote, argv[2], len, NULL)) {
        fprintf(stderr, "WriteProcessMemory failed\n"); return 1;
    }

    /* 4) remote thread on target's 32-bit LoadLibraryA */
    HANDLE th = CreateRemoteThread(h, NULL, 0,
        (LPTHREAD_START_ROUTINE)loadlib, remote, 0, NULL);
    if (!th) { fprintf(stderr, "CreateRemoteThread failed err=%lu\n", GetLastError()); return 1; }
    WaitForSingleObject(th, 10000);
    DWORD exitcode = 0;
    GetExitCodeThread(th, &exitcode);
    CloseHandle(th);
    printf("[test] remote thread exit = %lu (0x%lX)\n", exitcode, exitcode);

    /* collector worker sleeps ~1s before creating the shm, so give it time */
    Sleep(2500);

    /* 5) verify shared memory was created by the collector DLL's DllMain */
    HANDLE shm = OpenFileMappingA(FILE_MAP_READ, FALSE, "Local\\DfoVibrationShm");
    int pass = 0;
    if (shm) {
        printf("[test] PASS: Local\\DfoVibrationShm exists (DllMain auto-started)\n");
        CloseHandle(shm);
        pass = 1;
    } else {
        fprintf(stderr, "[test] FAIL: shared memory not created err=%lu\n", GetLastError());
    }

    VirtualFreeEx(h, remote, 0, MEM_RELEASE);
    CloseHandle(h);
    return pass ? 0 : 1;
}