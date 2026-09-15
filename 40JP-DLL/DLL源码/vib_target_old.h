#ifndef VIB_TARGET_OLD_H
#define VIB_TARGET_OLD_H
/* ============================================================
 * 版本锚点深模块（唯一版本剖面）—— ACT1 / ACT4 / ACT5
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
 * 剖面切换：编译期 -DVIB_TARGET=1(ACT1, 默认) / =4(ACT4) / =5(ACT5)
 *   构建脚本：build_old.bat(ACT1) / build_act4.bat(ACT4) / build_act5.bat(ACT5)
 * ============================================================ */

#define VIB_TARGET_ACT1   1
#define VIB_TARGET_ACT4   4
#define VIB_TARGET_ACT5   5
#define VIB_TARGET_40JP   40   /* ★40JP: ARAD.exe 2006-06-12 老客户端（E:\Game\Arad40） */
#ifndef VIB_TARGET
#define VIB_TARGET        VIB_TARGET_ACT1   /* 默认保持 ACT1 现网行为不变 */
#endif

#define VIB_ANCHOR_UNRESOLVED  0UL
#define VIB_ANCHOR_IS_SET(x)   ((x) != VIB_ANCHOR_UNRESOLVED)
/* ★40JP 新增 [13] HP 变更锚点（命中/受击源），容量 13→14；其余版本该槽 = UNRESOLVED。
 * ★[14] MP 变更锚点（回蓝源）再 +1 → 容量 15。 */
#define VIB_HOOK_COUNT         15           /* 数组容量必须与锚点表一致（教训 12） */

/* ★[13] HP 变更锚点（命中/受击无损源）—— 默认未定位，仅 40JP 剖面填充；
 * ACT1/ACT4/ACT5 无此锚点（它们用伤害数字咽喉 stub8/stub13 直接发命中），
 * 保持 UNRESOLVED 即被安装循环显式跳过，行为零回归。 */
#ifndef TGT_HOOK_HPCHANGE
#define TGT_HOOK_HPCHANGE     VIB_ANCHOR_UNRESOLVED
#define TGT_HOOKLEN_HPCHANGE  0
#define TGT_SIG_HPCHANGE      { 0,0,0,0,0,0,0,0 }
#endif
/* ★[14] MP 变更锚点（回蓝源）—— 默认未定位，仅 40JP 剖面填充。 */
#ifndef TGT_HOOK_MPSET
#define TGT_HOOK_MPSET        VIB_ANCHOR_UNRESOLVED
#define TGT_HOOKLEN_MPSET     0
#define TGT_SIG_MPSET         { 0,0,0,0,0,0,0,0 }
#endif

/* ★40JP 诊断探针总开关（vib_probe_table.h 的 24 个候选）。
 * 默认 0 = 不编译不进二进制；诊断构建用 -DVIB_PROBE_ENABLE=1（build_40jp_probe.bat）。
 * 采集完成后必须改回 0 出正式版（探针会 hook 24 个未定案地址，不适合长期运行）。 */
#ifndef VIB_PROBE_ENABLE
#define VIB_PROBE_ENABLE      0
#endif

/* ★[snd] 发送器字符串探针：使命已完成（实测 40JP 发送器只流过 UI/系统音效串，
 * 无技能名）→ 默认关闭，避免刷屏。需要时 -DVIB_SND_PROBE=1 重开。 */
#ifndef VIB_SND_PROBE
#define VIB_SND_PROBE         0
#endif

/* ★回复事件的最小净回血量（HP 与 MP 共用）：实测存在"细水长流"式回血
 * （HP 1230→1270 由一串 +1..+4 组成）与 `1758<->1759` 的 +1/−1 抖动。
 * 本阈值作用在【净回血累加器】上（涨则加、跌则减、下限 0、3s 无变化作废）：
 *   - 持续回血很快攒够 → 发事件；+1/−1 往返净增恒 0 → 永不误报。 */
#ifndef VIB_HEAL_MIN_GAIN
#define VIB_HEAL_MIN_GAIN     5
#endif
/* ★两次回复事件的最小间隔（ms） */
#ifndef VIB_HEAL_MIN_MS
#define VIB_HEAL_MIN_MS       800
#endif
/* ★"一轮回血结束"的安静判据（ms）：一口药分多次小步加血，安静这么久才算这一轮结束
 * → 一次事件（修"吃一次药震两次"）。 */
