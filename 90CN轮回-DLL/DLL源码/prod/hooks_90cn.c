/* ============================================================
 * 90CN 生产版 - hook 引擎 + 事件分发 + 轮询采集
 * 架构依据: 开发文档/四轮实测定稿_探针v1.3.md
 *   - FONT hook (damage_font) = 唯一采集 hook (a6 位分类, 四轮实证)
 *   - 行为事件/TextOutW = 死路线, 不实现
 *   - 镜头震动 4 hook = 预留接口 (CN_SHAKE_ENABLE=0 默认不装)
 *   - KILL/SCOREPT/RANK/SLOT/MOVE = 零 hook 只读轮询 (四轮实证)
 * 安全: insn_len 完整版 + 未知 opcode 拒绝 + VirtualProtect 写还原
 * ============================================================ */
#include <windows.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include "hooks_90cn.h"
#include "../common/vib_protocol.h"
#include "../common/vib_90cn_addrs.h"

/* 汇编 stub 符号 (C 侧无下划线, 汇编侧带) */
extern void vib_stub_0(void);
extern void vib_stub_1(void);
extern void vib_stub_2(void);
extern void vib_stub_3(void);
extern void vib_stub_4(void);
extern void vib_stub_5(void);
extern void vib_stub_6(void);
extern void vib_stub_7(void);

void *vib_tramp_0, *vib_tramp_1, *vib_tramp_2,
     *vib_tramp_3, *vib_tramp_4, *vib_tramp_5, *vib_tramp_6, *vib_tramp_7;

/* 采集输出回调 (dfovib_90cn.c 提供) */
extern void vib_collect(int type, DWORD strength, DWORD tick, DWORD count);
extern volatile int g_ringReady;
extern void dll_log2(const char *fmt, ...);

/* ---------------- 只读内存访问 (四轮验证的防御式读) ---------------- */
static int mem_readable(DWORD addr, DWORD size)
{
    MEMORY_BASIC_INFORMATION mbi;
    if (VirtualQuery((LPCVOID)addr, &mbi, sizeof(mbi)) == 0)
        return 0;
    if (mbi.State != MEM_COMMIT)
        return 0;
    if (mbi.Protect & (PAGE_NOACCESS | PAGE_GUARD))
        return 0;
    if ((DWORD)((BYTE *)mbi.BaseAddress + mbi.RegionSize - addr) < size)
        return 0;   /* 跨到未提交区 */
    return 1;
}

static DWORD try_read_dword(DWORD addr)
{
    if (!mem_readable(addr, 4)) return 0;
    return *(volatile DWORD *)addr;
}

static int in_text(DWORD addr)
{
    return addr >= CN_TEXT_LO && addr < CN_TEXT_HI;
}

