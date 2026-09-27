#ifndef VIB_TARGET_OLD_H
#define VIB_TARGET_OLD_H
/* ============================================================
 * 版本锚点深模块（唯一版本剖面）—— ACT1 / ACT5
 * ============================================================
 * 目的：把所有「随客户端版本变化」的地址 / 偏移 / 状态名指针 / hookLen
 *       集中到本文件。引擎(hooks_old.c)、采集(dfovib_old.c)、协议
 *       (vib_protocol_old.h) 只引用本模块符号，不再出现版本字面量。
 *       换版本 = 改本文件（或加一段 #elif），其余代码零改动。
 *
 * 关键约定（防崩游戏，vib-mod 铁律）：
 *   1. VIB_ANCHOR_UNRESOLVED(0) = 该版本尚未定位。
 *      安装/轮询遇到 0 必须【显式跳过并记日志】，绝不静默安装错地址
 *      （错地址 = E9 改写无关代码 = 游戏崩溃，与教训 1 同源）。
 *   2. 每个 hook 锚点带 8 字节期望签名 + 有效长度；安装前 memcmp，
 *      不匹配则拒绝安装（把「错地址=崩溃」降级为「错地址=拒绝启动」）。
 *   3. 【已证实】标注者方可用于事件判定；【未定位】一律 UNRESOLVED。
 *
 * 剖面切换：编译期 -DVIB_TARGET=1(ACT1, 默认) / =5(ACT5)
 *   构建脚本：build_old.bat(ACT1) / build_act5.bat(ACT5)
 * ============================================================ */

#define VIB_TARGET_ACT1   1
#define VIB_TARGET_ACT5   5
#define VIB_TARGET_60US   60
#ifndef VIB_TARGET
#define VIB_TARGET        VIB_TARGET_ACT1   /* 默认保持 ACT1 现网行为不变 */
#endif

#define VIB_ANCHOR_UNRESOLVED  0UL
#define VIB_ANCHOR_IS_SET(x)   ((x) != VIB_ANCHOR_UNRESOLVED)
#define VIB_HOOK_COUNT         13           /* 数组容量必须与锚点表一致（教训 12） */

/* ================================================================
 * 剖面：ACT1（LMA1S-release, 2008-06-17, ImageBase 0x400000）
 * 全部已定案（见 03/07 号文档 + 无损对接交接文档第五节）
 * ================================================================ */
#if VIB_TARGET == VIB_TARGET_ACT1

#define VIB_TARGET_NAME   "ACT1"
#define VIB_PROBE_COUNT   0                      /* 无定位探针(仅 60US 用) */
#define VIB_STR_WIDE      0                       /* 状态名 = ASCII */
#define VIB_IMAGE_LO      0x00401000UL            /* 模块映像区下界(vib_ptr_ok 排除堆) */
#define VIB_IMAGE_HI      0x00F3C000UL            /* 模块映像区上界 */
#define VIB_HIT_MERGE_MS  10                      /* 命中(0x01)合并窗: ACT1 原值 10ms（不变） */

/* ---- hook 锚点（顺序 = stub 索引, 不可乱序） ---- */
#define TGT_HOOK_SENDER      0x00433890UL   /* 统一事件发送器 cdecl(name,a2,a3,a4) 775→715 calls */
#define TGT_HOOK_BROADCAST   0x004E6DA0UL   /* 播报层 a2:2连击 3被击 5击杀 6STYLE 7TECHNIC */
#define TGT_HOOK_ACCUM       0x004D2C50UL   /* 加分汇聚 48 槽 */
#define TGT_HOOK_VOICE       0x004E8520UL   /* 技巧语音层 HK 槽 */
#define TGT_HOOK_RESULT      0x004E3B40UL   /* 结算震动实体 */
#define TGT_HOOK_FINALE      0x004BCA70UL   /* 地城阶段结算 R_STAGE_FINALE 发送者 */
#define TGT_HOOK_ATHIT       0x004748D0UL   /* 扣血结算 */
#define TGT_HOOK_COMBOUI     0x0042DCB0UL   /* 战斗 UI 加载器 Combo.img */
#define TGT_HOOK_DMGFONT     0x00470ED0UL   /* 伤害数字咽喉 = 命中唯一源 */
#define TGT_STUB_DMGFONT     vib_stub_8     /* ACT1 cdecl 5 参布局 */
#define TGT_HOOK_NOTIFY38    0x004217CEUL   /* 通知38 MonsterDie 直钩 */
#define TGT_HOOK_STATUSTICK  0x004D10F0UL   /* 状态 tick 调度器 (DOT/CC) */
#define TGT_HOOK_CRITEXIT    0x00790D93UL   /* 暴击权威出口(E8) — 永久跳过不安装 */
#define TGT_HOOK_CAMSHAKE    0x004E49E0UL   /* 镜头震动汇聚点 68 xref */

/* hookLen = 覆盖 ≥5 字节的指令边界（capstone/IDA 逐条确认） */
#define TGT_HOOKLEN_SENDER     6
#define TGT_HOOKLEN_BROADCAST  6
#define TGT_HOOKLEN_ACCUM      6
#define TGT_HOOKLEN_VOICE      5
#define TGT_HOOKLEN_RESULT     9
#define TGT_HOOKLEN_FINALE     6
#define TGT_HOOKLEN_ATHIT      9
#define TGT_HOOKLEN_COMBOUI    5
#define TGT_HOOKLEN_DMGFONT    5
#define TGT_HOOKLEN_NOTIFY38   7
#define TGT_HOOKLEN_STATUSTICK 5
#define TGT_HOOKLEN_CRITEXIT   5
#define TGT_HOOKLEN_CAMSHAKE   6