#ifndef VIB_HEAL_QUIET_MS
#define VIB_HEAL_QUIET_MS     350
#endif
/* ★持续回血的最大轮次窗口（ms）：像光环那样一直小步回血时，按此窗口脉冲，
 * 避免"永远等不到安静 → 永不发声"。 */
#ifndef VIB_HEAL_MAXWIN_MS
#define VIB_HEAL_MAXWIN_MS    1500
#endif

/* ================================================================
 * 剖面：ACT1（LMA1S-release, 2008-06-17, ImageBase 0x400000）
 * 全部已定案（见 03/07 号文档 + 无损对接交接文档第五节）
 * ================================================================ */
#if VIB_TARGET == VIB_TARGET_ACT1

#define VIB_TARGET_NAME   "ACT1"
#define VIB_STR_WIDE      0                       /* 状态名 = ASCII */
#define VIB_IMAGE_LO      0x00401000UL            /* 模块映像区下界(vib_ptr_ok 排除堆) */
#define VIB_IMAGE_HI      0x00F3C000UL            /* 模块映像区上界 */
#define VIB_POS_PROBE     0                       /* 坐标字段定位探针(仅定位期开) */
#define VIB_HIT_MERGE_MS  10                      /* 命中(0x01)合并窗: ACT1 原值 10ms 不变 */

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
#define VIB_POS_PROBE     0                       /* 坐标已定案(0x16C/0x170) */
#define VIB_HIT_MERGE_MS  10                      /* 命中(0x01)合并窗(ACT5 沿用 10ms) */

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
#define TGT_HOOK_BROADCAST   VIB_ANCHOR_UNRESOLVED
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
#define TGT_HOOKLEN_BROADCAST  0
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
#define TGT_SIG_BROADCAST  { 0,0,0,0,0,0,0,0 }
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

/* ---- 全局对象 / 偏移（未定位，保持 UNRESOLVED） ---- */
#define TGT_PLAYER_OBJ           VIB_ANCHOR_UNRESOLVED
#define TGT_TOWN_OBJ             VIB_ANCHOR_UNRESOLVED
#define TGT_PLAYER_XOFF          VIB_ANCHOR_UNRESOLVED
#define TGT_PLAYER_YOFF          VIB_ANCHOR_UNRESOLVED
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
 * 剖面：ACT4（DNF.exe, 2009-11-17, 17,231,872 B, ImageBase 0x400000）
 * 证据：work/out/act4_scan.txt / act4_probe3.txt / act4_choke2.txt /
 *       act4_cam_notify.txt / act4_flags_cam.txt / act4_solver2.txt
 * ⚠ 状态名为 ASCII（VIB_STR_WIDE=0），与 ACT1 同、与 ACT5 的宽串不同
 * ================================================================ */
#elif VIB_TARGET == VIB_TARGET_ACT4

#define VIB_TARGET_NAME   "ACT4"
#define VIB_STR_WIDE      0
#define VIB_IMAGE_LO      0x00401000UL
#define VIB_IMAGE_HI      0x0146E000UL            /* .text..nsn..vmp0 末 */
#define VIB_POS_PROBE     0                       /* 坐标已定案(0x174/0x178), 探针关闭减噪 */
/* ★命中(0x01)合并窗。背景：ACT4 的 0x01 全部出自同一伤害管道(0x864711)，
 * 一次"命中"会因主伤害+附加伤害等产生多条数字 → 直接逐条发会随装备变强而爆震。
 * 同一命中的多条数字在同一帧内产生，故用一个小窗合并成一条「玩家命中反馈」。
 * 10ms=沿用 ACT1；调大可更强地压制后期飙字刷屏，代价是极速连段(10-39ms)可能被合并。
 * 排障：DLL 日志 tick 行的 hm= 即为被本窗吞掉的条数(font a= 是实际发出的 0x01 数)。 */
#define VIB_HIT_MERGE_MS  20
/* 逐条打印 [h] 命中明细(VIB_HIT_PROBE=1 时开)。hev/hgrp 计数常驻, 此开关只控明细行。 */
#define VIB_HIT_PROBE     0

