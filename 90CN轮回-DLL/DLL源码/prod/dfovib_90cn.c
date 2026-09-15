/* ============================================================
 * 90CN 生产版 - 采集 DLL 主程序 (SorahkDFO 宿主对齐)
 * 挂载: DfoVibration_90CN_Loader.exe (v1.2+) 注入, DLL 文件名 = DfoVibration.dll
 *       (宿主只认游戏进程内的 DfoVibration.dll 模块名)
 * 链路: FONT hook + 只读轮询 -> Local\DfoVibrationShm 环形缓冲 -> 宿主发震动
 * 架构依据: 开发文档/四轮实测定稿_探针v1.3.md
 *   - 行为事件/TextOutW = 死路线, 不实现
 *   - 进图门控 (n2500 有效 + 30s) 后才装 hook/开采集 (探针方案)
 *   - 进程退出 (DETACH reserved!=NULL) 跳过清理, 防退出时序 AV (探针方案)
 * ============================================================ */
#include <windows.h>
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0501
#endif
#ifndef WINVER
#define WINVER 0x0501
#endif
#include <stdio.h>
#include <string.h>
#include "hooks_90cn.h"
#include "../common/vib_protocol.h"
#include "../common/vib_90cn_addrs.h"
#include "../common/ini_util.h"

/* ---------------- 镜头震动接口开关 ----------------
 * 1 = 进图门控通过后随 FONT hook 一起安装 4 个震动 hook (探针 v1.4 实测定位后启用)
 * 0 = 不安装 (震动路径未实证时的安全默认)
 * 2026-09-14 v1.1: CONVERGE 实测活跃 -> 启用; 分发对齐 US (只发 CRIT_SHAKE) */
#define CN_SHAKE_ENABLE  1

/* ---------------- 全局状态 ---------------- */
static HANDLE  g_hMap = NULL;
static VibShm *g_shm = NULL;
static volatile int g_running = 0;
volatile int g_ringReady = 0;   /* v1.5: 去 static (hooks_90cn.c 的 vib_dispatch_shake_entry 引用) */
static volatile int g_workerStarted = 0;
static volatile int g_collectEnabled = 0;   /* 门控通过后置 1 */
static HANDLE g_hWorker = NULL;
static HANDLE g_hCollector = NULL;
static HMODULE g_hDllMod = NULL;
static char g_iniPath[MAX_PATH] = "";
static char g_logPath[MAX_PATH] = "";

/* ---------------- 日志 ---------------- */
void dll_log2(const char *fmt, ...)
{
    FILE *fp;
    char buf[512];
    va_list ap;
    if (!g_logPath[0]) return;
    va_start(ap, fmt);
    _vsnprintf(buf, sizeof(buf) - 1, fmt, ap);
    va_end(ap);
    fp = fopen(g_logPath, "a");
    if (fp) { fprintf(fp, "[%lu] %s\n", (unsigned long)GetCurrentThreadId(), buf); fclose(fp); }
}

static void log_rotate_check(void)
{
    HANDLE h;
    DWORD size;
    char oldPath[MAX_PATH];
    if (!g_logPath[0]) return;
    h = CreateFileA(g_logPath, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE,
                    NULL, OPEN_EXISTING, 0, NULL);
    if (h == INVALID_HANDLE_VALUE) return;
    size = GetFileSize(h, NULL);
    CloseHandle(h);
    if (size > 2 * 1024 * 1024) {
        _snprintf(oldPath, sizeof(oldPath), "%s.old", g_logPath);
        DeleteFileA(oldPath);
        MoveFileA(g_logPath, oldPath);
    }
}