/* hook 签名（原 exe 头部 8 字节, 安装前校验；取自 E:\LX\DNF.exe） */
#define TGT_SIG_SENDER     { 0x55,0x8B,0xEC,0x8B,0x45,0x08,0x56,0x57 }
#define TGT_SIG_BROADCAST  { 0x55,0x8B,0xEC,0x83,0xEC,0x0C,0x8B,0x45 }
#define TGT_SIG_ACCUM      { 0x55,0x8B,0xEC,0x56,0x8B,0xF1,0x8B,0x0D }
#define TGT_SIG_VOICE      { 0x55,0x8B,0xEC,0x6A,0xFF,0x68,0x10,0x36 }
#define TGT_SIG_RESULT     { 0x56,0x8B,0xF1,0x8B,0x86,0x34,0x09,0x00 }
#define TGT_SIG_FINALE     { 0x55,0x8B,0xEC,0x56,0x8B,0xF1,0x8B,0x06 }
#define TGT_SIG_ATHIT      { 0x55,0x8B,0xEC,0x83,0xEC,0x10,0x56,0x8B }
#define TGT_SIG_COMBOUI    { 0x55,0x8B,0xEC,0x6A,0xFF,0x68,0x0B,0xB5 }
#define TGT_SIG_DMGFONT    { 0x55,0x8B,0xEC,0x6A,0xFF,0x68,0x7A,0xF2 }
#define TGT_SIG_NOTIFY38   { 0xC6,0x85,0x8A,0xE4,0xFF,0xFF,0x00,0xC6 }
#define TGT_SIG_STATUSTICK { 0x55,0x8B,0xEC,0x6A,0xFF,0x68,0x3C,0x28 }
#define TGT_SIG_CRITEXIT   { 0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00 } /* 不安装, 不校验 */
#define TGT_SIG_CAMSHAKE   { 0x55,0x8B,0xEC,0x8B,0x45,0x0C,0x53,0x8B }

/* ---- 状态名（exe 内明文 ASCII 地址） ---- */
#define TGT_STR_VICTORY_UP_LOOP      0x00A84350UL
#define TGT_STR_DEFEAT_DOWN_LOOP     0x00A8433CUL
#define TGT_STR_R_STAGE_FINALE       0x0099FBD0UL
#define TGT_STR_RESULT_SCORE_UP_LOOP 0x00A844ECUL
#define TGT_STR_RESULT_COME          0x00A84330UL
#define TGT_STR_RANK_UP              0x00A84360UL
#define TGT_STR_R_ALL_KILL           0x00999894UL
#define TGT_STR_R_FAINT              0x009998A0UL
#define TGT_STR_R_HK2_3GOOD          0x009A1738UL
#define TGT_STR_R_HK2_2GOOD          0x009A1744UL
#define TGT_STR_R_HK2_1GOOD          0x009A1750UL

/* ---- 全局对象 / 偏移 ---- */
#define TGT_PLAYER_OBJ           0x00B75A14UL  /* 当前玩家对象指针(副本态) */
#define TGT_TOWN_OBJ             0x00B75A18UL  /* 城镇态玩家对象指针(v13.38 双源) */
#define TGT_PLAYER_XOFF          0x13CUL       /* 坐标 x (float) */
#define TGT_PLAYER_YOFF          0x140UL       /* 坐标 y (float) */
#define TGT_PLAYER_OWNER_OFF     0x30UL        /* 角色对象归属偏移 */
#define TGT_PLAYER_HP_SLOT_OFF   0x1168UL      /* HP 加密槽(标记) idx@+4 */
#define TGT_PLAYER_MP_SLOT_OFF   0x1170UL      /* MP 加密槽 */
#define TGT_PLAYER_FRENZY_OFF    0x2A14UL      /* CNFrenzy 指针(0=未激活) */
#define TGT_FRENZY_VT            0x00A00754UL  /* CNFrenzy vftable 校验值 */
#define TGT_HIT_COUNT            0x00B5ED28UL  /* 被击计数器(受击主源) */
#define TGT_DAMAGE_TOTAL         0x00B5ED24UL  /* 累计受伤值(实测恒0, 只日志) */
#define TGT_SCORE_OBJ            0x00B5B410UL  /* 评分对象全局指针 */
#define TGT_SCORE_TOTAL_OFF      932           /* +932 总评(加密槽) */
#define TGT_MONSTER_REMAIN       0x00B6F608UL  /* (已证伪, 停用) */
#define TGT_BATTLE_CORE          0x00B5DCE0UL  /* 战斗核心单例 */
#define TGT_BATTLE_CORE_CTX_OFF  0x200010UL
#define TGT_CTX_LAYERMGR_OFF     0x000000B0UL
#define TGT_LAYER_VEC_BEGIN_OFF  0x00000010UL
#define TGT_LAYER_VEC_END_OFF    0x00000014UL
#define TGT_NUMOBJ_VFTABLE       0x0099D21CUL  /* CNRDNumberObject 唯一 vftable */
#define TGT_NUMOBJ_CRIT_OFF      0x000001ACUL
#define TGT_NUMOBJ_TYPE_OFF      0x000000C8UL
#define TGT_NUMOBJ_OWNER_OFF     0x000000E4UL
#define TGT_NUMOBJ_LIVE_CNT      0x00B5EFD4UL
#define TGT_ENC_TABLE_BASE       0x00CFD458UL  /* 加密表 ctx 全局(值=ctx) */
#define TGT_ENC_KEY_ADDR         0x00CFD45CUL  /* 运行时密钥全局地址(取值为 key) */
#define TGT_FIELD_DECODE         0x00402030UL  /* 槽解算器(dword) */
#define TGT_PLAYER_HIT_ENTRY     0x004A1B50UL
#define TGT_PLAYER_HIT_PUSH      0x004847BEUL

/* ================================================================
 * 剖面：ACT5（DNFACT5.exe, 2010, ImageBase 0x400000, Themida nsn 段）
 * 证据见 work/ida/out_01..07.txt + work/anchor_map.py 输出
 * ⚠ 字符串为 UTF-16 宽字符（VIB_STR_WIDE=1），与 ACT1 ASCII 不同
 * ================================================================ */
#elif VIB_TARGET == VIB_TARGET_ACT5

