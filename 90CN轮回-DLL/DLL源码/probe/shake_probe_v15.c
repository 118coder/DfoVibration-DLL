/* ============================================================
 * 90CN 探针 v1.5 - 震屏技能专属信号定位 (两轮对照实验)
 *
 * 背景 (v1.4 实测 + v1.2 生产版实测教训):
 *   - converge 唯一活跃调用者是高频通用攻击演出 (每技能每 hit), 非震屏专属
 *   - FONT 命中通道 (a6=0x01) 是"每个技能都震"的主源, 与震屏无关 (命中即震)
 *   - 用户目标: 定位"只有震屏技能才触发"的信号
 *
 * v1.5 修复 v1.4 的三个盲区:
 *   1) CAMDIFF 60 轮上限未绑进图门控 -> 战斗期零采样 (60 轮在进图前耗尽)
 *      v1.5: 绑定 inGame + 无轮数上限 + 30ms 采样 + 曾变偏移汇总表
 *   2) converge 日志 300ms 节流吞数据 -> 改为 10ms + ret 分布频次表 (完整统计)
 *   3) font stub 取参偏移错 ([esp+76]=a7) -> 修正 [esp+72]=a6 (v1.3 同款),
 *      并加 6 通道位分布统计 (0x01/0x02/0x04/0x08/0x10/0x20)
 *
 * 对照实验设计 (用户实测):
 *   轮 A: 全程使用"不震屏"技能   -> 记账
 *   轮 B: 全程使用"震屏"技能     -> 记账
 *   对比心跳汇总的"曾变偏移表"差异 = 震屏专属相机/控制器字段
 *
 * 部署: 文件名必须为 DfoVibration.dll (宿主/Loader 只认此名),
 *       实测时替换生产版 v1.2, 测完换回 (v1.2 在 release_backup/prod_v1.2)。
 * 日志: DfoVibration_shake_probe_dll.log
 *
 * 安全底座: v1.3/v1.4 已验证 (门控 n2500+30s / VirtualProtect / VEH 被动 /
 *           退出 reserved!=NULL 跳过清理 / insn_len 完整版)
 * ============================================================ */
#include <windows.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include "../common/vib_90cn_addrs.h"

/* ---------------- 日志 ---------------- */
static char g_logPath[MAX_PATH] = "";
static volatile LONG g_logBusy = 0;