/* ---- P0 加密四件套【已证实】 ---- */
/* key 初始化 sub_A63890: dword_F5EB9C = 0xA1B2C3D4 ^ ((rand()<<16)|rand())  (立即数 @0xa638d7)
 * 同函数 sub_A63220(0x200000) 建表；ctx 全局 = key-4（与 ACT1/ACT5 同布局）
 * 槽读写核心 sub_404A40: baseA=[ctx+0x44], baseB=[ctx+0x48]，
 *   明文 = *(baseA+4*idx) ^ (baseA+4*idx) ^ key（与 ACT1 sub_402030 同构） */
#define TGT_ENC_TABLE_BASE       0x00F5EB98UL
#define TGT_ENC_KEY_ADDR         0x00F5EB9CUL
#define TGT_FIELD_DECODE         0x00404A40UL

/* ---- hook 锚点 ---- */
/* [0] 统一事件发送器【已证实】：cdecl(name,a2,a3,a4)；
 *   VICTORY_UP_LOOP/DEFEAT_DOWN_LOOP/RESULT_COME/RANK_UP/RESULT_SCORE_UP_LOOP
 *   /R_STAGE_FINALE 六处 push offset 后均 `push -1; call 0x43D9B0`；
 *   入口 55 8B EC 8B 45 08 56 57（hookLen=6）。 */
#define TGT_HOOK_SENDER      0x0043D9B0UL
/* [8] 伤害数字生成咽喉【已证实】：6 参 cdecl(a1=Src,a2=x,a3=y,a4=z,a5=伤害,a6=标志)；
 *   尾部 `push [ebp+0x1c]; push [ebp+0x18]; call 0x488F90` 建 CNRDNumberObject
 *   （vftable 0xB0C6DC），ctor 按 a6 位 0x01/0x02/0x04/0x08 选精灵、0x10=特殊
 *   ——与 ACT1/ACT5 标志语义同构；9 个调用点；入口 SEH 55 8B EC 6A FF 68 D5 14 AC。
 *   ACT4 与 ACT5 同为 6 参 → 用 stub13（栈偏移 +4）复用 vib_dispatch_damage_font。 */
#define TGT_HOOK_DMGFONT     0x00489310UL
#define TGT_STUB_DMGFONT     vib_stub_13
/* [12] 镜头震动汇聚点【已证实】：__thiscall(this,a2=度数,a3)；
 *   a2/a3 存 this+0x11C/+0x120，a2==0 清 this+0x164/+0x168（停震）
 *   ——与 ACT1 sub_4E49E0 逐字段同构；99 个调用点；入口 55 8B EC 8B 45 0C 53 8B（hookLen=6）。 */
#define TGT_HOOK_CAMSHAKE    0x0051D370UL

/* [1] 播报层（评分点来源）【已证实】(2026-09-13)
 *   sub_51FA10(this=ecx, a2=[ebp+8], a3=[ebp+0xc])，ret 8；入口 `55 8B EC 83 EC 0C 8B 45`
 *   —— 与 ACT1 sub_4E6DA0 逐字节同构。a2 分支实测：`cmp eax,7 → HK_TECHNIC`、`cmp eax,6 → HK_STYLE`
 *   （0xB1283C / 0xB12830）。STYLE/TECHNIC 经玩家对象虚表 +0x10C 播出、不经统一发送器
 *   （[snd] 探针实测战斗期发送器内无评级串）→ 必须 hook 本层。→ VEV_KILLPOINT「评分点」。 */
#define TGT_HOOK_BROADCAST   0x0051FA10UL
/* 其余未定位 → 显式跳过（UNRESOLVED），绝不静默安装 */
#define TGT_HOOK_ACCUM       VIB_ANCHOR_UNRESOLVED
#define TGT_HOOK_VOICE       VIB_ANCHOR_UNRESOLVED
#define TGT_HOOK_RESULT      VIB_ANCHOR_UNRESOLVED
#define TGT_HOOK_FINALE      VIB_ANCHOR_UNRESOLVED
#define TGT_HOOK_ATHIT       VIB_ANCHOR_UNRESOLVED
#define TGT_HOOK_COMBOUI     VIB_ANCHOR_UNRESOLVED
/* [9] 通知38 = 怪物死亡批量确认【已证实】(2026-09-12)
 *   通知分发器 sub_41E720 的 case 38 块 @0x4273B3（跳转表 @0x4306E8[38]）：
 *   `word[ebp-0x188]`=怪物 id → `call 0x4F29F0(0x211, id)`（0x211=529，与 ACT5 同魔数）
 *   → `call 0xA7B9CD`(=__RTDynamicCast, &TD_IRDMonster 0xCD69CC) → 命中 IRDMonster 则处理。
 *   入口 `8D 8D 78 FE FF FF`（lea ecx,[ebp-0x188]，hookLen=6）；跳转表进入 → [esp] 非返回地址，
 *   参数不可用 → stub9 全忽略（与 ACT1/ACT5 同）。 */