/* ---------------- 共享内存环形缓冲 (US 同款 CAS 环) ----------------
 * VibShm 为 pack(1) 协议结构, 编译器无法证明 CAS 目标 4 字节对齐而告警;
 * 实际 MapViewOfFile 返回页对齐基址, head@52/tail@56/capacity@60 偏移均为
 * 4 的倍数, 对齐成立 (US 同款结构实机运行数月), 抑制该诊断 */
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wsync-alignment"
static void ring_push(VibEvent *ev)
{
    VibRing *r;
    DWORD head, tail, pos, sz = sizeof(VibEvent), first;

    if (!g_shm) return;
    r = &g_shm->ring;
    for (;;) {
        head = r->head;
        tail = r->tail;
        if ((DWORD)(head - tail) >= r->capacity)
            return;                       /* 满: 丢弃 (不影响游戏) */
        pos = head % r->capacity;
        if (pos + sz <= r->capacity) {
            memcpy(r->data + pos, ev, sz);
        } else {
            first = r->capacity - pos;
            memcpy(r->data + pos, ev, first);
            memcpy(r->data, (BYTE *)ev + first, sz - first);
        }
        if (InterlockedCompareExchange((volatile LONG *)&r->head,
                                       (LONG)(head + sz), (LONG)head) == (LONG)head) {
            break;
        }
    }
    InterlockedIncrement((volatile LONG *)&g_shm->seq);
    g_shm->lastTick = ev->tick;
}

void vib_collect(int type, DWORD strength, DWORD tick, DWORD count)
{
    VibEvent ev;
    if (!g_ringReady) return;
    ev.type = (DWORD)type;
    ev.strength = strength;
    ev.tick = tick;
    ev.reserved = count;
    ring_push(&ev);
}

static int shm_open(void)
{
    if (g_shm) return 1;
    g_hMap = CreateFileMappingA(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE,
                                0, sizeof(VibShm), VIB_SHM_NAME);
    if (!g_hMap) {
        g_hMap = OpenFileMappingA(FILE_MAP_ALL_ACCESS, FALSE, VIB_SHM_NAME);
        if (!g_hMap) return 0;
    }
    g_shm = (VibShm *)MapViewOfFile(g_hMap, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(VibShm));
    if (!g_shm) {
        CloseHandle(g_hMap);
        g_hMap = NULL;
        return 0;
    }
    if (g_shm->magic != VIB_SHM_MAGIC) {
        memset(g_shm, 0, sizeof(VibShm));
        g_shm->magic = VIB_SHM_MAGIC;
        g_shm->version = VIB_SHM_VERSION;
        g_shm->ring.capacity = VIB_RING_SIZE;
    }
    g_shm->gamePid = GetCurrentProcessId();
    g_shm->flags |= 1;
    return 1;
}

static void shm_close(void)
{
    if (g_shm) {
        g_shm->magic = 0;            /* 通知宿主连接失效 */
        g_shm->flags &= ~1;
        g_shm->flags &= ~2;
        UnmapViewOfFile(g_shm);
        g_shm = NULL;
    }
    if (g_hMap) { CloseHandle(g_hMap); g_hMap = NULL; }
}

/* ---------------- 只读内存访问 ---------------- */
static int mem_valid(DWORD addr, DWORD size)
{
    MEMORY_BASIC_INFORMATION mbi;
    if (VirtualQuery((LPCVOID)addr, &mbi, sizeof(mbi)) == 0)
        return 0;
    if (mbi.State != MEM_COMMIT)
        return 0;
    if (mbi.Protect & (PAGE_NOACCESS | PAGE_GUARD))
        return 0;
    if ((DWORD)((BYTE *)mbi.BaseAddress + mbi.RegionSize - addr) < size)
        return 0;
    return 1;
}

static DWORD try_read(DWORD addr)
{
    if (!mem_valid(addr, 4)) return 0;
    return *(volatile DWORD *)addr;
}

/* 安装前签名校验 */
static int sig_match(DWORD ea, const BYTE *expect, int n)
{
    if (!mem_valid(ea, n)) return 0;
    return memcmp((const void *)ea, expect, n) == 0;
}

/* ---------------- collector 线程 (6ms 聚合冲刷 + 轮询) ---------------- */
static DWORD WINAPI collector_thread(LPVOID p)
{
    (void)p;
    while (g_running) {
        if (g_collectEnabled) {
            vib_flush_counts();
            rank_poll();
            vib_flush_rank_extra();
            /* v1.5: 震屏改由 stub_6 (0x03349740 [shake screen] 词条执行器) 精确触发;
             * 0x790 上升沿机制代码保留, 防双源重复震动 (恢复 = 放开下行) */
            // vib_poll_shake_ctl();
        }
        Sleep(6);
    }
    return 0;
}

