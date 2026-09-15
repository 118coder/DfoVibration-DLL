/* ============================================================
 * DFO 老版本震动插件 - x86 inline hook 引擎（老版本锚点版）
 * 基于 1.5 版 hooks.c 引擎骨架（insn_len 变长 hook + trampoline 页）
 * 目标替换为老版本（LMA1S-release）锚点：
 *   [0] sub_433890 0x00433890  — 统一事件发送器（演出状态咽喉）
 *   [1] sub_4E6DA0 0x004E6DA0  — 播报层（a2: 2=连击 3=被击 5=击杀）
 *   [2] sub_4D2C50 0x004D2C50  — 加分汇聚（48 槽）
 *   [3] sub_4E8520 0x004E8520  — 技巧语音层（HK 槽）
 *   [4] sub_4E3B40 0x004E3B40  — 结算震动实体（3000~5600ms 衰减）
 *   [5] sub_4BCA70 0x004BCA70  — 地城阶段结算流程（R_STAGE_FINALE 发送者）
 * ============================================================ */
#include <windows.h>
#include <string.h>
#include "vib_protocol_old.h"
#include "common/vib_protocol.h"

/* 汇编 stub 符号 (C 侧无下划线, 汇编侧带) */
extern void vib_stub_0(void);
extern void vib_stub_1(void);
extern void vib_stub_2(void);
extern void vib_stub_3(void);
extern void vib_stub_4(void);
extern void vib_stub_5(void);
extern void vib_stub_6(void);
extern void vib_stub_7(void);
extern void vib_stub_8(void);
extern void vib_stub_9(void);
extern void vib_stub_10(void); /* ★v13.10 DOT 状态 tick 调度器 (sub_4D10F0) */
extern void vib_stub_11(void); /* ★v13.12 事件主结算 (sub_78EE00 暴击系) */
extern void vib_stub_12(void); /* ★v13.21 镜头震动汇聚点 (sub_4E49E0) */
extern void vib_stub_13(void); /* ★ACT5 伤害数字咽喉 (sub_4ACC00, cdecl 6 参布局) */

void *vib_tramp_0, *vib_tramp_1, *vib_tramp_2, *vib_tramp_3,
     *vib_tramp_4, *vib_tramp_5, *vib_tramp_6, *vib_tramp_7,
     *vib_tramp_8, *vib_tramp_9, *vib_tramp_10, *vib_tramp_11, /* ★v13.12 */
     *vib_tramp_12; /* ★v13.21 镜头震动 */

/* 采集输出回调(由 dfovib_old.c 提供) */
extern void vib_collect(int type, DWORD strength, DWORD tick, DWORD count);
extern volatile int g_ringReady;
extern void dll_log2(const char *fmt, ...);

/* 玩家掉血最近时刻(受击两源 poll_player_hit_old / poll_hit_counter_old 共写,
 * 暴击探针 SELF_ONLY 同窗关联只读 —— 零耦合; 默认 SELF_ONLY=0 时读取编译剔除) */
static volatile DWORD g_lastPlayerHitTick = 0;

/* ★v13.23 真实掉血佐证时刻(报告37 飘字区分): poll_player_hit_old 的 HP 下降沿
 * 只在 200ms 内有以下佐证之一时才发受击事件 —— 血之狂暴等主动 buff 的静默
 * 代价掉血(实测无红字飘字/被击计数不增/DOT 调度器不tick, 一场最多 1000+ 滴,
 * v13.20 复测会话 1027 滴全被误发 0x02 = "一直持续震动"根因)只记 [hp-drain]。
 * ★v13.24 实测修订: 200ms 关联窗在战斗中会被环境佐证(挨打/飘字)持续放行,
 * 血之狂暴滴血搭便车 → 改用累加器回绕指纹(机械级精准, 见 poll_player_hit_old)
 * + 受击佐证收紧(飘字标记只认 a5&0x02 且非 DOT(0x20)/回复(0x04) 的真受击字)。 */
static volatile DWORD g_lastPlayerFontHitTick = 0; /* stub8 真受击飘字(游戏线程时刻, 排除 DOT/回复字) */
static volatile DWORD g_lastFrenzyDrainTick = 0;   /* ★v13.24 血之狂暴累加器回绕时刻(扣血机械指纹) */

/* ---------------- 老版本事件采集 ----------------
 * 【核心】sub_433890 是统一事件发送器: cdecl(const char *name, a2, a3, a4)
 * 所有演出事件 (VICTORY_UP_LOOP / DEFEAT_DOWN_LOOP / R_STAGE_FINALE /
 * RESULT_SCORE_UP_LOOP / RESULT_COME / RANK_UP ...) 都经它发送。
 * hook 后我们从栈取 a1=name 指针, 与已知状态名比较, 命中即推事件。
 * 此函数可能被高频调用(715 调用点)，分派必须极短：先比较字符串头。
 */

/* 状态名常量地址（由版本剖面提供：ACT1=ASCII, ACT5=UTF-16 宽串） */
#define STR_VICTORY_UP_LOOP     TGT_STR_VICTORY_UP_LOOP
#define STR_DEFEAT_DOWN_LOOP    TGT_STR_DEFEAT_DOWN_LOOP
#define STR_R_STAGE_FINALE      TGT_STR_R_STAGE_FINALE
#define STR_RESULT_SCORE_UP_LOOP TGT_STR_RESULT_SCORE_UP_LOOP
#define STR_RESULT_COME         TGT_STR_RESULT_COME
#define STR_RANK_UP             TGT_STR_RANK_UP
#define STR_R_ALL_KILL          TGT_STR_R_ALL_KILL
#define STR_R_FAINT             TGT_STR_R_FAINT
#define STR_R_HK2_3GOOD         TGT_STR_R_HK2_3GOOD
#define STR_R_HK2_2GOOD         TGT_STR_R_HK2_2GOOD
#define STR_R_HK2_1GOOD         TGT_STR_R_HK2_1GOOD

/* 与运行期状态名比较（不依赖字符串类型，比较“字符序列”本身）：
 *   ACT1 = ASCII 窄串；ACT5 = UTF-16LE 宽串（VIB_STR_WIDE=1）。
 * 未定位的状态名(VIB_ANCHOR_UNRESOLVED=0) 直接返回 0（该分支在 ACT5 死亡）。
 * 地址可能因加载基址偏移, 用 VirtualQuery 安全预检。 */
static int safe_str_eq(DWORD addr, const char *s)
{
    int i;
    MEMORY_BASIC_INFORMATION mbi;
    if (!addr) return 0;
    if (VirtualQuery((LPCVOID)addr, &mbi, sizeof(mbi)) == 0) return 0;
    if (mbi.State != MEM_COMMIT || !(mbi.Protect & (PAGE_READONLY | PAGE_READWRITE | PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE)))
        return 0;
#if VIB_STR_WIDE
    {
        const wchar_t *p = (const wchar_t *)addr;
        for (i = 0; s[i]; i++) {
            if (p[i] != (wchar_t)(unsigned char)s[i]) return 0;
        }
        return p[i] == 0;
    }
#else
    {
        const char *p = (const char *)addr;
        for (i = 0; s[i]; i++) {
            if (p[i] != s[i]) return 0;
        }
        return p[i] == 0;
    }
#endif
}

#if VIB_TARGET == VIB_TARGET_ACT4
/* ★ACT4 命中分组计数(常驻诊断): 按 (同帧,同目标) 统计 —— 实测（2026-09-13 三种场景）
 * 证明 0x01 本身即"每目标每次命中一条"(同帧多条 = AoE 打中不同怪, 无同目标重复)。
 *   hev = 0x01 原始条数 ; hgrp = (同帧,同目标) 去重后的"命中"数。
 *   hev == hgrp  → 无重复(正常)；若 hev > hgrp → 出现同帧同目标多条(如后期附加伤害),
 *   此时可把 VIB_HIT_MERGE_MS 命中窗换成 (tick,obj) 去重。
 * VIB_HIT_PROBE=1 时额外逐条打印 [h] 行(默认 0, 定案后关)。 */
static DWORD s_hEv = 0, s_hGrp = 0, s_hCurTick = 0;
static DWORD s_hObj[32];
static int   s_hNObj = 0;
static void act4_hit_probe(DWORD obj, DWORD a5, DWORD ret, DWORD tick)
{
    int i, dup = 0;
    s_hEv++;
    if (tick != s_hCurTick) { s_hCurTick = tick; s_hNObj = 0; }
    for (i = 0; i < s_hNObj; i++) if (s_hObj[i] == obj) { dup = 1; break; }
    if (!dup) {
        if (s_hNObj < 32) s_hObj[s_hNObj++] = obj;
        s_hGrp++;
    }
#if VIB_HIT_PROBE
    dll_log2("[h] t=%u obj=%08X a5=%02X ret=%08X%s", tick, obj, a5, ret, dup ? " DUP" : "");
#else
    (void)a5; (void)ret;
#endif
}
DWORD vib_hitprobe_take_ev(void)  { DWORD v = s_hEv;  s_hEv = 0;  return v; }
DWORD vib_hitprobe_take_grp(void) { DWORD v = s_hGrp; s_hGrp = 0; return v; }
#else
DWORD vib_hitprobe_take_ev(void)  { return 0; }
DWORD vib_hitprobe_take_grp(void) { return 0; }
#endif