#define TGT_HOOK_NOTIFY38    0x004273B3UL
#define TGT_HOOK_STATUSTICK  VIB_ANCHOR_UNRESOLVED
#define TGT_HOOK_CRITEXIT    0x00790D93UL           /* 概念位, 永久不安装 */

#define TGT_HOOKLEN_SENDER     6
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

/* 签名 = 原 exe 目标入口前 8 字节（安装前 memcmp，不符即拒绝安装） */
#define TGT_SIG_SENDER     { 0x55,0x8B,0xEC,0x8B,0x45,0x08,0x56,0x57 }
#define TGT_SIG_BROADCAST  { 0x55,0x8B,0xEC,0x83,0xEC,0x0C,0x8B,0x45 }
#define TGT_SIG_ACCUM      { 0,0,0,0,0,0,0,0 }
#define TGT_SIG_VOICE      { 0,0,0,0,0,0,0,0 }
#define TGT_SIG_RESULT     { 0,0,0,0,0,0,0,0 }
#define TGT_SIG_FINALE     { 0,0,0,0,0,0,0,0 }
#define TGT_SIG_ATHIT      { 0,0,0,0,0,0,0,0 }
#define TGT_SIG_COMBOUI    { 0,0,0,0,0,0,0,0 }
#define TGT_SIG_DMGFONT    { 0x55,0x8B,0xEC,0x6A,0xFF,0x68,0xD5,0x14 }
#define TGT_SIG_NOTIFY38   { 0x8D,0x8D,0x78,0xFE,0xFF,0xFF,0x51,0x88 }
#define TGT_SIG_STATUSTICK { 0,0,0,0,0,0,0,0 }
#define TGT_SIG_CRITEXIT   { 0,0,0,0,0,0,0,0 }
#define TGT_SIG_CAMSHAKE   { 0x55,0x8B,0xEC,0x8B,0x45,0x0C,0x53,0x8B }

/* ---- 状态名（exe 内明文 ASCII 地址） ---- */
#define TGT_STR_VICTORY_UP_LOOP      0x00C37F9CUL
#define TGT_STR_DEFEAT_DOWN_LOOP     0x00C37F88UL
#define TGT_STR_R_STAGE_FINALE       0x00B0DD2CUL
#define TGT_STR_RESULT_SCORE_UP_LOOP 0x00C38094UL
#define TGT_STR_RESULT_COME          0x00C37F7CUL
#define TGT_STR_RANK_UP              0x00C37FACUL
#define TGT_STR_R_ALL_KILL           0x00B088BCUL
#define TGT_STR_R_FAINT              0x00B088C8UL
#define TGT_STR_R_HK2_3GOOD          0x00B128C8UL
#define TGT_STR_R_HK2_2GOOD          0x00B128D4UL
#define TGT_STR_R_HK2_1GOOD          0x00B128E0UL

/* ---- 全局对象 / 偏移 ---- */
/* 玩家对象全局【已证实】：954 处代码引用；咽喉标志位计算用它做归属比较
 * （0x48D89B / 0x568EF3：cmp [0xDA368C], [obj+0x428] 判"受害者==玩家"）。
 * 城镇源取相邻 +4（ACT1/ACT5 均为相邻对），待 [mvB] 探针确认。 */
#define TGT_PLAYER_OBJ           0x00DA368CUL
#define TGT_TOWN_OBJ             0x00DA3690UL
/* ★坐标【已证实】+0x174=x / +0x178=y（[pos] 探针实测：x 82→1135、y 173→479，
 * 为唯一同步变化的相邻对；ACT4 玩家结构比 ACT5 再大 8 字节）。+0x17C 随动作小幅变化(=z/朝向)。 */