#define VIB_TARGET_NAME   "ACT5"
#define VIB_PROBE_COUNT   0                      /* 无定位探针(仅 60US 用) */
#define VIB_STR_WIDE      1                       /* 状态名 = UTF-16LE */
#define VIB_IMAGE_LO      0x00401000UL
#define VIB_IMAGE_HI      0x0227E000UL            /* .text..nsn..vmp0 末 (0x227e000) */
/* ★命中(0x01)合并窗 20ms（2026-09-13，移植 ACT4 定案）：
 *   后期"疯狂震动"的真源头 = 群怪同帧多目标（同一次命中的多条数字同帧产生）。
 *   实测：同帧群怪 Δ=0ms（被合并）、同目标连段最小 31ms（不被吞）→ 20ms 落在 [0,31ms)
 *   内，是同时满足"合并群怪"与"保留连段"的取值。ACT1 原值 10ms 保留不变。
 *   诊断：DLL 日志 tick 行 hm= 即被本窗吞掉的 0x01 条数，font a= 是实发条数。 */
#define VIB_HIT_MERGE_MS  35                      /* 命中(0x01)合并窗: 20→25→35ms(用户试感, 2026-09-13) */
#define VIB_HIT_PROBE     0                       /* 0=只计数(定稿) / 1=逐条打印全部 [h] / 2=只打印重复(DUP/REV)行 */

/* ---- hook 锚点 ---- */
/* [0] 统一事件发送器：与 ACT1 sub_433890 同构，cdecl(wchar_t* name,a2,a3,a4)
 *     775 处 call xref；入口 55 8B EC 56 8B 75 08（普通指令，可 hook）。
 *     【已证实】由 VICTORY_UP_LOOP/DEFEAT_DOWN_LOOP/RESULT_COME/RANK_UP/
 *     RESULT_CHARACTER/COUNT_DOWN 的宽串 xref 全部汇入本函数确认。 */
#define TGT_HOOK_SENDER      0x0044D460UL
/* [1..12] 其余 11 个锚点【未定位】—— 必须保持 UNRESOLVED，安装时显式跳过。
 *   待实机探针/后续会话按 07 号文档《换版本适配速查》逐项定位后填入：
 *     BROADCAST/ACCUM/VOICE/RESULT/FINALE/ATHIT/COMBOUI/DMGFONT/
 *     NOTIFY38/STATUSTICK/CAMSHAKE */
/* [8] 伤害数字生成咽喉【已证实】—— 命中/受击/DOT/回复/暴击的主源。
 *   证据：sub_4ACC00 尾 `sub_4AC7B0(sub_4AC690(480), a5, a6)` → ctor(this,值,标志)；
 *         ctor 按 a6 位 0x01/0x02/0x04/0x08 选数字精灵、0x10=特殊（与 ACT1 标志同构）；
 *         12 个调用点（含疑似 DOT 三兄弟 sub_532050/5324B0/532C70）；入口 55 8B EC 6A FF。
 *   ACT5 cdecl 比 ACT1 多一个前导参 → 用 stub13（偏移 +4），复用同一 C 分派。 */
#define TGT_HOOK_DMGFONT     0x004ACC00UL
#define TGT_STUB_DMGFONT     vib_stub_13
/* [9] 通知38 = 怪物死亡批量确认【已证实】(2026-09-12)
 *   证据：通知分发器 sub_4266FA 的 case 38 块 loc_431386：
 *     sub_526590(529,id) → __RTDynamicCast(...,&IRDMonster,...)（失败再试 IRDAICharacter）
 *     → 按数量字节循环调 vtable+2028 —— 与 ACT1 notify38 结构一致，协议号沿用。
 *   入口 8D 8D 00 FE FF FF (lea ecx,[ebp-200h])，hookLen=6；
 *   该块由跳转表进入，[esp] 非返回地址 → 参数不可用，stub9 全忽略参数(与 ACT1 同)。 */
#define TGT_HOOK_NOTIFY38    0x00431386UL
/* [1] 播报层【已证实】(2026-09-12)：sub_555D60 __thiscall(this,a2,a3)，
 *   case a2==7 → "HK_TECHNIC"，a2==6 → "HK_STYLE"（与 ACT1 的 6=STYLE/7=TECHNIC 同码），
 *   入口 55 8B EC 83 EC 0C（hookLen=6）。→ 评分震动 = VEV_KILLPOINT「评分点」。
 *   (a2==3 在本层是评分累加 this[200]+=a3；a2 语义与 ACT1 不同, 故仅映射 6/7。) */
#define TGT_HOOK_BROADCAST   0x00555D60UL
#define TGT_HOOK_ACCUM       VIB_ANCHOR_UNRESOLVED
#define TGT_HOOK_VOICE       VIB_ANCHOR_UNRESOLVED
#define TGT_HOOK_RESULT      VIB_ANCHOR_UNRESOLVED
#define TGT_HOOK_FINALE      VIB_ANCHOR_UNRESOLVED
#define TGT_HOOK_ATHIT       VIB_ANCHOR_UNRESOLVED
#define TGT_HOOK_COMBOUI     VIB_ANCHOR_UNRESOLVED
#define TGT_HOOK_STATUSTICK  VIB_ANCHOR_UNRESOLVED
#define TGT_HOOK_CRITEXIT    0x00790D93UL   /* 概念位, 永久不安装(ACT1 遗留) */
/* [12] 镜头震动汇聚点【已证实】(2026-09-12 实机测试后静态确认)
 *   sub_552CF0: __thiscall(this, a2, a3) —— 存 a2/a3 到 this[61]/[62] 且 a2==0 清
 *   this[75]/[76]（停震），与 ACT1 sub_4E49E0 结构完全同构；
 *   105 个调用点均来自技能类(0x56-0x66 段)，实参是 150/200/300/500/1000/2000/3000
 *   这类时长常量；入口 55 8B EC 8B 45 0C（与 ACT1 逐字节相同），hookLen=6。
 *   复用 stub12 → VEV_KILL(21) 宿主「释放技能」滑块。 */
#define TGT_HOOK_CAMSHAKE    0x00552CF0UL

#define TGT_HOOKLEN_SENDER     7
#define TGT_HOOKLEN_BROADCAST  6
#define TGT_HOOKLEN_ACCUM      0
#define TGT_HOOKLEN_VOICE      0
#define TGT_HOOKLEN_RESULT     0
#define TGT_HOOKLEN_FINALE     0
#define TGT_HOOKLEN_ATHIT      0
#define TGT_HOOKLEN_COMBOUI    0
#define TGT_HOOKLEN_DMGFONT    5
#define TGT_HOOKLEN_NOTIFY38   6
#define TGT_HOOKLEN_STATUSTICK 0
#define TGT_HOOKLEN_CRITEXIT   0
#define TGT_HOOKLEN_CAMSHAKE   6