static void rotate_check(void)
{
    HANDLE h; DWORD size; char old[MAX_PATH];
    if (!g_logPath[0]) return;
    h = CreateFileA(g_logPath, GENERIC_READ, FILE_SHARE_READ|FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
    if (h == INVALID_HANDLE_VALUE) return;
    size = GetFileSize(h, NULL);
    CloseHandle(h);
    if (size > 4*1024*1024) {
        _snprintf(old, sizeof(old), "%s.old", g_logPath);
        DeleteFileA(old);
        MoveFileA(g_logPath, old);
    }
}

static void plog(const char *fmt, ...)
{
    FILE *fp; char buf[800]; va_list ap;
    if (!g_logPath[0]) return;
    if (InterlockedCompareExchange(&g_logBusy, 1, 0) != 0) return;
    va_start(ap, fmt);
    _vsnprintf(buf, sizeof(buf)-1, fmt, ap);
    va_end(ap);
    fp = fopen(g_logPath, "a");
    if (fp) {
        fprintf(fp, "[%lu] %s\n", (unsigned long)GetTickCount(), buf);
        fclose(fp);
    }
    InterlockedExchange(&g_logBusy, 0);
}

/* ---------------- 守卫读 ---------------- */
static int mem_readable(DWORD addr, DWORD span)
{
    MEMORY_BASIC_INFORMATION mbi;
    BYTE *p = (BYTE *)addr, *end;
    if (!addr) return 0;
    end = (BYTE *)(addr + span);
    if (end < p) return 0;
    while (p < end) {
        if (VirtualQuery((LPCVOID)p, &mbi, sizeof(mbi)) == 0) return 0;
        if (mbi.State != MEM_COMMIT) return 0;
        if (mbi.Protect & (PAGE_NOACCESS | PAGE_GUARD)) return 0;
        if ((BYTE *)mbi.BaseAddress + mbi.RegionSize >= end) return 1;
        p = (BYTE *)mbi.BaseAddress + mbi.RegionSize;
    }
    return 1;
}

static DWORD try_read(DWORD addr)
{
    if (!mem_readable(addr, 4)) return 0;
    return *(volatile DWORD *)addr;
}

static int in_text(DWORD a) { return a >= CN_TEXT_LO && a < CN_TEXT_HI; }

/* ---------------- VEH 被动记录器 (v1.2 方案) ---------------- */
static PVOID g_vehHandle = NULL;
static volatile int g_phase = 0;
static volatile int g_vehLogged = 0;

static LONG WINAPI probe_veh(PEXCEPTION_POINTERS ep)
{
    DWORD code = ep->ExceptionRecord->ExceptionCode;
    if (g_vehLogged < 20 && (code == EXCEPTION_ACCESS_VIOLATION ||
                              code == EXCEPTION_ILLEGAL_INSTRUCTION ||
                              code == EXCEPTION_PRIV_INSTRUCTION)) {
        g_vehLogged++;
        plog("EXCP phase=%d code=0x%08X eip=0x%08X",
             g_phase, code, (DWORD)(size_t)ep->ContextRecord->Eip);
    }
    return EXCEPTION_CONTINUE_SEARCH;
}

/* ---------------- hook 引擎 (v1.3 已验证) ---------------- */
#define PROBE_MAX_HOOKS 6
extern void probe_stub_0(void); extern void probe_stub_1(void);
extern void probe_stub_2(void); extern void probe_stub_3(void);
extern void probe_stub_4(void); extern void probe_stub_5(void);
void *probe_tramp_0, *probe_tramp_1, *probe_tramp_2,
     *probe_tramp_3, *probe_tramp_4, *probe_tramp_5;

static void *const s_stubs[PROBE_MAX_HOOKS] = {
    (void*)probe_stub_0, (void*)probe_stub_1, (void*)probe_stub_2,
    (void*)probe_stub_3, (void*)probe_stub_4, (void*)probe_stub_5
};
static void **const s_trampVars[PROBE_MAX_HOOKS] = {
    &probe_tramp_0, &probe_tramp_1, &probe_tramp_2,
    &probe_tramp_3, &probe_tramp_4, &probe_tramp_5
};

typedef struct {
    DWORD target; BYTE original[16]; int hookLen; int installed;
} ProbeHook;
static ProbeHook g_hooks[PROBE_MAX_HOOKS];
static int g_hookCount = 0;
static BYTE *g_trampPage = NULL;

static int insn_len(BYTE *p)
{
    BYTE op = p[0];
    switch (op) {
    case 0x90: case 0xC3: case 0xCC: case 0xF4:
    case 0xF5: case 0xF8: case 0xF9: case 0xFA: case 0xFB:
        return 1;
    case 0xC2: return 3;
    case 0x50: case 0x51: case 0x52: case 0x53: case 0x54:
    case 0x55: case 0x56: case 0x57:
    case 0x58: case 0x59: case 0x5A: case 0x5B: case 0x5C:
    case 0x5D: case 0x5E: case 0x5F:
    case 0x40: case 0x41: case 0x42: case 0x43: case 0x44:
    case 0x45: case 0x46: case 0x47: case 0x48: case 0x49:
    case 0x4A: case 0x4B: case 0x4C: case 0x4D: case 0x4E: case 0x4F:
        return 1;
    case 0x6A: return 2;
    case 0x68: return 5;
    case 0xE9: case 0xE8: return 5;
    case 0xEB: return 2;
    case 0x0F:
        return (p[1] & 0xF0) == 0x80 ? 6 : 2;
    case 0x66: case 0x67: case 0xF3: case 0xF2: case 0x2E:
    case 0x36: case 0x3E: case 0x26: case 0x64: case 0x65:
        return 1 + insn_len(p + 1);
    case 0xB0: case 0xB1: case 0xB2: case 0xB3: case 0xB4:
    case 0xB5: case 0xB6: case 0xB7: return 2;
    case 0xB8: case 0xB9: case 0xBA: case 0xBB: case 0xBC:
    case 0xBD: case 0xBE: case 0xBF: return 5;
    case 0xC7: {
        BYTE modrm = p[1];
        if ((modrm & 0xC0) == 0xC0) return 6;
        if ((modrm & 0x07) == 0x05) return 10;
        if ((modrm & 0xC0) == 0x40) return 7;
        if ((modrm & 0xC0) == 0x80) return 10;
        if ((modrm & 0x07) == 0x04) return 7;
        return 6;
    }
    case 0x83: {
        BYTE modrm = p[1];
        if ((modrm & 0xC0) == 0xC0) return 3;
        if ((modrm & 0x07) == 0x05) return 7;
        if ((modrm & 0xC0) == 0x40) return 4;
        if ((modrm & 0xC0) == 0x80) return 7;
        return 3;
    }
    case 0x81: {
        BYTE modrm = p[1];
        if ((modrm & 0xC0) == 0xC0) return 6;
        if ((modrm & 0x07) == 0x05) return 10;
        if ((modrm & 0xC0) == 0x40) return 7;
        if ((modrm & 0xC0) == 0x80) return 10;
        return 6;
    }
    case 0x8B: case 0x89: case 0x8A: case 0x88:
    case 0x3B: case 0x39: case 0x3A: case 0x38:
    case 0x03: case 0x01:
    case 0x85: case 0x84: case 0x87: case 0x86:
    case 0x33: case 0x31: case 0x2B: case 0x29:
    case 0xD1: case 0xD3: case 0xC1: case 0xFF:
    case 0x8D: case 0x69: case 0x6B:
    case 0xF7: case 0xF6: {
        BYTE modrm = p[1];
        if ((modrm & 0xC0) == 0xC0) return 2;
        if ((modrm & 0x07) == 0x05) return 6;
        if ((modrm & 0xC0) == 0x40) return 3;
        if ((modrm & 0xC0) == 0x80) return 6;
        return 2;
    }
    case 0xA1: case 0xA3: return 5;
    case 0x70: case 0x71: case 0x72: case 0x73: case 0x74:
    case 0x75: case 0x76: case 0x77: case 0x78: case 0x79:
    case 0x7A: case 0x7B: case 0x7C: case 0x7D: case 0x7E: case 0x7F:
        return 2;
    case 0xE1: case 0xE2: case 0xE3: return 2;
    case 0x80: {
        BYTE modrm = p[1];
        if ((modrm & 0xC0) == 0xC0) return 3;
        if ((modrm & 0x07) == 0x05) return 7;
        if ((modrm & 0xC0) == 0x40) return 4;
        if ((modrm & 0xC0) == 0x80) return 7;
        return 3;
    }
    default:
        return -1;
    }
}

static int calc_hook_len(DWORD ea)
{
    int len = 0;
    while (len < 5) {
        int l = insn_len((BYTE *)(ea + len));
        if (l <= 0) return -1;
        len += l;
    }
    return len;
}

static int sig_match(DWORD ea, const BYTE *expect, int n)
{
    if (!mem_readable(ea, n)) return 0;
    return memcmp((void *)ea, expect, n) == 0;
}

static int hook_one(int idx, DWORD target, const char *name,
                    const BYTE *expect, int expectLen)
{
    ProbeHook *h = &g_hooks[idx];
    int len;
    DWORD stubAddr;
    BYTE *t;

    h->target = 0; h->installed = 0;
    if (!in_text(target)) { plog("SELFCHK %s: 0x%08X 不在代码段, 拒绝", name, target); return 0; }
    if (!sig_match(target, expect, expectLen)) {
        BYTE got[16] = {0};
        if (mem_readable(target, expectLen)) memcpy(got, (void*)target, expectLen);
        plog("SELFCHK %s @0x%08X 签名不符! 期望 %02X%02X%02X%02X 实际 %02X%02X%02X%02X -> 拒绝hook",
             name, target, expect[0],expect[1],expect[2],expect[3], got[0],got[1],got[2],got[3]);
        return 0;
    }
    if (*(BYTE *)target == 0xE9 || *(BYTE *)target == 0xE8 ||
        *(BYTE *)target == 0xEB || *(BYTE *)target == 0xC3 || *(BYTE *)target == 0xC2) {
        plog("SELFCHK %s: 首指令为跳转/返回, 拒绝", name); return 0;
    }
    len = calc_hook_len(target);
    if (len < 5 || len > 16) { plog("SELFCHK %s: hookLen=%d 异常, 拒绝", name, len); return 0; }

    if (!g_trampPage) {
        g_trampPage = (BYTE *)VirtualAlloc(NULL, PROBE_MAX_HOOKS * 32,
                                           MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
        if (!g_trampPage) { plog("SELFCHK: trampoline 页分配失败"); return 0; }
    }
    stubAddr = (DWORD)(ULONG_PTR)s_stubs[idx];
    t = g_trampPage + idx * 32;
    memcpy(h->original, (void *)target, len);
    memcpy(t, h->original, len);
    t[len] = 0xE9;
    *(DWORD *)(t + len + 1) = target + len - (DWORD)(ULONG_PTR)(t + len + 5);
    *s_trampVars[idx] = t;

    {
        DWORD oldProt = 0;
        BYTE *p = (BYTE *)target;
        int j;
        if (!VirtualProtect(p, len, PAGE_EXECUTE_READWRITE, &oldProt)) {
            plog("SELFCHK %s: VirtualProtect 失败 err=%lu, 拒绝", name, GetLastError());
            return 0;
        }
        p[0] = 0xE9;
        *(DWORD *)(p + 1) = stubAddr - target - 5;
        for (j = 5; j < len; j++) p[j] = 0x90;
        VirtualProtect(p, len, oldProt, &oldProt);
        FlushInstructionCache(GetCurrentProcess(), p, len);
    }
    h->target = target; h->hookLen = len; h->installed = 1;
    plog("HOOK %s @0x%08X 安装成功 (len=%d)", name, target, len);
    return 1;
}

static void hooks_uninstall(void)
{
    int i;
    for (i = 0; i < g_hookCount; i++) {
        ProbeHook *h = &g_hooks[i];
        if (h->installed && h->target) {
            DWORD oldProt = 0;
            if (VirtualProtect((void *)h->target, h->hookLen, PAGE_EXECUTE_READWRITE, &oldProt)) {
                memcpy((void *)h->target, h->original, h->hookLen);
                VirtualProtect((void *)h->target, h->hookLen, oldProt, &oldProt);
                FlushInstructionCache(GetCurrentProcess(), (void *)h->target, h->hookLen);
            }
            h->installed = 0;
        }
    }
}

/* ---------------- hook 目标 + 期望签名 (静态验证 2026-09-14) ---------------- */
typedef struct { int idx; DWORD target; const char *name; const BYTE sig[8]; int sigLen; } HookDef;

static const HookDef g_hookDefs[PROBE_MAX_HOOKS] = {
    /* idx  目标                  名称             前 8 字节签名 (shake_static_v14.py 输出) */
    { 0, CN_SHAKE_CONVERGE, "shake_converge", {0x55,0x8B,0xEC,0x8A,0x45,0x24,0x56,0x8B}, 8 },
    { 1, CN_SHAKE_SCREEN,   "shake_screen",   {0x55,0x8B,0xEC,0x8B,0x45,0x2C,0x8B,0x55}, 8 },
    { 2, CN_SHAKE_READBAR,  "readbar_shake",  {0x55,0x8B,0xEC,0x51,0x8A,0x45,0x08,0x56}, 8 },
    { 3, CN_SHAKE_DIRECT,   "direct_shake",   {0x55,0x8B,0xEC,0x8B,0x45,0x08,0x53,0x56}, 8 },
    { 4, CN_DAMAGE_FONT,    "damage_font",    {0x55,0x8B,0xEC,0x6A,0xFF,0x68,0x00,0x00}, 6 },
    { 5, 0,                 "(reserve)",      {0,0,0,0,0,0,0,0}, 0 },
};

static const char *const g_hookNames[PROBE_MAX_HOOKS] = {
    "converge", "screen", "readbar", "direct", "font", "resv"
};

/* ---------------- 统一分发 (6 参 cdecl, 游戏线程) ----------------
 * v1.5 节流: converge(0) 10ms (v1.4 的 300ms 吞数据) / 其他 300ms / FONT(4) 1s
 * 附加统计 (不受日志节流影响, 心跳汇总打印):
 *   - converge retaddr 频次表 (两轮对照核心)
 *   - FONT a6 6 通道位分布
 *   - 震动控制器对象 this 捕获 (差分目标) */
static volatile LONG g_cnt[PROBE_MAX_HOOKS] = {0};
static volatile LONG g_seen[PROBE_MAX_HOOKS] = {0};   /* 是否出现过 */
static DWORD g_lastLog[PROBE_MAX_HOOKS] = {0};

#define RET_SLOTS 64
static volatile DWORD g_retAddr[RET_SLOTS];
static volatile LONG  g_retCnt[RET_SLOTS];
static volatile LONG  g_retSlots = 0;

static void ret_note(DWORD retaddr)
{
    LONG i, n = g_retSlots;
    for (i = 0; i < n; i++)
        if (g_retAddr[i] == retaddr) { InterlockedIncrement(&g_retCnt[i]); return; }
    if (n < RET_SLOTS) {
        if (InterlockedCompareExchange(&g_retSlots, n + 1, n) == n) {
            g_retAddr[n] = retaddr;
            InterlockedIncrement(&g_retCnt[n]);
        } else {
            ret_note(retaddr);   /* 竞争重试 (极罕见) */
        }
    }
}

/* FONT a6 位分布: 0x01 命中 / 0x02 受击 / 0x04 特效 / 0x08 状态 / 0x10 特殊 / 0x20 DOT */
static volatile LONG g_fontCh[6] = {0};

/* 震动控制器对象 (converge 等的 this, 动态捕获 -> 差分目标) */
static volatile DWORD g_shakeObj = 0;

void __cdecl probe_shake_dispatch(void *obj, DWORD a2, DWORD a3, DWORD a4,
                                  DWORD retaddr, int idx)
{
    DWORD now;
    LONG n;
    if (idx < 0 || idx >= PROBE_MAX_HOOKS) return;
    n = InterlockedIncrement(&g_cnt[idx]);
    now = GetTickCount();

    /* --- 统计 (节流外, 保证完整) --- */
    if (idx <= 3 && obj && !g_shakeObj) g_shakeObj = (DWORD)(ULONG_PTR)obj;
    if (idx == 0 && retaddr) ret_note(retaddr);
    if (idx == 4) {
        DWORD m = a2;   /* stub v1.5 修正: a2 槽 = a6 */
        if (m & 0x01) InterlockedIncrement(&g_fontCh[0]);
        if (m & 0x02) InterlockedIncrement(&g_fontCh[1]);
        if (m & 0x04) InterlockedIncrement(&g_fontCh[2]);
        if (m & 0x08) InterlockedIncrement(&g_fontCh[3]);
        if (m & 0x10) InterlockedIncrement(&g_fontCh[4]);
        if (m & 0x20) InterlockedIncrement(&g_fontCh[5]);
    }

    /* 日志 (限频) */
    {
        DWORD ivl = (idx == 4) ? 1000 : (idx == 0 ? 10 : 300);
        DWORD last = g_lastLog[idx];
        if (!g_seen[idx] || now - last >= ivl) {
            g_lastLog[idx] = now;
            if (InterlockedCompareExchange(&g_seen[idx], 1, 0) == 0 || now - last >= ivl) {
                /* 浮点解读辅助: 参数按位模式可能是 float */
                plog("SHK[%d] %s #%ld this=0x%08X a2=0x%08X(%.3f) a3=0x%08X(%.3f) a4=0x%08X(%.3f) ret=0x%08X",
                     idx, g_hookNames[idx], n,
                     (DWORD)(ULONG_PTR)obj,
                     a2, *(float *)&a2, a3, *(float *)&a3, a4, *(float *)&a4, retaddr);
            }
        }
    }
}

/* ---------------- 评分/槽函数指针 (生产版同款) ---------------- */
typedef int (__attribute__((thiscall)) *FnGetDmg)(int);
typedef int (__attribute__((thiscall)) *FnGetRank)(int, int);
typedef int (__attribute__((thiscall)) *FnVtSlot)(int);
typedef double (__attribute__((thiscall)) *FnGetSlot)(int, float, unsigned);

static float slot_bits(int n) { union { int i; float f; } u; u.i = n; return u.f; }

static int rank_obj_valid(DWORD r)
{
    if (!r || !mem_readable(r, 0x100)) return 0;
    return 1;
}

/* ---------------- worker ---------------- */
static volatile int g_running = 0;

static DWORD WINAPI probe_worker(LPVOID p)
{
    /* 震动 hook 计数快照 (心跳报增量) */
    LONG cntPrev[PROBE_MAX_HOOKS] = {0};
    /* 击杀/评分点 (生产版事件链验证) */
    DWORD lastKill = 0; int killInit = 0;
    DWORD lastScorePt = 0; int scoreInit = 0;
    /* 破甲/凌空槽评分 */
    LONG armorLast = 0, aerialLast = 0; int slotInit = 0;
    int lastSlot = -1;
    DWORD lastArmorRun = 0;
    /* RANK 等级 */
    LONG rankLast = -1; DWORD lastRankRun = 0;
    /* 相机差分 (v1.5: 绑进图门控 + 无上限 + 30ms + 曾变汇总表) */
    static BYTE camPrev[0x1000];
    static BYTE camTouched[0x1000 / 4];      /* 曾变标记 (1=dword 曾变) */
    static DWORD camTouchCnt[0x1000 / 4];    /* 变化次数 */
    int camInit = 0, camRounds = 0;
    DWORD lastCamRun = 0, camBase = 0;
    /* 震动控制器对象差分 (v1.5 新增: g_shakeObj 动态 this) */
    static BYTE objPrev[0x1000];
    static BYTE objTouched[0x1000 / 4];
    static DWORD objTouchCnt[0x1000 / 4];
    int objInit = 0;
    DWORD lastObjRun = 0, objBase = 0;
    DWORD workerStart = 0;
    int inGame = 0;
    DWORD heartbeat = 0;
    (void)p;

    Sleep(3000);
    {
        char dllPath[MAX_PATH], *slash;
        GetModuleFileNameA(NULL, dllPath, MAX_PATH);
        slash = strrchr(dllPath, '\\');
        if (slash) {
            *slash = 0;
            _snprintf(g_logPath, MAX_PATH, "%s\\DfoVibration_shake_probe_dll.log", dllPath);
        } else {
            strcpy(g_logPath, "DfoVibration_shake_probe_dll.log");
        }
    }
    rotate_check();
    plog("=================================================");
    plog("SHAKE-PROBE 90CN v1.5 pid=%lu (震屏专属信号定位; 两轮对照: 轮A不震屏技能/轮B震屏技能)",
         (unsigned long)GetCurrentProcessId());
    plog("ADDR CONVERGE=0x%08X SCREEN=0x%08X READBAR=0x%08X DIRECT=0x%08X FONT=0x%08X",
         CN_SHAKE_CONVERGE, CN_SHAKE_SCREEN, CN_SHAKE_READBAR, CN_SHAKE_DIRECT, CN_DAMAGE_FONT);
    plog("ADDR N2500=0x%08X RANK=0x%08X CAM=0x%08X",
         CN_N2500_PTR, CN_RANK_OBJ_PTR, CN_CAM_OBJ_PTR);

    workerStart = GetTickCount();
    plog("启动期只读观察中, 等待进图 (n2500 有效 + 30s) ...");

    while (g_running) {
        DWORD now = GetTickCount();
        DWORD pl, rank;

        /* ---- 进图门控 ---- */
        if (!inGame) {
            DWORD gp = try_read(CN_N2500_PTR);
            if (now - workerStart >= 30000 && gp && mem_readable(gp, 0xA00)) {
                int i, ok = 0;
                inGame = 1;
                plog("INGAME 门控通过 (uptime=%lums, pl=0x%08X), 开始安装震动 hook",
                     (unsigned long)(now - workerStart), gp);
                g_phase = 1;
                for (i = 0; i < 5; i++) {
                    const HookDef *d = &g_hookDefs[i];
                    if (!d->target) continue;
                    ok += hook_one(d->idx, d->target, d->name, d->sig, d->sigLen);
                }
                g_hookCount = 5;
                plog("HOOK 合计安装 %d/5", ok);
                g_phase = 0;
            }
        }

        pl = try_read(CN_N2500_PTR);
        rank = try_read(CN_RANK_OBJ_PTR);

        /* ---- 击杀脉冲 (生产版 VEV_KILL 链路) ---- */
        if (pl && mem_readable(pl + CN_KILL_COUNT_OFF, 4)) {
            DWORD cur = *(volatile DWORD *)(pl + CN_KILL_COUNT_OFF);
            if (!killInit) { lastKill = cur; killInit = 1; }
            else if (cur != lastKill) {
                plog("KILLPULSE 0x5DA0 %lu -> %lu", lastKill, cur);
                lastKill = cur;
            }
        }

        /* ---- 评分点 (生产版 VEV_KILLPOINT 链路) ---- */
        if (rank && mem_readable(rank + CN_SCORE_FINAL_KILL, 4)) {
            DWORD pt = *(volatile DWORD *)(rank + CN_SCORE_FINAL_KILL);
            if (!scoreInit) { lastScorePt = pt; scoreInit = 1; }
            else if (pt != lastScorePt) {
                plog("SCOREPT rank+0xC0C %lu -> %lu", lastScorePt, pt);
                lastScorePt = pt;
            }
        }

        /* ---- RANK 等级 (500ms 节流, 生产版同款调用) ---- */
        if (inGame && rank_obj_valid(rank) && now - lastRankRun >= 500) {
            int dmg, r;
            lastRankRun = now;
            dmg = ((FnGetDmg)CN_FN_SCORE_DMG)((int)rank);
            r = ((FnGetRank)CN_FN_RANK_LEVEL)((int)rank, dmg);
            if (r >= 0 && r <= 8 && r != rankLast) {
                plog("RANK level=%d (dmg=%d)", r, dmg);
                rankLast = r;
            }
        }

        /* ---- 破甲/凌空槽评分 (2s 节流, 生产版同款调用) ---- */
        if (inGame && rank_obj_valid(rank) && pl &&
            mem_readable(pl, 8) && now - lastArmorRun >= 2000) {
            DWORD vt = try_read(pl);
            DWORD fnaddr;
            lastArmorRun = now;
            if (vt) {
                fnaddr = try_read(vt + CN_VT_SLOT_OFF);
                if (in_text(fnaddr)) {
                    int slot = ((FnVtSlot)fnaddr)((int)pl);
                    if (slot != lastSlot) {
                        plog("SLOT 场景槽号 %d -> %d", lastSlot, slot);
                        lastSlot = slot;
                    }
                    if (slot >= 0 && slot <= 7) {
                        LONG armor = (LONG)(((FnGetSlot)CN_FN_SLOT_SCORE)((int)rank, slot_bits(4), (unsigned)slot)
                                  + ((FnGetSlot)CN_FN_SLOT_SCORE)((int)rank, slot_bits(5), (unsigned)slot));
                        LONG aerial = (LONG)(((FnGetSlot)CN_FN_SLOT_SCORE)((int)rank, slot_bits(10), (unsigned)slot)
                                   + ((FnGetSlot)CN_FN_SLOT_SCORE)((int)rank, slot_bits(14), (unsigned)slot));
                        if (!slotInit) { armorLast = armor; aerialLast = aerial; slotInit = 1; }
                        else {
                            if (armor != armorLast) {
                                plog("ARMOR %ld -> %ld (%+ld)", armorLast, armor, armor - armorLast);
                                armorLast = armor;
                            }
                            if (aerial != aerialLast) {
                                plog("AERIAL %ld -> %ld (%+ld)", aerialLast, aerial, aerial - aerialLast);
                                aerialLast = aerial;
                            }
                        }
                    }
                }
            }
        }

        /* ---- 相机差分 (v1.5: inGame 门控 + 30ms + 无轮数上限) ----
         * v1.4 教训: 60 轮×500ms 上限在进图前耗尽 -> 战斗期零采样。
         * 曾变偏移标记+计数 -> 心跳打印汇总表 (两轮对照核心输出) */
        if (inGame) {
            DWORD cam = try_read(CN_CAM_OBJ_PTR);
            if (cam && mem_readable(cam, 0x1000) && now - lastCamRun >= 30) {
                lastCamRun = now;
                if (!camInit || cam != camBase) {   /* 对象重建 -> 重置基准 */
                    memcpy(camPrev, (void *)cam, 0x1000);
                    camInit = 1; camBase = cam;
                    plog("CAMDIFF base 0x%08X (相机对象基准建立/重建)", cam);
                } else {
                    int diffs = 0;
                    DWORD off;
                    camRounds++;
                    for (off = 0; off < 0x1000; off += 4) {
                        DWORD a = *(volatile DWORD *)(cam + off);
                        DWORD b = *(volatile DWORD *)(camPrev + off);
                        if (a != b) {
                            DWORD slot = off >> 2;
                            camTouched[slot] = 1;
                            camTouchCnt[slot]++;
                            if (diffs < 16) {
                                plog("CAMDIFF +0x%03X %08X -> %08X (f=%.3f)",
                                     off, b, a, *(float *)&a);
                                diffs++;
                            }
                        }
                    }
                    if (diffs >= 16) plog("CAMDIFF (本轮变化>16, 仅打前16条; 汇总见心跳)");
                    memcpy(camPrev, (void *)cam, 0x1000);
                }
            } else if (!cam && camInit) {
                camInit = 0;
            }

            /* ---- 震动控制器对象差分 (v1.5 新增: converge this 动态捕获) ---- */
            {
                DWORD sObj = g_shakeObj;
                if (sObj && mem_readable(sObj, 0x1000) && now - lastObjRun >= 30) {
                    lastObjRun = now;
                    if (!objInit || sObj != objBase) {
                        memcpy(objPrev, (void *)sObj, 0x1000);
                        objInit = 1; objBase = sObj;
                        plog("SHAKEOBJ base 0x%08X (震动控制器对象基准建立/重建)", sObj);
                    } else {
                        int odiffs = 0;
                        DWORD off;
                        for (off = 0; off < 0x1000; off += 4) {
                            DWORD a = *(volatile DWORD *)(sObj + off);
                            DWORD b = *(volatile DWORD *)(objPrev + off);
                            if (a != b) {
                                DWORD slot = off >> 2;
                                objTouched[slot] = 1;
                                objTouchCnt[slot]++;
                                if (odiffs < 16) {
                                    plog("OBJDIFF +0x%03X %08X -> %08X (f=%.3f)",
                                         off, b, a, *(float *)&a);
                                    odiffs++;
                                }
                            }
                        }
                        if (odiffs >= 16) plog("OBJDIFF (本轮变化>16, 仅打前16条; 汇总见心跳)");
                        memcpy(objPrev, (void *)sObj, 0x1000);
                    }
                } else if (!sObj && objInit) {
                    objInit = 0;
                }
            }
        }

        /* ---- 心跳 (30s): 计数增量 + ret/FONT 分布 + 差分曾变汇总 ---- */
        if (now - heartbeat >= 30000) {
            int i;
            heartbeat = now;
            plog("HEARTBEAT alive inGame=%d vehLogged=%d camRounds=%d shakeObj=0x%08X cnt:",
                 inGame, g_vehLogged, camRounds, (DWORD)g_shakeObj);
            for (i = 0; i < 5; i++) {
                LONG c = g_cnt[i];
                if (c != cntPrev[i]) {
                    plog("  CNT[%d] %s %ld (+%ld)", i, g_hookNames[i], c, c - cntPrev[i]);
                    cntPrev[i] = c;
                }
            }
            /* converge retaddr 频次 (两轮对照核心输出) */
            {
                LONG k, ns = g_retSlots;
                if (ns > 0) {
                    plog("  RETDIST converge retaddr 频次 (%ld 种):", ns);
                    for (k = 0; k < ns; k++)
                        plog("    ret=0x%08X x%ld", (DWORD)g_retAddr[k], (LONG)g_retCnt[k]);
                }
            }
            /* FONT a6 位分布 */
            plog("  FONTCH hit=%ld dmg=%ld fx=%ld state=%ld sp=%ld dot=%ld",
                 (LONG)g_fontCh[0], (LONG)g_fontCh[1], (LONG)g_fontCh[2],
                 (LONG)g_fontCh[3], (LONG)g_fontCh[4], (LONG)g_fontCh[5]);
            /* 相机曾变偏移汇总 (两轮差异 = 震屏专属字段) */
            {
                DWORD off; int n = 0;
                char line[512];
                line[0] = 0;
                for (off = 0; off < 0x1000; off += 4) {
                    DWORD slot = off >> 2;
                    if (camTouched[slot]) {
                        n++;
                        if (n <= 20) {
                            char tmp[32];
                            _snprintf(tmp, sizeof(tmp), " +%lX:%lu", off, camTouchCnt[slot]);
                            strncat(line, tmp, sizeof(line) - strlen(line) - 1);
                        }
                    }
                }
                if (n) plog("  CAMTOUCH n=%d%s%s", n, line, n > 20 ? " ..." : "");
                else plog("  CAMTOUCH n=0 (无变化)");
            }
            /* 震动控制器曾变偏移汇总 */
            {
                DWORD off; int n = 0;
                char line[512];
                line[0] = 0;
                for (off = 0; off < 0x1000; off += 4) {
                    DWORD slot = off >> 2;
                    if (objTouched[slot]) {
                        n++;
                        if (n <= 20) {
                            char tmp[32];
                            _snprintf(tmp, sizeof(tmp), " +%lX:%lu", off, objTouchCnt[slot]);
                            strncat(line, tmp, sizeof(line) - strlen(line) - 1);
                        }
                    }
                }
                if (n) plog("  OBJTOUCH n=%d%s%s", n, line, n > 20 ? " ..." : "");
                else plog("  OBJTOUCH n=0 (无变化)");
            }
        }
        Sleep(10);
    }
    return 0;
}

/* ---------------- DllMain ---------------- */
static HANDLE g_hThread = NULL;

BOOL WINAPI DllMain(HINSTANCE hDll, DWORD reason, LPVOID reserved)
{
    (void)hDll;
    if (reason == DLL_PROCESS_ATTACH) {
        /* 日志名已在 worker 里初始化 (需等 3s 游戏稳定) */
        g_running = 1;
        g_vehHandle = AddVectoredExceptionHandler(1, probe_veh);
        DisableThreadLibraryCalls(hDll);
        g_hThread = CreateThread(NULL, 0, probe_worker, NULL, 0, NULL);
    } else if (reason == DLL_PROCESS_DETACH) {
        g_running = 0;
        /* v1.3 定案: 进程退出 (reserved!=NULL) 跳过清理, 防退出时序 AV */
        if (reserved == NULL) {
            if (g_hThread) { WaitForSingleObject(g_hThread, 1500); g_hThread = NULL; }
            hooks_uninstall();
        }
        if (g_vehHandle) { RemoveVectoredExceptionHandler(g_vehHandle); g_vehHandle = NULL; }
    }
    return TRUE;
}