#define TGT_PLAYER_XOFF          0x174UL
#define TGT_PLAYER_YOFF          0x178UL
#define TGT_PLAYER_OWNER_OFF     VIB_ANCHOR_UNRESOLVED
#define TGT_PLAYER_HP_SLOT_OFF   VIB_ANCHOR_UNRESOLVED
#define TGT_PLAYER_MP_SLOT_OFF   VIB_ANCHOR_UNRESOLVED
#define TGT_PLAYER_FRENZY_OFF    VIB_ANCHOR_UNRESOLVED
#define TGT_FRENZY_VT            0x00B91044UL   /* CNFrenzy vftable (RTTI 解出) */
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
#define TGT_NUMOBJ_VFTABLE       0x00B0C6DCUL   /* CNRDNumberObject vftable (RTTI 解出) */
#define TGT_NUMOBJ_CRIT_OFF      0
#define TGT_NUMOBJ_TYPE_OFF      0
#define TGT_NUMOBJ_OWNER_OFF     0
#define TGT_NUMOBJ_LIVE_CNT      VIB_ANCHOR_UNRESOLVED
#define TGT_PLAYER_HIT_ENTRY     VIB_ANCHOR_UNRESOLVED
#define TGT_PLAYER_HIT_PUSH      VIB_ANCHOR_UNRESOLVED

/* ================================================================
 * 剖面：40JP（ARAD.exe, 2006-06-12, 3,948,544 B, ImageBase 0x400000）
 * 目标：E:\Game\Arad40\ARAD.exe（i386，无壳，正常段表，导入表完好）
 * 证据：work/ida/ARAD.exe.i64（IDA 9.4 新自动分析库）+ work/out/*.txt
 * ⚠ 状态名为 ASCII（VIB_STR_WIDE=0）
 * ⚠ 与 ACT1/ACT4/ACT5 基址/结构全不同 —— 本剖面为重新定位结果，非照抄。
 * ================================================================ */
#elif VIB_TARGET == VIB_TARGET_40JP

#define VIB_TARGET_NAME   "40JP"
#define VIB_STR_WIDE      0
#define VIB_IMAGE_LO      0x00401000UL
#define VIB_IMAGE_HI      0x009FC000UL            /* 映像末 0x9FBBB0 上取整 */
/* 坐标已定标（+0xB0/+0xB4，见下）；探针关闭减少日志噪声。 */
#define VIB_POS_PROBE     0
#define VIB_HIT_MERGE_MS  10                      /* 沿用 ACT1；配合 HP 锚点压制群怪同帧 */
#define VIB_HIT_PROBE     0

/* ---- hook 锚点 ---- */
/* [0] 统一事件发送器【已证实】0x00432EA0
 *   证据（work/out/find_anchors.txt）：VICTORY_UP_LOOP / DEFEAT_DOWN_LOOP / RESULT_COME /
 *   RANK_UP / R_STAGE_FINALE / RESULT_SCORE_UP_LOOP / RESULT_CHARACTER / COUNT_DOWN /
 *   HK1_COUNT_1-3 / HK1_FIGHT / R_HK2_1-3GOOD 共 15 个状态串的 push 点，其后最近的
 *   call 全部汇入 0x432EA0（唯一）；IDA 反编译 = __cdecl(Src,a2,a3,a4)，非空则
 *   sub_65FE10(Src,0,a3,a4,0,0xFFFF,a2)。入口 `55 8B EC 8B 45 08 56 57` 与
 *   ACT1/ACT4 逐字节同构，x220/430 个引用。 */
#define TGT_HOOK_SENDER      0x00432EA0UL
/* [1] 播报层（评分点/被击/击杀）【已证实】0x00426820
 *   证据：HK_STYLE@0x6C3974 / HK_TECHNIC@0x6C3980 的 xref 落在 sub_426820；
 *   IDA 反编译 = __thiscall(this,a2,a3) switch(a2)：case 6→"HK_STYLE"、
 *   case 7→"HK_TECHNIC"、case 3→"HK_ATTACKED"(this+556 += a3 = 伤害)、
 *   case 2/4/5 同族 —— 与 ACT1 sub_4E6DA0 语义同构（6/7 经玩家对象虚表 +212 播出）。
 *   入口 `55 8B EC 8B 45 08 53 33`（push ebp/mov ebp,esp/mov eax,[ebp+8]/push ebx/xor ebx,ebx）。 */
