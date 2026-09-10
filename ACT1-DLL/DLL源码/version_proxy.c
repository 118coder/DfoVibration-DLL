/* ============================================================
 * DFO 老版本手柄震动 - version.dll 代理(免 Loader 自动挂载)
 * ============================================================
 * 挂载方式: 与工作区已接受的 SimSunFontHook 先例完全一致
 *   (E:\LX\A1模拟源码\tools\SimSunFontHook\):
 *   1) 新增 version.dll 到 DNF.exe 同目录(客户端目录原本没有
 *      version.dll, 不覆盖任何原始 DLL, 也不动 System32 原版);
 *   2) DNF.exe 导入表引用 version.dll(GetFileVersionInfoA/
 *      GetFileVersionInfoSizeA) -> Windows DLL 搜索\"应用目录优先
 *      于 System32\" -> 游戏启动即加载本代理;
 *   3) 延迟线程(1s 避开 loader lock)LoadLibraryA 同目录
 *      DfoVibration_OLD.dll -> 其 DllMain(ATTACH)自动启动 worker
 *      (建共享内存 Local\DfoVibrationShm + 装 hook) -> 事件流
 *      直达 SorahkDFO.exe 震动;
 *   4) 全部 version API 转发到 System32\version.dll 真身
 *      (GetSystemDirectoryW 绝对路径, 32 位自动重定向 SysWOW64)
 *      -> 游戏版本查询功能完全不受影响。
 *
 * 绝对不动: DNF.exe 自身 / 客户端已有 DLL / System32 原版 DLL。
 *
 * 编译(32 位, 与 DNF.exe 架构一致):
 *   i686-w64-mingw32-gcc -shared -O2 -static-libgcc \
 *     -o release\version.dll version_proxy.c version.def
 * 部署:
 *   version.dll + DfoVibration_OLD.dll 放 DNF.exe 同目录;
 *   直接启动 DNF.exe 即可, 不再需要 DfoVibration_OLD_Loader。
 * ============================================================ */
#include <windows.h>

#define COLLECT_DLL_NAME "DfoVibration_OLD.dll"
#define PROXY_WORKER_DELAY_MS 1000

static HMODULE g_self = NULL;
static HMODULE g_real = NULL;          /* 真实 System32\version.dll */
static volatile LONG g_workerStarted = 0;

/* ---- System32\version.dll 全部 17 个导出名(与 llvm-readobj 对照) ---- */
static const char *const kRealExports[] = {
    "GetFileVersionInfoA",
    "GetFileVersionInfoByHandle",
    "GetFileVersionInfoExA",
    "GetFileVersionInfoExW",
    "GetFileVersionInfoSizeA",
    "GetFileVersionInfoSizeExA",
    "GetFileVersionInfoSizeExW",
    "GetFileVersionInfoSizeW",
    "GetFileVersionInfoW",
    "VerFindFileA",
    "VerFindFileW",
    "VerInstallFileA",
    "VerInstallFileW",
    "VerLanguageNameA",
    "VerLanguageNameW",
    "VerQueryValueA",
    "VerQueryValueW",
};
enum { EXPORT_COUNT = 17 };

/* ---- 懒加载真实 System32\version.dll(绝对路径, 不撞同目录代理) ---- */
static HMODULE real_dll(void)
{
    if (!g_real) {
        wchar_t sys[MAX_PATH];
        wchar_t path[MAX_PATH];
        UINT n = GetSystemDirectoryW(sys, MAX_PATH);
        if (n > 0 && n < MAX_PATH - 12) {
            size_t i;
            int k;
            for (i = 0; sys[i] && i < MAX_PATH - 13; i++) path[i] = sys[i];
            path[i++] = L'\\';
            {
                static const wchar_t name[] = L"version.dll";
                for (k = 0; name[k] && i < MAX_PATH - 1; i++) path[i] = name[k++];
            }
            path[i] = 0;
            g_real = LoadLibraryW(path);
        }
    }
    return g_real;
}

