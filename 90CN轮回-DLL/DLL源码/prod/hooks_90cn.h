#ifndef VIB_HOOKS_90CN_H
#define VIB_HOOKS_90CN_H
/* ============================================================
 * 90CN 生产版 hook 引擎接口 (四轮探针定稿架构)
 *   - insn_len 完整版 (探针 v1.3, 未知 opcode 拒绝安装)
 *   - 写入/还原走 VirtualProtect (CN 代码页 RX, 与 US RWX 不同)
 *   - FONT hook = 唯一采集 hook (a6 位分类, 四轮实证)
 *   - 镜头震动 4 hook = 预留接口, CN_SHAKE_ENABLE=0 默认不安装
 * ============================================================ */
#include <windows.h>

#define VIB_MAX_HOOKS 8

/* 安装 hook: targets 为目标地址数组, 与 hooks_90cn_asm.s 的
 * stub 表一一对应 (顺序固定: 0=FONT, 1..4=震动接口)。
 * 返回成功安装的数量。追加式, 可多次调用。 */
int  hooks_install(const DWORD *targets, int count);

/* 全部卸载 (VirtualProtect 还原原始字节) */
void hooks_uninstall(void);

/* 是否已安装 */
int  hooks_active(void);

/* ---------------- FONT 采集 (stub_0, 探针四轮验证的 a6 提取) ----------------
 * damage_font(this, a2..a11) thiscall, a6=类型标志位:
 *   0x01 命中 0x02 受击 0x04 特效 0x08 状态 0x10 特殊 0x20 DOT
 * 游戏线程上下文: 只做原子计数, 冲刷由 collector 线程执行 */
void __cdecl vib_dispatch_font_hit(void *obj, DWORD a2, DWORD a3, DWORD a4, DWORD a5,
                                   DWORD a6, int type);

/* ---------------- 镜头震动预留接口 (stub_1..4, 默认不安装) ----------------
 * CN 对应函数已静态定位 (vib_90cn_addrs.h CN_SHAKE_*) 但未实测,
 * 定位确认后: 填地址 -> CN_SHAKE_ENABLE 改 1 -> 重编译即生效。
 * 参数布局按 US 同源函数签名预留, 必要时只调 asm stub 取参偏移。 */

/* 通用汇聚点 (US sub_1E25540 同源): retaddr 区分 技能/暴击特写 */
void __cdecl vib_dispatch_skillhit(void *obj, DWORD strength_bits,
                                   DWORD time_ms, DWORD retaddr, int type);
/* 纯镜头震动 [shake screen] (US sub_1E268F0 同源) */
void __cdecl vib_dispatch_shake(void *obj, DWORD strength_bits,
                                DWORD time_ms, int type);
/* 读条震屏 (US sub_1E25760 同源): flag 低字节==1 为开始 */
void __cdecl vib_dispatch_readshake(void *obj, DWORD flag_bits,
                                    DWORD strength_bits, int type);
/* 直接震屏 (US sub_1E26F00 同源): 目标容器空时直接写相机字段 */
void __cdecl vib_dispatch_directshake(void *obj, DWORD strength_bits, int type);

/* v1.5 震屏词条执行器 (stub_6 ← CN 0x03349740 = 技能表 [shake screen] 词条, 引擎名
 * "onShakeInput"): 只有带该词条的技能才触发 -> VEV_KILL "释放技能" 通道
 * (对齐老版 ACT4/ACT5 的镜头震动方案), 天然的技能级震屏区分 */
void __cdecl vib_dispatch_shake_entry(void *obj, DWORD arg, int type);

/* 怪物死亡 (stub_7, ⚠ 当前无 hook 目标 —— v2.3 起不再挂任何地址)
 * 历史候选 0x02536B60 (v1.9) / 0x024A18D0 (v2.0) / 0x02CC8220 (v2.2) 全部实测零触发, 已弃。
 * ★0915 校对: 对齐方向应为 90US 的 hook sub_F20BF0 (0xF20BF0, 被击者 HP<=0 确认死亡后
 *   由受击处理 sub_1C58AC0 调用; US stub 11)。CN 同源函数待定位后再接上本 stub。
 * -> VEV_TARGET_DIE -> 宿主【怪物死亡】滑块 idx14 */
void __cdecl vib_dispatch_target_die(void *obj, DWORD hp_lo, DWORD hp_hi, int type);

/* v2.8: 通用死亡候选探针 (stub 2..6) —— 只计数+记录, 不发送 */
void __cdecl vib_dispatch_probe(void *obj, int type);

/* v3.0: 通知/命令分发器 case 号直方图探针 */
void __cdecl vib_dispatch_notify(DWORD idx, int type);

/* v5.0: 击杀记录区变化检测 (collector 调用) */
void poll_death_block(void);

/* v2.8: 飘字分发器 0x013EEFF0 参数诊断 (stub 7) —— ecx + 6 个栈参 */
void __cdecl vib_dispatch_diag7(DWORD ecx_v, DWORD a1, DWORD a2, DWORD a3,
                                DWORD a4, DWORD a5, DWORD a6, int type);

/* v2.2: 死亡 hook 是否近期触发 (poll_kill 互斥用, 500ms 窗内) */
int vib_die_hook_recent(void);

/* 震屏字段轮询 (v1.3 新增, collector 6ms 调用): [converge this + 0x790]
 * 上升沿 (0->非0) = 震屏开始 -> VEV_CRIT_SHAKE(强度100/时长500ms)
 * 探针 v1.5 两轮对照实证: 轮B(震屏技能) 11 次交替 / 轮A(不震屏) 零变化 */
void vib_poll_shake_ctl(void);

/* ---------------- 轮询采集 (collector 线程调用, 零 hook 只读) ---------------- */
/* FONT 聚合计数冲刷 (每 6ms): 6 通道增量 -> VEV_FONT */
void vib_flush_counts(void);

/* 评分等级轮询 (内部 500ms 节流): FN_SCORE_DMG/FN_RANK_LEVEL 调用 -> VEV_RANKING */
void rank_poll(void);

/* 击杀/评分点/破甲凌空/移动 轮询 (每次调用即执行) */
void vib_flush_rank_extra(void);

/* 供汇编引用的每-hook trampoline 地址 */
extern void *vib_tramp_0;
extern void *vib_tramp_1;
extern void *vib_tramp_2;
extern void *vib_tramp_3;
extern void *vib_tramp_4;
extern void *vib_tramp_5;

#endif /* VIB_HOOKS_90CN_H */