/* 签名：仅已定位锚点填真值；全 0 = 未定位不校验（被 skip 也到不了校验） */
#define TGT_SIG_SENDER     { 0x55,0x8B,0xEC,0x56,0x8B,0x75,0x08,0x85 }
#define TGT_SIG_BROADCAST  { 0x55,0x8B,0xEC,0x83,0xEC,0x0C,0x8B,0x45 }
#define TGT_SIG_ACCUM      { 0,0,0,0,0,0,0,0 }
#define TGT_SIG_VOICE      { 0,0,0,0,0,0,0,0 }
#define TGT_SIG_RESULT     { 0,0,0,0,0,0,0,0 }
#define TGT_SIG_FINALE     { 0,0,0,0,0,0,0,0 }
#define TGT_SIG_ATHIT      { 0,0,0,0,0,0,0,0 }
#define TGT_SIG_COMBOUI    { 0,0,0,0,0,0,0,0 }
#define TGT_SIG_DMGFONT    { 0x55,0x8B,0xEC,0x6A,0xFF,0x68,0x27,0xBF }
#define TGT_SIG_NOTIFY38   { 0x8D,0x8D,0x00,0xFE,0xFF,0xFF,0x51,0x88 }
#define TGT_SIG_STATUSTICK { 0,0,0,0,0,0,0,0 }
#define TGT_SIG_CRITEXIT   { 0,0,0,0,0,0,0,0 }
#define TGT_SIG_CAMSHAKE   { 0x55,0x8B,0xEC,0x8B,0x45,0x0C,0x53,0x8B }

/* ---- 状态名（UTF-16LE 宽串地址；ACT1 为 ASCII 同内容） ---- */
/* 证据：work/anchor_map.py 的 dword 立即数引用点 + IDA XrefsTo */
#define TGT_STR_VICTORY_UP_LOOP      0x00E99390UL  /* ref @0xaa2cd9 sub_AA2900 */
#define TGT_STR_DEFEAT_DOWN_LOOP     0x00E9936CUL  /* ref @0xaa2cf9 sub_AA2900 */
#define TGT_STR_R_STAGE_FINALE       0x00D0FCECUL  /* ref @0x4b5472/0x4cb760/0x55f399 */
#define TGT_STR_RESULT_SCORE_UP_LOOP 0x00E994FCUL  /* 串存在(代码经表引用) */
#define TGT_STR_RESULT_COME          0x00E99354UL  /* ref @0xaa2818 */
#define TGT_STR_RANK_UP              0x00E993B0UL  /* ref @0xaa2cc6 */
#define TGT_STR_R_ALL_KILL           VIB_ANCHOR_UNRESOLVED  /* ACT5 无此串 */
#define TGT_STR_R_FAINT              VIB_ANCHOR_UNRESOLVED  /* ACT5 无此串 */
#define TGT_STR_R_HK2_3GOOD          VIB_ANCHOR_UNRESOLVED
#define TGT_STR_R_HK2_2GOOD          VIB_ANCHOR_UNRESOLVED
#define TGT_STR_R_HK2_1GOOD          VIB_ANCHOR_UNRESOLVED

/* ---- 全局对象 / 偏移 ---- */
/* 玩家对象全局 dword_10B2FDC 已被实机 [pos] 探针证实【正确】（对象上有随走位平滑
 * 变化的 float）。坐标字段实机标定：+0x16C=x / +0x170=y（+0x174=z）。
 * 注意 ACT5 比 ACT1 的 +0x13C/+0x140 整体 +0x30（结构变大）——[pos] 探针实测值
 * 600→1333→548 随走位变化。 */
#define TGT_PLAYER_OBJ           0x010B2FDCUL
/* 城镇玩家对象【候选】：dword_10B2FE0。依据 sub_576EA0 里两者成对管理
 * （`*(this+171)==dword_10B2FDC` / `*(this+170)==dword_10B2FE0`），
 * 对应 ACT1 的 B75A14(副本)/B75A18(城镇) 双源。坐标偏移先用同一 0x16C/0x170，
 * 由 `[mvB] obj=.. xy=(..)` 日志判定。 */
#define TGT_TOWN_OBJ             0x010B2FE0UL
#define TGT_PLAYER_XOFF          0x16CUL
#define TGT_PLAYER_YOFF          0x170UL
#define TGT_PLAYER_OWNER_OFF     VIB_ANCHOR_UNRESOLVED
#define TGT_PLAYER_HP_SLOT_OFF   VIB_ANCHOR_UNRESOLVED  /* ACT1 +0x1168 在 ACT5 已不存在 */
#define TGT_PLAYER_MP_SLOT_OFF   VIB_ANCHOR_UNRESOLVED
#define TGT_PLAYER_FRENZY_OFF    VIB_ANCHOR_UNRESOLVED
#define TGT_FRENZY_VT            0x00DCE954UL  /* CNFrenzy vftable (work/hunt1_out.txt) */
#define TGT_HIT_COUNT            VIB_ANCHOR_UNRESOLVED
#define TGT_DAMAGE_TOTAL         VIB_ANCHOR_UNRESOLVED
#define TGT_SCORE_OBJ            VIB_ANCHOR_UNRESOLVED
#define TGT_SCORE_TOTAL_OFF      0
#define TGT_MONSTER_REMAIN       VIB_ANCHOR_UNRESOLVED
#define TGT_BATTLE_CORE          VIB_ANCHOR_UNRESOLVED
#define TGT_BATTLE_CORE_CTX_OFF  0x200010UL
#define TGT_CTX_LAYERMGR_OFF     0x000000B0UL
#define TGT_LAYER_VEC_BEGIN_OFF  0x00000010UL
#define TGT_LAYER_VEC_END_OFF    0x00000014UL
#define TGT_NUMOBJ_VFTABLE       0x00D0F714UL  /* CNRDNumberObject vftable (work/hunt1_out.txt) */
#define TGT_NUMOBJ_CRIT_OFF      0
#define TGT_NUMOBJ_TYPE_OFF      0
#define TGT_NUMOBJ_OWNER_OFF     0
#define TGT_NUMOBJ_LIVE_CNT      VIB_ANCHOR_UNRESOLVED