static FARPROC real_fn(const char *name)
{
    HMODULE r = real_dll();
    if (!r) return NULL;
    return GetProcAddress(r, name);
}

/* ---- 通用转发宏: 以别名 Real_xxx 实现(def 里映射到导出名 xxx) ---- */
#define FWD_STUB(ret, alias, fnname, ARGS_DECL, ARGS_CALL) \
    ret WINAPI alias ARGS_DECL \
    { \
        FARPROC p = real_fn(fnname); \
        if (!p) return (ret)0; \
        return ((ret (WINAPI *)ARGS_DECL)p) ARGS_CALL; \
    }

/* 1. GetFileVersionInfoA(LPCSTR, DWORD, DWORD, LPVOID) */
FWD_STUB(BOOL, Real_GetFileVersionInfoA, "GetFileVersionInfoA",
    (LPCSTR a, DWORD b, DWORD c, LPVOID d),
    (a, b, c, d))

/* 2. GetFileVersionInfoByHandle(HANDLE, DWORD, DWORD, LPVOID) */
FWD_STUB(BOOL, Real_GetFileVersionInfoByHandle, "GetFileVersionInfoByHandle",
    (HANDLE a, DWORD b, DWORD c, LPVOID d),
    (a, b, c, d))

/* 3. GetFileVersionInfoExA(DWORD, LPCSTR, DWORD, DWORD, LPVOID) */
FWD_STUB(BOOL, Real_GetFileVersionInfoExA, "GetFileVersionInfoExA",
    (DWORD f, LPCSTR a, DWORD b, DWORD c, LPVOID d),
    (f, a, b, c, d))

/* 4. GetFileVersionInfoExW(DWORD, LPCWSTR, DWORD, DWORD, LPVOID) */
FWD_STUB(BOOL, Real_GetFileVersionInfoExW, "GetFileVersionInfoExW",
    (DWORD f, LPCWSTR a, DWORD b, DWORD c, LPVOID d),
    (f, a, b, c, d))

/* 5. GetFileVersionInfoSizeA(LPCSTR, LPDWORD) */
FWD_STUB(DWORD, Real_GetFileVersionInfoSizeA, "GetFileVersionInfoSizeA",
    (LPCSTR a, LPDWORD b),
    (a, b))

/* 6. GetFileVersionInfoSizeExA(DWORD, LPCSTR, LPDWORD) */
FWD_STUB(DWORD, Real_GetFileVersionInfoSizeExA, "GetFileVersionInfoSizeExA",
    (DWORD f, LPCSTR a, LPDWORD b),
    (f, a, b))

/* 7. GetFileVersionInfoSizeExW(DWORD, LPCWSTR, LPDWORD) */
FWD_STUB(DWORD, Real_GetFileVersionInfoSizeExW, "GetFileVersionInfoSizeExW",
    (DWORD f, LPCWSTR a, LPDWORD b),
    (f, a, b))

/* 8. GetFileVersionInfoSizeW(LPCWSTR, LPDWORD) */
FWD_STUB(DWORD, Real_GetFileVersionInfoSizeW, "GetFileVersionInfoSizeW",
    (LPCWSTR a, LPDWORD b),
    (a, b))

/* 9. GetFileVersionInfoW(LPCWSTR, DWORD, DWORD, LPVOID) */
FWD_STUB(BOOL, Real_GetFileVersionInfoW, "GetFileVersionInfoW",
    (LPCWSTR a, DWORD b, DWORD c, LPVOID d),
    (a, b, c, d))

/* 10. VerFindFileA(DWORD, LPCSTR, LPCSTR, LPCSTR, LPSTR, PUINT, LPSTR, PUINT) */
FWD_STUB(DWORD, Real_VerFindFileA, "VerFindFileA",
    (DWORD a, LPCSTR b, LPCSTR c, LPCSTR d, LPSTR e, PUINT f, LPSTR g, PUINT h),
    (a, b, c, d, e, f, g, h))