/* ---------------- worker 线程 (门控 + 安装 + ini 开关) ---------------- */
/* ini/log 按 DLL 自身文件名派生 (宿主只认 DfoVibration.dll 模块名):
 *   DfoVibration.dll -> DfoVibration.ini / DfoVibration_dll.log (US 生态同款) */
static void build_paths(void)
{
    char dllPath[MAX_PATH];
    char *slash, *dot;

    GetModuleFileNameA(g_hDllMod, dllPath, MAX_PATH);
    slash = strrchr(dllPath, '\\');
    if (!slash) { strcpy(g_iniPath, "DfoVibration.ini"); goto derivLog; }
    *slash = 0;
    dot = strrchr(slash + 1, '.');
    if (dot) *dot = 0;                       /* 去掉 .dll */
    _snprintf(g_iniPath, MAX_PATH, "%s\\%s.ini", dllPath, slash + 1);
derivLog:
    strcpy(g_logPath, g_iniPath);
    dot = strrchr(g_logPath, '.');
    if (dot) *dot = 0;
    strcat(g_logPath, "_dll.log");
}

static DWORD WINAPI worker_thread(LPVOID param)
{
    int lastEnabled = -1;
    VibConfig cfg;
    DWORD workerStart;

    (void)param;
    /* 延迟 1s: 避开 DllMain loader lock 窗口 */
    Sleep(1000);
    build_paths();
    log_rotate_check();
    dll_log2("========== 90CN 生产版 v7.0 (手感对齐US: FONT聚合/MOVE速度映射/轮询6ms) pid=%lu ini=%s ==========",
             (unsigned long)GetCurrentProcessId(), g_iniPath);
    workerStart = GetTickCount();

    for (;;) {
        DWORD now;
        if (!g_running) break;
        now = GetTickCount();

        /* ini 开关 (enabled=0 时卸载并停采集, 宿主/用户可控) */
        if (g_iniPath[0]) ini_load_config(g_iniPath, &cfg);
        else cfg.enabled = 1;

        /* 心跳 (30s) */
        {
            static DWORD s_beat = 0;
            if (now - s_beat >= 30000) {
                s_beat = now;
                dll_log2("alive enabled=%d collect=%d hooks=%d shm=%d",
                         cfg.enabled, g_collectEnabled, hooks_active(), g_shm ? 1 : 0);
            }
        }

        if (cfg.enabled != lastEnabled) {
            dll_log2("enabled change %d -> %d", lastEnabled, cfg.enabled);
            if (cfg.enabled) {
                if (shm_open()) {
                    lastEnabled = cfg.enabled;
                } else {
                    dll_log2("shm_open FAILED (宿主未运行? 保持重试)");
                    Sleep(2000);
                    continue;
                }
            } else {
                g_collectEnabled = 0;
                hooks_uninstall();
                g_ringReady = 0;
                shm_close();
                lastEnabled = cfg.enabled;
            }
        }

        /* 进图门控 (探针四轮方案): n2500 指针有效 + 启动 >=30s */
        if (!g_collectEnabled && cfg.enabled) {
            DWORD gp = try_read(CN_N2500_PTR);
            if (gp && mem_valid(gp, 0x100) && now - workerStart >= 30000) {
                /* FONT hook: 签名校验先行 (探针四轮验证的签名) */
                static const BYTE fontSig[6] = { 0x55, 0x8B, 0xEC, 0x6A, 0xFF, 0x68 };
                DWORD targets[8];
                int n = 0, ok = 0;

                if (sig_match(CN_DAMAGE_FONT, fontSig, 6)) {
                    targets[n++] = CN_DAMAGE_FONT;
                } else {
                    dll_log2("SELFCHK damage_font @0x%08X 签名不符, 拒绝 hook (版本变了?)",
                             CN_DAMAGE_FONT);
                }
                /* ---------------- v4.0 稳定基线 ----------------
                 * 背景: 死亡信号的多轮探索(同源定位 / 计数探针 / ACT 通知分发器路线)都未落地,
                 *   且 v3.1 因 stub 收尾破坏 eax 导致崩溃。v4.0 把**探针 hook 全部移除**,
                 *   只保留已验证成熟的采集通道, 保证可正常游玩:
                 *     - FONT hook (damage_font 0x013ECEC0): 命中/受击/特效/状态/特殊/DOT 六通道
                 *     - converge (0x026382C0): 相机震动汇聚点 (技能级震屏分类用)
                 *     - 轮询: 释放技能(pl+0x5DA0)/评分点/评分等级/破甲/凌空/移动
                 * 引擎侧保留本轮的两个真实修复(与是否启用无关):
                 *   ① insn_len 补 SIB(rm==0x04) 分支
                 *   ② stub 收尾改 `jmp dword ptr [tramp]`(不破坏 eax)
                 * 【怪物死亡】通道 (VEV_TARGET_DIE) 仍无信号源 —— 见交接文档 §四 现状与选项。 */
                targets[n++] = CN_SHAKE_CONVERGE;
                if (n > 0) ok = hooks_install(targets, n);
                dll_log2("门控通过 (uptime=%lums, pl=0x%08X), hooks=%d/%d%s",
                         (unsigned long)(now - workerStart), gp, ok, n,
#if CN_SHAKE_ENABLE
                         " (含震动接口)"
#else
                         " (震动接口预留未启用)"
#endif
                        );
                g_collectEnabled = 1;
                g_ringReady = hooks_active() ? 1 : 1;   /* 轮询事件不依赖 hook 成功 */
                g_shm->flags |= 2;
            }
        }
        Sleep(500);
    }

    /* worker 退出 (仅显式卸载路径; 进程退出时 DETACH 不等线程) */
    g_collectEnabled = 0;
    hooks_uninstall();
    g_ringReady = 0;
    shm_close();
    return 0;
}