/* ---- P0 加密四件套【已证实】 ---- */
/* sub_BDBC60: dword_127481C = 0xA1B2C3D4 ^ ((rand()<<16)|rand())  (key mix 全库唯一 @0xbdbca7)
 * sub_BDB660(0x400000): dword_1274818 = ctx; ctx+0=容量0x400000; ctx+0x44=表A; ctx+0x48=表B
 * sub_402100(this): v1=*(0x127481C); 校验 (v1 ^ slot[0]) - slotAddr == slot[1];
 *                   idx=slot[1]; 明文 = v1 ^ (baseA+4*idx) ^ *(baseA+4*idx) —— 与 ACT1 同构 */
#define TGT_ENC_TABLE_BASE       0x01274818UL  /* ctx 全局 */
#define TGT_ENC_KEY_ADDR         0x0127481CUL  /* 运行时密钥全局地址 */
#define TGT_FIELD_DECODE         0x00402100UL  /* 槽解算器 sub_402100 */
#define TGT_PLAYER_HIT_ENTRY     VIB_ANCHOR_UNRESOLVED
#define TGT_PLAYER_HIT_PUSH      VIB_ANCHOR_UNRESOLVED

/* ================================================================
 * 剖面：60US（DFO.exe 5.7MB Themida 壳 / 脱壳版 DFO_fixed_v3.exe 16.8MB,
 *        2010-07-15, ImageBase 0x400000, 两版 VA 布局一致 —— 静态分析基于
 *        脱壳版, 实机注入时由签名自校验把关）
 * 证据链：work/60us/p01..p34 + work/ida/out_ida_A..R.txt（交接文档 17 号）
 * ⚠ 字符串为 UTF-16LE 宽字符（VIB_STR_WIDE=1）
 * ⚠ 本剖面为【静态阶段】首轮：3 个 hook 已证实安装, 其余 UNRESOLVED
 *   （伤害数字/notify38/玩家对象/HP 槽未定位 —— 换新架构, 见交接文档）
 * ================================================================ */
#elif VIB_TARGET == VIB_TARGET_60US

#define VIB_PROFILE_ACT5_FAMILY 1                /* 宽串+同构分发家族(ACT5/60US) */
#define VIB_TARGET_NAME   "60US"
#define VIB_STR_WIDE      1                       /* 状态名 = UTF-16LE */
#define VIB_IMAGE_LO      0x00401000UL
#define VIB_IMAGE_HI      0x01414000UL            /* .text..rsrc..SCY 末 (0x1414000) */
#define VIB_HIT_MERGE_MS  35                      /* 命中(0x01)合并窗: 沿用 ACT5 定案 */
#define VIB_HIT_PROBE     0                       /* 0=只计数(定稿) / 1=逐条 / 2=只打重复 */

/* ---- hook 锚点 ---- */
/* [0] 统一事件发送器【已证实】：sub_4479B0 cdecl(wchar_t* name, a2, a3, a4)，
 *   与 ACT5 sub_44D460 同构。证据：VICTORY_UP_LOOP@0x9f32e7 / DEFEAT_DOWN_LOOP /
 *   RESULT_COME / RANK_UP 等 41 个状态宽串的 push 点全部 call 本函数；
 *   调用形态 push 0; push 0; push -1; push offset str; call 0x4479B0。
 *   入口 55 8B EC 56 8B 75 08（push ebp/mov ebp,esp/push esi/mov esi,[ebp+8]
 *   =7 字节, hookLen=7 落在指令边界）。复用 stub0。 */
#define TGT_HOOK_SENDER      0x004479B0UL
/* [1] 播报层【已证实】：sub_53BE70 __thiscall(this, a2, a3, ...)，
 *   内部 cmp eax,7 → "HK_TECHNIC"@0xcdc330、cmp eax,6 → "HK_STYLE"@0xcdc31c
 *   （与 ACT1/ACT5 的 6=STYLE / 7=TECHNIC 同码）→「评分点」VEV_KILLPOINT。
 *   入口 55 8B EC 83 EC 10（hookLen=6）。复用 stub1。 */
#define TGT_HOOK_BROADCAST   0x0053BE70UL
/* [12] 镜头震动汇聚点【已证实】：sub_538900 __thiscall(this, a2, a3) ——
 *   写 a2/a3 到 this+0x11C/+0x120, a2==0 时清 this+0x164/+0x168（停震），
 *   与 ACT1 sub_4E49E0 / ACT5 sub_552CF0 结构同构；101 个调用点全来自技能类，
 *   实参为 150/200/300/500/1000/2000/3000 时长常量。入口 55 8B EC 8B 45 0C
 *   （与 ACT1 逐字节相同, hookLen=6）。复用 stub12 → VEV_KILL(21)「释放技能」。 */
#define TGT_HOOK_CAMSHAKE    0x00538900UL
/* [8] 伤害数字咽喉【★已定案 2026-09-24】：sub_498980 = 伤害数字生成器
 *   __thiscall(this, a1..a3 未知, a4=宿主对象, a5=伤害) —— 函数内 0x498CC3
 *   `mov ecx, eax` 把标志位作为第 6 栈参传给生成器内的 ctor 调用(0x498CC5)。
 *   证据(实机 67 事件分段定案, 交接文档 17 号六N):
 *     flags=0x01 打怪 / 0x11 暴击(bit4) / 0x02 受击(bit1) / 0x21 怪物DOT(bit5)
 *     ★bit0 是通用分类位(三条通道都带), 不可单独当命中判据 —— 必须按
 *       bit1(受击)/bit5(DOT)/bit4(暴击) 顺序预先分流。
 *   挂钩位置选型: 函数入口(非相对指令, 不依赖 rel32 修位 —— v5.1 曾在此处踩坑)。
 *   入口 55 8B EC 6A FF 68 A5 69（hookLen=10 落在指令边界）。
 *   stub14 复用 ACT5 stub13 的压序(thiscall + 5 栈参, 布局一致)。 */