#define TGT_HOOK_BROADCAST   0x00426820UL
/* [8] 伤害数字咽喉【已证实 2026-09-14, v0.11 实装】0x004D7960 = 浮动数字生成器
 *   sprintf("%d", abs(arg_C)) 逐位取字形（sub_418DD0, 调色板基 = arg_10 类型）→
 *   5 帧弹出动画 → 池化对象(CNRDPooledObject vtable)注册进 [this+0x74] 特效管理器。
 *   发现路径：首测探针 ret 归因(0x4D71B0 每次命中主处理器) → 池化生成器家族 →
 *   sprintf/拆位指纹；实机谱系（探针 DMGNUM 191 样本, work/out/ida_dmgfont_hunt*.log）：
 *   type 0=普通伤害 / 1=暴击 / 2=HP回复(绿) / 3=MP回复(蓝) / 4=HUD 常显(恒值, 忽略)。
 *   cdecl(arg_0=x?, arg_4=y?, arg_8=?, arg_C=值, arg_10=类型, arg_14, arg_18)。
 *   入口 `55 8B EC 6A FF 68 44 C5`（hookLen=5；与 ACT1 0x470ED0 签名同构, 仅 SEH 偏移异）。
 *   复用 stub8（cdecl 布局同 ACT1: a4 槽=值, a5 槽=类型）→ 40JP 专属分派:
 *   type 2/3 → FONT 0x08 回复直判; type 0/1 不用（命中保持 [13] HP 派生真命中）。 */
#define TGT_HOOK_DMGFONT     0x004D7960UL
#define TGT_STUB_DMGFONT     vib_stub_8
#define TGT_HOOK_ACCUM       VIB_ANCHOR_UNRESOLVED
#define TGT_HOOK_VOICE       VIB_ANCHOR_UNRESOLVED
#define TGT_HOOK_RESULT      VIB_ANCHOR_UNRESOLVED
#define TGT_HOOK_FINALE      VIB_ANCHOR_UNRESOLVED
#define TGT_HOOK_ATHIT       VIB_ANCHOR_UNRESOLVED
#define TGT_HOOK_COMBOUI     VIB_ANCHOR_UNRESOLVED
#define TGT_HOOK_NOTIFY38    VIB_ANCHOR_UNRESOLVED
#define TGT_HOOK_STATUSTICK  VIB_ANCHOR_UNRESOLVED
#define TGT_HOOK_CRITEXIT    0x00790D93UL        /* 概念位, 永久不安装(ACT1 遗留) */
/* [12] 镜头震动汇聚点【已证实 2026-09-13】
 * 0x00424120 = __thiscall(this=相机, a2=度数, a3=时长)：
 *   `*(this+50)=a2; *(this+51)=a3; ...; if (a2==0){ *(this+59)=0; *(this+60)=0; }`
 *   → 度数/时长存 this+0xC8/+0xCC；a2==0 清 this+0xEC/+0xF0（停震）
 *   —— 与 ACT1 sub_4E49E0 / ACT4 sub_51D370 结构完全同构（仅偏移不同：
 *   ACT4 是 0x11C/0x120/0x164/0x168）。
 * 35 个代码调用点，全部落在 0x5Axxxx–0x64xxxx = 技能/特效代码区（与 ACT4 的 99 个同族）。
 * 入口 `55 8B EC 8B 45 0C 53 56 57 8B`（hookLen=6 覆盖 push ebp/mov ebp,esp/mov eax,[ebp+0Ch]）。
 * ⚠ 注意：本函数入口与 ACT1/ACT4 签名只差第 8 字节（0x56 vs 0x8B），
 *   8 字节整匹配签名会漏掉 —— 是靠「相邻字段双写」形状扫出来的（work/out/setter_cands.txt）。
 * 复用 stub12 → vib_dispatch_camera_shake → VEV_KILL(21) 宿主「释放技能」滑块。 */
#define TGT_HOOK_CAMSHAKE    0x00424120UL
/* [13] HP 变更锚点（命中/受击无损源）【已证实为函数，语义见 hooks_old.c】
 *   0x004D4D40 = 角色 HP 变更(__thiscall, virtual)：
 *   入口 `55 8B EC 83 EC 08 56 8B F1 8B 06 FF 90 34 01 00 00`
 *   调用点 0x004D8E18-27 实证压参 = push arg4 / push arg3 / push arg2 / mov ecx,edi / call。
 *   IDA: v9=arg2(新HP,可负=致死); if(arg2<0){v20=GetHP-arg2;} 末尾 vtable+732(this, 钳制后HP)。
 *   → 新HP 下降 = 该对象掉血（纯只读推断，不写游戏内存）。 */