/* ---------------- insn_len 完整版 (探针 v1.3, 四轮实机验证) ---------------- */
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
    case 0x83: {                            /* grp1 r/m, imm8 (含 imm 字节) */
        BYTE modrm = p[1];
        if ((modrm & 0xC0) == 0xC0) return 3;
        if ((modrm & 0x07) == 0x05) return 7;
        if ((modrm & 0xC0) == 0x40) return 4;
        if ((modrm & 0xC0) == 0x80) return 7;
        return 3;
    }
    case 0x81: {                            /* grp1 r/m, imm32 (含 imm 字节) */
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
        /* ★v3.1 修复: rm==0x04 是 SIB 寻址, 原实现会漏掉 SIB 字节与 disp
         * (教训: `jmp dword ptr [eax*4 + 绝对表地址]` = FF 24 85 + disp32 = **7 字节**,
         *  原实现返回 2 -> hook 长度错位、trampoline 损坏。insn_len 漏 SIB 是真 bug) */
        if ((modrm & 0x07) == 0x04) {
            BYTE sib = p[2];
            if ((modrm & 0xC0) == 0x00 && (sib & 0x07) == 0x05) return 7; /* [idx*4+disp32] */
            if ((modrm & 0xC0) == 0x40) return 4;                        /* [..+disp8] */
            if ((modrm & 0xC0) == 0x80) return 7;                        /* [..+disp32] */
            return 3;                                                    /* 无 disp */
        }
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
        return -1;  /* 未知 opcode 一律拒绝 (探针 v1.3 定案) */
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

/* ---------------- hook 安装/卸载 (VirtualProtect 版) ---------------- */
typedef struct {
    DWORD target;
    BYTE  original[16];
    int   hookLen;
    int   installed;
} HookCtx;

static HookCtx g_hooks[VIB_MAX_HOOKS];
static int g_hookCount = 0;
static int g_installed = 0;
static BYTE *g_trampPage = NULL;

static void *const s_stubs[VIB_MAX_HOOKS] = {
    (void *)vib_stub_0, (void *)vib_stub_1, (void *)vib_stub_2,
    (void *)vib_stub_3, (void *)vib_stub_4, (void *)vib_stub_5,
    (void *)vib_stub_6, (void *)vib_stub_7
};
static void **const s_trampVars[VIB_MAX_HOOKS] = {
    &vib_tramp_0, &vib_tramp_1, &vib_tramp_2,
    &vib_tramp_3, &vib_tramp_4, &vib_tramp_5,
    &vib_tramp_6, &vib_tramp_7
};

int hooks_install(const DWORD *targets, int count)
{
    int i, ok = 0;
    int base = g_hookCount;   /* 追加式: 可多次调用 */

    if (base + count > VIB_MAX_HOOKS) count = VIB_MAX_HOOKS - base;
    if (count <= 0) return 0;

    for (i = 0; i < count; i++) {
        HookCtx *h = &g_hooks[base + i];
        DWORD target = targets[i];
        BYTE b0;
        int len;

        h->target = 0;
        h->installed = 0;
        if (!in_text(target)) {
            dll_log2("HOOK [%d] 0x%08X 不在代码段, 拒绝", base + i, target);
            continue;
        }
        if (!mem_readable(target, 16)) {
            dll_log2("HOOK [%d] 0x%08X 不可读, 拒绝", base + i, target);
            continue;
        }
        b0 = *(BYTE *)target;
        if (b0 == 0xE9 || b0 == 0xE8 || b0 == 0xEB || b0 == 0xC3 || b0 == 0xC2) {
            dll_log2("HOOK [%d] 0x%08X 首指令为跳转/返回, 拒绝", base + i, target);
            continue;
        }
        len = calc_hook_len(target);
        if (len < 5 || len > 16) {
            dll_log2("HOOK [%d] 0x%08X hookLen=%d 异常, 拒绝", base + i, target, len);
            continue;
        }
        if (!g_trampPage) {
            g_trampPage = (BYTE *)VirtualAlloc(NULL, VIB_MAX_HOOKS * 32,
                                               MEM_COMMIT | MEM_RESERVE,
                                               PAGE_EXECUTE_READWRITE);
            if (!g_trampPage) {
                dll_log2("HOOK: trampoline 页分配失败");
                return ok;
            }
        }
        h->target = target;
        h->hookLen = len;
        memcpy(h->original, (void *)target, len);
        /* trampoline: 原指令 + E9 回跳 */
        {
            BYTE *t = g_trampPage + (base + i) * 32;
            DWORD stubAddr = (DWORD)(ULONG_PTR)s_stubs[base + i];
            memcpy(t, h->original, len);
            t[len] = 0xE9;
            *(DWORD *)(t + len + 1) = target + len - (DWORD)(ULONG_PTR)(t + len + 5);
            *s_trampVars[base + i] = t;
            /* 写 patch: CN 代码页 RX, 临时改权限 (探针方案) */
            {
                DWORD oldProt = 0;
                BYTE *p = (BYTE *)target;
                int j;
                if (!VirtualProtect(p, len, PAGE_EXECUTE_READWRITE, &oldProt)) {
                    dll_log2("HOOK [%d] 0x%08X VirtualProtect 失败 err=%lu, 拒绝",
                             base + i, target, GetLastError());
                    h->target = 0;
                    continue;
                }
                p[0] = 0xE9;
                *(DWORD *)(p + 1) = stubAddr - target - 5;
                for (j = 5; j < len; j++) p[j] = 0x90;
                VirtualProtect(p, len, oldProt, &oldProt);
                FlushInstructionCache(GetCurrentProcess(), p, len);
            }
        }
        h->installed = 1;
        ok++;
        dll_log2("HOOK [%d] @0x%08X 安装成功 (len=%d)", base + i, target, len);
    }
    if (base + count > g_hookCount) g_hookCount = base + count;
    if (ok > 0) g_installed = 1;
    return ok;
}

void hooks_uninstall(void)
{
    int i;
    for (i = 0; i < g_hookCount; i++) {
        HookCtx *h = &g_hooks[i];
        if (h->installed && h->target) {
            /* CN 代码页只读, 还原前临时改权限 (探针 v1.3 定案) */
            DWORD oldProt = 0;
            if (VirtualProtect((void *)h->target, h->hookLen,
                               PAGE_EXECUTE_READWRITE, &oldProt)) {
                memcpy((void *)h->target, h->original, h->hookLen);
                VirtualProtect((void *)h->target, h->hookLen, oldProt, &oldProt);
                FlushInstructionCache(GetCurrentProcess(), (void *)h->target, h->hookLen);
            }
            h->installed = 0;
        }
    }
    g_installed = 0;
}

int hooks_active(void)
{
    return g_installed;
}

/* ---------------- FONT 采集 (聚合计数, US 新架构"纯飘字单源"平移) ----------------
 * hook 回调在游戏线程: 只做原子计数, 冲刷由 collector 执行 (6ms)。
 * a6 位语义四轮实证与 US 完全一致:
 *   0x01 命中 0x02 受击 0x04 特效 0x08 状态 0x10 特殊 0x20 DOT */
static volatile LONG g_cnt_hp      = 0;  /* 0x20 DOT */
static volatile LONG g_cnt_special = 0;  /* 0x10 特殊攻击 */
static volatile LONG g_cnt_state   = 0;  /* 0x08 状态 */
static volatile LONG g_cnt_effect  = 0;  /* 0x04 装备特效 */
static volatile LONG g_cnt_attack  = 0;  /* 0x01 命中 */
static volatile LONG g_cnt_hit     = 0;  /* 0x02 受击 */

/* ---------------- v2.4 判定级探针 (只记录, 不改变任何发送行为) ----------------
 * 目的: 用一次对照实验把 CN **自己**的"怪物死亡"信号定下来。
 * 依据 (2026-09-15 全 .text 同源比对, work/find_cn_death_v8.py):
 *   90US 的死亡链 (sub_1C58AC0 受击处理 → sub_F212A0/F20BF0) 在 CN 客户端里
 *   **没有同源函数** —— 380109 个函数头全扫, 最高相似度 70.3% (真同源阈值 ~97%,
 *   校准对: 击杀特写 99.1% / case15 96.7% / converge 98.9% / damage_font 97.8%)。
 *   即: 私服把死亡/任务这块改写了, 90US 的定位思路在此不适用 —— 这正是
 *   v1.9/v2.0/v2.2 三次 hook 全零触发的根本原因。
 * 故改用 CN 原生候选, 本探针负责把它们记录到可判定:
 *   候选① pl+0x5DA0 上升沿 (现配【释放技能】; 探针记录称"全场 8 次击杀",
 *          但 US 侧又称其为技能释放计数 —— 语义未定, 需实验判定)
 *   候选② rank+0xC0C 归零 (四轮探针记录称"怪死后归零")
 *   候选③ 旁证: pl+0x5DA4/0x5DA8/0x5DAC (US 击杀处理会置 [5748]=1/[5749]=0)
 * 判定实验: 轮A 只放技能不杀怪 / 轮B 只用普攻杀 N 只怪 —— 见交接文档测试协议 */
static volatile LONG g_a6hist[256];      /* 原始 a6 低字节直方图 (累计) */
static volatile LONG g_a6hi = 0;         /* a6 > 0xFF 的计数 */
static volatile LONG g_tot_font = 0;     /* FONT 总调用次数 (累计) */
static volatile LONG g_pulse_rise = 0;   /* pl+0x5DA0  0->1 次数 */
static volatile LONG g_pulse_fall = 0;   /* pl+0x5DA0  1->0 次数 */
static volatile LONG g_spt_inc = 0;      /* rank+0xC0C 上升次数 */
static volatile LONG g_spt_reset = 0;    /* rank+0xC0C 归零次数 */
static volatile DWORD g_probe_last = 0;

/* ---------------- v2.8 通用「死亡候选探针」+ 分发器参数诊断 (只计数+记录, 不发送) ----------------
 * 背景: v2.5 / v2.6 两次"同源目标"实测都零触发 -> **同源匹配只能确认函数身份,
 *       不能确认"现在跑的是它"**。于是改用"计数法": 把若干候选挂上 hook,
 *       只看**每局被调用几次** —— 真正的"每次死亡走一次"的函数应≈击杀数。
 * 槽位 2..6 (原 screen/readbar/direct/onShakeInput/死亡分支, 实测全零触发 -> 全部改作探针):
 *   [2] 0x032E7380 死亡管理器候选 (US sub_2AA1320 同源 75.5%)
 *   [3] 0x024DA7B0 击杀类型检查候选 (US sub_1CEAD00 同源 72.4%)
 *   [4] 0x02518770 玩家vtable槽40候选 (US sub_1D1E9F0 同源 80.0%)
 *   [5] 0x02493A40 击杀包装候选   (US sub_1CA90A0 同源 53.7%)
 *   [6] 0x024C4B10 受击结算入口   (US sub_1CD6D30 同源 93.3%) —— **对照组**:
 *       用于区分"函数根本没被调用" vs "v2.6 的分支没走到"
 * 槽位 7: 0x013EEFF0 飘字分发器 (damage_font 的唯一直接调用者, 30 个调用者=确定在线)
 *   —— 抓 6 个栈参, 用"同一只怪连打 12 下期间哪个参数不变"锁定被击者指针。 */
static volatile LONG g_probe_hits[8];
static volatile LONG g_diag_n = 0;
/* g_die_seen 随【怪物死亡】一并停用 (v6.0): 其自增与打印均已注释 */

/* ---------------- v5.0 击杀记录区变化检测 (【怪物死亡】正式信号) ----------------
 * 实测依据(v4.2): 评分对象 score+0x924..0x99C(约 120 字节)被整体重写, 次数与击杀数一致,
 *   值恒为加密态(乱码) -> 与老版记录"击杀记录容器 score+2904+20*idx, 6×20=120 字节,
 *   {怪物ID→计数}, XOR 加密"完全对应。
 * 判定: **该块任一字发生变化 = 新增一条击杀记录 = 一次怪物死亡** (无需解密)。
 * 去重: 200ms (同一次记录写入会同时改多个字, 靠轮询只记一次)。
 * 诊断: 每次触发打 tick + 本次变化的字段数; 5s 汇总触发总次数, 便于与击杀数核对。 */
#define DK_OFF   0x0900
#define DK_N     64                    /* 0x0900..0x0A00 = 256 字节 (覆盖 0x924..0x99C) */
static DWORD g_dk_prev[DK_N];
static int   g_dk_init = 0;
static DWORD g_dk_lastTick = 0;
static DWORD g_dk_pollTick = 0;
static volatile LONG g_dk_events = 0;

static void poll_death_block(void)
{
    DWORD now = GetTickCount();
    DWORD score;
    int i, changed = 0;
    if (now - g_dk_pollTick < 100) return;      /* 100ms 轮询 */
    g_dk_pollTick = now;
    score = try_read_dword(CN_RANK_OBJ_PTR);
    if (!score) return;
    for (i = 0; i < DK_N; i++) {
        DWORD v = try_read_dword(score + DK_OFF + i * 4);
        if (!g_dk_init) { g_dk_prev[i] = v; continue; }
        if (v != g_dk_prev[i]) { g_dk_prev[i] = v; changed++; }
    }
    if (!g_dk_init) { g_dk_init = 1; return; }
    if (changed > 0 && (now - g_dk_lastTick) >= 200) {
        g_dk_lastTick = now;
        InterlockedIncrement(&g_dk_events);
        /* ================= 【怪物死亡】已注释停用 (2026-09-15, v6.0) =================
         * 原因: ① 该区块经实测是"每秒定时冲刷的评分区"(触发间隔≈1000ms, 与击杀无关)——
         *           v4.2 里"恰好变化 5 次"是 300ms 采样对上 1s 冲刷的**混叠假象**;
         *       ② 【怪物死亡】通道最终未找到任何可用信号(详见
         *          `开发文档/怪物死亡信号_调查总结_20260915.md`)。
         * 处置: **不向宿主发送 VEV_TARGET_DIE**, 避免宿主【怪物死亡】滑块被误触发。
         * 本函数 (poll_death_block) 亦已不再被 collector 调用; 代码保留供日后研究。
         * ==========================================================================
        if (g_ringReady)
            vib_collect(VEV_TARGET_DIE, 100, now, 1);
        dll_log2("DEATHBLK #%ld tick=%lu 变化字段数=%d", (long)g_dk_events,
                 (unsigned long)now, changed);
        */
        (void)changed;
    }
}

/* ---------------- v3.0 通知/命令分发器 case 直方图探针 (只计数, 不发送) ----------------
 * 老版(ACT1/ACT4/ACT5)的死亡信号 = hook 客户端**通知分发器的 case38**(MonsterDie)。
 * 本版先不定 case 号: hook 7 个候选 switch 站点的 `jmp [eax*4+跳表]`, 在跳转前取 eax(=case 号)
 * 做直方图 -> 杀 N 只怪后, **计数恰好 =N 的 case 号就是 MonsterDie**。
 * hook 索引 -> 站点地址的对应关系在安装时打进日志(install_map), 用于解读直方图。 */
static volatile LONG g_nfy_cnt[8][1024];

/* ---------------- v4.2 字段变化扫描 (评分对象 + 玩家对象) ----------------
 * 目的: 找"每只怪死亡变一次、且单调递增"的字段 = CN 的击杀计数。
 * 依据: ① 老版 US 的击杀记录容器在 score+2904(本轮实测 CN 该区无变化);
 *       ② US 击杀处理对**玩家对象** n2500[5748]=1 / [5749]=0,
 *          CN 探针实测同簇 0x5DA4=1 / 0x5DA8=0 (在 0x5DA0 释放技能标志旁边)。
 * 做法: 每 300ms 快照两个窗口, 统计每个 dword 偏移"改变了几次"+首值/现值;
 *   只打印 **变化 2..12 次 且 现值>首值**(单调递增) 的偏移 —— 计数器才符合。
 * 零 hook, 纯守卫式只读。 */
#define SW_N  1024                       /* 每窗口 dword 数 (4KB) */
static DWORD g_sw_prev[2][SW_N];
static DWORD g_sw_first[2][SW_N];
static DWORD g_sw_cnt[2][SW_N];
static int   g_sw_init = 0;
static DWORD g_sw_last = 0;

/* 窗口0 = 评分对象 +0x0000; 窗口1 = 玩家对象 +0x5000 (含 0x5DA0 簇) */
static DWORD sw_base(int w)
{
    return (w == 0) ? try_read_dword(CN_RANK_OBJ_PTR)
                    : try_read_dword(CN_N2500_PTR) + 0x5000;
}

static void poll_scan(void)
{
    DWORD now = GetTickCount();
    int w, i;
    if (now - g_sw_last < 300) return;
    g_sw_last = now;
    for (w = 0; w < 2; w++) {
        DWORD base = sw_base(w);
        if (!base) continue;
        for (i = 0; i < SW_N; i++) {
            DWORD v = try_read_dword(base + i * 4);
            if (!g_sw_init) { g_sw_prev[w][i] = v; g_sw_first[w][i] = v; continue; }
            if (v != g_sw_prev[w][i]) {
                g_sw_prev[w][i] = v;
                if (g_sw_cnt[w][i] < 0xFFFFu) g_sw_cnt[w][i]++;
            }
        }
    }
    g_sw_init = 1;
}

static void scan_dump(void)
{
    int w, i, shown;
    for (w = 0; w < 2; w++) {
        shown = 0;
        for (i = 0; i < SW_N; i++) {
            DWORD c = g_sw_cnt[w][i];
            if (c >= 2 && c <= 12 && g_sw_prev[w][i] > g_sw_first[w][i]) {
                dll_log2("SCAN[%d] +0x%04X 变化 %lu 次 首值=%lu -> 现值=%lu%s",
                         w, (w == 0 ? 0 : 0x5000) + i * 4, (unsigned long)c,
                         (unsigned long)g_sw_first[w][i], (unsigned long)g_sw_prev[w][i],
                         ((unsigned long)g_sw_first[w][i] + c == (unsigned long)g_sw_prev[w][i])
                             ? "  ★每次+1" : "");
                if (++shown >= 12) { dll_log2("SCAN[%d] ...(其余略)", w); break; }
            }
        }
        if (!shown) dll_log2("SCAN[%d] 无『变化2..12次且递增』的字段", w);
    }
}

void __cdecl vib_dispatch_notify(DWORD idx, int type)
{
    if (type < 1 || type > 7) return;
    if (idx < 1024) InterlockedIncrement(&g_nfy_cnt[type][idx]);
    else InterlockedIncrement(&g_nfy_cnt[type][1023]);   /* 溢出桶 */
}

/* 取前 4 名的 case 号与计数 (供 5s 汇总打印) */
static void nfy_dump(void)
{
    int t, i, k, m;
    for (t = 1; t <= 7; t++) {
        LONG tv[4] = { 0, 0, 0, 0 };
        int  ti[4] = { -1, -1, -1, -1 };
        int any = 0;
        for (i = 0; i < 1024; i++) {
            LONG c = g_nfy_cnt[t][i];
            if (c <= 0) continue;
            any = 1;
            for (k = 0; k < 4; k++) {
                if (c > tv[k]) {
                    for (m = 3; m > k; m--) { tv[m] = tv[m - 1]; ti[m] = ti[m - 1]; }
                    tv[k] = c; ti[k] = i;
                    break;
                }
            }
        }
        if (any)
            dll_log2("NOTIFY[%d] case直方图 top4: #%d x%ld | #%d x%ld | #%d x%ld | #%d x%ld",
                     t, ti[0], (long)tv[0], ti[1], (long)tv[1],
                     ti[2], (long)tv[2], ti[3], (long)tv[3]);
    }
}

void __cdecl vib_dispatch_probe(void *obj, int type)
{
    LONG n;
    if (type < 0 || type > 7) return;
    n = InterlockedIncrement(&g_probe_hits[type]);
    if (n <= 40)
        dll_log2("PROBEHOOK[%d] #%ld obj=0x%08X", type, (long)n,
                 (DWORD)(ULONG_PTR)obj);
}

void __cdecl vib_dispatch_diag7(DWORD ecx_v, DWORD a1, DWORD a2, DWORD a3,
                                DWORD a4, DWORD a5, DWORD a6, int type)
{
    LONG n;
    (void)type;
    n = InterlockedIncrement(&g_diag_n);
    if (n <= 400)
        dll_log2("DIAG7 #%ld ecx=0x%08X a1=0x%08X a2=0x%08X a3=0x%08X "
                 "a4=0x%08X a5=0x%08X a6=0x%08X",
                 (long)n, ecx_v, a1, a2, a3, a4, a5, a6);
}

void __cdecl vib_dispatch_font_hit(void *obj, DWORD a2, DWORD a3, DWORD a4, DWORD a5,
                                   DWORD a6, int type)
{
    (void)type; (void)obj;
    InterlockedIncrement(&g_tot_font);
    if (a6 > 0xFFu) InterlockedIncrement(&g_a6hi);
    else InterlockedIncrement(&g_a6hist[a6 & 0xFFu]);
    /* v2.7 已判读完毕: damage_font 的 this/a2..a6 全是**显示参数**(this 每次命中都变、
     * a3 恒 0xFF、a4 离散枚举、a5 像坐标) -> 被击者不在飘字层, 故移除逐条诊断日志。 */
    (void)a2; (void)a3; (void)a4; (void)a5; (void)obj;
    /* 优先级与引擎一致: 0x20 > 0x10 > 0x08 > 0x04 > 0x01 > 0x02 */
    if (a6 & 0x20)      InterlockedIncrement(&g_cnt_hp);
    else if (a6 & 0x10) InterlockedIncrement(&g_cnt_special);
    else if (a6 & 0x08) InterlockedIncrement(&g_cnt_state);
    else if (a6 & 0x04) InterlockedIncrement(&g_cnt_effect);
    else if (a6 & 0x01) InterlockedIncrement(&g_cnt_attack);
    else if (a6 & 0x02) InterlockedIncrement(&g_cnt_hit);
}

void vib_flush_counts(void)
{
    LONG v;
    /* v6.1 手感对齐 90US (用户 2026-09-15 拍板, 基准路线 S4+): 恢复 US 生产版聚合形态
     * (90US hooks.c vib_flush_counts 同款): 每类每冲刷发 1 条 count=N。
     * v1.4 的逐条拆发是按老版 ACT4/ACT5 手感基准的拍板, 与 90US 手感相反, 本版撤销。
     * 宿主侧 count 一次性计入 combo_hits/density_hits/burst_hits (vibration.rs:995)。
     * 32 上限保留 (US 无上限; 仅防异常计数风暴, 实战不可达)。 */
    v = InterlockedExchange(&g_cnt_hp, 0);
    if (v > 32) v = 32;
    if (v > 0) vib_collect(VEV_FONT, 0x20, GetTickCount(), (DWORD)v);
    v = InterlockedExchange(&g_cnt_special, 0);
    if (v > 32) v = 32;
    if (v > 0) vib_collect(VEV_FONT, 0x10, GetTickCount(), (DWORD)v);
    v = InterlockedExchange(&g_cnt_state, 0);
    if (v > 32) v = 32;
    if (v > 0) vib_collect(VEV_FONT, 0x08, GetTickCount(), (DWORD)v);
    v = InterlockedExchange(&g_cnt_effect, 0);
    if (v > 32) v = 32;
    if (v > 0) vib_collect(VEV_FONT, 0x04, GetTickCount(), (DWORD)v);
    v = InterlockedExchange(&g_cnt_attack, 0);
    if (v > 32) v = 32;
    if (v > 0) vib_collect(VEV_FONT, 0x01, GetTickCount(), (DWORD)v);
    v = InterlockedExchange(&g_cnt_hit, 0);
    if (v > 32) v = 32;
    if (v > 0) vib_collect(VEV_FONT, 0x02, GetTickCount(), (DWORD)v);
}

/* ---------------- 镜头震动预留接口 (CN_SHAKE_ENABLE=0 时不安装) ----------------
 * 分发逻辑完整可用: 平方根放大 + 去重 + 发送。
 * 定位确认后启用步骤:
 *   1. vib_90cn_addrs.h 填 CN_SHAKE_* 地址 (已有静态候选)
 *   2. CN_SHAKE_ENABLE 改 1 (dfovib_90cn.c 安装段)
 *   3. 重编译部署。若参数语义与 US 签名不同, 只调 hooks_90cn_asm.s 取参偏移 */
static volatile DWORD g_shakeRecent = 0;

static DWORD shake_amplify(float f)
{
    DWORD s;
    if (f <= 0.0f) f = 0.0f;
    if (f > 1.0f) f = 1.0f;
    s = (DWORD)(sqrtf(f) * 100.0f);   /* 平方根放大: 小强度充分展开 (US 同款) */
    if (s < 25) s = 25;
    if (s > 100) s = 100;
    return s;
}

/* CRIT_SHAKE 分类窗口 (探针 v1.4 实测 + US 同源指令比对定位, 2026-09-14):
 *   0x024A3A20~0x024A3C29 = 击杀特写 (US sub_1CB80A0 同源, 196 指令同构仅地址重定位)
 *   0x0252D550~0x0252D615 = 死亡特写 case15 (US sub_1D30270 同源; 实测活跃:
 *                            26 次调用恒 ret=0x0252D5BC, 参数 1.5f/100/150 硬编码) */
static const struct { DWORD lo, hi; } s_critShakeWins[] = {
    { 0x024A3A20, 0x024A3C29 },   /* 击杀特写 */
    { 0x0252D550, 0x0252D615 },   /* 死亡特写 case15 (实测活跃) */
};

/* ---------------- 震屏控制器对象 + 字段轮询 (v1.3 新增) ----------------
 * 探针 v1.5 两轮对照实证 (2026-09-14):
 *   converge 的 this 对象 +0x790 = 震屏标志 (0x100=震屏中 / 0=停止, 实测持续 ~500ms)
 *   轮B(震屏技能) 11 次 0<->0x100 交替; 轮A(不震屏技能) 该字段零变化
 *   上升沿伴随 +0x168(0->1) / +0x184+0x3D8+0x5FC(写演出对象指针) / +0x52C 计数
 * 实现: collector 线程 6ms 轮询 [this+0x790] 上升沿 -> 发 VEV_CRIT_SHAKE(强度100/时长500ms) */
#define CN_SHAKE_CTL_FLAG_OFF   0x790
static volatile DWORD g_shakeCtlObj = 0;   /* converge 的 this (动态捕获) */
static DWORD g_shakeCtlLast = 0;           /* 上次标志值 (上升沿检测) */

void __cdecl vib_dispatch_skillhit(void *obj, DWORD strength_bits,
                                   DWORD time_ms, DWORD retaddr, int type)
{
    DWORD i;
    int isCrit = 0;

    /* this = 震屏控制器对象 (探针 v1.5 实证 +0x790 震屏标志) */
    if (obj) g_shakeCtlObj = (DWORD)(ULONG_PTR)obj;

    /* 相机系统内部调用 -> 忽略去重 (探针 v1.4 静态比对定位, US 同款语义):
     *   0x02639600~0x026396E6 帧更新重放 / 0x026396F0~0x0263972B screen 转发
     *   0x02637B60~0x02637BEB 震屏结束收尾 */
    if (retaddr >= 0x02639600 && retaddr < 0x0263972B) return;
    if (retaddr >= 0x02637B60 && retaddr < 0x02637BEB) return;

    for (i = 0; i < sizeof(s_critShakeWins) / sizeof(s_critShakeWins[0]); i++) {
        if (s_critShakeWins[i].lo && retaddr >= s_critShakeWins[i].lo &&
            retaddr < s_critShakeWins[i].hi) { isCrit = 1; break; }
    }
    /* 【CRIT_SHAKE 发送搁置】v1.2 (2026-09-14): v1.1 实测 0x252D550 窗口是高频通用攻击演出
     * (每技能每 hit 触发, 同 tick 17 连发), 并非 US 低频死亡特写 -> 所有技能乱震, 已停发。
     * 窗口判定保留 (日志 crit=%d 可观测), 恢复发送 = 放开下方 collect:
     *   0x024A3A20 击杀特写 (US sub_1CB80A0 同源, 实测零触发, 启用前先验证频率)
     *   0x252D550 通用演出 (US 同源但 CN 行为漂移, 单独启用 = 全技能乱震, 勿用) */
    // if (isCrit)
    //     vib_collect(VEV_CRIT_SHAKE, shake_amplify(*(float *)&strength_bits), now, time_ms);
    /* 【技能震动】搁置 (对齐 US/ACT4/ACT5 现状): 震屏技能专属路径未定位, 不发。
     * 将来恢复: 放开下方 collect (VEV_SKILL_HIT) */
    // vib_collect(VEV_SKILL_HIT, shake_amplify(*(float *)&strength_bits), now, time_ms);
    dll_log2("SHAKE skillhit type=%d str=%.3f amp=%lu time=%lu ret=0x%08X crit=%d",
             type, *(float *)&strength_bits, shake_amplify(*(float *)&strength_bits),
             time_ms, retaddr, isCrit);
}

void __cdecl vib_dispatch_shake(void *obj, DWORD strength_bits, DWORD time_ms, int type)
{
    DWORD now;
    (void)obj; (void)type;
    now = GetTickCount();
    if (now - g_shakeRecent < 100) return;
    g_shakeRecent = now;
    /* 【镜头震动】搁置 (对齐 US): screen 转发实测零调用, 不发送 */
    // vib_collect(VEV_SHAKE_SCREEN, shake_amplify(*(float *)&strength_bits), now, time_ms);
    dll_log2("SHAKE screen str=%.3f time=%lu", *(float *)&strength_bits, time_ms);
}

void __cdecl vib_dispatch_readshake(void *obj, DWORD flag_bits,
                                    DWORD strength_bits, int type)
{
    DWORD now;
    (void)obj; (void)type;
    if ((BYTE)flag_bits != 1) return;   /* 只采"开始震屏" (0=停止), US 语义 */
    now = GetTickCount();
    if (now - g_shakeRecent < 100) return;
    g_shakeRecent = now;
    /* 【镜头震动】搁置 (对齐 US): readbar 实测仅"停止"标志, 不发送 */
    // vib_collect(VEV_SHAKE_SCREEN, shake_amplify(*(float *)&strength_bits), now, 200);
    dll_log2("SHAKE readbar str=%.3f", *(float *)&strength_bits);
}

void __cdecl vib_dispatch_directshake(void *obj, DWORD strength_bits, int type)
{
    DWORD now;
    (void)obj; (void)type;
    now = GetTickCount();
    if (now - g_shakeRecent < 100) return;
    g_shakeRecent = now;
    /* 【镜头震动】搁置 (对齐 US): direct 实测零调用, 不发送 */
    // vib_collect(VEV_SHAKE_SCREEN, shake_amplify(*(float *)&strength_bits), now, 200);
    dll_log2("SHAKE direct str=%.3f", *(float *)&strength_bits);
}

/* ---------------- 震屏字段轮询 (v1.3: collector 6ms 调用) ----------------
 * [g_shakeCtlObj + 0x790] 上升沿 (0 -> 非0) = 震屏开始
 * v1.4 对齐老版 (ACT4/ACT5): 发 VEV_KILL "释放技能"rank 通道 (老版 sub_4E49E0 同款语义,
 *   该滑块承载"击杀+镜头震动"双源, 老版用户拍板) + 100ms 去重 (老版同款) */
void vib_poll_shake_ctl(void)
{
    DWORD obj = g_shakeCtlObj;
    DWORD v;
    static DWORD s_last = 0;
    DWORD tick;

    if (!obj) return;
    v = try_read_dword(obj + CN_SHAKE_CTL_FLAG_OFF);
    if (v && !g_shakeCtlLast) {
        tick = GetTickCount();
        if (tick - s_last >= 100) {
            s_last = tick;
            vib_collect(VEV_KILL, 100, tick, 1);   /* v1.7: 与词条执行器同走【释放技能】通道 */
            dll_log2("SHAKE ctl.0x790 上升沿 -> VEV_KILL (obj=0x%08X v=0x%X)", obj, v);
        }
    }
    g_shakeCtlLast = v;
}

/* ---------------- 震屏词条执行器分发 (v1.5, stub_6 ← CN 0x03349740) ----------------
 * 领域知识(用户): 技能表里 [shake screen] 词条 (=引擎名 "onShakeInput") 决定
 *   "哪个技能震动、怎么震动"; 引擎执行该词条时进入 0x03349740
 *   (结构: this+0x5E5 启用标志 / this+0x5E0 词条表 -> 查 "onShakeInput" -> 执行处理器)。
 *   **带该词条的技能才走到这里 = 天然的技能级震屏区分** (老版 hook 震屏出口的等价方案)。
 * 通道(v1.7 定案, 用户指正 + 宿主源码核对): **VEV_KILL = 宿主【释放技能】滑块**
 *   (宿主 vibration.rs:60 "VEV_KILL=21 = 释放技能"; 滑块 idx 9 per config.rs:89)。
 *   技能震屏就对应【释放技能】这一个滑块, 没有其他 (用户 2026-09-14)。
 *   v1.6 曾误改 VEV_SHAKE_SCREEN(镜头震动滑块 idx10, 增益默认为 0), 已回退。
 * 参数: 强度 100 满幅, count=1 (与宿主 rank 通道语义一致, 老版同款)。 */
void __cdecl vib_dispatch_shake_entry(void *obj, DWORD arg, int type)
{
    static DWORD s_last = 0;
    DWORD tick = GetTickCount();
    (void)obj; (void)type;
    if (!g_ringReady) return;
    if (tick - s_last < 100) return;   /* 100ms 去重 (老版同款) */
    s_last = tick;
    vib_collect(VEV_KILL, 100, tick, 1);
    dll_log2("SHAKE entry(0x03349740) arg=0x%X -> VEV_KILL(释放技能)", arg);
}

/* ---------------- 怪物死亡 hook 分发 (v2.6, stub_7 ← CN 0x024C4CB5) ----------------
 * 目标 = **CN 0x024C4CB5 = 受击结算函数 0x024C4B10 内「剩余HP<=0」的死亡分支入口**
 *   (0x024C4B10 = US sub_1CD6D30 同源 93.3%; 该分支与 US 的
 *    `sub_10716B0(v23)==9 && SHIDWORD(a2)<=0 && (a2<0 || lo==0)` 条件逐条对应)。
 *   全 .text 只有一条 `jl` 跳入该地址 -> **只在死亡时触发**, 不需要自行判 HP 或复刻类型闸。
 * 被击者由 asm stub_7 从 **esi** 取 (函数中部 ecx 已被破坏)。
 * 通道: VEV_TARGET_DIE=2 -> 宿主【怪物死亡】滑块 idx14。
 * 防误报三道闸 (保守留用; 若日志出现"限速触发"说明判断有误):
 *   ① 排除玩家自身 (obj == n2500)  ② 100ms 去重  ③ 1 秒内最多 10 次限速 + 超限告警日志。
 * 历史: v2.5 曾 hook 0x024F2210 (US 死亡回调同源 97.4%), **装成功但整局零触发** -> 已弃。
 *   教训: 同源度高 ≠ 会被调用, 必须实测调用计数验证 (铁律四)。 */
static volatile DWORD g_lastDieHookTick = 0;

int vib_die_hook_recent(void)
{
    DWORD t = g_lastDieHookTick;
    return (t != 0) && (GetTickCount() - t < 500);
}

void __cdecl vib_dispatch_target_die(void *obj, DWORD hp_lo, DWORD hp_hi, int type)
{
    static DWORD s_last = 0;
    static DWORD s_dbg = 0;
    static LONG  s_seen = 0;
    DWORD tick = GetTickCount();
    DWORD player;
    DWORD victim = (DWORD)(ULONG_PTR)obj;
    (void)type;

    if (!g_ringReady) return;
    if (victim == 0) return;
    if (!mem_readable(victim, 8)) return;
    player = try_read_dword(CN_N2500_PTR);
    if (player && victim == player) return;            /* 玩家自身: 忽略 */

    /* 诊断: 每 500ms 记录一次真实 HP 参数 (用于确认 a2 = 剩余HP 的语义) */
    if (tick - s_dbg >= 500) {
        s_dbg = tick;
        dll_log2("SETTLE victim=0x%08X hp=0x%08X:0x%08X (hi:lo)", victim, hp_hi, hp_lo);
    }

    /* ★死亡判定: 64 位 a2 = 剩余HP <= 0
     * 与 CN 0x024C4B10 内自身的判定同义: `cmp [ebp+0Ch],0 / jg 活着 / jl 死亡 /
     * test edi,edi / jne 活着` -> (hi<0) || (hi==0 && lo==0)。
     * 在入口自己判、而不是 hook 分支, 是为了绕开分支前那道"对象类型==9"的闸
     * (实测: 函数被调用 806 次, 而该分支 0 次 -> 闸挡掉了实战死亡)。 */
    /* ================= 【怪物死亡】已注释停用 (2026-09-15, v6.0) =================
     * 本路径(v2.9: hook 受击结算入口自判 HP<=0)实测无效: 该函数 806 次调用里 ecx 恒为玩家对象,
     * 被入口闸过滤; 且整个"找死亡信号"的探索最终未找到可用信号
     * (详见 `开发文档/怪物死亡信号_调查总结_20260915.md`)。
     * 处置: **不向宿主发送 VEV_TARGET_DIE**。stub_2 亦不注册, 本函数实际不会被调用。
     * ==========================================================================
    if (((LONG)hp_hi < 0) || (hp_hi == 0 && hp_lo == 0)) {
        if (tick - s_last < 100) return;
        s_last = tick;
        g_lastDieHookTick = tick;
        InterlockedIncrement(&s_seen);
        InterlockedIncrement(&g_die_seen);
        vib_collect(VEV_TARGET_DIE, 100, tick, 1);
        dll_log2("TARGETDIE(0x024C4B10 入口 HP<=0) #%ld victim=0x%08X", (long)s_seen, victim);
    }
    */
    (void)hp_hi; (void)hp_lo; (void)s_last; (void)s_seen;
}

/* ---------------- 评分等级轮询 (四轮实证: FN_SCORE_DMG + FN_RANK_LEVEL) ---------------- */
typedef int (__attribute__((thiscall)) *FnGetDmg)(int thisptr);
typedef int (__attribute__((thiscall)) *FnGetRank)(int thisptr, int dmg);

static volatile LONG g_rankLast = -1;

void rank_poll(void)
{
    /* v7.0 对齐 90US: US rank_poll 随 6ms 冲刷每轮调用 (同源游戏函数高频调用在 US
     * 实测数月安全), 去掉 v4.x 的 500ms 节流 -> 评分等级脉冲检测延迟从最高 500ms
     * 降到 ~6ms。事件仍只在等级变化时发送, 无风暴风险。 */
    DWORD now = GetTickCount();
    DWORD obj;
    FnGetDmg fnDmg;
    FnGetRank fnRank;
    int dmg, r;

    obj = try_read_dword(CN_RANK_OBJ_PTR);
    if (!obj) return;
    fnDmg = (FnGetDmg)CN_FN_SCORE_DMG;
    fnRank = (FnGetRank)CN_FN_RANK_LEVEL;
    dmg = fnDmg((int)obj);
    r = fnRank((int)obj, dmg);
    if (r < 0 || r > 8) return;
    if ((LONG)r == g_rankLast) return;
    g_rankLast = (LONG)r;
    vib_collect(VEV_RANKING, (DWORD)r, now, 1);
    dll_log2("RANK level=%d (dmg=%d)", r, dmg);
}

/* ---------------- 释放技能轮询 (玩家+0x5DA0 脉冲, 四轮实证 8 次) ----------------
 * ⚠ 该字段实测是 0/1 标志位 (非累加计数器): 日志里 +1 与 +4294967295 成对交替。
 *   靠"仅上升沿 collect"做到一次技能一个事件 —— 功能正确, 日志中 -1 那行只是噪音。 */
static volatile DWORD g_killLast = 0;

static void poll_kill(DWORD now)
{
    DWORD player = try_read_dword(CN_N2500_PTR);
    DWORD cnt;
    (void)now;   /* v1.9: 改日志态后不再用于 collect (恢复发送时移除) */
    if (!player) { g_killLast = 0; return; }
    cnt = try_read_dword(player + CN_KILL_COUNT_OFF);
    if (cnt >= 0x7FFFFFFFu) return;
    if (cnt != g_killLast) {
        if (cnt > g_killLast) {
            InterlockedIncrement(&g_pulse_rise);
            /* ★v2.3 语义修正 (依据 90US 实施记录 事件采集实施记录合集.md):
             *   US: VEV_KILL=21 "释放技能: strength=次数 (n2500[5747])"; US poll_kill 亦发 VEV_KILL。
             *   CN 的 pl+0x5DA0 与之对应 = **释放技能标志** (放技能时置 1, 结束回落 0),
             *   ★不是击杀计数 (交接文档"击杀计数 0x5DA0"系四轮实证误判, 已订正;
             *     90US 亦在 v12/v14 节自我澄清 n2500[5747] 非击杀)。
             * 故本通道 → VEV_KILL → 宿主【释放技能】滑块 (idx9), 与用户"技能震屏=【释放技能】"一致。
             * 怪物死亡 = 独立信号: 90US 用 hook sub_F20BF0 (HP<=0 死亡后处理), CN 待定位同源。 */
            vib_collect(VEV_KILL, cnt - g_killLast, now, 1);
        } else {
            InterlockedIncrement(&g_pulse_fall);
        }
        dll_log2("SKILLCAST(poll 0x5DA0) +%lu (total=%lu) -> VEV_KILL(释放技能)",
                 (unsigned long)(cnt - g_killLast), (unsigned long)cnt);
        g_killLast = cnt;
    }
}

/* ---------------- 评分点轮询 (rank+0xC0C 累积, 四轮实证) ---------------- */
static volatile DWORD g_scoreLast = 0;

static void poll_scorepoint(DWORD now)
{
    DWORD score = try_read_dword(CN_RANK_OBJ_PTR);
    DWORD fk;
    if (!score) { g_scoreLast = 0; return; }
    fk = try_read_dword(score + CN_SCORE_FINAL_KILL);
    if (fk >= 100000000u) return;
    if (fk != g_scoreLast) {
        if (fk > g_scoreLast && g_scoreLast != 0) {  /* 首帧只记基准 */
            InterlockedIncrement(&g_spt_inc);
            vib_collect(VEV_KILLPOINT, fk - g_scoreLast, now, 1);
        } else if (fk == 0 && g_scoreLast != 0) {
            /* v2.4 判定级探针候选②: 四轮探针记录称"怪死后归零" */
            InterlockedIncrement(&g_spt_reset);
            dll_log2("PROBE 评分点归零 %lu -> 0", (unsigned long)g_scoreLast);
        }
        g_scoreLast = fk;
    }
}

/* ---------------- 破甲/凌空槽评分轮询 (FN_SLOT_SCORE, 四轮实证数值递增) ---------------- */
typedef int (__attribute__((thiscall)) *FnVtSlot)(int thisptr);
typedef double (__attribute__((thiscall)) *FnGetSlot)(int thisptr, float slot, unsigned int idx);

static float slot_bits(int n)
{
    union { int i; float f; } u;
    u.i = n;
    return u.f;
}

static volatile LONG g_armorLast = 0;
static volatile LONG g_aerialLast = 0;

static void poll_armor_aerial(DWORD now)
{
    /* v7.0 对齐 90US: 去掉 v4.x 的 2s 节流 (US 同函数随 6ms 冲刷每轮调用),
     * 破甲/凌空脉冲检测延迟从最高 2s 降到 ~6ms。事件仍只在数值上升时发送。 */
    DWORD player, vt, fnaddr, score;
    FnGetSlot fnSlot;
    int slot;
    LONG armor, aerial;

    player = try_read_dword(CN_N2500_PTR);
    score = try_read_dword(CN_RANK_OBJ_PTR);
    if (!player || !score) return;
    vt = try_read_dword(player);
    if (!vt) return;
    fnaddr = try_read_dword(vt + CN_VT_SLOT_OFF);
    if (!in_text(fnaddr)) return;
    slot = ((FnVtSlot)fnaddr)((int)player);
    if (slot < 0 || slot > 7) return;
    fnSlot = (FnGetSlot)CN_FN_SLOT_SCORE;

    armor = (LONG)(fnSlot((int)score, slot_bits(4), (unsigned)slot)
            + fnSlot((int)score, slot_bits(5), (unsigned)slot));
    aerial = (LONG)(fnSlot((int)score, slot_bits(10), (unsigned)slot)
             + fnSlot((int)score, slot_bits(14), (unsigned)slot));

    if (armor != g_armorLast) {
        LONG prev = g_armorLast;
        g_armorLast = armor;
        if (armor > prev && prev != 0)   /* 首帧只记基准 */
            vib_collect(VEV_ARMOR_BREAK, (DWORD)(armor - prev), now, 1);
    }
    if (aerial != g_aerialLast) {
        LONG prev = g_aerialLast;
        g_aerialLast = aerial;
        if (aerial > prev && prev != 0)
            vib_collect(VEV_AERIAL, (DWORD)(aerial - prev), now, 1);
    }
}

/* ---------------- 移动轮询 (n2500+0xD0 = 玩家 X 坐标, 四轮 NDIFF 实证) ----------------
 * CN 无 US 的 N964 位置全局, 改用玩家对象 X 坐标单轴判定 (Y 未定位);
 * v6.2 对齐 90US (用户 2026-09-15): 强度 = 速度映射 (US poll_move 同款算法),
 *   速度经 XOR 解密表解码 (CN_ENC_TABLE_BASE); CN_MOVE_SPEED_OFF=0x9A8 为 [V] 未证实,
 *   解码失败/非法值自动回退 40 (不劣于 v6.1 固定值), 解码值进 PROBE[5s] 供实机标定 */
#define MOVE_STOP_MS   300
#define MOVE_TELEPORT  500.0f

static volatile float g_moveX = 0.0f;
static volatile DWORD g_movePlayer = 0;
static volatile DWORD g_lastMoveTick = 0;
static volatile int   g_moving = 0;
static volatile int   g_moveInit = 0;
static int   g_moveDec = 0;            /* 最近一次速度解码原始值 (标定用, collector 线程私有) */
static DWORD g_moveStr = 40;           /* 最近一次发送的移动强度 (标定用) */

/* XOR 解密表解码 (US hooks.c vib_decode 同款, 表基址换 CN):
 *   base1 = [CN_ENC_TABLE_BASE]; tbl2 = [base1 + hi*4 + 0x24];
 *   t = [tbl2 + lo*4 + 0x2114]; 明文 = ((t&0xFFFF)<<16 | t&0xFFFF) ^ [adr+offset]
 * 全程 try_read 守卫式只读, 解不出返回 0 (调用方回退)。 */
static int vib_decode(DWORD adr, unsigned int offset)
{
    DWORD enc, hi, lo, base1, tbl2, t, v, enc2;
    enc = try_read_dword(adr);
    hi = enc >> 16;
    lo = enc & 0xFFFF;
    base1 = try_read_dword(CN_ENC_TABLE_BASE);
    if (!base1) return 0;
    tbl2 = try_read_dword(base1 + hi * 4 + 0x24);
    if (!tbl2) return 0;
    t = try_read_dword(tbl2 + lo * 4 + 0x2114);
    v = ((t & 0xFFFF) << 16) | (t & 0xFFFF);
    enc2 = try_read_dword(adr + offset);
    return (int)(v ^ enc2);
}

static void poll_move(DWORD now)
{
    DWORD player = try_read_dword(CN_N2500_PTR);
    DWORD px;
    float x, dx;

    if (!player) { g_moveInit = 0; g_moving = 0; return; }
    px = try_read_dword(player + CN_PL_XPOS_OFF);
    x = *(float *)&px;
    if (!g_moveInit || player != g_movePlayer) {
        /* 首帧或对象切换 (城镇<->副本): 只记基准 */
        g_moveX = x;
        g_movePlayer = player;
        g_moveInit = 1;
        return;
    }
    dx = x - g_moveX;
    g_moveX = x;
    if (dx < 0) dx = -dx;
    if (dx >= MOVE_TELEPORT) return;             /* 瞬移/换图: 只更新基准 */
    if (dx >= 1.0f) {
        DWORD str = 40;                          /* 解码失败回退值 */
        int s = vib_decode(player + CN_MOVE_SPEED_OFF, 4);
        g_moveDec = s;
        if (s > 0) {
            if (s > 2000) s = 2000;
            str = (DWORD)((unsigned)s * 100u / 2000u);
            if (str < 5) str = 5;
            if (str > 100) str = 100;
        }
        g_moveStr = str;
        vib_collect(VEV_MOVE, str, now, 0);
        g_moving = 1;
        g_lastMoveTick = now;
    } else if (g_moving && now - g_lastMoveTick > MOVE_STOP_MS) {
        vib_collect(VEV_MOVE, 0, now, 0);
        g_moving = 0;
    }
}

/* ---------------- v2.4 探针汇总 (采集线程调用, 5s 节流) ----------------
 * 输出两行: 计数汇总 + a6 直方图。用户做"轮A/轮B"对照实验后,
 * 看这两个数即可判定 pl+0x5DA0 与 rank+0xC0C 各自的真实语义。 */
static void vib_probe_report(void)
{
    DWORD now = GetTickCount();
    DWORD player, v0 = 0, v4 = 0, v8 = 0, vc = 0;
    char hb[256];
    int i, p = 0, shown = 0;

    if (now - g_probe_last < 5000) return;
    g_probe_last = now;

    player = try_read_dword(CN_N2500_PTR);
    if (player) {
        v0 = try_read_dword(player + CN_KILL_COUNT_OFF);
        v4 = try_read_dword(player + CN_KILL_COUNT_OFF + 4);
        v8 = try_read_dword(player + CN_KILL_COUNT_OFF + 8);
        vc = try_read_dword(player + CN_KILL_COUNT_OFF + 12);
    }

    hb[0] = '\0';
    for (i = 0; i < 256 && shown < 8; i++) {
        LONG c = g_a6hist[i];
        if (!c) continue;
        if (p < (int)sizeof(hb) - 24)
            p += snprintf(hb + p, sizeof(hb) - (size_t)p, "%s0x%02X=%ld",
                          shown ? " " : "", i, (long)c);
        shown++;
    }
    if (g_a6hi && p < (int)sizeof(hb) - 24)
        snprintf(hb + p, sizeof(hb) - (size_t)p, "%s>FF=%ld", p ? " " : "", (long)g_a6hi);

    dll_log2("PROBE[5s] FONT总=%ld | 0x5DA0 升=%ld 降=%ld | 评分点 升=%ld 归零=%ld | "
             "pl+5DA0=%lu 5DA4=%lu 5DA8=%lu 5DAC=%lu",
             (long)g_tot_font, (long)g_pulse_rise, (long)g_pulse_fall,
             (long)g_spt_inc, (long)g_spt_reset,
             (unsigned long)v0, (unsigned long)v4, (unsigned long)v8, (unsigned long)vc);
    dll_log2("PROBE[5s] FONT a6 直方图(累计): %s", hb[0] ? hb : "(空)");
    /* v6.2 MOVE 速度解码标定: dec 恒 0 或 str 恒 40 = CN_MOVE_SPEED_OFF(0x9A8) 未证实,
     * 需按 US 0x9A8 簇重扫; str 随跑动/走动变化 = 标定通过 */
    dll_log2("PROBE[5s] MOVE dec=%d str=%lu moving=%d",
             g_moveDec, (unsigned long)g_moveStr, g_moving);
    /* 【怪物死亡】已注释停用 (v6.0) —— 不再打印其计数, 避免日志提示一个已停用的功能 */
    nfy_dump();
    scan_dump();
    /* v2.8: 死亡候选探针命中数 (真正的"每次死亡一次"的函数应≈击杀数) */
    dll_log2("PROBE[5s] 候选钩子命中: [2]死亡管理器=%ld [3]击杀类型=%ld "
             "[4]vtable槽40=%ld [5]击杀包装=%ld [6]受击结算(对照)=%ld | [7]分发器记录=%ld",
             (long)g_probe_hits[2], (long)g_probe_hits[3], (long)g_probe_hits[4],
             (long)g_probe_hits[5], (long)g_probe_hits[6], (long)g_diag_n);
}

void vib_flush_rank_extra(void)
{
    DWORD now = GetTickCount();
    poll_kill(now);
    poll_scorepoint(now);
    poll_armor_aerial(now);
    poll_move(now);
    poll_scan();
    vib_probe_report();
}