/* sub_433890 分发: obj 无用(名字即事件), a2/a3 供未来扩展 */
void __cdecl vib_dispatch_event_sender(void *obj, DWORD nameAddr, DWORD a3, int type)
{
    DWORD tick = GetTickCount();
    (void)obj; (void)type;
    /* 命中检查(按重要级)。
 * ★v10 评分恢复：用户实测"评分系统似乎消失"——第六轮回退把结算评分
 *   (VICTORY/RESULT_SCORE_UP_LOOP/RANK_UP)全改 VEVO_(26+) 宿主丢失。
 *   恢复为宿主认的低频结算通道(结算类 is_settlement 放行、战斗期不触发 → 不狂震):
 *   通关评级 → VEV_RANKING(等级0=满档); 评分滚动 → VEV_RANKING(等级随进度);
 *   评级上升 → VEV_RANKING(满档); 失败/倒下 → VEV_CRIT_SHAKE(特写震屏, 低频)。
 *   RANKING 满震教训仍在: 这些只在结算演出触发, 战斗期 stub1 case6/7 STYLE/TECHNIC
 *   仍走 RATING(丢弃), 不引入狂震源。BOSS死/清场 → VEV_FINAL_KILL(宿主通道17, 低频)。 */
    if (safe_str_eq(nameAddr, "R_STAGE_FINALE")) {
        vib_collect(VEV_FINAL_KILL, 80, tick, a3);   /* 翻牌/结算外壳 → 结算震 */
    } else if (safe_str_eq(nameAddr, "R_ALL_KILL")) {
        /* 通知127(全灭/BOSS清房) → BOSS 死亡强震(宿主通道17) */
        vib_collect(VEV_FINAL_KILL, 100, tick, a3);
    } else if (safe_str_eq(nameAddr, "VICTORY_UP_LOOP")) {
        vib_collect(VEV_RANKING, 0, tick, 0);         /* 通关 → 评级满档 */
    } else if (safe_str_eq(nameAddr, "DEFEAT_DOWN_LOOP")) {
        vib_collect(VEV_CRIT_SHAKE, 80, tick, 800);   /* 失败 → 特写震屏 */
    } else if (safe_str_eq(nameAddr, "R_FAINT")) {
        vib_collect(VEV_CRIT_SHAKE, 100, tick, 600);  /* 倒下/死亡演出 */
    } else if (safe_str_eq(nameAddr, "RESULT_SCORE_UP_LOOP")) {
        vib_collect(VEV_RANKING, a3 > 8 ? 8 : a3, tick, 0); /* 评分滚动 → 评级档 */
    } else if (safe_str_eq(nameAddr, "RESULT_COME")) {
        vib_collect(VEV_CRIT_SHAKE, 40, tick, 300);   /* 结果界面出现 → 轻震 */
    } else if (safe_str_eq(nameAddr, "RANK_UP")) {
        vib_collect(VEV_RANKING, 0, tick, 0);         /* 评级上升 → 满档 */
    } else if (safe_str_eq(nameAddr, "R_HK2_3GOOD")) {
        /* ★v12: 战斗内连击评级(子代理定案): R_HK2_3GOOD@0x9A1738 …按档映射 RANKING */
        vib_collect(VEV_RANKING, 2, tick, 0);         /* 3GOOD → 档2(低) */
    } else if (safe_str_eq(nameAddr, "R_HK2_2GOOD")) {
        vib_collect(VEV_RANKING, 5, tick, 0);         /* 2GOOD → 档5(中) */
    } else if (safe_str_eq(nameAddr, "R_HK2_1GOOD")) {
        vib_collect(VEV_RANKING, 8, tick, 0);         /* 1GOOD → 档8(满) */
    }
#if VIB_TARGET == VIB_TARGET_ACT5
    /* ★ACT5 评分/连击播报（2026-09-12）：ACT5 没有 ACT1 的 {2,3,5,6,7} 播报层 switch，
     * 改按统一事件发送器收到的状态串匹配。枚举 762 个调用点的状态串得到
     *   HK1_COUNT_1/2/3 (0xE96C08/20/38) = 战斗内连击/评分播报档位；HK1_FIGHT=开打。
     * 用户口径“评分走『评分点』(语音播报触发)” → 发 VEV_KILLPOINT(12) = 「评分点」滑块。 */
    else if (safe_str_eq(nameAddr, "HK1_COUNT_1")) {
        dll_log2("[sd] HK1_COUNT_1 -> KILLPOINT"); vib_collect(VEV_KILLPOINT, 100, tick, 1);
    } else if (safe_str_eq(nameAddr, "HK1_COUNT_2")) {
        dll_log2("[sd] HK1_COUNT_2 -> KILLPOINT"); vib_collect(VEV_KILLPOINT, 100, tick, 1);
    } else if (safe_str_eq(nameAddr, "HK1_COUNT_3")) {
        dll_log2("[sd] HK1_COUNT_3 -> KILLPOINT"); vib_collect(VEV_KILLPOINT, 100, tick, 1);
    }
#elif VIB_TARGET == VIB_TARGET_ACT4
    /* ★ACT4 评分/连击播报（2026-09-12）：枚举统一事件发送器 0x43D9B0 的 792 个调用点实参串，
     * ACT4 同样有 HK1_COUNT_1/2/3（与 ACT5 同名同义 = 战斗内连击/评分播报档位，均推给本发送器）；
     * ACT4 的 STYLE/TECHNIC（0xB12833/0xB1283F）**无任何代码/指针表引用**（已成死串）→
     * 走 HK1 档位，发 VEV_KILLPOINT(12) = 宿主「评分点」滑块。证据：work/out/act4_sender_strings.txt */
    else if (safe_str_eq(nameAddr, "HK1_COUNT_1")) {
        dll_log2("[sd] HK1_COUNT_1 -> KILLPOINT"); vib_collect(VEV_KILLPOINT, 100, tick, 1);
    } else if (safe_str_eq(nameAddr, "HK1_COUNT_2")) {
        dll_log2("[sd] HK1_COUNT_2 -> KILLPOINT"); vib_collect(VEV_KILLPOINT, 100, tick, 1);
    } else if (safe_str_eq(nameAddr, "HK1_COUNT_3")) {
        dll_log2("[sd] HK1_COUNT_3 -> KILLPOINT"); vib_collect(VEV_KILLPOINT, 100, tick, 1);
    }
#endif
}

/* sub_4E6DA0 播报层: a2 类型(2=连击 3=被击 5=击杀 6=STYLE 7=TECHNIC)
 * 第三轮定案(报告 18): a2==3 的唯一触发点 = sub_483F50@0x4847BE,
 *   `sub_4E6DA0(*(obj+0xE4)+0xF4, 3, v68)` —— a3 就是 v68=伤害值!
 * 所以被击强度直接用 a3(伤害) 映射, 不再用固定 55。 */
void __cdecl vib_dispatch_broadcast(void *obj, DWORD a2, DWORD a3, int type)
{
    DWORD tick = GetTickCount();
    DWORD dmg;
    (void)obj; (void)type;
#if VIB_TARGET == VIB_TARGET_ACT5
    /* ★ACT5 播报层 = sub_555D60(this,a2,a3)：a2==7→"HK_TECHNIC"、a2==6→"HK_STYLE"
     * （与 ACT1 的 6=STYLE/7=TECHNIC 同码，已由字符串引用点静态证实）→「评分点」。
     * 其余 a2 语义与 ACT1 不同（a2=3 是评分累加 this[200]+=a3），不映射以免误报。 */
    (void)dmg;
    if (a2 == 6 || a2 == 7) {
        dll_log2("[sd] broadcast a2=%u (STYLE/TECHNIC) -> KILLPOINT", a2);
        vib_collect(VEV_KILLPOINT, 100, tick, 1);
    }
    return;
#elif VIB_TARGET == VIB_TARGET_ACT4
    /* ★ACT4 播报层 = sub_51FA10(this=ecx, a2=[ebp+8], a3=[ebp+0xc])，ret 8；
     * 入口 `55 8B EC 83 EC 0C 8B 45` 与 ACT1 sub_4E6DA0 逐字节同构（同一编译器/同一代源码）。
     * a2 分支实测（反汇编）：`cmp eax,7 → "HK_TECHNIC"`、`cmp eax,6 → "HK_STYLE"`（与 ACT1/ACT5 同码）；
     * 注意：STYLE/TECHNIC 是经**玩家对象虚表 +0x10C** `player->vt[0x10C](name,-1,0,0)` 播出的，
     * 不经统一发送器 0x43D9B0（[snd] 探针实测战斗期发送器里没有评级串）→ 必须 hook 本层。
     * 发 VEV_KILLPOINT(12) = 宿主「评分点」滑块。证据：work/out/act4_broadcast2.txt */
    (void)dmg;
    if (a2 == 6 || a2 == 7) {
        dll_log2("[sd] broadcast a2=%u (%s) -> KILLPOINT", a2, a2 == 6 ? "HK_STYLE" : "HK_TECHNIC");
        vib_collect(VEV_KILLPOINT, 100, tick, 1);
    }
    return;
#else
    switch (a2) {
    case 2: vib_collect(VEV_COMBO, 40, tick, 0); break;
    case 3: /* 被击: 回退第四轮 VEV_DAMAGE(1)(宿主无分支→丢弃不震, 防狂) */
        dmg = a3;
        if (dmg > 100) dmg = 100;
        vib_collect(VEV_DAMAGE, dmg > 20 ? dmg : 30, tick, dmg);
        break;
    case 5: vib_collect(VEV_KILL, 100, tick, 0); break;  /* 击杀 */
    /* ★v13.13 用户指令: 评分走「评分点」通道(出现操作/语音播报时), 不走评分等级强度。
     * 老 ACT 评分 = STYLE/TECHNIC 语音播报 → VEV_KILLPOINT(12) = 宿主「评分点」滑块
     * (rank_gain[0], GUI「评分特效震动」卡片第一项)。不走 VEV_RANKING(评分等级
     * 强度 F~SSS 满幅, 用户明确说别走这个)。 */
    case 6: vib_collect(VEV_KILLPOINT, 100, tick, 0); break; /* STYLE → 评分点 */
    case 7: vib_collect(VEV_KILLPOINT, 100, tick, 0); break; /* TECHNIC → 评分点 */
    default: break;
    }
#endif
}

/* sub_4D2C50 加分汇聚: 每次加分调用 → 命中轻震
 * ★第五轮降噪: 与 stub6 命中(sub_4748D0)通道重复, 改为"显著加分才补震":
 *   a3 < 3 (微弱加分, 每刀都有) 不发, 只在大加分(暴击/高伤/评分点)时补一次;
 * ★v13fix(清脆手感): count 固定 1 —— 宿主 combo_hits += count 是连击放大与
 *   burst(窗口内 ≥6 连 → 800ms 持续震)的燃料, 把伤害值/加分值当 count 会让
 *   每刀 combo 直接 +60~100 → 一刀即触发 burst/连击放大 → "砍什么都持续
 *   震一下"(用户实测不爽)。定案(阶段报告24): "stub6 命中 count=1 →
 *   短促脉冲不再持续嗡鸣"; v10 又改回伤害值作 count 是手感拖沓的根因,
 *   此处统一拉回 count=1。强度由宿主 font_attack 槽表达, 与 count 无关。 */
void __cdecl vib_dispatch_accumulate(void *obj, DWORD a2, DWORD a3, int type)
{
    /* ★v13.31 停发命中(空转, hook 保留) —— 对齐新版架构"命中=纯飘字单源":
     * 本锚点是评分点累加器, 旧版在 a3>=3(大加分)时补发 0x01 —— 它与
     * "命中必然伴随飘字"不等价(大加分按评分逻辑触发, 与每段伤害数字
     * 不同步), 是用户实测鬼剑士多段普攻"只有第一击和最后一击震"的
     * 残留源之一(v13.30 已把 stub6 一并停发, 见下)。评分震动由 stub1
     * (sub_4E6DA0 a2=6/7 → VEV_KILLPOINT「评分点」滑块)独立承担,
     * 不受本空转影响。 */
    (void)obj; (void)a2; (void)a3; (void)type;
}

/* sub_4E8520 语音层: HK 技巧语音槽标志 → 技巧事件 */
/* sub_4E8520 语音层: 宿主无 RATING 分支 → 事件丢弃不震(回退第四轮, 不狂) */
void __cdecl vib_dispatch_voice(void *obj, DWORD a2, DWORD a3, int type)
{
    (void)obj; (void)a2; (void)a3; (void)type;
    vib_collect(VEV_RATING, 50, GetTickCount(), 0);
}

/* sub_4E3B40 结算震动实体: 回退第四轮 VEV_RATING(a2)(宿主无分支→丢弃不震) */
void __cdecl vib_dispatch_result_shake(void *obj, DWORD a2, DWORD a3, int type)
{
    (void)obj; (void)a3; (void)type;
    vib_collect(VEV_RATING, a2 > 100 ? 100 : a2, GetTickCount(), 0);
}

/* sub_4BCA70 地城阶段结算: 回退第四轮 VEVO_STAGE_FINALE(26)(宿主无分支→丢弃) */
void __cdecl vib_dispatch_finale(void *obj, DWORD a2, DWORD a3, int type)
{
    (void)obj; (void)a2; (void)a3; (void)type;
    vib_collect(VEVO_STAGE_FINALE, 100, GetTickCount(), 0);
}

/* sub_4748D0 攻击命中/伤害应用 (stub6, __userpurge: a1@ecx=被击目标,
 * a4@[esp+4]普通伤害标志==1, a5@[esp+8]伤害信息块指针)
 * ★v13.31 起本 hook 空转不采集(命中=stub8 飘字单源, 对齐新版架构)——
 *   以下为历史判定逻辑存档:
 * ★第七轮定案(子代理B 轮4/5/7 铁证): **0x3E4 = 攻击者归属槽** ——
 *   sub_4A9990 在"命中且伤害>0"时把攻击者的归属写入被击目标+0x3E4。
 *   正确判定:
 *     attackerOwner==player && target!=player  → 玩家攻击命中怪物 → 震(FONT+0x01)
 *     target==player                         → 玩家被击(交给 stub1 a2==3 通道)
 *   [旧版写反: 把"玩家在打怪"误判为"玩家被打"直接 return → 命中永不震 =
 *    用户实测"命中无震动"的根本原因]
 * 伤害值: a5 指向伤害信息块, 块内 +4 处第二个指针 → 其值 = 伤害量(0..100 封顶) */