#undef  TGT_HOOK_HPCHANGE
#define TGT_HOOK_HPCHANGE    0x004D4D40UL
#undef  TGT_HOOKLEN_HPCHANGE
#define TGT_HOOKLEN_HPCHANGE  6
#undef  TGT_SIG_HPCHANGE
#define TGT_SIG_HPCHANGE      { 0x55,0x8B,0xEC,0x83,0xEC,0x08,0x56,0x8B }
/* [14] MP 变更锚点（回蓝源）【已证实】
 * 0x004D4F90 = 角色 MP 变更(__thiscall, virtual)：与 HP setter 0x4D4D40 形状完全同构 ——
 *   同样的 +0x134/+0x2D4 前置检查；`[ebp+8]`=新MP `[ebp+0xC]`=flag(char) `[ebp+0x10]`=来源；
 *   同样 clamp 到上限，但上限取 vtable **+0x2F4**（HP 用 +0x2E0/+0x2E4）。
 * 发现路径：从 HP setter 的 vtable 槽（0x6CCAA4 等 6 处）取相邻槽 +4 → 0x4D4F90。
 * 入口 `55 8B EC 56 8B F1 8B 06 FF 90 34 01 00 00`（hookLen=6）。 */
#undef  TGT_HOOK_MPSET
#define TGT_HOOK_MPSET        0x004D4F90UL
#undef  TGT_HOOKLEN_MPSET
#define TGT_HOOKLEN_MPSET     6
#undef  TGT_SIG_MPSET
#define TGT_SIG_MPSET         { 0x55,0x8B,0xEC,0x56,0x8B,0xF1,0x8B,0x06 }

#define TGT_HOOKLEN_SENDER     6
#define TGT_HOOKLEN_BROADCAST  6
#define TGT_HOOKLEN_ACCUM      0
#define TGT_HOOKLEN_VOICE      0
#define TGT_HOOKLEN_RESULT     0
#define TGT_HOOKLEN_FINALE     0
#define TGT_HOOKLEN_ATHIT      0
#define TGT_HOOKLEN_COMBOUI    0
#define TGT_HOOKLEN_DMGFONT    5
#define TGT_HOOKLEN_NOTIFY38   0
#define TGT_HOOKLEN_STATUSTICK 0
#define TGT_HOOKLEN_CRITEXIT   0
#define TGT_HOOKLEN_CAMSHAKE   6

/* 签名 = 原 exe 目标入口前 8 字节（安装前 memcmp，不符即拒绝安装） */
#define TGT_SIG_SENDER     { 0x55,0x8B,0xEC,0x8B,0x45,0x08,0x56,0x57 }
#define TGT_SIG_BROADCAST  { 0x55,0x8B,0xEC,0x8B,0x45,0x08,0x53,0x33 }
#define TGT_SIG_ACCUM      { 0,0,0,0,0,0,0,0 }
#define TGT_SIG_VOICE      { 0,0,0,0,0,0,0,0 }
#define TGT_SIG_RESULT     { 0,0,0,0,0,0,0,0 }
#define TGT_SIG_FINALE     { 0,0,0,0,0,0,0,0 }
#define TGT_SIG_ATHIT      { 0,0,0,0,0,0,0,0 }
#define TGT_SIG_COMBOUI    { 0,0,0,0,0,0,0,0 }
#define TGT_SIG_DMGFONT    { 0x55,0x8B,0xEC,0x6A,0xFF,0x68,0x44,0xC5 }
#define TGT_SIG_NOTIFY38   { 0,0,0,0,0,0,0,0 }
#define TGT_SIG_STATUSTICK { 0,0,0,0,0,0,0,0 }
#define TGT_SIG_CRITEXIT   { 0,0,0,0,0,0,0,0 }
#define TGT_SIG_CAMSHAKE   { 0x55,0x8B,0xEC,0x8B,0x45,0x0C,0x53,0x56 }