#define TGT_HOOK_DMGFONT     0x00498980UL
#define TGT_STUB_DMGFONT     vib_stub_14
#define TGT_PLAYER_OBJ_HINT  0x08AD0000UL  /* 受击宿主实测值(运行期对象, 非静态地址,
                                            * 仅作独立确认时的线索, 勿直接当全局用) */
/* [9] 通知38 怪物死亡【★已定案 2026-09-24】：0x4290D9 = noti 分发器 sub_420230
 *   跳表 jpt_42029C 的 case38 目标（服务端源码 Net/ChannelPackets.cs 里
 *   "noti 38 DIE_MONSTER @0x4290d9 完整读法" 与之一字不差 —— 双源互证）。
 *   结构与 ACT5 noti38(0x431386) 完全同构:
 *     movzx eax,[ebp-0x1DC](怪物线号) → push 0x211 → call 0x50b820(实体查找)
 *     → __RTDynamicCast(0xC00D0B) 带 TD 0xF42928=".?AVIRDMonster@@" (失败再试
 *       0xF42908=".?AVIRDAICharacter@@") → 按数量字节循环调 vtable+0x2028。
 *   入口 8D 95 24 FE FF FF（lea edx,[ebp-1DCh]）; 由跳转表进入 ⇒ [esp] 非返回
 *   地址、参数不可用 → stub9 全忽略参数（与 ACT1/ACT5 同款, 复用 stub9）。
 *   hookLen=6 落在指令边界。 */
#define TGT_HOOK_NOTIFY38    0x004290D9UL
#define TGT_HOOK_ACCUM       VIB_ANCHOR_UNRESOLVED
#define TGT_HOOK_VOICE       VIB_ANCHOR_UNRESOLVED
#define TGT_HOOK_RESULT      VIB_ANCHOR_UNRESOLVED
#define TGT_HOOK_FINALE      VIB_ANCHOR_UNRESOLVED
#define TGT_HOOK_ATHIT       VIB_ANCHOR_UNRESOLVED
#define TGT_HOOK_COMBOUI     VIB_ANCHOR_UNRESOLVED
/* [10] 状态 tick 调度器(DOT/CC)【★已定案 2026-09-24】：sub_519640 __thiscall(status)，
 *   switch(*(this+4) - 1) 17 路跳表 @0x519A94，与 ACT1 sub_4D10F0 逐项同构：
 *     类型码 2 → sub_517480(中毒) / 9 → 0x51763f 族(燃烧) / 11 → sub_5176a0(出血)
 *     / 8 → sub_517ac0 / 1 → 0x519079 族(迟缓) …（对照 ACT1 的 2/9/0xB 三兄弟）
 *   入口 55 8B EC 6A FF 68 FB AA（与 ACT1 同款 5 字节 SEH 头）→ 复用 stub10。
 *   注意: ACT1 stub10 语义 = "玩家正处于伤害型状态 tick"≈玩家掉血, DOT 集 {2,9,0xB}
 *   在 v13.26 已改道飘字驱动, 本锚仅保留 CC 集 {1,3,7,8} 续发。 */
#define TGT_HOOK_STATUSTICK  0x00519640UL
#define TGT_HOOK_CRITEXIT    VIB_ANCHOR_UNRESOLVED  /* 概念位, 不安装(ACT1 遗留) */

#define TGT_HOOKLEN_SENDER     7
#define TGT_HOOKLEN_BROADCAST  6
#define TGT_HOOKLEN_CAMSHAKE   6
#define TGT_HOOKLEN_ACCUM      0
#define TGT_HOOKLEN_VOICE      0
#define TGT_HOOKLEN_RESULT     0
#define TGT_HOOKLEN_FINALE     0
#define TGT_HOOKLEN_ATHIT      0
#define TGT_HOOKLEN_COMBOUI    0
#define TGT_HOOKLEN_DMGFONT    10
#define TGT_HOOKLEN_NOTIFY38   6
#define TGT_HOOKLEN_STATUSTICK 5
#define TGT_HOOKLEN_CRITEXIT   0

/* 签名：取自 DFO_fixed_v3.exe 入口 8 字节（与 DFO.exe VA 布局一致, md5 已核对） */
#define TGT_SIG_SENDER     { 0x55,0x8B,0xEC,0x56,0x8B,0x75,0x08,0x68 }
#define TGT_SIG_BROADCAST  { 0x55,0x8B,0xEC,0x83,0xEC,0x10,0x8B,0x45 }
#define TGT_SIG_CAMSHAKE   { 0x55,0x8B,0xEC,0x8B,0x45,0x0C,0x53,0x8B }
#define TGT_SIG_ACCUM      { 0,0,0,0,0,0,0,0 }
#define TGT_SIG_VOICE      { 0,0,0,0,0,0,0,0 }
#define TGT_SIG_RESULT     { 0,0,0,0,0,0,0,0 }
#define TGT_SIG_FINALE     { 0,0,0,0,0,0,0,0 }
#define TGT_SIG_ATHIT      { 0,0,0,0,0,0,0,0 }
#define TGT_SIG_COMBOUI    { 0,0,0,0,0,0,0,0 }
#define TGT_SIG_DMGFONT    { 0x55,0x8B,0xEC,0x6A,0xFF,0x68,0xA5,0x69 }
#define TGT_SIG_NOTIFY38   { 0x8D,0x95,0x24,0xFE,0xFF,0xFF,0x52,0x88 }
#define TGT_SIG_STATUSTICK { 0x55,0x8B,0xEC,0x6A,0xFF,0x68,0xFB,0xAA }
#define TGT_SIG_CRITEXIT   { 0,0,0,0,0,0,0,0 }

/* ---- 状态名（UTF-16LE 宽串地址, work/60us/find_strings_60us 输出核实） ---- */
/* 60US 比 ACT5 多出 R_ALL_KILL / R_FAINT / R_HK2_*（ACT1 同名串存在）→
 * 发送器分发按 ACT1 公共分支自动生效, HK1_COUNT_* 走家族分支。 */