void __cdecl vib_dispatch_attack_hit(void *obj, DWORD a4, DWORD a5, int type)
{
    /* ★v13.31 停发命中(空转, hook 保留) —— 对齐新版架构"命中=纯飘字单源":
     * 本锚点是扣血结算层, 与 stub8(飘字咽喉)对同一段伤害双发(靠去抖合并),
     * 且覆盖不到"命中必然伴随飘字"之外的边缘场景归属(暴击底震/多段同步)。
     * v13.30 起命中去抖降 40ms 后, stub6+stub8 的双源到达间隔若跨过 40ms
     * 窗仍会造成一次多余震动; 用户拍板(2026-09-07)命中严格按飘字逻辑 ——
     * 命中反馈唯一源 = stub8 每段伤害数字逐条直发, 本函数退出采集。
     * 暴击刀仍走 stub8 的 0x10 特殊通道(与新版优先级链一致)。
     * ★v13.32 探针: [s6] 逐条记录结算到达(a4==1), 与 stub8 的 [f] 对比
     * 两层到达时差与覆盖面(近战多段节奏定案用, 数据采集后可移除);
     * a4!=1(状态记录调用)只做 2s 计数。 */
    if (a4 == 1) {
        dll_log2("[s6] a4=1 t=%u", GetTickCount());
    } else {
        static DWORD s_ncnt = 0, s_ntick = 0;
        DWORD t = GetTickCount();
        s_ncnt++;
        if (t - s_ntick >= 2000) {
            if (s_ncnt) dll_log2("[s6] a4!=1 x%u (2s)", s_ncnt);
            s_ncnt = 0; s_ntick = t;
        }
    }
}

/* sub_42DCB0 战斗 UI 加载器 (stub7): 注册 Combo.img 等战斗界面图集
 * 0x42DF16 push 0x999F3C = Combo.img 注册; sub_42DCB0 进入 = 战斗界面激活
 * → 宿主 VEV_COMBO(9) 连击横幅通道 (combo_hits += 10) — 第四轮新增 */
/* sub_42DCB0 (stub7): 回退第四轮无此锚点 → 空转不发(防止进战斗 UI 高频触发 COMBO) */
void __cdecl vib_dispatch_combo_ui(void *obj, DWORD a2, DWORD a3, int type)
{
    (void)obj; (void)a2; (void)a3; (void)type;
    /* 不发事件(空转) */
}

/* ★第六轮定案(三路子代理 A/B/C 融合):
 * sub_470ED0 = 伤害数字生成咽喉 —— 所有伤害来源(9 个调用者: sub_4748D0/sub_4CF1E0/
 *   sub_4CF6B0/sub_4D0960/sub_51B0F0/sub_528D20/sub_58C8C0/sub_78EE00 等)必经此函数
 *   生成伤害文字(数字上屏)。这就是"玩家命中怪物 → 伤害文字出现"的确定性锚点。
 * __cdecl(a1,a2,a3,a4,a5): a4=伤害值(普通伤害), a5=类型标志位图(与 1.5 FONT a6 同构):
 *   0x02=受击(玩家被打的数字), 其余(0x01 攻击 / HP 等)=非受击 → 攻击命中怪物。
 * 判定(子代理A最终版): 攻击命中 = (a5 & 0x01) && !(a5 & 0x02);
 *      a5&0x02 受击数字 → 跳过; a5 无 0x01(回血0x04/HP0x20) → 跳过。
 *      → 玩家攻击命中怪物 → VEV_FONT+0x01(宿主攻击通道, 与 stub2 共用 120ms 去抖槽)。
 * ★v13fix(清脆手感): count 固定 1(不再用伤害值作连击数) —— 宿主 combo_hits
 *   += count 是连击放大/burst(≥6 连 800ms 持续震)的燃料; 伤害值 a4(1..100)
 *   作 count 会让一刀 combo 直接 +几十 → 立即爆 burst → "砍什么都持续震一下"。
 *   强度由宿主 font_attack 槽表达, 与 count 无关。
 *   ② 返回地址 0x0058C958(=sub_58C8C0 的调用点 0x58C953 call 的下一条) = 可破坏物/建筑
 *       → 跳过命中通道(建筑破坏不走战斗命中, 用户实测"建筑像受伤效果" → 区分开)。 */
void __cdecl vib_dispatch_damage_font(void *obj, DWORD a2, DWORD a4, DWORD a5, DWORD ret, int type)
{
    DWORD tick = GetTickCount();
    DWORD player;
    (void)a2; (void)a4; (void)type;
    (void)ret; /* ★v13.33 [f] 探针带 ret(来源调用点), 采集完可移除 */
    player = VIB_ANCHOR_IS_SET(OLD_PLAYER_OBJ) ? *(DWORD *)OLD_PLAYER_OBJ : 0;
    (void)player; /* ★v13.14: a5 已编码玩家状态, 无需对象归属读取 */
    /* ★v13.32 诊断探针(sa_melee_hit_p1 静态定案的动态验证, 数据采集后可移除):
     * 静态定案 sub_4748D0 的 a5 ∈ {1=玩家打怪掉血, 0=非玩家攻击掉血, 4=回血},
     * 0x02 仅出自 DOT 三兄弟(0x23), 0x04 出自 sub_528D20, 0x10 出自 0x790D93。
     * ★v13.33 加 ret: 格斗家实测 177 条 a5=01 无法静态归因(9 静态调用点均
     * 对不上 [s6] 只 6 次结算), 用返回地址动态定案真实来源。
     * 本探针逐条记录带标志位飘字(近战多段节奏/a5 谱系定案用);
     * a5==0(怪打玩家/怪打怪/可破坏物)只做 2s 计数防刷屏。 */
    if (a5 & (FONT_FLAG_PLAYER_ATTACK | FONT_FLAG_PLAYER_HIT | FONT_FLAG_SPECIAL |
              FONT_FLAG_EFFECT | FONT_FLAG_HP)) {
        dll_log2("[f] a5=%02X ret=%08X t=%u", a5, ret, tick);
#if VIB_TARGET == VIB_TARGET_ACT4
        if (a5 & FONT_FLAG_PLAYER_ATTACK) act4_hit_probe((DWORD)obj, a5, ret, tick);
#endif
    } else {
        static DWORD s_zcnt = 0, s_ztick = 0;
        s_zcnt++;
        if (tick - s_ztick >= 2000) {
            if (s_zcnt) dll_log2("[f0] a5=00 ret=%08X x%u (2s)", ret, s_zcnt);
            s_zcnt = 0; s_ztick = tick;
        }
    }
    /* ★v13.9fix 玩家受击/HP 飘字判定修复(sa_font_src 实证):
     * 受击对象 a1 是"角色表现对象"包装类, 判别是否"玩家"必须看归属:
     *   *(a1 + 0x3E4) == B75A14  → 这是玩家的飘字(归属是玩家)。
     * 旧代码用 a1==B75A14 直比永不成立 → 玩家受伤/中毒掉血从不震;
     * 命中场景走 a5&0x01 分支不依赖此处, 所以"命中能震、受伤/DOT 不能震"
     * 的现象完全吻合。改用归属判定(带 IsBadReadPtr 保护)。 */
    /* ★v13.14 sub_470ED0 只负责 0x01(攻击命中)/0x02(受击) 两类:
     *  - a1-a3 是飘字坐标, 非受害对象 → 旧的 obj+0x3E4 归属判定读的是坐标垃圾,
     *    必须删除(受击/特殊/DOT 全因此不振 或 双震)。
     *  - a5 标志已由调用者编码: 0x02=玩家被打, 0x01=玩家攻击命中怪物。
     *  - DOT(0x20) 由 sub_4D10F0 独立接管; 特殊(0x10) v13.13 曾由 sub_78EE00
     *    接管 → v13.16 移除 0x790D93 patch 后通道悬空 → ★v13.18 恢复到本函数
     *    (sa_stub8_crit_restore_result 定案: a5&0x10 全库仅 0x790D93 一处组装,
     *    其余 8 调用点全为无 0x10 常量, 误报结构性为零, 置信~90%)。 */
    /* ★v13.28 玩家 DOT 红字(0x23)→0x20 通道(跳字驱动, 用户拍板保留);
     * 其余 0x20 位字体(攻击字)恢复老版本路径 ——
     * v13.25 的一刀切拦截(if a5&0x20 return)误吞了带 0x20 位的命中字体,
     * 是"命中反馈丢失"的根因(v13.25 部署后命中抱怨立即出现, 时间线吻合)。
     * ★v13.29 怪物侧 DOT 红字(出血/中毒, a5=0x21: 0x20∧无0x02)不再混入
     * 0x01 命中通道 —— 用户实测: 十字斩/血之狂暴使怪物出血后, 每条出血跳字
     * 都按命中强度震、还持续喂宿主连击计数(is_attack→combo_hits), 震感太大。
     * 改发宿主 a6=0x04「装备特效」滑块(pidx15, 唯一空闲 FONT 通道): 独立
     * 强度可调、拉零关闭、不进 is_attack 连击。真命中(0x01 无 0x20)不变。
     * (DOT 扣血结算走 4CF1E0 族自身路径不经 4748D0 → stub6 不会发出血震,
     *  本分支即怪物 DOT 的唯一事件出口; sa_v15_dot/sa_dot_pass2 定案。) */
    if ((a5 & FONT_FLAG_HP) && (a5 & FONT_FLAG_PLAYER_HIT)) {
        vib_collect(VEV_FONT, FONT_FLAG_HP, tick, 1); /* 宿主「持续伤害 DOT 反馈 0x20」 */
        return;
    }
    if (a5 & FONT_FLAG_HP) { /* 剩余 0x20 字 = 怪物侧 DOT(0x21/0x20, 受害者非玩家) */
        static DWORD s_mdotLog;
        if (tick - s_mdotLog >= 5000) { /* 5s 节流, 只作链路验证不刷屏 */
            s_mdotLog = tick;
            dll_log2("[mdot] monster-DOT font a5=%08X -> host ch 0x04", a5);
        }
        vib_collect(VEV_FONT, FONT_FLAG_EFFECT, tick, 1); /* 宿主「装备特效 0x04」 */
        return;
    }
    if (a5 & FONT_FLAG_PLAYER_HIT) { /* 0x02: 玩家受击飘字 = 玩家被打 */
        /* ★v13.14 根因修正: sub_470ED0 是 __cdecl(a1,a2,a3,a4,a5), a1-a3 是飘字
         * 坐标(x,y,z), NOT 受害对象! 旧代码读 obj+0x3E4==player == 读坐标内存,
         * 永不成立 → 受击从不发 (用户实测受击=100 也无效)。
         * 定案(sa_hit_anchor L92/L98): a5&0x02 本身 = "HP变动对象==玩家=受击",
         * 调用者(sub_4748D0 0x474941)已把"受害者归属==玩家"编码进 a5 的 0x02 位。
         * → 受击判定只看 a5&0x02, 无需对象归属读取。
         * (怪打玩家出暴击 = a5 0x10|0x02: 也从这里走受击通道, 语义正确)
         * ★v13.24 标记收紧(代理B 校正): 玩家吃 DOT 时 a5=0x23(0x02|0x20) 也进
         * 本分支 —— 0x20 位仅 DOT 三处理器置位 = 完美 DOT 标记。真受击佐证
         * (供 HP 沿关联门)只认 0x02 且无 0x20(DOT)/0x04(回复) 的字; 发送行为
         * 不变(DOT 掉血仍双通道, v13.20 已验收)。 */
        if (!(a5 & FONT_FLAG_HP) && !(a5 & FONT_FLAG_EFFECT))
            g_lastPlayerFontHitTick = tick; /* ★v13.24 真受击飘字佐证时刻 */
        vib_collect(VEV_FONT, FONT_FLAG_PLAYER_HIT, tick, 1); /* 宿主受击通道 */
        return;
    }
    /* ★v13.18 暴击/破招/背击 特殊伤害恢复(sa_stub8_crit_restore_result 定案):
     * 玩家打出的特殊伤害 = a5 0x10(特殊) | 0x01(玩家攻), 无 0x02(非受击, 上面
     * 已早退)。用户指定统一走宿主「特殊攻击反馈(0x10)」滑块(pidx13)。
     * ★v13.31 本函数成为命中反馈唯一源(stub2/stub6 已空转退出, 对齐新版
     * "命中=纯飘字单源"架构): 每段伤害数字逐条直发 0x01(40ms 去抖仅防
     * 极端高频), 暴击字按 0x10 优先级走特殊通道(与新版
     * vib_dispatch_common_hit 优先级链一致)。 */
    if ((a5 & FONT_FLAG_SPECIAL) && (a5 & FONT_FLAG_PLAYER_ATTACK)) {
        vib_collect(VEV_FONT, FONT_FLAG_SPECIAL, tick, 1); /* 宿主「特殊攻击反馈 0x10」滑块 */
        return;
    }
    /* ★v13.20 回复/治疗震动(用户指定通道): 飘绿色回复数字时 a5 只带 0x04
     * (v13.9 sa_font_src 实证: 无 0x01/0x02 位, 旧代码在下方 0x01 门被静默丢弃)。
     * 发 FONT 0x08 → 宿主「玩家状态变化反馈」滑块 —— 治疗本质是玩家状态变化,
     * 独立滑块可单独调强度/拉零关闭。[heal] 日志供实机验证(喝药/治疗技能即触发,
     * a4=回复量; 若发现非回复场景误触发, 再按 a5 组合收紧)。 */
    if (a5 & FONT_FLAG_EFFECT) { /* 0x04: 本版语义=回复数字(协议头历史注释叫"特效飘字") */
        dll_log2("[heal] amt=%u a5=%08X", a4, a5);
        vib_collect(VEV_FONT, FONT_FLAG_STATE, tick, 1); /* 宿主「玩家状态变化反馈 0x08」 */
        return;
    }
    if (!(a5 & FONT_FLAG_PLAYER_ATTACK)) return; /* 0x01 扣血/攻击命中位缺 → 非命中 */
    /* 建筑区分已在汇编层([esp+40]==0x58C958 → 跳 tramp 不调 C) */
    vib_collect(VEV_FONT, FONT_FLAG_PLAYER_ATTACK, tick, 1); /* count=1: 一刀一计数 */
}