/* ---- 状态名（仅作文档；分发用 safe_str_eq 比运行期字符串内容） ---- */
#define TGT_STR_VICTORY_UP_LOOP      0x00724A30UL
#define TGT_STR_DEFEAT_DOWN_LOOP     0x00724A1CUL
#define TGT_STR_R_STAGE_FINALE       0x006F9A94UL
#define TGT_STR_RESULT_SCORE_UP_LOOP 0x006C8880UL
#define TGT_STR_RESULT_COME          0x00724A10UL
#define TGT_STR_RANK_UP              0x00724A40UL
#define TGT_STR_R_ALL_KILL           VIB_ANCHOR_UNRESOLVED  /* 40JP 无此串 */
#define TGT_STR_R_FAINT              VIB_ANCHOR_UNRESOLVED  /* 40JP 无此串 */
#define TGT_STR_R_HK2_3GOOD          0x006C3A84UL
#define TGT_STR_R_HK2_2GOOD          0x006C3A90UL
#define TGT_STR_R_HK2_1GOOD          0x006C3A9CUL

/* ---- 全局对象 / 偏移 ---- */
/* 玩家对象全局【已证实】0x008F7C04
 *   证据：sub_426820(播报层) 用 dword_8F7C04 作对象调 vtable+212 播 "HK_STYLE"；
 *   sub_4D4D40(HP 变更) 第 378 行 `(*a6 vt+576)(a6)==dword_8F7C04 || a6==dword_8F7C04`
 *   = 「来源==玩家」归属比较。393 处引用；城镇源取相邻 +4（ACT1/ACT4/ACT5 均为相邻对）。 */
#define TGT_PLAYER_OBJ           0x008F7C04UL
#define TGT_TOWN_OBJ             0x008F7C08UL
/* ★坐标偏移【已实机标定】：+0xB0 = x / +0xB4 = y / +0xB8 = z(高度)
 * 证据：探针采集（work/out/… + 40JP 实机日志）中 [pos] 只有这一对
 * （以及城镇对象 [posB] 的同一对）随移动平滑同步变化：
 *   x 854→856→780→570→542→577→551→517→452→426→407→316→407→451→479→421→361→300→278
 *   y 326→286→295→332→300→298→286→255→237→208→262→330
 * x 值域 0..1012（大范围，横向）、y 值域 0..330（小范围，纵深）—— 与 DNF 2.5D 地图一致。 */
#define TGT_PLAYER_XOFF          0x0B0UL
#define TGT_PLAYER_YOFF          0x0B4UL
#define TGT_PLAYER_OWNER_OFF     VIB_ANCHOR_UNRESOLVED
#define TGT_PLAYER_HP_SLOT_OFF   VIB_ANCHOR_UNRESOLVED
#define TGT_PLAYER_MP_SLOT_OFF   VIB_ANCHOR_UNRESOLVED
#define TGT_PLAYER_FRENZY_OFF    VIB_ANCHOR_UNRESOLVED
#define TGT_FRENZY_VT            VIB_ANCHOR_UNRESOLVED
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
#define TGT_NUMOBJ_VFTABLE       VIB_ANCHOR_UNRESOLVED
#define TGT_NUMOBJ_CRIT_OFF      0
#define TGT_NUMOBJ_TYPE_OFF      0
#define TGT_NUMOBJ_OWNER_OFF     0
#define TGT_NUMOBJ_LIVE_CNT      VIB_ANCHOR_UNRESOLVED
#define TGT_PLAYER_HIT_ENTRY     VIB_ANCHOR_UNRESOLVED
#define TGT_PLAYER_HIT_PUSH      VIB_ANCHOR_UNRESOLVED

/* ---- P0 加密四件套【未定位】----
 * 40JP 全库搜 0xA1B2C3D4 立即数 = 0 命中（work/out/scan_markers.txt）—— 2006 版
 * 早于该加密方案或采用别种密钥；本版不依赖加密字段（HP 锚点走 vtable，不读加密槽），
 * 故全部保持 UNRESOLVED。使用方（enc_key_old 等）已按 0 安全短路。 */
#define TGT_ENC_TABLE_BASE       VIB_ANCHOR_UNRESOLVED
#define TGT_ENC_KEY_ADDR         VIB_ANCHOR_UNRESOLVED
#define TGT_FIELD_DECODE         VIB_ANCHOR_UNRESOLVED

#else
#error "VIB_TARGET must be 1 (ACT1), 4 (ACT4), 5 (ACT5) or 40 (40JP)"
#endif

#endif /* VIB_TARGET_OLD_H */