/* ---------------- 导出 (注入器/宿主可选调用, 幂等) ---------------- */
__declspec(dllexport) void __cdecl DfoVibrationLoaded(void)
{
    if (InterlockedCompareExchange((volatile LONG *)&g_workerStarted, 1, 0) == 0) {
        g_running = 1;
        CreateThread(NULL, 0, worker_thread, NULL, 0, NULL);
        CreateThread(NULL, 0, collector_thread, NULL, 0, NULL);
    }
}

__declspec(dllexport) void __cdecl VibPluginLoaded(void)
{
    DfoVibrationLoaded();
}

__declspec(dllexport) void __cdecl VibPluginDeinit(void)
{
    g_running = 0;
}

BOOL WINAPI DllMain(HINSTANCE hDll, DWORD reason, LPVOID reserved)
{
    if (reason == DLL_PROCESS_ATTACH) {
        g_hDllMod = (HMODULE)hDll;
        DisableThreadLibraryCalls(hDll);
        if (InterlockedCompareExchange((volatile LONG *)&g_workerStarted, 1, 0) == 0) {
            g_running = 1;
            g_hWorker = CreateThread(NULL, 0, worker_thread, NULL, 0, NULL);
            g_hCollector = CreateThread(NULL, 0, collector_thread, NULL, 0, NULL);
        }
    } else if (reason == DLL_PROCESS_DETACH) {
        /* 探针四轮定案: 进程退出 (reserved != NULL) 时清理必崩 (退出时序 AV),
         * 只停线程标志, 由 OS 统一回收; 显式 FreeLibrary 才走完整卸载 */
        g_running = 0;
        if (reserved == NULL) {
            if (g_hWorker) { WaitForSingleObject(g_hWorker, 1500); g_hWorker = NULL; }
            if (g_hCollector) { WaitForSingleObject(g_hCollector, 1500); g_hCollector = NULL; }
            g_collectEnabled = 0;
            hooks_uninstall();
            g_ringReady = 0;
            shm_close();
        }
    }
    return TRUE;
}