/* ★v12 死亡直钩(宿主规则, 子代理定案 sa_hostrule_anchor_result):
 * 0x004217CE = 通知38(MonsterDie) handler —— 服务端逐只怪死亡确认,
 *   模拟端每只怪死亡发一次, 单目标低频(非批量), 语义≈1.5 sub_F20BF0。
 * hookLen=7 (C6 85 8A E4 FF FF 00 = mov byte [ebp-1B76h],0, E9+2×NOP)。
 * 进入即"该怪已死亡确认" → VEV_TARGET_DIE(宿主死亡通道, 事件到达即震)。
 * 强度=100 满幅, count=1 (宿主 inject_rank: s=rank_gain[14]×1.0 → 满幅)。 */
void __cdecl vib_dispatch_target_die(void *obj, DWORD a2, DWORD a3, int type)
{
    DWORD tick = GetTickCount();
    (void)obj; (void)a2; (void)a3; (void)type;
    dll_log2("[die] notify38 -> TARGET_DIE t=%u", tick);   /* ★ACT5 验证探针 */
    vib_collect(VEV_TARGET_DIE, 100, tick, 1); /* 怪物死亡确认 → 宿主死亡通道 */
}

/* ★v13.21 镜头震动汇聚点分发 (stub12 ← sub_4E49E0, sa_cc_state 第5轮定案)
 * 语义: 全游戏 68 处技能/特效震屏的唯一出口(等价新版 sub_1E25540 汇聚点)。
 *   __thiscall(this=相机对象, a2=震屏度数, a3=第二参); a2==0 分支=停震调用。
 * 通道: VEV_KILL(21) → 宿主 rank_gain[9]「释放技能」滑块(用户拍板 2026-09-07)。
 *   宿主对 VEV_KILL 的 strength 字段不参与计算(只走 rank 增益), 发满幅 100。
 * 防风暴: degree==0 不发(停震); 100ms 去重(同一抖动多源叠加只发首震)。
 * 注: 播报层 case5(击杀)也发 VEV_KILL —— 该滑块现承载「击杀+镜头震动」双源,
 *   与用户拍板一致(镜头震动接入释放技能通道)。 */
void __cdecl vib_dispatch_camera_shake(void *obj, DWORD degree, DWORD a3, int type)
{
    static DWORD s_last = 0;
    DWORD tick = GetTickCount();
    (void)type;
    if (!g_ringReady) return;
    if (degree == 0) return;              /* 停震调用(游戏清 +0x164/+0x168) 不发 */
    if (tick - s_last < 100) return;      /* 100ms 去重 */
    s_last = tick;
    dll_log2("[shake] degree=%u a3=%u obj=%08X", degree, a3, (DWORD)obj);
    /* 用户修正设定(2026-09-12): 镜头震动接【释放技能】接口 = VEV_KILL(21) → 宿主 rank idx9。
     * (V3 宿主另有 VEV_SHAKE_SCREEN(22) 的独立「镜头震动」通道，但用户指定走「释放技能」。) */
    vib_collect(VEV_KILL, 100, tick, 1);
}

/* ★v13.10 DOT 持续伤害掉血震动 (pass2/pass3 定案 sa_dot_pass2_anchor_result):
 * 新锚点 sub_4D10F0@0x4D10F0 —— 每角色 ActiveStatus 状态 tick 调度器,
 *   switch(*(a1+4)): case 2→sub_4D0960(中毒) case 9→sub_4CF6B0(出血/燃烧)
 *   case 0xB→sub_4CF1E0(状态扣血综合)。三兄弟都最终 call sub_470ED0 生成红字。
 * 根因之前: 现有 stub8 hook sub_470ED0 是"攻击飘字"咽喉, 但报告 sa_hit_anchor
 *   L300 明确 [DOT/持续伤害/回血走 sub_4CF1E0 族, 不经过 4748D0/470ED0]
 *   → 中毒/燃烧红字从不触发 stub8, 三合一归属修复也没用(根本不到这函数)。
 * 判定(入口): __usercall a1@ecx = 受害角色对象; 玩家归属 = *(a1+0x30)==B75A14
 *   (注意: 0x30 是【角色对象】归属偏移, 非 0x3E4【飘字记录对象】——pass2 已修正);
 *   *(a1+4) = ActiveStatus 类型码(下表)。
 * 事件: VEV_FONT FONT_HP(0x20) → 宿主 DOT 通道(P_FONT_HP 强度槽 + item_lr[7])。
 * 无建筑需排除(归属位天然过滤非玩家); 入口 5 字节(E9+trampoline)可 hook。
 * 局限: 入口看不到 v>0 符号位(函数内 setnle 才算出), 语义="玩家正处于
 * 伤害型状态 tick"≈玩家掉血(状态类扣血恒为正)。
 * ★v13.22 CC 扩展(报告 36 定案, 修正 v13.21 条件码路线翻车):
 *   类型码表 = ActiveStatus 类型枚举(容差表倒序模型, 报告 36):
 *     0=迟缓 1=冰冻 2=中毒✓ 3=眩晕 4=诅咒✓ 5=失明✓ 6=感电✓ 7=石化
 *     8=睡眠✓ 9=燃烧✓ 10=诅咒甲? 11=出血✓ ...(9/11 勘误见报告 37)
 * ★v13.26 DOT 改道飘字驱动(用户拍板"跳字触发比较好"):
 *   DOT 集 {2,9,0xB} 停发(每 tick 一条 = 用户实测"持续伤害狂震"根因),
 *   改由 stub8 每红字一条 0x20(游戏侧 500ms 节流, 节奏与新版一致);
 *   本函数仅保留 CC 集 {1,3,7,8} = 冰冻/眩晕/石化/睡眠 的保持式续发
 *   (被控期间 tick 每帧到达 = 天然续发; 解控 tick 停 = 宿主 hold 到期自然停)。
 *   [ccst] 探针: 玩家身上一切状态类型 tick, 每类型 5s 一条 → 实测定案
 *   hold/confuse 等剩余类型号后扩集(教训 18: 先实测再进判定集)。 */
void __cdecl vib_dispatch_dot_active(void *a1, DWORD a2, DWORD a3, int type)
{
    DWORD tick = GetTickCount();
    DWORD player, code;
    static DWORD s_probeLast[24];   /* [ccst] 每类型 5s 节流(类型 0..23) */
    (void)a2; (void)a3; (void)type;
    if (!a1) return;
    if (!VIB_ANCHOR_IS_SET(OLD_PLAYER_OBJ)) return;   /* ★版本剖面: ACT5 未定位 → 跳过 */
    player = *(DWORD *)OLD_PLAYER_OBJ;
    if (!player || player == 0xFFFFFFFF || player < 0x10000) return;
    /* 玩家归属: 角色对象 a1 的 [0x30] == B75A14 (pass2 实证, 非 0x3E4) */
    if (IsBadReadPtr((LPCVOID)((BYTE *)a1 + 0x30), 4)) return;
    if (*(DWORD *)((BYTE *)a1 + 0x30) != player) return;
    /* ActiveStatus 类型码: CC 集{1=冰冻,3=眩晕,7=石化,8=睡眠}(v13.26 DOT 已改道) */
    if (IsBadReadPtr((LPCVOID)((BYTE *)a1 + 4), 4)) return;
    code = *(DWORD *)((BYTE *)a1 + 4);
    /* [ccst] 探针: 记录玩家一切状态类型(教训18 实测先行; 5s/类型节流防刷屏) */
    if (code < 24) {
        if (tick - s_probeLast[code] >= 5000) {
            s_probeLast[code] = tick;
            dll_log2("[ccst] type=%u tick(ActiveStatus on player)", code);
        }
    }
    if (code != 1 && code != 3 && code != 7 && code != 8) return; /* ★v13.26 仅 CC 集 */
    /* ★v13.26 DOT 改道: {2,9,0xB} 不再在此发事件 —— 每tick一条=用户实测
     * "持续伤害狂震"根因; 改由 stub8 每红字一条 0x20(游戏侧 500ms 节流, 节奏与新版一致)。
     * 仅保留 CC 集{1,3,7,8} 的保持式续发(被控制=持续震到解除, 用户拍板)。
     * ★v13.27 改原子计数。
     * ★v13.34 发 0x60(0x20|0x40 保持式标记): 宿主据 0x40 区分 CC 保持震
     * (mode1 hold 平滑保持)与玩家 DOT 红字跳字(纯 0x20 → mode3 衰减脉冲,
     * 每红字一震) —— 修复多 DOT 叠加时 hold 无缝续期的"长时间持续震动"。 */
    vib_collect(VEV_FONT, FONT_FLAG_HP | FONT_FLAG_OTHER, tick, 1); /* 0x60: CC 保持式 */
}

