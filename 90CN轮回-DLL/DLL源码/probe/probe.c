/* ============================================================
 * DfoVibration 90CN 探针 DLL (标定轮)
 * 用途: 实机采集 90CN 客户端运行时数据, 验证/修正静态定位的地址
 * 安全设计:
 *   - 全部内存读带 VirtualQuery 守卫
 *   - hook 前逐字节校验函数签名, 不匹配则拒绝安装
 *   - 游戏函数调用前校验对象指针/跨度/vtable, 并带 SEH 兜底
 *   - 只读采集 + trampoline 原样转发, 不改游戏行为
 * 日志: 与 DLL 同目录 <DLL名>_dll.log (2MB 轮转)
 * 配置: 同目录 <DLL名>.ini [Probe] HookFont/HookBehavior/HookShake/
 *       HookText/CallRank (默认全 1)
 * ============================================================ */
#include <windows.h>
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0501
#endif
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <wchar.h>
#include <tlhelp32.h>
#include "../common/vib_90cn_addrs.h"

/* ---------------- 日志 ---------------- */
static char g_logPath[MAX_PATH] = "";
static char g_iniPath[MAX_PATH] = "";
static volatile LONG g_logBusy = 0;

static void rotate_check(void)
{
    HANDLE h; DWORD size; char old[MAX_PATH];
    if (!g_logPath[0]) return;
    h = CreateFileA(g_logPath, GENERIC_READ, FILE_SHARE_READ|FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
    if (h == INVALID_HANDLE_VALUE) return;
    size = GetFileSize(h, NULL);
    CloseHandle(h);
    if (size > 2*1024*1024) {
        _snprintf(old, sizeof(old), "%s.old", g_logPath);
        DeleteFileA(old);
        MoveFileA(g_logPath, old);
    }
}

static void plog(const char *fmt, ...)
{
    FILE *fp; char buf[800]; va_list ap;
    if (!g_logPath[0]) return;
    if (InterlockedCompareExchange(&g_logBusy, 1, 0) != 0) return; /* hook 线程不并发写 */
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

static int in_text(DWORD a)   { return a >= CN_TEXT_LO && a < CN_TEXT_HI; }
static int in_rdata(DWORD a)  { return a >= CN_RDATA_LO && a < CN_RDATA_HI; }
static int in_data(DWORD a)   { return a >= CN_DATA_LO && a < CN_DATA_HI; }
static int in_heap(DWORD a)   { return a >= 0x00400000 && a < 0x80000000; } /* 宽松: 非空且用户空间 */

/* ---------------- VEH 被动记录器 ----------------
 * v1.2: 不再从 VEH 里 longjmp (会抛弃异常分发链/SEH 链, 曾导致游戏崩溃),
 * 只被动记录异常 + 当前探针阶段, 全部透传给游戏自身的处理链。
 * 安全靠"进图门控 + 渐进启用 + 调用前校验", 不靠事后拦截。 */
static PVOID g_vehHandle = NULL;
static volatile int g_phase = 0;        /* 0=只读 1=装hook 2=评分调用 3=槽调用 */
static volatile int g_vehLogged = 0;

static LONG WINAPI probe_veh(PEXCEPTION_POINTERS ep)
{
    DWORD code = ep->ExceptionRecord->ExceptionCode;
    if (g_vehLogged < 20 && (code == EXCEPTION_ACCESS_VIOLATION ||
                             code == EXCEPTION_ILLEGAL_INSTRUCTION ||
                             code == EXCEPTION_PRIV_INSTRUCTION)) {
        g_vehLogged++;
        plog("EXCP phase=%d code=0x%08X eip=0x%08X",
             g_phase, code,
             (DWORD)(size_t)ep->ContextRecord->Eip);
    }
    return EXCEPTION_CONTINUE_SEARCH;
}

/* ---------------- 配置 ---------------- */
static int cfgHookFont = 1, cfgHookBehavior = 1, cfgHookShake = 1;
static int cfgHookText = 1, cfgCallRank = 1;

static void load_ini(void)
{
    char buf[256]; char *k, *v; FILE *fp;
    fp = fopen(g_iniPath, "r");
    if (!fp) return;
    while (fgets(buf, sizeof(buf), fp)) {
        k = buf; while (*k == ' ' || *k == '\t') k++;
        if (_strnicmp(k, "HookFont", 8) == 0)      { v = strchr(k, '='); if (v) cfgHookFont = atoi(v+1); }
        else if (_strnicmp(k, "HookBehavior", 12) == 0) { v = strchr(k, '='); if (v) cfgHookBehavior = atoi(v+1); }
        else if (_strnicmp(k, "HookShake", 9) == 0)     { v = strchr(k, '='); if (v) cfgHookShake = atoi(v+1); }
        else if (_strnicmp(k, "HookText", 8) == 0)      { v = strchr(k, '='); if (v) cfgHookText = atoi(v+1); }
        else if (_strnicmp(k, "CallRank", 8) == 0)      { v = strchr(k, '='); if (v) cfgCallRank = atoi(v+1); }
    }
    fclose(fp);
}

/* ---------------- hook 引擎 (沿用老版已实机验证的实现) ---------------- */
#define PROBE_MAX_HOOKS 8
extern void probe_stub_0(void); extern void probe_stub_1(void);
extern void probe_stub_2(void); extern void probe_stub_3(void);
extern void probe_stub_4(void); extern void probe_stub_5(void);
extern void probe_stub_6(void); extern void probe_stub_7(void);
void *probe_tramp_0, *probe_tramp_1, *probe_tramp_2, *probe_tramp_3,
     *probe_tramp_4, *probe_tramp_5, *probe_tramp_6, *probe_tramp_7;

static void *s_stubs[PROBE_MAX_HOOKS] = {
    (void*)probe_stub_0, (void*)probe_stub_1, (void*)probe_stub_2, (void*)probe_stub_3,
    (void*)probe_stub_4, (void*)probe_stub_5, (void*)probe_stub_6, (void*)probe_stub_7
};
static void **s_trampVars[PROBE_MAX_HOOKS] = {
    &probe_tramp_0, &probe_tramp_1, &probe_tramp_2, &probe_tramp_3,
    &probe_tramp_4, &probe_tramp_5, &probe_tramp_6, &probe_tramp_7
};

typedef struct {
    DWORD target; BYTE original[16]; int hookLen; int installed;
} ProbeHook;
static ProbeHook g_hooks[PROBE_MAX_HOOKS];
static int g_hookCount = 0;
static BYTE *g_trampPage = NULL;

/* v1.3: 完整版指令长度计算 (与 US 生产版 hooks.c 完全一致, 含 ModRM 解码)。
 * v1.2 及之前用简化版 (表外指令一律按1字节), 把 6 字节指令腰斩导致行为 hook 全部装歪。 */
static int insn_len(BYTE *p)
{
    BYTE op = p[0];
    switch (op) {
    case 0x90: case 0xC3: case 0xCC: case 0xF4:
    case 0xF5: case 0xF8: case 0xF9: case 0xFA: case 0xFB:
        return 1;
    case 0xC2: return 3;                    /* retn imm16 */
    case 0x50: case 0x51: case 0x52: case 0x53: case 0x54:
    case 0x55: case 0x56: case 0x57:         /* push reg */
    case 0x58: case 0x59: case 0x5A: case 0x5B: case 0x5C:
    case 0x5D: case 0x5E: case 0x5F:         /* pop reg */
    case 0x40: case 0x41: case 0x42: case 0x43: case 0x44:
    case 0x45: case 0x46: case 0x47: case 0x48: case 0x49:
    case 0x4A: case 0x4B: case 0x4C: case 0x4D: case 0x4E: case 0x4F:
        return 1;
    case 0x6A: return 2;                    /* push imm8 */
    case 0x68: return 5;                    /* push imm32 */
    case 0xE9: case 0xE8: return 5;         /* jmp/call rel32 */
    case 0xEB: return 2;                    /* jmp rel8 */
    case 0x0F:                              /* 两字节 opcode */
        return (p[1] & 0xF0) == 0x80 ? 6 : 2;
    case 0x66: case 0x67: case 0xF3: case 0xF2: case 0x2E:
    case 0x36: case 0x3E: case 0x26: case 0x64: case 0x65:
        return 1 + insn_len(p + 1);         /* 前缀 + 后续 */
    case 0xB0: case 0xB1: case 0xB2: case 0xB3: case 0xB4:
    case 0xB5: case 0xB6: case 0xB7: return 2;   /* mov reg8, imm8 */
    case 0xB8: case 0xB9: case 0xBA: case 0xBB: case 0xBC:
    case 0xBD: case 0xBE: case 0xBF: return 5;   /* mov reg, imm32 */
    case 0xC7: {                            /* mov r/m, imm32 */
        BYTE modrm = p[1];
        if ((modrm & 0xC0) == 0xC0) return 6;
        if ((modrm & 0x07) == 0x05) return 10;
        if ((modrm & 0xC0) == 0x40) return 7;
        if ((modrm & 0xC0) == 0x80) return 10;
        if ((modrm & 0x07) == 0x04) return 7;
        return 6;
    }
    case 0x83: {                            /* grp1 r/m, imm8 (v1.3 修复: 含 imm 字节) */
        BYTE modrm = p[1];
        if ((modrm & 0xC0) == 0xC0) return 3;
        if ((modrm & 0x07) == 0x05) return 7;
        if ((modrm & 0xC0) == 0x40) return 4;
        if ((modrm & 0xC0) == 0x80) return 7;
        return 3;
    }
    case 0x81: {                            /* grp1 r/m, imm32 (v1.3 修复: 含 imm 字节) */
        BYTE modrm = p[1];
        if ((modrm & 0xC0) == 0xC0) return 6;
        if ((modrm & 0x07) == 0x05) return 10;
        if ((modrm & 0xC0) == 0x40) return 7;
        if ((modrm & 0xC0) == 0x80) return 10;
        return 6;
    }
    case 0x8B: case 0x89: case 0x8A: case 0x88: /* mov reg<->r/m */
    case 0x3B: case 0x39: case 0x3A: case 0x38: /* cmp */
    case 0x03: case 0x01:                        /* add */
    case 0x85: case 0x84: case 0x87: case 0x86: /* test/xchg */
    case 0x33: case 0x31: case 0x2B: case 0x29: /* xor/sub */
    case 0xD1: case 0xD3: case 0xC1: case 0xFF: /* shift/inc */
    case 0x8D: case 0x69: case 0x6B:            /* lea/imul */
    case 0xF7: case 0xF6: {                     /* test/mul/not */
        BYTE modrm = p[1];
        if ((modrm & 0xC0) == 0xC0) return 2;
        if ((modrm & 0x07) == 0x05) return 6;
        if ((modrm & 0xC0) == 0x40) return 3;
        if ((modrm & 0xC0) == 0x80) return 6;
        return 2;
    }
    case 0xA1: case 0xA3: return 5;            /* mov eax,[moffs] */
    case 0x70: case 0x71: case 0x72: case 0x73: case 0x74:
    case 0x75: case 0x76: case 0x77: case 0x78: case 0x79:
    case 0x7A: case 0x7B: case 0x7C: case 0x7D: case 0x7E: case 0x7F:
        return 2;                               /* jcc rel8 */
    case 0xE1: case 0xE2: case 0xE3: return 2;  /* loop/jcxz */
    case 0x80: {                                /* cmp r/m, imm8 */
        BYTE modrm = p[1];
        if ((modrm & 0xC0) == 0xC0) return 3;
        if ((modrm & 0x07) == 0x05) return 7;
        if ((modrm & 0xC0) == 0x40) return 4;
        if ((modrm & 0xC0) == 0x80) return 7;
        return 3;
    }
    default:
        return -1;  /* v1.3: 未知 opcode 一律拒绝安装 (宽网挂任意函数, 不能赌长度) */
    }
}

static int calc_hook_len(DWORD ea)
{
    int len = 0;
    while (len < 5) {
        int l = insn_len((BYTE *)(ea + len));
        if (l <= 0) return -1;   /* 未知 opcode: 拒绝 */
        len += l;
    }
    return len;
}

/* 安装前签名校验: 目标前 N 字节必须等于期望值 */
static int sig_match(DWORD ea, const BYTE *expect, int n)
{
    if (!mem_readable(ea, n)) return 0;
    return memcmp((void *)ea, expect, n) == 0;
}

static void dyn_hooks_uninstall(void);   /* v1.3 宽网 hook 卸载 (定义在后面) */

static void hooks_uninstall(void)
{
    int i;
    for (i = 0; i < g_hookCount; i++) {
        ProbeHook *h = &g_hooks[i];
        if (h->installed && h->target) {
            /* v1.3: 代码页只读, 还原前必须临时改权限 (v1.2 在此裸写导致退出时 AV) */
            DWORD oldProt = 0;
            if (VirtualProtect((void *)h->target, h->hookLen, PAGE_EXECUTE_READWRITE, &oldProt)) {
                memcpy((void *)h->target, h->original, h->hookLen);
                VirtualProtect((void *)h->target, h->hookLen, oldProt, &oldProt);
                FlushInstructionCache(GetCurrentProcess(), (void *)h->target, h->hookLen);
            }
            h->installed = 0;
        }
    }
    dyn_hooks_uninstall();
}

/* 安装单个 hook (索引固定 0-7, 带 SEH 断言前缀) */
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
    stubAddr = (DWORD)s_stubs[idx];
    t = g_trampPage + idx * 32;
    memcpy(h->original, (void *)target, len);
    memcpy(t, h->original, len);
    t[len] = 0xE9;
    *(DWORD *)(t + len + 1) = target + len - (DWORD)(t + len + 5);
    *s_trampVars[idx] = t;

    /* 90CN 代码页是 RX (US 老客户端是 RWX), 写入前临时改页权限, 写完还原 */
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

/* ---------------- hook 目标 + 期望签名 ---------------- */
typedef struct { int idx; DWORD target; const char *name; const BYTE sig[8]; int sigLen; int cfgOn; } HookDef;

static const HookDef g_hookDefs[PROBE_MAX_HOOKS] = {
    { 0, CN_ON_ATTACK,      "OnAttack",      {0x8B,0x89,0xD0,0x06,0x00,0x00}, 6, 1 },
    { 1, CN_ON_DAMAGE,      "OnDamage",      {0x55,0x8B,0xEC,0x56,0x8B,0xF1}, 6, 1 },
    { 2, CN_ON_START_BATTLE,"OnStartBattle", {0x8B,0x89,0xD0,0x06,0x00,0x00}, 6, 1 },
    { 3, CN_ON_RESET_COMBO, "OnResetCombo",  {0x55,0x8B,0xEC,0x8B,0x89,0xD0}, 6, 1 },
    { 4, CN_ON_ACTION_END,  "OnActionEnd",   {0x55,0x8B,0xEC,0x56,0x8B,0xF1}, 6, 1 },
    { 5, CN_ON_TARGET_DIE,  "OnTargetDie",   {0x8B,0x89,0xD0,0x06,0x00,0x00}, 6, 1 },
    { 6, CN_ON_SHAKE_INPUT, "onShakeInput",  {0x55,0x8B,0xEC,0x80,0xB9}, 5, 1 },
    { 7, CN_DAMAGE_FONT,    "damage_font",   {0x55,0x8B,0xEC,0x6A,0xFF,0x68}, 6, 1 },
};

/* ---------------- v1.3: 动态 stub 宽网 hook ----------------
 * 行为事件注册表 (0x42B4304, 26 项) 里的 6 个目标已由上面 asm stub 覆盖,
 * 其余 20 项 + 读表器用"动态生成的计数 stub"全量挂上, 实证哪条路径活跃。
 * stub 模板 (35 字节, RWX 页内生成):
 *   pushfd / pushad / push ecx(this) / 取 arg1,arg2,retaddr / 压参 / call handler
 *   / popad / popfd / jmp trampoline                              */
#define PROBE_DYN_MAX  24
#define DYN_SLOT       96            /* 每 hook: stub 48B + trampoline 48B */

typedef struct {
    DWORD target; BYTE original[16]; int hookLen; int installed;
    const char *name;
} DynHook;
static DynHook g_dyn[PROBE_DYN_MAX];
static BYTE *g_dynPage = NULL;
static volatile LONG g_cntDyn[PROBE_DYN_MAX] = {0};
static volatile LONG g_dynFirst[PROBE_DYN_MAX] = {0};

static void __attribute__((stdcall)) probe_dyn_handler(int idx, DWORD a1, DWORD a2,
                                                       DWORD thisv, DWORD retaddr)
{
    if (idx < 0 || idx >= PROBE_DYN_MAX) return;
    InterlockedIncrement(&g_cntDyn[idx]);
    if (!g_dynFirst[idx]) {
        g_dynFirst[idx] = 1;
        plog("DYN[%d] %s @0x%08X 首次调用! this=0x%08X a1=0x%08X a2=0x%08X ret=0x%08X",
             idx, g_dyn[idx].name, g_dyn[idx].target, thisv, a1, a2, retaddr);
    }
}

static int hook_one_dyn(int idx, DWORD target, const char *name)
{
    DynHook *h = &g_dyn[idx];
    BYTE *stub, *tramp;
    int len, j;
    DWORD rel;
    if (!in_text(target)) { plog("DYNCHK %s: 0x%08X 不在代码段, 跳过", name, target); return 0; }
    if (!mem_readable(target, 8)) { plog("DYNCHK %s: 不可读, 跳过", name); return 0; }
    {
        BYTE b = *(BYTE *)target;
        if (b == 0xE9 || b == 0xE8 || b == 0xEB || b == 0xC3 || b == 0xC2) {
            plog("DYNCHK %s: 首指令为跳转/返回(可能已被hook), 跳过", name); return 0;
        }
    }
    len = calc_hook_len(target);
    if (len < 5 || len > 16) { plog("DYNCHK %s: hookLen=%d 异常, 跳过", name, len); return 0; }
    if (!g_dynPage) {
        g_dynPage = (BYTE *)VirtualAlloc(NULL, PROBE_DYN_MAX * DYN_SLOT,
                                         MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
        if (!g_dynPage) { plog("DYNCHK: 动态页分配失败"); return 0; }
    }
    stub = g_dynPage + idx * DYN_SLOT;
    tramp = stub + 48;
    h->target = target; h->hookLen = len; h->installed = 0; h->name = name;
    memcpy(h->original, (void *)target, len);
    /* trampoline: 原字节 + 跳回 target+len */
    memcpy(tramp, h->original, len);
    tramp[len] = 0xE9;
    *(DWORD *)(tramp + len + 1) = target + len - (DWORD)(tramp + len + 5);
    /* stub 模板 */
    stub[0] = 0x9C;                                  /* pushfd */
    stub[1] = 0x60;                                  /* pushad */
    stub[2] = 0x51;                                  /* push ecx (this) */
    stub[3] = 0x8B; stub[4] = 0x44; stub[5] = 0x24; stub[6] = 0x2C;  /* mov eax,[esp+0x2C] arg1 */
    stub[7] = 0x8B; stub[8] = 0x4C; stub[9] = 0x24; stub[10] = 0x30; /* mov ecx,[esp+0x30] arg2 */
    stub[11] = 0x8B; stub[12] = 0x54; stub[13] = 0x24; stub[14] = 0x28; /* mov edx,[esp+0x28] ret */
    stub[15] = 0x52;                                 /* push edx */
    stub[16] = 0x51;                                 /* push ecx */
    stub[17] = 0x50;                                 /* push eax */
    stub[18] = 0x68;                                 /* push idx */
    *(DWORD *)(stub + 19) = (DWORD)idx;
    stub[23] = 0xE8;                                 /* call probe_dyn_handler */
    rel = (DWORD)(size_t)probe_dyn_handler - (DWORD)(size_t)(stub + 28);
    *(DWORD *)(stub + 24) = rel;
    stub[28] = 0x61;                                 /* popad */
    stub[29] = 0x9D;                                 /* popfd */
    stub[30] = 0xE9;                                 /* jmp tramp */
    *(DWORD *)(stub + 31) = (DWORD)(size_t)tramp - (DWORD)(size_t)(stub + 35);
    /* 写 E9 到目标 (VirtualProtect 同上) */
    {
        DWORD oldProt = 0;
        BYTE *p = (BYTE *)target;
        if (!VirtualProtect(p, len, PAGE_EXECUTE_READWRITE, &oldProt)) {
            plog("DYNCHK %s: VirtualProtect 失败 err=%lu, 跳过", name, GetLastError());
            return 0;
        }
        p[0] = 0xE9;
        *(DWORD *)(p + 1) = (DWORD)(size_t)stub - target - 5;
        for (j = 5; j < len; j++) p[j] = 0x90;
        VirtualProtect(p, len, oldProt, &oldProt);
        FlushInstructionCache(GetCurrentProcess(), p, len);
    }
    h->installed = 1;
    plog("DYNHOOK [%d] %s @0x%08X 安装成功 (len=%d stub=0x%08X)", idx, name, target, len, (DWORD)(size_t)stub);
    return 1;
}

static void dyn_hooks_uninstall(void)
{
    int i;
    if (!g_dynPage) return;
    for (i = 0; i < PROBE_DYN_MAX; i++) {
        DynHook *h = &g_dyn[i];
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

/* 宽网安装: 事件表 26 项 (跳过已由 asm stub 覆盖的 6 个) + 读表器 */
static void install_dyn_widenet(void)
{
    static char names[PROBE_DYN_MAX][12];
    int i, n = 0;
    int hooked = 0;
    for (i = 0; i < CN_EVENT_TABLE_CNT; i++) {
        DWORD fn = try_read(CN_EVENT_TABLE + 4 * i);
        int skip = 0, k;
        if (!in_text(fn)) { plog("WIDENET 表[%d] 0x%08X 非代码, 跳过", i, fn); continue; }
        for (k = 0; k < 6; k++)                 /* 0-5 号 asm hook 目标 */
            if (fn == g_hookDefs[k].target) { skip = 1; break; }
        if (skip || n >= PROBE_DYN_MAX) continue;
        _snprintf(names[n], 11, "T%02d", i);
        if (hook_one_dyn(n, fn, names[n])) hooked++;
        n++;
    }
    if (n < PROBE_DYN_MAX) {
        _snprintf(names[n], 11, "RD1");
        if (hook_one_dyn(n, CN_TBL_READER, names[n])) hooked++;
        n++;
    }
    plog("WIDENET 合计安装 %d/%d (表项+读表器)", hooked, n);
}

/* ---------------- TextOutW IAT hook ---------------- */
typedef BOOL (WINAPI *FnTextOutW)(HDC, int, int, LPCWSTR, int);
static FnTextOutW g_origTextOutW = NULL;

static void probe_textout(LPCWSTR s)
{
    /* 简易去重: 与上一条相同则限频 5s */
    static wchar_t last[96] = {0};
    static DWORD lastTick = 0;
    DWORD now = GetTickCount();
    wchar_t txt[96];
    int i;
    if (!s || !s[0]) return;
    for (i = 0; i < 95 && s[i]; i++) txt[i] = s[i];
    txt[i] = 0;
    if (wcscmp(txt, last) == 0 && now - lastTick < 5000) return;
    wcscpy(last, txt);
    lastTick = now;
    plog("TXT \"%ls\"", txt);
}

static BOOL WINAPI MyTextOutW(HDC hdc, int x, int y, LPCWSTR s, int c)
{
    if (s && c > 0 && c < 96 && g_origTextOutW)
        probe_textout(s);
    return g_origTextOutW(hdc, x, y, s, c);
}

static int textout_hook_install(void)
{
    DWORD *slot;
    if (!in_data(CN_TEXTOUTW_IAT) && !in_rdata(CN_TEXTOUTW_IAT)) {
        plog("SELFCHK TextOutW IAT 0x%08X 地址异常, 拒绝", CN_TEXTOUTW_IAT);
        return 0;
    }
    if (!mem_readable(CN_TEXTOUTW_IAT, 4)) {
        plog("SELFCHK TextOutW IAT 不可读, 拒绝");
        return 0;
    }
    slot = (DWORD *)CN_TEXTOUTW_IAT;
    g_origTextOutW = (FnTextOutW)*slot;
    if (!g_origTextOutW || (DWORD)g_origTextOutW == (DWORD)MyTextOutW) {
        plog("SELFCHK TextOutW IAT 槽值异常 0x%08X", (DWORD)g_origTextOutW);
        return 0;
    }
    /* 槽值应指向 gdi32 (系统 DLL 高地址) */
    if ((DWORD)g_origTextOutW < 0x70000000) {
        plog("SELFCHK TextOutW IAT 槽值 0x%08X 不像系统 DLL, 谨慎放行", (DWORD)g_origTextOutW);
    }
    /* IAT 页同样是只读 (v1.1 在此裸写直接 AV), 临时改权限再写 */
    {
        DWORD oldProt = 0;
        if (!VirtualProtect(slot, 4, PAGE_READWRITE, &oldProt)) {
            plog("SELFCHK TextOutW IAT VirtualProtect 失败 err=%lu, 拒绝", GetLastError());
            return 0;
        }
        *slot = (DWORD)MyTextOutW;
        VirtualProtect(slot, 4, oldProt, &oldProt);
    }
    plog("HOOK TextOutW IAT 安装成功 (orig=0x%08X)", (DWORD)g_origTextOutW);
    return 1;
}

static void textout_hook_uninstall(void)
{
    if (g_origTextOutW && mem_readable(CN_TEXTOUTW_IAT, 4)) {
        DWORD *slot = (DWORD *)CN_TEXTOUTW_IAT;
        if (*slot == (DWORD)MyTextOutW) {
            DWORD oldProt = 0;
            if (VirtualProtect(slot, 4, PAGE_READWRITE, &oldProt)) {
                *slot = (DWORD)g_origTextOutW;
                VirtualProtect(slot, 4, oldProt, &oldProt);
            }
        }
        g_origTextOutW = NULL;
    }
}

/* ---------------- hook 回调 (游戏线程, 极短) ---------------- */
static volatile LONG g_cntBehavior[6] = {0};
static volatile LONG g_cntShake = 0;
static volatile LONG g_cntFont[8] = {0};      /* 按 a6 位分类计数: idx0=0x01..idx5=0x20, idx6=其他位, idx7=总数 */
static volatile DWORD g_lastBehaviorTick[6] = {0};
static volatile DWORD g_lastFontTick = 0;

void __cdecl probe_behavior(void *obj, DWORD a2, DWORD a3, int idx)
{
    if (idx < 0 || idx > 5) return;
    InterlockedIncrement(&g_cntBehavior[idx]);
    /* 限频日志: 每类 1s 一条 */
    {
        DWORD now = GetTickCount();
        if (now - g_lastBehaviorTick[idx] >= 1000) {
            g_lastBehaviorTick[idx] = now;
            plog("BEH idx=%d obj=0x%08X a2=0x%08X a3=0x%08X", idx, (DWORD)obj, a2, a3);
        }
    }
}

void __cdecl probe_shake(void *obj, DWORD a2, int idx)
{
    (void)idx;
    InterlockedIncrement(&g_cntShake);
    plog("SHAKE onShakeInput obj=0x%08X a2=0x%08X", (DWORD)obj, a2);
}

void __cdecl probe_font(void *obj, DWORD a6, int idx)
{
    static int dumpLeft = 5;
    (void)idx;
    InterlockedIncrement(&g_cntFont[7]);
    if      (a6 & 0x20) InterlockedIncrement(&g_cntFont[5]);
    else if (a6 & 0x10) InterlockedIncrement(&g_cntFont[4]);
    else if (a6 & 0x08) InterlockedIncrement(&g_cntFont[3]);
    else if (a6 & 0x04) InterlockedIncrement(&g_cntFont[2]);
    else if (a6 & 0x01) InterlockedIncrement(&g_cntFont[0]);
    else if (a6 & 0x02) InterlockedIncrement(&g_cntFont[1]);
    else                InterlockedIncrement(&g_cntFont[6]);
    /* v1.3: 前 5 个事件深挖对象前 0x48 字节 (找伤害值偏移, 与 a6 语义对齐) */
    if (dumpLeft > 0 && obj && mem_readable((DWORD)obj, 0x48)) {
        DWORD *w = (DWORD *)obj;
        dumpLeft--;
        plog("FONTDUMP obj=0x%08X a6=0x%08X +00:%08X %08X %08X %08X +10:%08X %08X %08X %08X +20:%08X %08X %08X %08X +30:%08X %08X %08X %08X +40:%08X",
             (DWORD)obj, a6,
             w[0], w[1], w[2], w[3], w[4], w[5], w[6], w[7],
             w[8], w[9], w[10], w[11], w[12], w[13], w[14], w[15], w[16], w[17]);
    }
    {
        DWORD now = GetTickCount();
        if (now - g_lastFontTick >= 500) {
            g_lastFontTick = now;
            plog("FONT obj=0x%08X a6=0x%08X", (DWORD)obj, a6);
        }
    }
}

/* ---------------- 评分系统安全调用 ---------------- */
typedef int    (__attribute__((thiscall)) *FnGetDmg)(int);
typedef int    (__attribute__((thiscall)) *FnGetRank)(int, int);
typedef double (__attribute__((thiscall)) *FnGetSlot)(int, float, unsigned int);
typedef int    (__attribute__((thiscall)) *FnVtSlot)(int);

static int rank_obj_valid(DWORD rank)
{
    if (!in_heap(rank)) return 0;
    /* rank_level 读 this+0xBE8; score_dmg 读 this+0xC0C(在续体); 跨度到 0xC10 */
    if (!mem_readable(rank, 0x0C10 + 8)) return 0;
    return 1;
}

static int n2500_valid_for_call(DWORD pl)
{
    DWORD vt;
    if (!in_heap(pl)) return 0;
    if (!mem_readable(pl, 8)) return 0;
    vt = try_read(pl);
    if (!in_rdata(vt) && !in_data(vt)) return 0;
    if (!mem_readable(vt, 0xC54 + 8)) return 0;
    return 1;
}

/* 主动调用: 安全靠"进图门控 + 对象校验 + 渐进启用", 不再从 VEH longjmp */
static DWORD g_rankLevel = 0, g_rankDmg = 0;
static int g_rankOk = 0;
static DWORD g_rankOkCount = 0;   /* 成功轮数, 作为槽调用解锁条件 */
static int g_rankLogged = 0;

static void do_rank_calls(void)
{
    DWORD rank = try_read(CN_RANK_OBJ_PTR);
    DWORD pl;
    int dmg, lvl;
    if (!rank_obj_valid(rank)) { g_rankOk = 0; return; }
    pl = try_read(CN_N2500_PTR);
    if (!n2500_valid_for_call(pl)) { g_rankOk = 0; return; }
    if (!g_rankLogged) {
        g_rankLogged = 1;
        plog("RANK 首次调用 rank=0x%08X pl=0x%08X (评分=0x%08X 等级=0x%08X)",
             rank, pl, CN_FN_SCORE_DMG, CN_FN_RANK_LEVEL);
    }
    dmg = ((FnGetDmg)CN_FN_SCORE_DMG)((int)rank);
    lvl = ((FnGetRank)CN_FN_RANK_LEVEL)((int)rank, dmg);
    g_rankDmg = (DWORD)dmg;
    g_rankLevel = (DWORD)lvl;
    g_rankOk = 1;
    g_rankOkCount++;
}

/* vtable+0xC54 取场景槽号 + slot_score 采样 */
static int g_lastSlot = -1;
static double g_slotA[2] = {0, 0}; /* 槽4/5 合计, 槽10/14 合计 */
static int g_slotLogged = 0;

static void do_slot_calls(void)
{
    DWORD pl, vt, fnaddr;
    int slot;
    DWORD rank;
    union { int i; float f; } u;
    if (!cfgCallRank) return;
    pl = try_read(CN_N2500_PTR);
    if (!n2500_valid_for_call(pl)) return;
    vt = try_read(pl);
    fnaddr = try_read(vt + CN_VT_SLOT_OFF);
    if (!in_text(fnaddr)) { plog("SLOT vtable+0x%X=0x%08X 不在代码段", CN_VT_SLOT_OFF, fnaddr); return; }
    if (!g_slotLogged) {
        g_slotLogged = 1;
        plog("SLOT 首次调用 vt=0x%08X fn=0x%08X rank_fn=0x%08X",
             vt, fnaddr, CN_FN_SLOT_SCORE);
    }
    rank = try_read(CN_RANK_OBJ_PTR);
    if (!rank_obj_valid(rank)) return;
    slot = ((FnVtSlot)fnaddr)((int)pl);
    if (slot != g_lastSlot) {
        plog("SLOT 场景槽号 %d -> %d", g_lastSlot, slot);
        g_lastSlot = slot;
    }
    if (slot >= 0 && slot <= 7) {
        double a;
        u.i = 4;  a  = ((FnGetSlot)CN_FN_SLOT_SCORE)((int)rank, u.f, (unsigned)slot);
        u.i = 5;  a += ((FnGetSlot)CN_FN_SLOT_SCORE)((int)rank, u.f, (unsigned)slot);
        g_slotA[0] = a;
        u.i = 10; a  = ((FnGetSlot)CN_FN_SLOT_SCORE)((int)rank, u.f, (unsigned)slot);
        u.i = 14; a += ((FnGetSlot)CN_FN_SLOT_SCORE)((int)rank, u.f, (unsigned)slot);
        g_slotA[1] = a;
    }
}

/* ---------------- worker ---------------- */
static volatile int g_running = 0;

static DWORD WINAPI probe_worker(LPVOID p)
{
    DWORD lastKill[4] = {0};       /* 0x5DA0, 0x5DA4, 0x5DA8, 0x5DAC */
    DWORD lastScorePt = 0;
    DWORD lastSnap = 0, lastRankLog = 0, lastSlotLog = 0, lastPosLog = 0, lastCamLog = 0;
    int posInit = 0;
    static BYTE ndiffPrev[0xA00];
    int ndiffLeft = 40;
    int behaviorPrev[6] = {0};
    int fontPrev[8] = {0};
    int shakePrev = 0;
    int dynPrev[PROBE_DYN_MAX] = {0};
    DWORD heartbeat = 0;
    int vtDumped = 0;
    DWORD workerStart = 0;
    int inGame = 0;
    (void)p;

    Sleep(3000); /* 等游戏初始化 */
    rotate_check();
    plog("=================================================");
    plog("PROBE 90CN v1.3 pid=%lu ini=%s", (unsigned long)GetCurrentProcessId(), g_iniPath);
    plog("CFG HookFont=%d HookBehavior=%d HookShake=%d HookText=%d CallRank=%d",
         cfgHookFont, cfgHookBehavior, cfgHookShake, cfgHookText, cfgCallRank);
    plog("ADDR N2500=0x%08X RANK=0x%08X FONT=0x%08X CAM=0x%08X N964=0x%08X ENC=0x%08X UI=0x%08X",
         CN_N2500_PTR, CN_RANK_OBJ_PTR, CN_MEM_G_DAMAGE_FONT, CN_CAM_OBJ_PTR,
         CN_N964_POS, CN_ENC_TABLE_BASE, CN_UI_MGR_PTR);
    plog("ADDR FONT_FN=0x%08X SCORE_DMG=0x%08X RANK_LVL=0x%08X SLOT=0x%08X TEXTOUT_IAT=0x%08X",
         CN_DAMAGE_FONT, CN_FN_SCORE_DMG, CN_FN_RANK_LEVEL, CN_FN_SLOT_SCORE, CN_TEXTOUTW_IAT);

    /* IAT 槽初值观察 (在 hook 之前) */
    if (mem_readable(CN_TEXTOUTW_IAT, 4))
        plog("SELFCHK TextOutW IAT 槽初值=0x%08X", try_read(CN_TEXTOUTW_IAT));

    /* v1.2: hook 与主动调用全部延迟到"进图门控"通过后, 启动期只做只读观察
     * v1.3: N964 位置地址实测为错, 门控改为 n2500 玩家对象有效 + 30s */
    workerStart = GetTickCount();
    plog("启动期只读观察中, 等待进图 (n2500 有效 + 30s) ...");

    while (g_running) {
        DWORD now = GetTickCount();
        DWORD pl, rank, mgr;

        /* ---- 进图门控: n2500 玩家对象可读 + uptime>=30s 才装 hook/启用调用 ---- */
        if (!inGame) {
            DWORD gp = try_read(CN_N2500_PTR);
            if (now - workerStart >= 30000 && gp && mem_readable(gp, 0xA00)) {
                inGame = 1;
                plog("INGAME 门控通过 (uptime=%lums, pl=0x%08X), 开始安装 hook",
                     (unsigned long)(now - workerStart), gp);
                g_phase = 1;
                {
                    int i, ok = 0;
                    for (i = 0; i < PROBE_MAX_HOOKS; i++) {
                        const HookDef *d = &g_hookDefs[i];
                        int on = (i <= 5) ? cfgHookBehavior : ((i == 6) ? cfgHookShake : cfgHookFont);
                        if (!on) { plog("HOOK %s 跳过 (ini 关闭)", d->name); continue; }
                        ok += hook_one(d->idx, d->target, d->name, d->sig, d->sigLen);
                    }
                    g_hookCount = PROBE_MAX_HOOKS;
                    plog("HOOK 合计安装 %d/8", ok);
                }
                if (cfgHookText) textout_hook_install();
                install_dyn_widenet();   /* v1.3: 行为事件表宽网 (26 项跳 6 + 读表器) */
                g_phase = 0;
            }
        }

        /* ---- 击杀计数器族 (0x5DA0 主 + 邻居对照) ---- */
        pl = try_read(CN_N2500_PTR);
        if (pl && mem_readable(pl, 0x5DAC + 8)) {
            DWORD cur[4];
            cur[0] = *(volatile DWORD *)(pl + CN_KILL_COUNT_OFF);
            cur[1] = *(volatile DWORD *)(pl + 0x5DA4);
            cur[2] = *(volatile DWORD *)(pl + 0x5DA8);
            cur[3] = *(volatile DWORD *)(pl + 0x5DAC);
            if (cur[0] != lastKill[0] || cur[1] != lastKill[1] ||
                cur[2] != lastKill[2] || cur[3] != lastKill[3]) {
                plog("KILL 0x5DA0=%lu(+%ld) 0x5DA4=%lu 0x5DA8=%lu 0x5DAC=%lu",
                     cur[0], (long)cur[0]-(long)lastKill[0], cur[1], cur[2], cur[3]);
                lastKill[0] = cur[0]; lastKill[1] = cur[1];
                lastKill[2] = cur[2]; lastKill[3] = cur[3];
            }
        }

        /* ---- 评分点累加器 (rank+0xC0C, 纯读) ---- */
        rank = try_read(CN_RANK_OBJ_PTR);
        if (rank && mem_readable(rank, 0xC0C + 4)) {
            DWORD pt = *(volatile DWORD *)(rank + CN_SCORE_FINAL_KILL);
            if (pt != lastScorePt) {
                plog("SCOREPT rank+0xC0C %lu -> %lu", lastScorePt, pt);
                lastScorePt = pt;
            }
        }

        /* ---- 评分等级 (进图后 1s 一次; 连续 30 轮成功才解锁槽调用) ---- */
        if (cfgCallRank && inGame && g_rankOk != -1 && now - lastRankLog >= 1000) {
            lastRankLog = now;
            g_phase = 2;
            do_rank_calls();
            g_phase = 0;
            if (g_rankOk == 1)
                plog("RANK dmg=%lu level=%lu", g_rankDmg, g_rankLevel);
        }

        /* ---- 槽号 + 槽评分 (评分调用连续 30 轮成功后, 2s 一次) ---- */
        if (cfgCallRank && inGame && g_rankOkCount >= 30 && now - lastSlotLog >= 2000) {
            lastSlotLog = now;
            g_phase = 3;
            do_slot_calls();
            g_phase = 0;
            plog("SLOTVAL 破甲(4+5)=%.2f 凌空(10+14)=%.2f", g_slotA[0], g_slotA[1]);
        }

        /* ---- v1.3: n2500 玩家对象浮点差分 (找位置字段, 每 2s 一次, 上限 40 轮) ----
         * N964 全局实测为错 -> 改为对比 n2500 前 0xA00 字节的 DWORD 变化,
         * 移动期间持续变化的偏移即位置/朝向候选 (离线分析 NDIFF 日志) */
        if (inGame && ndiffLeft > 0 && now - lastPosLog >= 2000) {
            DWORD gp = try_read(CN_N2500_PTR);
            if (gp && mem_readable(gp, 0xA00)) {
                DWORD *w = (DWORD *)gp;
                int changed = 0, i;
                if (!posInit) {
                    memcpy(ndiffPrev, w, 0xA00);
                    posInit = 1;
                    plog("NDIFF 基线建立 (n2500=0x%08X)", gp);
                } else if (memcmp(ndiffPrev, w, 0xA00) != 0) {
                    for (i = 0; i < 0xA00/4 && changed < 60; i++) {
                        if (ndiffPrev[i] != w[i]) {
                            if (changed < 48)
                                plog("NDIFF +0x%03X %08X->%08X", i*4, ndiffPrev[i], w[i]);
                            changed++;
                        }
                    }
                    if (changed >= 60) plog("NDIFF ... 变化超 60 项, 截断");
                    memcpy(ndiffPrev, w, 0xA00);
                }
                ndiffLeft--;
            }
            lastPosLog = now;
        }

        /* ---- 相机 (cam+1952 震屏强度 + 直接震屏字段) ---- */
        if (now - lastCamLog >= 1000) {
            DWORD cam = try_read(CN_CAM_OBJ_PTR);
            if (cam && mem_readable(cam, 1952 + 8)) {
                float s = *(volatile float *)(cam + CN_CAM_SHAKE_STR);
                if (s != 0.0f)
                    plog("CAMSHAKE 相机+1952=%.3f", s);
            }
            lastCamLog = now;
        }

        /* ---- 飘字容器链快照 (5s) ---- */
        if (now - lastSnap >= 5000) {
            lastSnap = now;
            mgr = try_read(CN_MEM_G_DAMAGE_FONT);
            if (mgr) {
                DWORD cont = mem_readable(mgr + 8, 4) ? try_read(mgr + 8) : 0;
                DWORD node = (cont && mem_readable(cont + 4, 4)) ? try_read(cont + 4) : 0;
                DWORD cnt = try_read(CN_FONT_COUNTER);
                plog("SNAP n2500=0x%08X rank=0x%08X mgr=0x%08X cont=0x%08X node=0x%08X fontcnt=%lu cam=0x%08X ui=0x%08X",
                     pl, rank, mgr, cont, node, cnt,
                     try_read(CN_CAM_OBJ_PTR), try_read(CN_UI_MGR_PTR));
            } else {
                plog("SNAP mgr=NULL n2500=0x%08X rank=0x%08X", pl, rank);
            }
            /* hook 计数器增量 */
            {
                int i;
                for (i = 0; i < 6; i++) {
                    int c = g_cntBehavior[i];
                    if (c != behaviorPrev[i]) {
                        plog("CNT BEH[%d] %d (+%d)", i, c, c - behaviorPrev[i]);
                        behaviorPrev[i] = c;
                    }
                }
                for (i = 0; i < 8; i++) {
                    int c = g_cntFont[i];
                    if (c != fontPrev[i]) {
                        plog("CNT FONT[%d] %d (+%d)%s", i, c, c - fontPrev[i],
                             i == 7 ? " (总)" : "");
                        fontPrev[i] = c;
                    }
                }
                {
                    int c = g_cntShake;
                    if (c != shakePrev) {
                        plog("CNT SHAKE %d (+%d)", c, c - shakePrev);
                        shakePrev = c;
                    }
                }
                /* v1.3: 宽网 hook 计数增量 (活跃路径实证) */
                {
                    int i, total = 0;
                    for (i = 0; i < PROBE_DYN_MAX; i++) {
                        int c = g_cntDyn[i];
                        total += c;
                        if (c != dynPrev[i]) {
                            plog("CNT DYN[%d] %s %d (+%d)", i, g_dyn[i].name ? g_dyn[i].name : "?",
                                 c, c - dynPrev[i]);
                            dynPrev[i] = c;
                        }
                    }
                    (void)total;
                }
            }
        }

        /* ---- vtable 槽位 dump (进图后一次, 找闪避 getter 用) ---- */
        if (!vtDumped && inGame && pl && n2500_valid_for_call(pl)) {
            DWORD vt = try_read(pl);
            if (mem_readable(vt, 0x700)) {
                /* US 闪避=0x358; 漂移候选 0x358/0x378/0x398/0x3b8/0x3d8 + 槽号 0xC54 */
                static const int offs[] = { 0x358, 0x360, 0x368, 0x370, 0x378,
                                            0x380, 0x388, 0x390, 0x398, 0x3a0,
                                            0x3b8, 0x3d8, 0x62C, 0x6AC, 0xC54 };
                int i;
                plog("VTABLE n2500 vtable=0x%08X 槽位观察:", vt);
                for (i = 0; i < 15; i++) {
                    DWORD fn = try_read(vt + offs[i]);
                    plog("VTABLE [+0x%03X] = 0x%08X %s", offs[i], fn,
                         in_text(fn) ? "(code)" : "");
                }
                vtDumped = 1;
            }
        }

        /* ---- 心跳 (30s) ---- */
        if (now - heartbeat >= 30000) {
            int dynTotal = 0, di;
            for (di = 0; di < PROBE_DYN_MAX; di++) dynTotal += g_cntDyn[di];
            heartbeat = now;
            plog("HEARTBEAT alive inGame=%d rankOk=%d rankN=%lu slotN=%d dynTotal=%d ndiffLeft=%d vehLogged=%d",
                 inGame, g_rankOk, (unsigned long)g_rankOkCount, g_lastSlot,
                 dynTotal, ndiffLeft, g_vehLogged);
        }

        Sleep(200);
    }
    hooks_uninstall();
    textout_hook_uninstall();
    plog("PROBE 退出");
    return 0;
}

/* ---------------- 导出 + DllMain ---------------- */
__declspec(dllexport) void __cdecl DfoVibrationLoaded(void)
{
    /* 幂等: 由 DllMain 启动 */
}

__declspec(dllexport) void __cdecl VibPluginLoaded(void)
{
    DfoVibrationLoaded();
}

__declspec(dllexport) void __cdecl VibPluginDeinit(void)
{
    g_running = 0;
}

static HMODULE g_self = NULL;

static void derive_paths(void)
{
    char path[MAX_PATH];
    char *slash, *dot;
    if (!GetModuleFileNameA(g_self, path, MAX_PATH)) return;
    slash = strrchr(path, '\\');
    if (!slash) return;
    *slash = 0;
    dot = NULL;
    {
        char *p = strrchr(slash + 1, '.');
        if (p) { *p = 0; dot = p; }
        _snprintf(g_iniPath, sizeof(g_iniPath), "%s\\%s.ini", path, slash + 1);
        _snprintf(g_logPath, sizeof(g_logPath), "%s\\%s_dll.log", path, slash + 1);
        if (dot) *dot = '.';
    }
}

BOOL WINAPI DllMain(HINSTANCE hDll, DWORD reason, LPVOID reserved)
{
    (void)reserved;
    if (reason == DLL_PROCESS_ATTACH) {
        g_self = (HMODULE)hDll;
        DisableThreadLibraryCalls(hDll);
        derive_paths();
        load_ini();
        /* 确认宿主是 DNF (主模块基址 0x400000 且代码段签名匹配), 否则不启动 */
        if (mem_readable(0x400000, 2) && sig_match(CN_DAMAGE_FONT, g_hookDefs[7].sig, 6)) {
            g_running = 1;
            g_vehHandle = AddVectoredExceptionHandler(1, probe_veh);
            CreateThread(NULL, 0, probe_worker, NULL, 0, NULL);
            plog("DllMain: 宿主校验通过, worker 已启动 (VEH=%d)", g_vehHandle ? 1 : 0);
        } else {
            plog("DllMain: 宿主不是 90CN DNF (基址/签名不符), 探针不启动");
        }
    } else if (reason == DLL_PROCESS_DETACH) {
        /* v1.3: 进程退出 (reserved!=NULL) 时什么都不做 —— 进程即将消亡, 还原字节无意义,
         * 且退出阶段还原曾触发只读页 AV; 只有 FreeLibrary 卸载 (reserved==NULL) 才清理 */
        if (reserved == NULL) {
            g_running = 0;
            hooks_uninstall();
            textout_hook_uninstall();
        }
        if (g_vehHandle) {
            RemoveVectoredExceptionHandler(g_vehHandle);
            g_vehHandle = NULL;
        }
    }
    return TRUE;
}