#define TGT_STR_VICTORY_UP_LOOP      0x00E425C0UL
#define TGT_STR_DEFEAT_DOWN_LOOP     0x00E4259CUL
#define TGT_STR_R_STAGE_FINALE       0x00CCF0C0UL
#define TGT_STR_RESULT_SCORE_UP_LOOP 0x00E428B8UL
#define TGT_STR_RESULT_COME          0x00E42584UL
#define TGT_STR_RANK_UP              0x00E425E0UL
#define TGT_STR_R_ALL_KILL           0x00CC04E4UL
#define TGT_STR_R_FAINT              0x00CC04FCUL
#define TGT_STR_R_HK2_3GOOD          0x00CDC514UL
#define TGT_STR_R_HK2_2GOOD          0x00CDC52CUL
#define TGT_STR_R_HK2_1GOOD          0x00CDC544UL

/* ---- 全局对象 / 偏移 ---- */
/* ★玩家对象全局【已定案 2026-09-28】= 0x0104F1D4（模拟器项目 client_hook 称 jj_0）：
 *   ① client_hook/patch_table.zig:4473 `local_player = 0x104F1D4`(其生产补丁 DLL 的
 *     本地角色判定); docs/archive/DEBUG_NOTES.md 活体：全库仅两处写 —— 角色加载尾巴
 *     0x40ABF2 call sub_55B7D0 挂上(jj_0 = *(stage+516))、析构 sub_55ADF0 清零;
 *     HUD/镜头/铺图都读它 → 城镇+副本通用。
 *   ② 本库伤害归属比较：0x49D83B `cmp [0x104F1D4],[esi+0x458]`(sete→标志bit0)、
 *     0x58E89A `mov edx,[0x104F1D4]`(0x07 标志组装) —— 与 [us] 事件宿主语义一致。
 *   ③ 全库 1019 处引用(p54)。
 * ⚠负结果（仍有效）：+0x155C 槽(505 处引用)解密后与 22 比较 = 限时状态块状态ID,
 *   绝不是 HP（ACT5 v13.17 同型雷, 勿踩第二次）。+0x1828 同属状态块。 */
#define TGT_PLAYER_OBJ           0x0104F1D4UL
/* 城镇玩家对象【已证伪 2026-09-28 实测, 撤回 UNRESOLVED】：
 *   0x0104F1D8 是【每张地图的标记对象】, 不是城镇玩家 —— [mvB] 实测：坐标长时间
 *   静止(29,429 / 866,288 / 474,234=门标记), 且对象指针本身随换图变化
 *   (0x1CD83000→0x1CD8E000); 留着只会在换图时发幻影移动脉冲。
 *   60US 城镇移动由源 A(0x104F1D4) 直接管（实测坐标全程活）, 无需第二源。
 *   若日后要独立城镇源, 从 CNUser 派生对象创建链另找。 */
#define TGT_TOWN_OBJ             VIB_ANCHOR_UNRESOLVED
/* 坐标【已定案 2026-09-28】= +0x198(x) / +0x19C(y)：client_hook live-verified
 * (patch_table.zig:6088 场外哨兵 -10000.0f=0xC61C4000 落在实体 +0x198/+0x19C 双
 * dword; test_room_clear.py 实体模型 (0x198,x),(0x19C,y)); 本库读点 598/403 处
 * (p57)。代际: ACT1 +0x13C → ACT5 +0x16C → 60US +0x198。 */
#define TGT_PLAYER_XOFF          0x198UL
#define TGT_PLAYER_YOFF          0x19CUL
#define TGT_PLAYER_OWNER_OFF     VIB_ANCHOR_UNRESOLVED
#define TGT_PLAYER_HP_SLOT_OFF   VIB_ANCHOR_UNRESOLVED
#define TGT_PLAYER_MP_SLOT_OFF   VIB_ANCHOR_UNRESOLVED
#define TGT_PLAYER_FRENZY_OFF    VIB_ANCHOR_UNRESOLVED
#define TGT_FRENZY_VT            0x00D7A22CUL  /* CNFrenzy vftable (RTTI 链解出, p07) */
#define TGT_HIT_COUNT            VIB_ANCHOR_UNRESOLVED
#define TGT_DAMAGE_TOTAL         VIB_ANCHOR_UNRESOLVED
#define TGT_SCORE_OBJ            VIB_ANCHOR_UNRESOLVED
#define TGT_SCORE_TOTAL_OFF      0
#define TGT_MONSTER_REMAIN       VIB_ANCHOR_UNRESOLVED
#define TGT_BATTLE_CORE          VIB_ANCHOR_UNRESOLVED
#define TGT_BATTLE_CORE_CTX_OFF  0x200000UL     /* 60US 容量 0x200000（ACT5 为 0x400000） */
#define TGT_CTX_LAYERMGR_OFF     0x000000B0UL
#define TGT_LAYER_VEC_BEGIN_OFF  0x00000010UL
#define TGT_LAYER_VEC_END_OFF    0x00000014UL
#define TGT_NUMOBJ_VFTABLE       0x00CCE7B4UL  /* CNRDNumberObject vftable (RTTI 链, p07) */
#define TGT_NUMOBJ_CRIT_OFF      0
#define TGT_NUMOBJ_TYPE_OFF      0
#define TGT_NUMOBJ_OWNER_OFF     0
#define TGT_NUMOBJ_LIVE_CNT      VIB_ANCHOR_UNRESOLVED

/* ---- P0 加密四件套【已证实】（p24/p25 + IDA 探针 R 双表写入复核） ----
 * sub_B17A90: dword_12101DC = 0xA1B2C3D4 ^ rand 混合（0xA1B2C3D4 全库唯一
 *   @0xb17ad7）; sub_B17420: dword_12101D8 = ctx, 容量 0x200000;
 *   baseA = ctx+0x44 / baseB = ctx+0x48（与 ACT1 同构, +17*4=0x44）。
 * sub_402060(this): 明文 = key ^ (baseA+4*idx) ^ *(baseA+4*idx),
 *   idx = slot[+4]; 完整性 (key ^ slot[0]) - slotAddr == slot[+4]。
 *   —— hooks_old.c 的 enc_key_old()/read_enc_dword()/enc_slot_ok() 零改动。 */