/* ★v13.13 暴击/破招/背击/额外 特殊伤害 (sa_hitbyte_map 定案):
 * 新锚点 sub_78EE00@0x78EE00 = 事件主结算(战斗事件处理器, 间接调度)。
 * 它在 0x790D93 送 sub_470ED0 前组装 a5 标志: v321 = (HIBYTE(a3)?0x10:0)|玩家攻|玩家受。
 * → HIBYTE(a3) 是四小类承载字段: {3,4,5,9} = 四类特殊伤害类型码 (高置信坐实:
 *   sub_78EE00 0628-0635 行 vtable+244「必播动画」门控精确含 3/4/5/9)。
 * ★v13.13 用户指令(核心): "不管是暴击还是背击什么特殊伤害, 都走「特殊攻击反馈
 *   (0x10)」滑块, 不然不方便测试。"
 *   → 四类特殊伤害【统一】发 VEV_FONT + FONT_FLAG_SPECIAL(0x10) → 宿主「特殊攻击
 *     反馈 (0x10)」滑块(pidx13/P_FONT_SPECIAL)。不细分到 VEV_CRIT/BREAK/BACK
 *     (宿主 GUI 那些评分细分滑块默认0/被注释搁置, 用户不便测试)。
 *   → 与 v13.11 差异: v13.11 用 stub8(sub_470ED0)收0x10(实机收不到暴击);
 *      v13.13 用 stub11(sub_78EE00)直读 HIBYTE(a3)是能真正收到暴击的正确锚点,
 *      故此时发 0x10 会真的震(宿主滑块已开)。
 * ★v13.15 (sa_monolayer_crit_result 定案): 锚点从 sub_78EE00 入口 0x78EE00
 *   移到内部 0x790D93(唯一权威出口, E8 近调用 sub_470ED0 处, hookLen=5)。
 *   原因: sub_78EE00 入口 0x78EE32/38/42 把参数区(arg_8)-7重写、0x78EE53 清
 *   arg_4 → 入口时刻 HIBYTE(a3)=0 结构性收不到暴击。0x790D93 处 a5 才组装完。
 *   判定(归属无需解对象): 玩家暴击 = (a5&0x10特殊) && (a5&0x01玩家攻) && !(a5&0x02非受击)。
 *   → 一次暴击一次事件, 必触发。asm 读 a5=*(esp+44) 传 C。 */
void __cdecl vib_dispatch_battle_event(void *obj, DWORD a2, DWORD a3, int type)
{
    DWORD t = GetTickCount();
    DWORD a5 = a3;   /* asm 第3参 = a5 标志 (0x790D93 组装完成的标志) */
    (void)obj; (void)a2; (void)type;
    if (!a5) return;
    /* 玩家暴击/破招/背击: 特殊(0x10) + 玩家攻(0x01) + 非受击(0x02无) */
    if ((a5 & FONT_FLAG_SPECIAL) && (a5 & FONT_FLAG_PLAYER_ATTACK) && !(a5 & FONT_FLAG_PLAYER_HIT)) {
        vib_collect(VEV_FONT, FONT_FLAG_SPECIAL, t, 1); /* 宿主「特殊攻击反馈0x10」滑块 */
    }
}

/* ---------------- hook 安装引擎(复用 1.5 骨架) ---------------- */

typedef struct {
    DWORD target;
    BYTE  original[16];
    int   hookLen;
    void *stub;
    void **trampVar;
    int   installed;   /* ★版本剖面: 未定位/校验失败者保持 0, 卸载与自检按此跳过 */
} HookDesc;

static HookDesc g_hooks[VIB_HOOK_COUNT]; /* ★版本剖面: 容量宏与锚点表同源(教训 12 结构性防越界);
                             * ★v13.12 教训: 越界写会冲掉紧随的 g_hookCount/g_active/
                             * g_trampPage 指针 -> trampoline 写往垃圾地址 -> 注入后
                             * 游戏直接卡退(实测), 数组容量必须与注册表同步 */
static int g_hookCount = 0;
static int g_active = 0;
static BYTE *g_trampPage = NULL;

/* 指令长度探测(x86, 覆盖 5 字节缺口的最小指令对齐) */
static int insn_len(BYTE *p)
{
    /* 覆盖常用定长指令: 5=E9; 6=push imm32; 3=mov reg,imm8/inc/dec... */
    /* 精简实现(1.5 版已验证可用): 用 IDA 分析结果手工给定各目标 hookLen */
    return 0; /* 由下面的表驱动 */
}

/* 由 sa_verify 验证轮反汇编确认的每目标覆盖长度(指令边界对齐):
 * 0x433890: push ebp(1)+mov ebp,esp(2)+mov eax,[ebp+8](3) = 6, 返回点 push esi
 * 0x4E6DA0: push ebp(1)+mov ebp,esp(2)+sub esp,0Ch(3) = 6, 返回点 mov eax,[ebp+8]
 * 0x4D2C50: push ebp(1)+mov ebp,esp(2)+push esi(1)+mov esi,ecx(2) = 6, 返回点 mov ecx,dword_B5D6F0
 * 0x4E8520: push ebp(1)+mov ebp,esp(2)+push 0FFFFFFFFh(2) = 5, 返回点 push offset SEH
 * 0x4E3B40: push esi(1)+mov esi,ecx(2)+mov eax,[esi+934h](6) = 9, 返回点 test eax,eax
 * 0x4BCA70: push ebp(1)+mov ebp,esp(2)+push esi(1)+mov esi,ecx(2) = 6, 返回点 mov eax,[esi]
 */
/* sub_4748D0 头部: push ebp(1)+mov ebp,esp(2)+sub esp,10h(3)+push esi(1)+mov esi,ecx(2) = 9 字节
 * sub_42DCB0 头部: push ebp(1)+mov ebp,esp(2)+push 0FFFFFFFFh(2) = 5 字节(SEH push 前)
 * sub_470ED0 头部: push ebp(1)+mov ebp,esp(2)+push 0FFFFFFFFh(2) = 5 字节(SEH push 前, 同 42DCB0) */
/* 每目标 hookLen：由版本剖面提供（ACT1 老值 / ACT5 新值；未定位=0 会被跳过） */
static const int g_targetLens[VIB_HOOK_COUNT] = {
    TGT_HOOKLEN_SENDER,
    TGT_HOOKLEN_BROADCAST,
    TGT_HOOKLEN_ACCUM,
    TGT_HOOKLEN_VOICE,
    TGT_HOOKLEN_RESULT,
    TGT_HOOKLEN_FINALE,
    TGT_HOOKLEN_ATHIT,
    TGT_HOOKLEN_COMBOUI,
    TGT_HOOKLEN_DMGFONT,
    TGT_HOOKLEN_NOTIFY38,
    TGT_HOOKLEN_STATUSTICK,
    TGT_HOOKLEN_CRITEXIT,
    TGT_HOOKLEN_CAMSHAKE,
};

/* 每目标期望签名（原 exe 头部 8 字节）：安装前 memcmp 校验。
 * 全 0 = 不校验（未定位锚点会被 skip，到不了校验；critexit 永不安装）。
 * 目的：把「ACT5 地址猜错 → E9 改写无关代码 → 崩游戏」降级为「拒绝安装 + 日志」。 */
static const BYTE g_targetSig[VIB_HOOK_COUNT][8] = {
    TGT_SIG_SENDER,
    TGT_SIG_BROADCAST,
    TGT_SIG_ACCUM,
    TGT_SIG_VOICE,
    TGT_SIG_RESULT,
    TGT_SIG_FINALE,
    TGT_SIG_ATHIT,
    TGT_SIG_COMBOUI,
    TGT_SIG_DMGFONT,
    TGT_SIG_NOTIFY38,
    TGT_SIG_STATUSTICK,
    TGT_SIG_CRITEXIT,
    TGT_SIG_CAMSHAKE,
};

/* 目标可 hook 判定：代码段保护通常是 PAGE_EXECUTE_READ(只读可执行)，
 * 不能要求"已可写"，否则 0/6 全失败（实机已验证）。只要页已提交且
 * 可执行即可，真正改权限由 hook_one 里的 VirtualProtect 完成。 */
static int target_writable(DWORD ea)
{
    MEMORY_BASIC_INFORMATION mbi;
    if (VirtualQuery((LPCVOID)ea, &mbi, sizeof(mbi)) == 0) return 0;
    if (mbi.State != MEM_COMMIT) return 0;
    return (mbi.Protect & (PAGE_EXECUTE | PAGE_EXECUTE_READ |
                           PAGE_EXECUTE_READWRITE | PAGE_EXECUTE_WRITECOPY)) != 0;
}

/* 安装单个 hook(idx 用于取 g_targetLens) */
static int hook_one(HookDesc *h, int idx)
{
    DWORD oldProtect;
    BYTE patch[16];
    DWORD rel;
    int len = g_targetLens[idx];
    const BYTE *sig = g_targetSig[idx];
    int sigAllZero = 1, k;

    if (len <= 0 || len > 16) {
        dll_log2("[hook] idx=%d unresolved/len invalid (%d) -> skip", idx, len);
        return 0;
    }
    if (!target_writable(h->target)) {
        dll_log2("[hook] idx=%d target 0x%08X rejected by writability check", idx, h->target);
        return 0;
    }
    /* ★版本剖面签名校验: 防错地址被 E9 改写(ACT5 未定位/猜错场景的安全网) */
    for (k = 0; k < 8; k++) if (sig[k]) { sigAllZero = 0; break; }
    if (!sigAllZero && memcmp((const void *)h->target, sig, 8) != 0) {
        dll_log2("[hook] idx=%d target 0x%08X sig mismatch -> REFUSE (wrong version/address?)", idx, h->target);
        return 0;
    }
    /* 保存原始字节 + 读前 len 字节 */
    memcpy(h->original, (BYTE *)h->target, len);
    h->hookLen = len;

    /* 分配 trampoline 页 */
    if (!g_trampPage) {
        g_trampPage = (BYTE *)VirtualAlloc(NULL, VIB_HOOK_COUNT * 32, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE); /* ★版本剖面: 容量宏同源 */
        if (!g_trampPage) {
            dll_log2("[hook] idx=%d trampoline page alloc failed", idx);
            return 0;
        }
    }
    /* 写 trampoline: 原指令 + E9 回跳 (stub tramp 由汇编使用)
     * 注意: 必须用 idx 定位块, 不能用 g_hookCount —— 安装循环中
     * g_hookCount 始终为 0(最后才赋值), 会令 6 块 trampoline 全部
     * 重叠写在页首, 6 个 stub 跳同一地址, 游戏逻辑被劫持卡死(实测)。 */
    {
        BYTE *t = g_trampPage + idx * 32;
        memcpy(t, h->original, len);
        /* ★v13.16 rel32 修位: 若首指令是相对调用/跳转(E8 call/E9 jmp),
         * 原 4 字节位移相对原 target 计算, 移到 tramp 页后必须重算位移,
         * 否则 call 跳到错误地址 → 崩溃(暴击 0x790D93 E8 rel32 实测卡死)。 */
        if (h->original[0] == 0xE8 || h->original[0] == 0xE9) {
            /* 原位移(相对 target+5) */
            LONG orig_disp = *(LONG *)(h->original + 1);
            DWORD orig_target = (DWORD)h->target + 5 + (DWORD)orig_disp;
            LONG new_disp = (LONG)(orig_target - (DWORD)(t + 1) - 5);
            *(LONG *)(t + 1) = new_disp;
        }
        *h->trampVar = t;
        /* rel32 回跳(从 len 处到原 target+len) */
        rel = (DWORD)(h->target + len) - (DWORD)(t + len) - 5;
        t[len] = 0xE9;
        *(DWORD *)(t + len + 1) = rel;
    }

    /* patch 原函数: E9 <rel32 to stub> */
    if (!VirtualProtect((LPVOID)h->target, len, PAGE_EXECUTE_READWRITE, &oldProtect)) {
        dll_log2("[hook] idx=%d target 0x%08X VirtualProtect RW failed err=%lu", idx, h->target, GetLastError());
        return 0;
    }
    rel = (DWORD)h->stub - h->target - 5;
    patch[0] = 0xE9;
    *(DWORD *)(patch + 1) = rel;
    memset(patch + 5, 0x90, len - 5);
    memcpy((BYTE *)h->target, patch, len);
    if (!VirtualProtect((LPVOID)h->target, len, oldProtect, &oldProtect)) {
        dll_log2("[hook] idx=%d target 0x%08X VirtualProtect restore failed", idx, h->target);
        return 0;
    }
    h->installed = 1;
    dll_log2("[hook] idx=%d OK 0x%08X len=%d", idx, h->target, len);
    return 1;
}

