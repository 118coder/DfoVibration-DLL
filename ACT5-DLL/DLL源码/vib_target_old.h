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

#else
#error "VIB_TARGET must be 1 (ACT1) or 5 (ACT5)"
#endif

#endif /* VIB_TARGET_OLD_H */