#define TGT_ENC_TABLE_BASE       0x012101D8UL  /* ctx 全局 */
#define TGT_ENC_KEY_ADDR         0x012101DCUL  /* 运行时密钥全局地址 */
#define TGT_FIELD_DECODE         0x00402060UL  /* 槽解算器 sub_402060 */
#define TGT_PLAYER_HIT_ENTRY     VIB_ANCHOR_UNRESOLVED
#define TGT_PLAYER_HIT_PUSH      VIB_ANCHOR_UNRESOLVED

/* ---- ★伤害通道定位探针 v5（log-only, 不发震动; 定案后移除） ----
 * 排雷史: v1 显示侧零触发; v2 sub_45C740 入口=每帧太热; v3/v3.1 内联出口
 *   0x45D19F 两次拒装(签名7字节零填bug)→ v3.2 装机成功零触发;
 *   ★v4 实机(2026-09-24, 133 行日志)四条通道全部命中, 结构定案:
 *     生成器 sub_498980 (thiscall, a4=宿主对象 a5=伤害 a6=标志) —— 27 次
 *     构造器 sub_498620 (thiscall, a1=伤害 a2=标志) —— 27 次, 与生成器一一对应;
 *     生成器内 call 构造器在 0x498CC5 (其 ret=0x498CCA 在 [pb1] 每场 27/27 兑现,
 *     即伤害数字构造的唯一来源), 生成器尾段再调 vft+0x88 放置数字对象。
 *     解码器 0x517040 零触发(该场无服务端事件): 通道存在但非主路径;
 *     inline 出口 0x45D19F 5 次(本地计算通道, 极低频, 非主源)。
 *     ★构造器 a2 位定义静态定案(0x498689 起): bit0→值码0x8F, bit1→0x36,
 *       bit2→0x4A, bit3→正负(0x2C/0x40), bit4→类型字段=1, bit5→类型字段=2。
 *     ★生成器 a6→构造器 a2 原样透传(0x498CBB-0x498CC5 中间无运算);
 *       生成器第4栈参 a4 = 宿主(数字挂靠对象), 尾段 vft+0x88 用它 + 宽度居中。
 *     ★生成器 9 个直接调用点 = 场景区分钥匙(v5 逐点挂钩, 用探针 id 区分):
 *       0x8C4128(主路径, 该场 228/338) / 0x5172E2+0x51784E+0x5191A4(解码族)
 *       / 0x57FC8F(fmul) / 0x58E8D8+0x58F62D / 0x60C044 / 0x49D86B
 *     ★构造器后段 0x4986CE = a2 位解码完成后现场(a1@[ebp+8], a2@[ebp+0xC] 仍活)。
 * v5 探针(槽0=regs stub0, 槽1..3=通用栈捕获):
 *   p0 = 0x4986CE 构造器加工点(regs: eax/a2结果, ecx, ebp 现场)
 *   p1..p3 = 生成器调用点(栈捕获: ecx=this, a1..a5) —— 一次三场景,
 *            先跑 0x8C4128 / 0x5172E2 / 0x58E8D8, 按结果换址。
 *
 * ★★ v5.1 装机策略修正(重要, 血泪): 生成器调用点是 **call(E8 rel32) 指令**,
 *   而探针引擎的 trampoline 原样重放覆盖字节【不修 rel32】—— E8 移到 tramp 页
 *   后相对位移失效 → 跳到错误地址 → 崩溃(ACT1 0x790D93 已踩过一次, 见 v13.16)。
 *   v5.0 若直接照上面投产, 挂钩瞬间即崩(签名/边界全对也没用 —— 是重放问题)。
 *   两种改法: (a) 给探针引擎加 rel32 修位(主引擎 hook_one 已有此逻辑, 照抄);
 *             (b) 改挂【函数入口】(非相对指令), 用 ret(返回地址) 区分调用点。
 *   已选 (a): 引擎加修位后 call 点可安全挂钩, "一个调用点一个探针槽"最直观。
 *   (b) 保留为后备: 生成器入口 0x498980 sig {55,8B,EC,6A,FF,68,A5,69} len=10,
 *       构造器入口 0x498620 sig {55,8B,EC,6A,FF,68,7E,69} len=10, 均无相对指令。
 *   签名纪律: 必须 ≥8 字节(含边界外首字节)。 */
#define VIB_PROBE_COUNT   0   /* ★定案后已撤探针 */
#define TGT_PROBE_MODE_0  0      /* 挂钩: regs 捕获 stub0(构造器加工点) */
#define TGT_PROBE_MODE_1  0      /* 挂钩: 通用栈捕获 stub1 */
#define TGT_PROBE_MODE_2  0      /* 挂钩: 通用栈捕获 stub2 */
#define TGT_PROBE_MODE_3  0      /* 挂钩: 通用栈捕获 stub3 */
#define TGT_PROBE_ADDR_0  0x004986CEUL   /* 构造器: a2 位解码完成后现场 */
#define TGT_PROBE_LEN_0   8              /* 8B C8 | 83 E1 10 | 89 4D EC (8B 整条) */
#define TGT_PROBE_SIG_0   { 0x8B,0xC8,0x83,0xE1,0x10,0x89,0x4D,0xEC }
#define TGT_PROBE_ADDR_1  0x008C4128UL   /* 生成器调用点: 主路径 */
#define TGT_PROBE_LEN_1   5
#define TGT_PROBE_SIG_1   { 0xE8,0x53,0x48,0xBD,0xFF,0x8A,0x45,0x0F }
#define TGT_PROBE_ADDR_2  0x005172E2UL   /* 生成器调用点: 解码族(服务端) */
#define TGT_PROBE_LEN_2   5
#define TGT_PROBE_SIG_2   { 0xE8,0x99,0x16,0xF8,0xFF,0x83,0xC4,0x18 }
#define TGT_PROBE_ADDR_3  0x0058E8D8UL   /* 生成器调用点: 5 栈参全量 */
#define TGT_PROBE_LEN_3   5
#define TGT_PROBE_SIG_3   { 0xE8,0xA3,0xA0,0xF0,0xFF,0x83,0xC4,0x18 }

#else
#error "VIB_TARGET must be 1 (ACT1), 5 (ACT5) or 60 (60US)"
#endif

#endif /* VIB_TARGET_OLD_H */