/* 安装全部(由 worker 调用, 游戏线程上下文外) */
int hooks_install_old(void)
{
    static void (*stubPtrs[VIB_HOOK_COUNT])(void) = {
        (void (*)(void))vib_stub_0,
        (void (*)(void))vib_stub_1,
        (void (*)(void))vib_stub_2,
        (void (*)(void))vib_stub_3,
        (void (*)(void))vib_stub_4,
        (void (*)(void))vib_stub_5,
        (void (*)(void))vib_stub_6,
        (void (*)(void))vib_stub_7,
        (void (*)(void))TGT_STUB_DMGFONT, /* [8] 伤害数字咽喉: ACT1=stub8 / ACT5=stub13(参数布局不同) */
        (void (*)(void))vib_stub_9,
        (void (*)(void))vib_stub_10, /* ★v13.10 DOT 状态tick调度器 */
        (void (*)(void))vib_stub_11, /* ★v13.12 sub_78EE00 事件主结算(暴击/破招/背击) */
        (void (*)(void))vib_stub_12, /* ★v13.21 sub_4E49E0 镜头震动汇聚点 */
    };
    int i, n = 0;
    /* ★版本剖面: 目标地址全部来自 vib_target_old.h；未定位项 = 0 将被显式跳过 */
    DWORD targets[VIB_HOOK_COUNT] = {
        TGT_HOOK_SENDER,      /* [0]  统一事件发送器 */
        TGT_HOOK_BROADCAST,   /* [1]  播报层 */
        TGT_HOOK_ACCUM,       /* [2]  加分汇聚 */
        TGT_HOOK_VOICE,       /* [3]  技巧语音层 */
        TGT_HOOK_RESULT,      /* [4]  结算震动实体 */
        TGT_HOOK_FINALE,      /* [5]  地城阶段结算 */
        TGT_HOOK_ATHIT,       /* [6]  扣血结算 */
        TGT_HOOK_COMBOUI,     /* [7]  战斗UI加载器(Combo.img) */
        TGT_HOOK_DMGFONT,     /* [8]  伤害数字咽喉(命中锚点) */
        TGT_HOOK_NOTIFY38,    /* [9]  通知38 MonsterDie 死亡直钩 */
        TGT_HOOK_STATUSTICK,  /* [10] 状态tick调度器(DOT/CC) */
        TGT_HOOK_CRITEXIT,    /* [11] 暴击权威出口(E8) — 永不安装 */
        TGT_HOOK_CAMSHAKE,    /* [12] 镜头震动汇聚点 */
    };
    dll_log2("[hook] target=%s anchors=%d", VIB_TARGET_NAME, (int)VIB_HOOK_COUNT);

    /* ★v13.16 无损改造: 移除会卡死的暴击 inline patch(0x790D93 是 E8 相对调用,
     * E9 JMP 改写 + trampoline 重放会崩)。暴击改为无损轮询(见 poll_crit)。
     * 其余锚是普通指令(非 E8/E9 开头), 无损改写安全。
     * 此处仍遍历 13 但跳过 index 11(stub11/tramp11 保留备用, 不安装)。 */
    for (i = 0; i < VIB_HOOK_COUNT; i++) {
        if (i == 11) continue;   /* ★v13.16 跳过暴击 inline patch(卡死源) */
        if (!VIB_ANCHOR_IS_SET(targets[i])) {
            /* ★版本剖面: 未定位锚点绝不安装(ACT5 大量锚点待实机标定) */
            dll_log2("[hook] idx=%d UNRESOLVED in %s -> skip", i, VIB_TARGET_NAME);
            continue;
        }
        g_hooks[i].target = targets[i];
        g_hooks[i].stub = stubPtrs[i];
        switch (i) {
        case 0: g_hooks[i].trampVar = &vib_tramp_0; break;
        case 1: g_hooks[i].trampVar = &vib_tramp_1; break;
        case 2: g_hooks[i].trampVar = &vib_tramp_2; break;
        case 3: g_hooks[i].trampVar = &vib_tramp_3; break;
        case 4: g_hooks[i].trampVar = &vib_tramp_4; break;
        case 5: g_hooks[i].trampVar = &vib_tramp_5; break;
        case 6: g_hooks[i].trampVar = &vib_tramp_6; break;
        case 7: g_hooks[i].trampVar = &vib_tramp_7; break;
        case 8: g_hooks[i].trampVar = &vib_tramp_8; break;
        case 9: g_hooks[i].trampVar = &vib_tramp_9; break;
        case 10: g_hooks[i].trampVar = &vib_tramp_10; break; /* ★v13.10 DOT */
        case 12: g_hooks[i].trampVar = &vib_tramp_12; break; /* ★v13.21 镜头震动 */
        }
        if (hook_one(&g_hooks[i], i)) n++;
    }
    g_hookCount = n;
    g_active = (n > 0);
    return n;
}

/* ★v13.39 hook 完整性自检(新架构基建, 排障用): 返回首字节 != E9 的已装 hook 数
 * (运行时自保护/第三方改写会把 E9 覆盖掉)。 */
int hooks_intact_bad_old(void)
{
    int i, bad = 0;
    for (i = 0; i < VIB_HOOK_COUNT; i++) {
        if (g_hooks[i].installed && *(BYTE *)g_hooks[i].target != 0xE9) bad++;
    }
    return bad;
}

void hooks_uninstall_old(void)
{
    int i;
    /* ★版本剖面: 按 installed 标志卸载(锚点索引可能不连续: 未定位项被跳过) */
    for (i = 0; i < VIB_HOOK_COUNT; i++) {
        DWORD oldProtect;
        if (!g_hooks[i].installed) continue;
        VirtualProtect((LPVOID)g_hooks[i].target, g_hooks[i].hookLen, PAGE_EXECUTE_READWRITE, &oldProtect);
        memcpy((BYTE *)g_hooks[i].target, g_hooks[i].original, g_hooks[i].hookLen);
        VirtualProtect((LPVOID)g_hooks[i].target, g_hooks[i].hookLen, oldProtect, &oldProtect);
        g_hooks[i].installed = 0;
    }
    g_hookCount = 0;
    g_active = 0;
}

int hooks_active_old(void) { return g_active; }

/* ============================================================
 * 第三轮加密字段读法(报告 17-22 定案, 见 docs/加密字段访问算法_参考.md)
 * ★v13.19 修正(sa_hp_formula_fix_result): 明文 = 存储值 ^ *(DWORD*)CFD45C ^ (表基+4*idx)
 *   —— dword_CFD45C 是密钥全局变量的【值】(每次启动随机, sub_901E40), 不是常数!
 *   表基A = *(dword_CFD458 + 17);  idx = 槽描述[+4];  容量 = *(dword_CFD458) = 0x200000
 * 只读不写(写路径会破坏完整性标记触发 TerSafe)
 * ============================================================ */
/* ★v13.19 运行时密钥: 旧代码把密钥地址 0x00CFD45C 当 XOR 立即数(IDA 符号
 * dword_CFD45C 的语义误读), 是 v13.18 HP 完整性公式实机 ok 恒 0 的全部根因;
 * score/monster 解密绝对值同步失真(沿检测对固定 XOR 偏移侥幸可用, 但序关系
 * 不保持 → HP 沿检测不可用)。只读解引用, IsBadReadPtr 防注销期。 */
static DWORD enc_key_old(void)
{
    if (IsBadReadPtr((LPCVOID)OLD_ENC_KEY_ADDR, 4)) return 0;
    return *(DWORD *)OLD_ENC_KEY_ADDR;                /* 不是常数! */
}

static DWORD read_enc_dword(DWORD slotPtr)
{
    DWORD tbl, baseA, cap, idx, ea, raw, key;
    tbl  = *(DWORD *)OLD_ENC_TABLE_BASE;              /* dword_CFD458 → ctx */
    if (!tbl) return 0;
    cap  = *(DWORD *)tbl;                             /* ctx+0 = 容量 0x200000 */
    baseA = *(DWORD *)(tbl + 17 * 4);                 /* 表 A 指针 */
    if (!baseA || baseA < 0x10000) return 0;
    key = enc_key_old();
    if (!key) return 0;
    idx   = *(DWORD *)(slotPtr + 4);                  /* 槽下标 */
    /* ★v13.19 池不变量: idx < 容量(sa_hp_formula_fix: 实测 0x54302 在池内,
     * 表A 8MB/0x200000 项; cap 读 0 视为未知跳过本检, 退化为 v13.18 行为) */
    if (cap && idx >= cap) return 0;
    ea    = baseA + 4 * idx;
    /* ★v13.18 ea 有效性检查(sa_player_hp_slot_result 定案): 异常 idx(伪随机)会把
     * ea 打到任意内存, 裸读 *(DWORD*)ea 可能 AV 崩游戏 —— v13.17 曾用错误槽
     * 0x1828 把状态块完整性标记当 idx, 就是这个雷(潜伏崩游戏风险, 必须堵)。 */
    if (ea < 0x10000) return 0;
    if (IsBadReadPtr((LPCVOID)ea, 4)) return 0;
    raw   = *(DWORD *)ea;
    return raw ^ key ^ ea;                            /* ★v13.19 key=运行时值 */
}

/* ★v13.19 加密槽完整性校验(sub_402030 语义, sa_hp_formula_fix 实机三样本验证):
 * (key ^ slot[0]) - slotAddr == slot[1] → 槽可信; key=运行时密钥。
 * 实测样本回算: key=0x9CDEEF02(该会话), (key^m0)-slotAddr 三组全部精确 == i1。 */
static int enc_slot_ok(DWORD slotPtr)
{
    DWORD v0, v1, key;
    if (!slotPtr || slotPtr < 0x10000) return 0;
    if (IsBadReadPtr((LPCVOID)slotPtr, 8)) return 0;
    key = enc_key_old();
    if (!key) return 0;
    v0 = *(DWORD *)slotPtr;
    v1 = *(DWORD *)(slotPtr + 4);
    return ((key ^ v0) - slotPtr) == v1;
}

/* 轮询: 评分对象 dword_B5B410 → +932 总评上升沿
 * +932 是受保护浮点(加密双表), 必须用 read_enc_dword 解明文。
 * ★第五轮: 命中震动已由 stub6(sub_4748D0)/stub2(加分汇聚) 覆盖,
 *   评分轮询不再发事件(它是"事件持续增加+狂震"的最大源: 宿主 RANKING
 *   通道无节流直接满震; 6ms 高频×200ms 去抖仍持续)。只保留内部状态
 *   跟踪, 供后续结算对照(结算类事件由 stub0 的 RESULT_SCORE_UP_LOOP
 *   → RANKING 单独触发, 低频必达)。 */
