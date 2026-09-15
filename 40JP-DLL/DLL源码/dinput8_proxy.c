/* ============================================================
 * DFO 老版本手柄震动 - dinput8.dll 代理(免 Loader 自动挂载)
 * ============================================================
 * 作用: 把 DfoVibration_OLD.dll 变成"免 Loader 自动加载":
 *   1) DLL 改名为 dinput8.dll 放在 DNF.exe 同目录 —— 老版本 DNF
 *      导入表必定加载 dinput8.dll(DirectInput 手柄/输入), Windows
 *      DLL 搜索顺序"应用目录优先于 System32" → 游戏启动即加载本代理。
 *   2) 代理线程(延迟 1s 避开 loader lock)LoadLibrary 同目录的
 *      DfoVibration_OLD.dll → 其 DllMain(DLL_PROCESS_ATTACH)自动
 *      启动 worker(建共享内存 + 装 hook) → 事件流直达 SorahkDFO.exe。
 *   3) 转发真实 System32\dinput8.dll 的全部 6 个导出, 保证游戏
 *      的 DirectInput 功能不受影响(第一次调用时懒加载真实 DLL)。
 *
 * 编译(32 位, 与 DNF.exe 架构一致):
 *   i686-w64-mingw32-gcc -shared -O2 -static-libgcc dinput8_proxy.c -o dinput8.dll
 * 部署:
 *   dinput8.dll + DfoVibration_OLD.dll 一起放 DNF.exe 同目录;
 *   以后直接启动 DNF.exe 即可, 不再需要 DfoVibration_OLD_Loader。
 * ============================================================ */
#include <windows.h>
#include <guiddef.h>

#define COLLECT_DLL_NAME "DfoVibration_OLD.dll"
#define PROXY_WORKER_DELAY_MS 1000

static HMODULE g_self = NULL;
static HMODULE g_real = NULL;          /* 真实 System32\dinput8.dll */
static volatile LONG g_workerStarted = 0;

/* ---- 懒加载真实 dinput8.dll(绝对路径, 不会撞到同目录代理) ---- */
static HMODULE real_dll(void)
{
    if (!g_real) {
        wchar_t sys[MAX_PATH];
        wchar_t path[MAX_PATH];
        UINT n = GetSystemDirectoryW(sys, MAX_PATH);
        if (n > 0 && n < MAX_PATH - 12) {
            size_t i;
            for (i = 0; sys[i] && i < MAX_PATH - 13; i++) path[i] = sys[i];
            path[i++] = L'\\';
            {
                static const wchar_t name[] = L"dinput8.dll";
                int k = 0;
                while (name[k] && i < MAX_PATH - 1) path[i++] = name[k++];
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

/* ---- 通用转发宏: 导出同名单函数, 直达真实 DLL ----
 * 注意: 导出名由 build_proxy.bat 传入的 dinput8.def 文件控制
 * (无 @n 修饰), 与 DNF.exe 导入表 "DirectInput8Create" 精确匹配。
 * 这里不再用 __declspec(dllexport), 避免 mingw 自动生成 @n 名。 */
#define FWD_STUB(ret, name, fnname, ARGS_DECL, ARGS_CALL) \
    ret WINAPI name ARGS_DECL \
    { \
        FARPROC p = real_fn(fnname); \
        if (!p) return (ret)0; \
        return ((ret (WINAPI *)ARGS_DECL)p) ARGS_CALL; \
    }

/* 1. DirectInput8Create(HINSTANCE hinst, DWORD dwVersion, REFIID riidltf,
 *    LPVOID *ppvOut, LPUNKNOWN punkOuter) */
FWD_STUB(HRESULT, DirectInput8Create, "DirectInput8Create",
    (HINSTANCE hinst, DWORD dwVersion, const IID *riidltf, void **ppvOut, void *punkOuter),
    (hinst, dwVersion, riidltf, ppvOut, punkOuter))

/* 2. DllCanUnloadNow(void) */
FWD_STUB(HRESULT, DllCanUnloadNow, "DllCanUnloadNow",
    (void), ())

/* 3. DllGetClassObject(REFCLSID rclsid, REFIID riid, LPVOID *ppv) */
FWD_STUB(HRESULT, DllGetClassObject, "DllGetClassObject",
    (const CLSID *rclsid, const IID *riid, void **ppv),
    (rclsid, riid, ppv))

/* 4. DllRegisterServer(void) */
FWD_STUB(HRESULT, DllRegisterServer, "DllRegisterServer",
    (void), ())

/* 5. DllUnregisterServer(void) */
FWD_STUB(HRESULT, DllUnregisterServer, "DllUnregisterServer",
    (void), ())

/* 6. GetdfDIJoystick(HWND hwnd, LPVOID lpvOut, DWORD dwVersion)
 *    (Wine spec: ptr ptr long; 非 DirectInput8Create 主路径, 兼容性转发) */
FWD_STUB(HRESULT, GetdfDIJoystick, "GetdfDIJoystick",
    (HWND hwnd, void *lpvOut, DWORD dwVersion),
    (hwnd, lpvOut, dwVersion))

/* ---- 采集 DLL 自动拉起线程 ----
 * DllMain 里不能直接 LoadLibrary(loader lock 风险), 延迟 1s 再拉:
 * 与采集 DLL 自身的 worker 延迟(1s)叠加, 全程避开 loader lock。 */
static DWORD WINAPI proxy_worker(LPVOID p)
{
    (void)p;
    Sleep(PROXY_WORKER_DELAY_MS);
    {
        char path[MAX_PATH];
        char *slash;
        DWORD n = GetModuleFileNameA(g_self, path, MAX_PATH);
        if (n == 0 || n >= MAX_PATH) return 0;
        slash = strrchr(path, '\\');
        if (!slash) return 0;
        *(slash + 1) = 0;
        /* 拼出同目录 COLLECT_DLL_NAME(手工拼接, 避免 _s 依赖) */
        {
            size_t used = (size_t)(slash - path) + 1;
            size_t room = MAX_PATH - used;
            size_t k = 0;
            while (COLLECT_DLL_NAME[k] && k < room - 1) {
                path[used + k] = COLLECT_DLL_NAME[k];
                k++;
            }
            path[used + k] = 0;
        }
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
        /* 真实 DLL 由系统持有, 不释放; 采集 DLL 由自身 DllMain detach 清理 */
    }
    return TRUE;
}