/* 11. VerFindFileW(DWORD, LPCWSTR, LPCWSTR, LPCWSTR, LPWSTR, PUINT, LPWSTR, PUINT) */
FWD_STUB(DWORD, Real_VerFindFileW, "VerFindFileW",
    (DWORD a, LPCWSTR b, LPCWSTR c, LPCWSTR d, LPWSTR e, PUINT f, LPWSTR g, PUINT h),
    (a, b, c, d, e, f, g, h))

/* 12. VerInstallFileA(DWORD,LPCSTR,LPCSTR,LPCSTR,LPCSTR,LPSTR,PUINT) */
FWD_STUB(DWORD, Real_VerInstallFileA, "VerInstallFileA",
    (DWORD a, LPCSTR b, LPCSTR c, LPCSTR d, LPCSTR e, LPSTR f, PUINT g),
    (a, b, c, d, e, f, g))

/* 13. VerInstallFileW(DWORD,LPCWSTR,LPCWSTR,LPCWSTR,LPCWSTR,LPWSTR,PUINT) */
FWD_STUB(DWORD, Real_VerInstallFileW, "VerInstallFileW",
    (DWORD a, LPCWSTR b, LPCWSTR c, LPCWSTR d, LPCWSTR e, LPWSTR f, PUINT g),
    (a, b, c, d, e, f, g))

/* 14. VerLanguageNameA(DWORD, LPSTR, DWORD) */
FWD_STUB(DWORD, Real_VerLanguageNameA, "VerLanguageNameA",
    (DWORD a, LPSTR b, DWORD c),
    (a, b, c))

/* 15. VerLanguageNameW(DWORD, LPWSTR, DWORD) */
FWD_STUB(DWORD, Real_VerLanguageNameW, "VerLanguageNameW",
    (DWORD a, LPWSTR b, DWORD c),
    (a, b, c))

/* 16. VerQueryValueA(LPCVOID, LPCSTR, LPVOID*, PUINT) */
FWD_STUB(BOOL, Real_VerQueryValueA, "VerQueryValueA",
    (LPCVOID a, LPCSTR b, LPVOID *c, PUINT d),
    (a, b, c, d))

/* 17. VerQueryValueW(LPCVOID, LPCWSTR, LPVOID*, PUINT) */
FWD_STUB(BOOL, Real_VerQueryValueW, "VerQueryValueW",
    (LPCVOID a, LPCWSTR b, LPVOID *c, PUINT d),
    (a, b, c, d))

/* ---- 采集 DLL 自动拉起线程(延迟 1s, 避开 loader lock) ---- */
static DWORD WINAPI proxy_worker(LPVOID p)
{
    (void)p;
    Sleep(PROXY_WORKER_DELAY_MS);
    {
        char path[MAX_PATH];
        char *slash;
        DWORD n = GetModuleFileNameA(g_self, path, MAX_PATH);
        size_t used;
        size_t room;
        size_t k;
        if (n == 0 || n >= MAX_PATH) return 0;
        slash = strrchr(path, '\\');
        if (!slash) return 0;
        *(slash + 1) = 0;
        used = (size_t)(slash - path) + 1;
        room = MAX_PATH - used;
        for (k = 0; COLLECT_DLL_NAME[k] && k < room - 1; k++) {
            path[used + k] = COLLECT_DLL_NAME[k];
        }
        path[used + k] = 0;
        /* 加载即触发采集 DLL 的 DllMain -> 自动启动 worker/collector */
        LoadLibraryA(path);
    }
    return 0;
}

BOOL WINAPI DllMain(HINSTANCE hDll, DWORD reason, LPVOID reserved)
{
    (void)reserved;
    if (reason == DLL_PROCESS_ATTACH) {
        g_self = hDll;
        DisableThreadLibraryCalls(hDll);
        if (InterlockedCompareExchange(&g_workerStarted, 1, 0) == 0) {
            HANDLE h = CreateThread(NULL, 0, proxy_worker, NULL, 0, NULL);
            if (h) CloseHandle(h);
        }
    } else if (reason == DLL_PROCESS_DETACH) {
        /* 真身由系统持有, 不释放; 采集 DLL 由自身 DllMain detach 清理 */
    }
    return TRUE;
}