void poll_score_old(void)
{
    static DWORD s_last = 0;
    DWORD ptr, val;
    if (!VIB_ANCHOR_IS_SET(OLD_SCORE_OBJ)) return;   /* ★版本剖面: ACT5 未定位 → 跳过 */
    ptr = *(DWORD *)OLD_SCORE_OBJ;
    if (!ptr) return;
    val = read_enc_dword(ptr + OLD_SCORE_TOTAL_OFF);  /* 解密读 +932 */
    if (val != s_last) s_last = val;  /* 仅跟踪, 不发事件 */
}

/* ★v13.19 已停用(sa_die_poll_anomaly_result 定案 + 用户确认死亡震动已由
 * stub9(0x4217CE 通知38 MonsterDie)实现):
 *   B6F608 并非「剩余怪物计数」——全库 10 处 xref 无任何死亡递减点, 它是
 *   sub_467190 房间脚本重建时 type-3 生成条目的清零重数计数, 实机恒 0,
 *   即 12 会话 [die]=0 的完整解释。
 *   密钥修复(v13.19)后若继续按旧读法检测下降沿, 房间重建的计数回落会
 *   误报 VEV_TARGET_DIE → 与 stub9 双震。故整体停发事件, 函数保留空壳
 *   维持 collector 调用面不变。真·怪物对象数在 B6F610(ctor/dtor ±1),
 *   按用户指示不再深挖死亡方向。 */
void poll_monsters_old(void)
{
}

/* ★v13.18 无损受击(辅助源→★v13.25 纯日志): 轮询玩家 HP 下降沿 —— 读法全面修正
 * (sa_player_hp_slot_result 四轮IDA一锤定音):
 *   真 HP 槽 = player+0x1168(完整性标记)/+0x116C(表下标), MP 同构 +0x1170。
 *   铁证: 游戏自身 GetHP thunk 0x652A20 `add ecx,1168h; jmp sub_402030`;
 *   SetHP/受击扣血(sub_483F50@0x48426D)/DOT 扣血(sub_4CF1E0)/周期重同步
 *   (sub_48BA40) 全走 +0x1168。
 *   ★旧定案 0x1824/0x1828 是错误地址(那是 +0x1814 起「限时状态块
 *   SuperArmor 系」的激活/剩余字段, 非 HP)。v13.17 按旧定案读 =
 *   把状态块伪随机完整性标记当表下标 → 读垃圾且潜伏 AV 崩游戏。
 * 受击主源 = poll_hit_counter_old(B5ED28, 精确受击判定); 本函数降为辅助:
 *   HP 下降沿覆盖「一切掉血」(含 DOT, 与 DOT 通道并存可能双震, 实测再议),
 *   上升沿(回血/治疗)暂只日志。事件与主源共用 FONT 0x02 去抖子槽自然合并。 */
void poll_player_hit_old(void)
{
    static DWORD s_hpLast = 0;
    static int   s_hpValid = 0;
    static DWORD s_probe = 0;
    DWORD player, hpSlot, hp = 0, mp = 0;
    int okHp, okMp;
    DWORD tick = GetTickCount();
    if (!g_ringReady) return;
    /* ★版本剖面: ACT5 的玩家对象/HP 槽偏移未定位 → 整个 HP 观测量跳过(防读 0 崩) */
    if (!VIB_ANCHOR_IS_SET(OLD_PLAYER_OBJ) || !VIB_ANCHOR_IS_SET(OLD_PLAYER_HP_SLOT_OFF)) return;
    player = *(DWORD *)OLD_PLAYER_OBJ;
    if (player == 0 || player == 0xFFFFFFFF || player < 0x10000) { s_hpValid = 0; return; }
    hpSlot = player + OLD_PLAYER_HP_SLOT_OFF;
    okHp = enc_slot_ok(hpSlot);
    okMp = enc_slot_ok(player + OLD_PLAYER_MP_SLOT_OFF);
    if (okHp) hp = read_enc_dword(hpSlot);
    if (okMp) mp = read_enc_dword(player + OLD_PLAYER_MP_SLOT_OFF);
    /* ★v13.19 诊断: 2s 探测日志 —— 无条件打印解密尝试 + 运行时密钥:
     * stg: 3=按 idx 解密成功 2=ea 越界/不可读 1=baseA 无效 0=tbl 无效
     * dec=解密结果(idx=slot[+4]); ea=表A+4*idx; key=*(CFD45C) 运行时密钥。
     * ★实测教训14: 完整性公式(ok)与槽活性(raw)是两回事——v13.18 公式实机
     * 证伪(ok 恒 0)期间解密从未被尝试, 看不到解密值就无法定位公式错误。 */
    if (tick - s_probe >= 2000) {
        DWORD r0 = 0, r1 = 0, dec = 0, ea = 0, key, tbl, baseA, idx;
        int stg = 0;
        s_probe = tick;
        if (!IsBadReadPtr((LPCVOID)hpSlot, 8)) {
            r0 = *(DWORD *)hpSlot;
            r1 = *(DWORD *)(hpSlot + 4);
        }
        key = enc_key_old();
        tbl = *(DWORD *)OLD_ENC_TABLE_BASE;
        if (tbl) {
            baseA = *(DWORD *)(tbl + 17 * 4);
            if (baseA && baseA >= 0x10000) {
                idx = r1;                     /* 槽下标 = slot[+4] */
                ea = baseA + 4 * idx;
                if (ea >= 0x10000 && !IsBadReadPtr((LPCVOID)ea, 4)) {
                    dec = *(DWORD *)ea;
                    dec = dec ^ key ^ ea;     /* ★v13.19 运行时密钥 */
                    stg = 3;
                } else stg = 2;
            } else stg = 1;
        }
        dll_log2("[hp-probe] p=%08X ok=%d,%d hp=%u mp=%u raw=%08X,%08X stg=%d dec=%u ea=%08X key=%08X",
                 player, okHp, okMp, hp, mp, r0, r1, stg, dec, ea, key);
    }
    if (!okHp || hp == 0 || hp >= 0x10000000UL) { s_hpValid = 0; return; } /* ★v13.19 值域防线: 2008 DNF HP 上限远低于 268M(sa_hp_formula_fix 建议) */
    /* ★v13.24 血之狂暴累加器指纹(机械级精准, sa_frenzy_result P5 定案):
     * CNFrenzy(player+0x2A14) 的扣血累加器在 obj+0x98(float): 每帧 += 攻速×dt×0.001,
     * 蓄满 1.0 扣 (int)acc 点血并保留小数 —— 累加器数值【下降】当且仅当刚刚扣血。
     * 每周期追踪, 回绕即打 g_lastFrenzyDrainTick 指纹; HP 沿见指纹(150ms 内)直接判
     * frenzy 滴血, 与战斗/中毒等环境佐证完全无关(v13.23b 的 200ms 关联窗在战斗中
     * 被环境佐证持续放行 = "依旧狂震"的残留根因)。 */
    {
        static float  s_prevAcc = 0.0f;
        static int    s_accValid = 0;
        DWORD fobj = 0;
        if (!IsBadReadPtr((LPCVOID)(player + OLD_PLAYER_FRENZY_OFF), 4))
            fobj = *(DWORD *)(player + OLD_PLAYER_FRENZY_OFF);
        if (fobj > 0x10000 && !IsBadReadPtr((LPCVOID)fobj, 4)
            && *(DWORD *)fobj == TGT_FRENZY_VT          /* CNFrenzy vtable 校验(版本剖面) */
            && !IsBadReadPtr((LPCVOID)(fobj + 0x98), 4)) {
            float acc = *(volatile float *)(fobj + 0x98);
            if (s_accValid && acc < s_prevAcc - 0.001f)
                g_lastFrenzyDrainTick = tick;        /* 回绕 = 刚刚扣血 */
            s_prevAcc = acc;
            s_accValid = 1;
        } else {
            s_prevAcc = 0.0f;
            s_accValid = 0;
        }
    }
    if (s_hpValid && hp < s_hpLast) {
        /* ★v13.25 HP 沿降级为【纯日志】(对齐新版架构, 报告37): 新版 DLL 无任何
         * HP 观测震动源 —— 受击唯一来源是飘字/计数管线, 血之狂暴等无数字掉血
         * 结构上不可见。老版 HP 沿的 0x02 在 v13.24 门下已 100% 冗余: 放行时
         * (corrHit/corrFont)主源(计数器/stub8)在同一 120ms 去抖窗内必已发过
         * 同通道事件, 沿事件恒被合并; 唯一"独立发声"场景恰是误报(佐证窗内的
         * frenzy 滴血, 37248 会话 1027 滴)。故删除发事件, 保留全套指纹/关联
         * 标记做【日志诊断】—— 掉血来源可追溯, 震动零参与。
         * 受击震动完全由主源承载: poll_hit_counter_old(B5ED28) + stub8(0x02字)。 */
        if (g_lastFrenzyDrainTick && tick - g_lastFrenzyDrainTick <= 150) {
            dll_log2("[hp-drain] HP %u -> %u drop=%u (frenzy-acc 指纹, 不震)",
                     s_hpLast, hp, s_hpLast - hp);
        } else {
            int corrHit  = g_lastPlayerHitTick && tick - g_lastPlayerHitTick <= 200;
            int corrFont = g_lastPlayerFontHitTick && tick - g_lastPlayerFontHitTick <= 200;
            if (corrHit || corrFont)
                dll_log2("[hp-hit] HP %u -> %u drop=%u (corr, 主源已震, 沿不重复发)",
                         s_hpLast, hp, s_hpLast - hp);
            else
                dll_log2("[hp-drain] HP %u -> %u drop=%u (silent: 无受击佐证, 不震)",
                         s_hpLast, hp, s_hpLast - hp);
        }
        g_lastPlayerHitTick = tick;   /* 供暴击探针 SELF_ONLY 同窗关联(默认关) */
    } else if (s_hpValid && hp > s_hpLast) {
        /* 回血/治疗(暂只日志; 治疗震动是否启用待用户拍板) */
        dll_log2("[hp-rise] HP %u -> %u (+%u)", s_hpLast, hp, hp - s_hpLast);
    }
    s_hpLast = hp;
    s_hpValid = 1;
}

/* ★v13.18 无损受击主源: 轮询被击计数器 dword_B5ED28 增量(纯只读, 零patch)
 * 定案(sa_b5ed28_hitcounter_result, 五轮IDA批处理, 置信~95%):
 *   唯一++点 0x4A1BFC(sub_4A1B50 受击入口内)处于双重门控:
 *   门1 被击者==B75A14(玩家专属 —— 玩家打怪/怪互殴/队友受击均不++)
 *   门2 vtable+1388「有效受击」(敌对阵营 + 非无敌帧 + 伤害参数有效)
 *   → 语义=「本地玩家被敌方有效命中」, 与游戏自身受击表现一致。
 *   普通 DWORD 无需解密、读无副作用; 唯一清零 sub_466B50(离开副本/teardown/
 *   会话级初始化)。
 * 防御: new<old(清零)只 re-baseline 不发事件; 每周期 delta 合并为一次事件
 *   (多段攻击一周期内多次++只发一震, 防风暴)。
 * 与 poll_player_hit_old(HP沿) 互补: HP 沿含 DOT/掉血语义模糊, 本计数器
 *   精确=受击判定次数(DOT 不经过受击虚函数)。两源同发时由 vib_collect 的
 *   FONT 0x02 去抖子槽(120ms)自然合并为一震。 */
void poll_hit_counter_old(void)
{
    static DWORD s_last = 0;
    static int   s_valid = 0;
    DWORD v, tick, dmgTotal = 0;
    if (!g_ringReady) return;
    if (!VIB_ANCHOR_IS_SET(OLD_HIT_COUNT)) return;   /* ★版本剖面: ACT5 未定位 → 跳过 */
    if (IsBadReadPtr((LPCVOID)OLD_HIT_COUNT, 4)) { s_valid = 0; return; }
    v = *(DWORD *)OLD_HIT_COUNT;
    if (!s_valid) { s_last = v; s_valid = 1; return; }
    if (v == s_last) return;
    tick = GetTickCount();
    if (v > s_last) {
        /* 辅读同块累计受伤值(B5ED24, 同门控同清零), 供未来强度分级(暂只日志) */
        if (!IsBadReadPtr((LPCVOID)OLD_DAMAGE_TOTAL, 4))
            dmgTotal = *(DWORD *)OLD_DAMAGE_TOTAL;
        dll_log2("[hit-ctr] %u -> %u (+%u) dmgTotal=%u", s_last, v, v - s_last, dmgTotal);
        vib_collect(VEV_FONT, FONT_FLAG_PLAYER_HIT, tick, 1); /* host「玩家受击反馈 0x02」 */
        g_lastPlayerHitTick = tick;   /* 供暴击探针 SELF_ONLY 同窗关联(默认关) */
    }
    /* v < s_last = 离开副本清零 → re-baseline, 不发事件 */
    s_last = v;
}

/* ★v13.22 poll_cc_old 已删除(报告 36): 条件码 +0x42C/+0x1434 是「动作/状态
 * 汇总」字段而非「状态身份」——码 8 实测=玩家攻击动作(每刀触发未命中也触发,
 * 1007 条 [cc] 日志铁证), v13.21 据此误判导致攻击震 0x20 通道, 已回退。
 * CC 判定改走 ActiveStatus 类型字段(vib_dispatch_dot_active 扩类型集
 * {1=冰冻,3=眩晕,7=石化,8=睡眠}, stub10 已实测工作的调度器)。
 * 条件码消费侧定案留档: 码 9=硬控共码(sub_48A460 case9 → CONDITION_STUN,
 * 眩晕/冰冻/石化 tick 共写), 码 8=攻击动作, 3/4/5=受击反应, 6/11/22=其他动作。 */

/* ============================================================
 * ★v13.18 暴击数字对象无损轮询探针(sa_crit_poll_spec_result 定案)
 * 纯读枚举链(每步一次 DWORD 读, 全静态已证实):
 *   mgr = *(0xB5DCE0) → ctx = *(mgr+0x200010) → layer = *(ctx+0xB0)
 *   → 主 vector [ *(layer+0x10), *(layer+0x14) )
 * 元素过滤: vftable==0x99D21C(CNRDNumberObject 唯一) 且 +0x1AC 非0非-1
 * 合法堆指针 = 一枚活的暴击数字(构造清0@0x470BE8 / 仅暴击写@0x470EA8 /
 * 析构清0 / 432B 池复用无残留 —— 不变量已证)。
 * 沿检测: 活暴击数 0→>0 上升沿触发一次, 锁存到归零(数字数百 ms 存续期
 * 内只触发一次); 默认 LOG_ONLY 只打 [crit] 标定日志(T1~T11 清单见规格),
 * 实机标定通过后 VIB_CRIT_POLL_MODE 改 2 才发事件(FONT_SPECIAL 0x10)。
 * 安全读: VirtualQuery 预检单层(GCC 无 MSVC __try; 规格明示 6ms 轮询足够)。
 * 离线自门控: mgr 无效即短路(1 次读开销); FAIL 路径 30s 一条状态日志(T1 标定)。
 * ============================================================ */
#define VIB_CRIT_POLL_ENABLE     1   /* 总开关: 0=探针不编译进轮询循环 */
#define VIB_CRIT_POLL_MODE       1   /* 1=LOG_ONLY(默认/出厂) 2=EVENTS(标定后手动改) */
#define VIB_CRIT_POLL_MAX_ELEMS  1024
#define VIB_CRIT_POLL_SELF_ONLY  0   /* 1=与玩家HP下降沿同窗±150ms时抑制(实验, 默认关) */
#define VIB_CRIT_POLL_RETRIGGER  0   /* 1=锁存期新增暴击补发轻震(连暴增强, 默认关) */
#define VIB_CRIT_POLL_LOG_MS     5000UL  /* 链通状态摘要节流 */
#define VIB_CRIT_FAIL_LOG_MS     30000UL /* 链断状态摘要节流(T1 标定) */

/* 合法用户态堆指针判据(规格[3.1], 纯数值零异常):
 * 非0非-1、4对齐、[0x10000,0x7FFF0000)、排除模块映像区(池块必在堆) */
static int vib_ptr_ok(DWORD p)
{
    if (p == 0 || p == 0xFFFFFFFFUL) return 0;
    if (p & 3) return 0;
    if (p < 0x00010000UL || p >= 0x7FFF0000UL) return 0;
    if (p >= VIB_IMAGE_LO && p < VIB_IMAGE_HI) return 0;   /* 模块映像区(版本剖面) */
    return 1;
}

/* VirtualQuery 页预检(不触碰页、零异常、TerSafe 无感; 规格[3.2]第一层) */
static int vib_page_ok(DWORD addr)
{
    MEMORY_BASIC_INFORMATION mbi;
    if (VirtualQuery((LPCVOID)addr, &mbi, sizeof(mbi)) != sizeof(mbi)) return 0;
    if (mbi.State != MEM_COMMIT) return 0;
    if (mbi.Protect & (PAGE_NOACCESS | PAGE_GUARD)) return 0;
    return 1;
}

/* 预检+读: 失败置 *ok=0 返回 0(单层防护, GCC 无 __try)。
 * ★只对地址做页可读性预检 —— 不做 vib_ptr_ok: 首跳读的全局(B5DCE0 等)
 * 本身位于模块映像区(vib_ptr_ok 会判 0 → 探针被编译器判死代码整段消除,
 * 实测踩雷); 读出的【值】是否合法堆指针由调用方 vib_ptr_ok(值) 判定。 */
static DWORD vib_sread(DWORD addr, int *ok)
{
    *ok = vib_page_ok(addr);
    if (!*ok) return 0;
    return *(volatile DWORD *)addr;
}

void poll_crit_number_old(void)
{
#if !VIB_CRIT_POLL_ENABLE
    (void)0;
#else
    static DWORD s_critCnt = 0;    /* 上一轮活暴击数(沿检测状态) */
    static int   s_locked = 0;     /* 沿去抖锁: 一批暴击只触发一次 */
    static DWORD s_logTick = 0;    /* 链通摘要节流 */
    static DWORD s_failTick = 0;   /* 链断摘要节流 */
    DWORD mgr, ctx, layer, begin, end, p, vt, crit;
    DWORD i, cnt = 0, digits = 0, crits = 0;
    int ok;

    /* S1~S3: 三跳指针链(离线时第一跳即失败, 本函数退化为 1 次读开销) */
    /* ★版本剖面: ACT5 战斗核心/数字对象 vftable 未定位 → 探针整体跳过 */
    if (!VIB_ANCHOR_IS_SET(OLD_BATTLE_CORE) || !VIB_ANCHOR_IS_SET(OLD_NUMOBJ_VFTABLE)) return;
    mgr = vib_sread(OLD_BATTLE_CORE, &ok);
    if (!ok || !vib_ptr_ok(mgr)) {
        /* FAIL: 链断(城镇/加载中/撕裂) → 复位沿状态, 30s 一条 T1 标定日志 */
        DWORD rawMgr = 0;
        DWORD now = GetTickCount();
        s_critCnt = 0; s_locked = 0;
        if (vib_page_ok(OLD_BATTLE_CORE)) rawMgr = *(volatile DWORD *)OLD_BATTLE_CORE;
        if (now - s_failTick >= VIB_CRIT_FAIL_LOG_MS) {
            s_failTick = now;
            dll_log2("[crit] chain-off mgrRaw=%08X", rawMgr);
        }
        return;
    }
    ctx = vib_sread(mgr + OLD_BATTLE_CORE_CTX_OFF, &ok);   if (!ok || !vib_ptr_ok(ctx)) goto FAIL;
    layer = vib_sread(ctx + OLD_CTX_LAYERMGR_OFF, &ok);    if (!ok || !vib_ptr_ok(layer)) goto FAIL;

    /* S4~S6: 向量边界(规格 S4/S5: begin/end 为 0 允许=空表; 非 0 须合法堆指针) */
    begin = vib_sread(layer + OLD_LAYER_VEC_BEGIN_OFF, &ok); if (!ok) goto FAIL;
    end   = vib_sread(layer + OLD_LAYER_VEC_END_OFF, &ok);   if (!ok) goto FAIL;
    if (begin && !vib_ptr_ok(begin)) goto FAIL;
    if (end && !vib_ptr_ok(end)) goto FAIL;
    if (end < begin || ((end - begin) & 3)) goto FAIL;
    cnt = (end - begin) >> 2;
    if (cnt > VIB_CRIT_POLL_MAX_ELEMS) goto FAIL;   /* 撕裂快照, 整轮放弃 */
    if (begin == 0 && end == 0) goto EDGE;          /* 空表 */

    /* S7~S10: 逐元素扫描(全程不调用游戏函数、不写内存) */
    for (i = 0; i < cnt; i++) {
        p = vib_sread(begin + 4 * i, &ok);
        if (!ok || !vib_ptr_ok(p)) continue;
        vt = vib_sread(p, &ok);
        if (!ok || vt != OLD_NUMOBJ_VFTABLE) continue;
        digits++;                                   /* 一枚活数字对象 */
        crit = vib_sread(p + OLD_NUMOBJ_CRIT_OFF, &ok);
        if (!ok || crit == 0 || crit == 0xFFFFFFFFUL || !vib_ptr_ok(crit)) continue;
        crits++;                                    /* ★一枚活暴击数字 */
    }
    goto EDGE;

FAIL:
    s_critCnt = 0; s_locked = 0;
    return;

EDGE:
    /* 沿检测+去抖: 上升沿(0→>0 且未锁存)触发一次并上锁; 归零解锁 */
    if (!s_locked && s_critCnt == 0 && crits > 0) {
#if VIB_CRIT_POLL_SELF_ONLY
        /* 与玩家掉血同窗 ±150ms → 视为"自己被怪暴击"(受击 0x02 已同帧发), 抑制 */
        if (g_lastPlayerHitTick && GetTickCount() - g_lastPlayerHitTick <= 150) {
            dll_log2("[crit] suppressed(self-hit) crits=%u", crits);
        } else
#endif
        {
#if VIB_CRIT_POLL_MODE == 2
            vib_collect(VEV_FONT, FONT_FLAG_SPECIAL, GetTickCount(), 1);
            dll_log2("[crit] EDGE fire crits=%u digits=%u", crits, digits);
#else
            dll_log2("[crit] EDGE (log-only) crits=%u digits=%u", crits, digits);
#endif
        }
        s_locked = 1;
    }
#if VIB_CRIT_POLL_RETRIGGER
    else if (s_locked && crits > 0 && crits > s_critCnt) {
#if VIB_CRIT_POLL_MODE == 2
        vib_collect(VEV_FONT, FONT_FLAG_SPECIAL, GetTickCount(), 1);
#endif
        dll_log2("[crit] RETRIGGER +%u", crits - s_critCnt);
    }
#endif
    if (crits == 0) s_locked = 0;
    s_critCnt = crits;

    /* 链通状态摘要(标定 T2~T6/T11 用), 5s 一条 */
    {
        DWORD now = GetTickCount();
        if (now - s_logTick >= VIB_CRIT_POLL_LOG_MS) {
            s_logTick = now;
            dll_log2("[crit] poll mgr=%08X ctx=%08X layer=%08X cnt=%u digits=%u crits=%u",
                     mgr, ctx, layer, cnt, digits, crits);
        }
    }
#endif
}
