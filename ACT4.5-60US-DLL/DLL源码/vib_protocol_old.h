#ifndef VIB_PROTOCOL_OLD_H
#define VIB_PROTOCOL_OLD_H
/* ============================================================
 * DFO 老版本手柄震动插件 - 共享协议扩展 v1
 * 基于 1.5 版 vib_protocol.h（common/vib_protocol.h），
 * 补充老版本（LMA1S-release）专属事件类型与锚点。
 * 事件类型与 1.5 版兼容（0-25 语义一致），仅新增老版本扩展段。
 * ============================================================ */
#include <windows.h>
#include "vib_target_old.h"   /* ★版本锚点深模块：本文件 OLD_* 取值由它统一提供 */

#pragma pack(push, 1)

/* ---- 老版本扩展事件类型（26 起，与 1.5 不冲突） ---- */
enum VibEventTypeOld {
    VEVO_STAGE_FINALE   = 26, /* 最终阶段结算启动（R_STAGE_FINALE, 0x4BCC38） */
    VEVO_VICTORY        = 27, /* 通关判定（VICTORY_UP_LOOP, 0x87200B） */
    VEVO_DEFEAT         = 28, /* 失败判定（DEFEAT_DOWN_LOOP, 0x87202B） */
    VEVO_SCORE_UP       = 29, /* 评分上升动画（RESULT_SCORE_UP_LOOP, 0x874BF0） */
    VEVO_RESULT_COME    = 30, /* 结果界面出现（RESULT_COME, 0x871B3A） */
    VEVO_RANK_UP        = 31, /* 评级上升（RANK_UP） */
    VEVO_SCORE_SCROLL   = 32, /* 分数滚动动画完成（RESULT_FLASH） */
    VEVO_REWARD         = 33, /* 奖励弹出（REWARD_*） */
    VEVO_BOSS_DEAD      = 34, /* BOSS 死亡（LastStrikeForKillBoss.ani 演出） */
    VEVO_FONT_HIT       = 35, /* 伤害飘字渲染（%sNumber.img → sub_5F2630） */
    VEVO_FONT_COMBO     = 36, /* 连击数字更新（Combo.img，图集序 33/34） */
};

/* ---- 老版本评分/事件锚点（2026-08-31 IDA 分析确认） ---- */
/* 统一事件咽喉：sub_433890(0x433890) —— 所有演出事件发送器
 * cdecl(const char *name, int a2, int a3, int a4)
 * → strcmp(name,"") 非空则 sub_90DF30(name,0,a3,a4,0,0xFFFF,a2,0)
 * hook 它的栈参 [esp+4]（首个参数 = 事件名指针）做字符串匹配 */
#define OLD_ANCHOR_EVENT_SENDER      0x00433890UL /* sub_433890 */

/* 胜负判定：0x87200B push "VICTORY_UP_LOOP"; call sub_433890
 *          0x87202B push "DEFEAT_DOWN_LOOP"; call sub_433890
 * [esi+0xC0] > 0 → 胜利；<= 0 → 失败（演出类对象 esi） */
#define OLD_VICTORY_PUSH             0x0087200BUL
#define OLD_DEFEAT_PUSH              0x0087202BUL

/* 评分对象：dword_B5B410 (0xB5B410) = 全局评分对象指针
 * 对象 +932 = 总评数值；sub_4D2410 将该值映射百分制 0~105
 * 加分汇聚：sub_4D2C50(0x4D2C50) 48 槽累加
 * 播报层：sub_4E6DA0(0x4E6DA0) switch a2: 2=连击 3=被击 5=击杀 6=STYLE 7=TECHNIC */
#define OLD_SCORE_OBJ                0x00B5B410UL
#define OLD_SCORE_TOTAL_OFF          932
#define OLD_SCORE_ACCUMULATOR        0x004D2C50UL
#define OLD_EVENT_BROADCAST          0x004E6DA0UL

/* 全局单例簇（sa_global 确认） */
#define OLD_UI_MANAGER               0x00B5B380UL /* UI/战斗UI管理器（sub_42DCB0 构造） */
#define OLD_MAIN_CONTEXT             0x00D01D00UL /* 主上下文（sub_9054C0 直返，+36/+40 管理器槽） */
#define OLD_ENC_TABLE_BASE           0x00CFD458UL /* 加密属性表基址（值=ctx 对象，ctx+0=容量 0x200000） */
#define OLD_ENC_KEY_ADDR             0x00CFD45CUL /* ★v13.19 定案(sa_hp_formula_fix): 密钥全局变量的【地址】。
                                                   * 密钥值 = *(DWORD*)0x00CFD45C = 0xA1B2C3D4 ^ ((rand()<<16)|rand()),
                                                   * 每次启动随机(sub_901E40), 必须运行时解引用, 绝不可当立即数!
                                                   * 旧宏 OLD_ENC_XOR_KEY 曾把本地址当 XOR 常数 → v13.18 实测
                                                   * HP 公式 ok 恒 0 的全部根因; score/monster 解密值同步失真。 */
#define OLD_ENC_XOR_KEY              OLD_ENC_KEY_ADDR /* 兼容别名: 语义=密钥地址, 使用必须经 enc_key_old() 解引用 */
#define OLD_MONSTER_CONTAINER        0x00BDBFCCUL /* 怪物容器头 */
#define OLD_MONSTER_LIST             0x00BDBF9CUL /* 怪物条目数组（88B/项） */
#define OLD_TEXT_MANAGER             0x00B5DC68UL /* 文本管理器 */
#define OLD_DUNGEON_INFO             0x00B5DCECUL /* 副本信息对象 */

/* 战斗/结算判定链 */
#define OLD_BATTLE_STATE             0x00B5D6F0UL /* dword_B5D6F0，+16==3 表示地城模式 */
#define OLD_MONSTER_REMAIN           0x00B6F608UL /* dword_B6F608 = 剩余怪物计数（<=0 清场） */
#define OLD_FINALE_SENDER            0x004BCA70UL /* sub_4BCA70 = 地城阶段结算流程（R_STAGE_FINALE 发送者） */

/* 字段解密（读任何加密字段前必经）：sub_402030(this)
 * ★v13.19 语义定案(sa_hp_formula_fix_result): v1=dword_CFD45C 是【密钥全局变量
 * 的值】(运行时随机), 不是常数 0x00CFD45C; 旧注释把 IDA 符号 dword_CFD45C
 * 误读成立即数, 是 v13.18 HP 公式 ok 恒 0 的根因。
 * 校验: (key ^ slot[0]) - slotAddr == slot[1]; 解密: 表值 ^ key ^ ea。 */
#define OLD_FIELD_DECODE             0x00402030UL

/* 结算震动实体：sub_4E3B40（3000~5600ms 衰减 → sub_8FDA80 双通道物理输出） */
#define OLD_RESULT_SHAKE             0x004E3B40UL

/* 伤害字体渲染入口（若需飘字级采集）：
 * sub_5F2630 → xref 0x5F2743 加载 %sNumber.img（手雷/伤害数字）
 * sub_45CFF0 → xref 0x45D12E 加载 Interface/mininumberset.img */
#define OLD_FONT_HIT_RENDER          0x005F2630UL
#define OLD_FONT_MINIRENDER          0x0045CFF0UL

/* ---- 第三轮源头定案新增锚点（2026-08-31，报告 17-22） ---- */
/* 玩家对象：dword_B75A14 = CNUser 当前玩家（849 refs；写点 0x4FD92D attach / 0x4FD39A 清除）
 * vtable+228 = 语音标签播放("HK_*")；+0x2270 = 存活标志；BF163C = 主框架（非玩家） */
#define OLD_PLAYER_OBJ                0x00B75A14UL

/* ★v13.18 玩家 HP 槽修正(sa_player_hp_slot_result 四轮IDA一锤定音, 推翻报告17-22旧定案):
 * 真 HP 槽描述符 = obj+0x1168(完整性标记) / +0x116C(表下标); MP 同构 +0x1170。
 * 铁证: 游戏自身 GetHP thunk 0x00652A20 `add ecx,1168h; jmp sub_402030`;
 * SetHP(sub_481C90)/受击扣血(sub_483F50@0x48426D)/DOT扣血(sub_4CF1E0)/
 * 周期重同步(sub_48BA40) 全走 +0x1168 槽。
 * 明文 = 存储 ^ *(DWORD*)CFD45C ^ (表基A+4*idx); 表A/B 是镜像冗余(非64位双表), 读A即可。
 * 完整性校验: (*(DWORD*)CFD45C ^ slot[0]) - slotAddr == slot[1], 不过即丢本轮。
 * 旧宏 0x1828/0x1824 已证伪: 那是 +0x1814 起「限时状态块(SuperArmor系)」的
 * 激活/剩余字段, 非 HP —— v13.17 按旧定案读 = 把状态块伪随机完整性标记当
 * 表下标, 读垃圾且潜伏 AV 崩游戏风险。
 * 「vtable+728 HP 速查口」亦证伪: sub_47F450 是 +0x1864 明文字段 setter, 调用会破坏字段。 */
#define OLD_PLAYER_HP_SLOT_OFF        0x1168UL  /* HP: 完整性标记@+0x1168, idx@+0x116C */
#define OLD_PLAYER_MP_SLOT_OFF        0x1170UL  /* MP: 同构(完整性@+0x1170, idx@+0x1174) */

/* 玩家受击（第三轮定案，报告 18/22）：
 * sub_4A1B50 = 受击入口（++B5ED28 被击次数 + 受击特效）
 * sub_483F50 @0x4847BE = a2==3 被击播报唯一触发点（if B75A14==this && v68>0: sub_4E6DA0(3,v68)）
 * 0x4810EC 是 a2==0 重置分支（非被击，早前误判已更正） */
#define OLD_PLAYER_HIT_ENTRY          0x004A1B50UL
#define OLD_PLAYER_HIT_PUSH           0x004847BEUL

/* ★v13.18 受击统计块（sa_b5ed28_hitcounter_result 五轮IDA定案, 置信~95%）：
 * B5ED28 = 本地玩家被击次数(普通DWORD, 唯一++点 0x4A1BFC 在「被击者==B75A14
 *   && vtable+1388 敌方有效命中(非无敌帧)」双重门控内; 玩家打怪/怪互殴/无敌帧
 *   被击均不++, 零误报)。唯一清零 sub_466B50(离开副本/teardown/会话初始化)。
 * B5ED24 = 同门控同清零的累计受伤值(可用于强度分级, 暂只日志)。 */
#define OLD_HIT_COUNT                 0x00B5ED28UL
#define OLD_DAMAGE_TOTAL              0x00B5ED24UL

/* 玩家攻击命中（报告 22）：sub_4748D0 = 伤害应用+数字显示决策
 * a1[249]==B75A14 → 玩家被打；否则玩家攻击命中；v10=伤害、a3 &0x10=暴击 */
#define OLD_PLAYER_ATTACK_HIT         0x004748D0UL

/* 主框架震屏接口：BF163C vtable+28 (19,5)/(20,2)/(20,6) = 游戏自带命中震屏 */
#define OLD_MAIN_FRAME                0x00BF163CUL

/* 战斗/结算对象生命周期（报告 20）：
 * B759C8 = 当前战斗对象（唯一写点 sub_4FCA90@0x4FCAB8，mode6→B75AA8、7→B75CB8）
 * 结算窗 +0xC0 胜负字段（>0 胜 / ==0 无 / <0 败；0x871FDE 读） */
#define OLD_BATTLE_OBJ                0x00B759C8UL
#define OLD_RESULT_WIN_FLAG           0x00871FDEUL

/* ---- ★v13.21/v13.22 镜头震动 + 被控制（报告 35/36, 2026-09-07） ----
 * 镜头震动汇聚点: sub_4E49E0, 68 处 xref(全部技能/特效震屏的唯一出口, 等价新版
 * sub_1E25540)。__thiscall(this=相机对象, a2=震屏度数, a3=第二参):
 *   入口 6B 普通指令 push ebp(1)+mov ebp,esp(2)+mov eax,[ebp+arg_4](3), 返回点
 *   0x004E49E6 push ebx —— 非 E8/E9/JMP, E9+trampoline hook 安全(铁律合规)。
 *   度数存 this+0x11C, a3 存 this+0x120; a2==0 分支清 +0x164/+0x168(=停震调用)。
 *   v13.21 实测: hook 安装成功(idx=12 OK), 无崩溃无副作用; 触发待震屏技能验证。
 * ⚠️ 条件码字段(教训 18, 报告 36 实测推翻 v13.21 判定):
 *   +0x42C(明文)/+0x1434(加密镜像, 滞后) = 「动作/状态汇总」非「状态身份」:
 *     8=玩家攻击动作(每刀触发, 未命中也触发——v13.21 误判"被抓"致攻击震 0x20,
 *     已回退), 9=硬控共码(眩晕/冰冻/石化 tick 共写; 消费侧铁证 sub_48A460
 *     case9→CONDITION_STUN), 3/4/5=受击反应, 6/11/22=其他自身动作, -1=无。
 *   到期时刻 +0x440。勿再用于事件判定(留档供日志参考)。
 * ★v13.22 CC 正确信号源 = ActiveStatus 类型字段(状态对象+4):
 *   {1=冰冻, 3=眩晕, 7=石化, 8=睡眠}(容差表倒序模型+消费侧动画铁证, 报告 36),
 *   由 stub10(sub_4D10F0, 已实测) 的 vib_dispatch_dot_active 扩类型集实现。 */
#define OLD_CAM_SHAKE_APPLY           0x004E49E0UL /* sub_4E49E0 镜头震动汇聚点(68 xref) */
#define OLD_PLAYER_COND_OFF           0x42CUL      /* 明文条件码(动作汇总: 8=攻击 9=硬控; 勿用于判定) */
#define OLD_PLAYER_CONDEXP_OFF        0x440UL      /* 条件到期 tick(SetCondition 第3参) */
#define OLD_PLAYER_COND_ENC_OFF       0x1434UL     /* 加密条件码镜像(随明文联动; 勿用于判定) */

/* ★v13.23 血之狂暴(Frenzy)显式标志(sa_frenzy_result 五轮 IDA 定案):
 * player+0x2A14 = CNFrenzy 对象指针 —— 施法时 sub_638640@0x642FE4 检查为 0
 * 则创建 CNFrenzy 并写入(0x643011); buff 结束清 0(0x6373BE 等)。
 * 掉血链: CNFrenzy::tick(sub_5384F0)→sub_537D50(dt×攻速×0.001 累加器@this+0x98
 * 蓄满 1.0 扣 1 点)→vt+0x328(sub_48100)→尾部 vt+0x32C(sub_4A1E40@0x4811E5)→
 * sub_481C90→写 +0x1168 加密槽 —— 全程无伤害数字(sub_470ED0)、不走 sub_4D10F0
 * 调度器、被击计数 B5ED28 不增(用户实测"无红字"吻合; 三重关联门全闭 → 静默)。
 * HP 地板保护@0x537DFF(不足扣时设 1)。判定: *(player+0x2A14)!=0 = buff 激活中。 */
#define OLD_PLAYER_FRENZY_OFF         0x2A14UL     /* CNFrenzy 对象指针(0=未激活) */

/* ---- 老版本专用共享内存旗标 ---- */
#define VIB_SHM_OLD_FLAG_FINALE      0x00000001UL
#define VIB_SHM_OLD_FLAG_VICTORY     0x00000002UL
#define VIB_SHM_OLD_FLAG_DEFEAT      0x00000004UL
#define VIB_SHM_OLD_FLAG_SCORE_UP    0x00000008UL

/* ---- ★v13.18 暴击数字对象无损轮询(sa_crit_poll_spec_result 定案, 默认 LOG_ONLY) ----
 * 纯读枚举链: mgr=*(B5DCE0) → ctx=*(mgr+0x200010) → layer=*(ctx+0xB0)
 *   → 主 vector [*(layer+0x10), *(layer+0x14)); 元素过滤 vftable==0x99D21C
 *   且 +0x1AC 非合法堆指针排除后非0 = 活暴击数字。
 * 更正旧报告: 真 vftable=0x99D21C(0xA9ED88/D4 是 RTTI 结构);
 * B5C4C4 是调试日志 string 非容器; 0x470BE8 是清零、暴击写点 0x470EA8。 */
#define OLD_BATTLE_CORE          0x00B5DCE0UL  /* 战斗核心单例(离线 0xFFFFFFFF) */
#define OLD_BATTLE_CORE_CTX_OFF  0x200010UL    /* +0x200010 主上下文(sub_438850 静态等价) */
#define OLD_CTX_LAYERMGR_OFF     0x000000B0UL  /* ctx+0xB0 图层管理器(sub_4C7AE0 的 a1) */
#define OLD_LAYER_VEC_BEGIN_OFF  0x00000010UL  /* 主 vector<CNRDObject*> begin */
#define OLD_LAYER_VEC_END_OFF    0x00000014UL  /* 主 vector end */
#define OLD_NUMOBJ_VFTABLE       0x0099D21CUL  /* CNRDNumberObject 唯一 vftable */
#define OLD_NUMOBJ_CRIT_OFF      0x000001ACUL  /* +0x1AC 暴击动画指针(非0即暴击) */
#define OLD_NUMOBJ_TYPE_OFF      0x000000C8UL  /* +0xC8 SetType(恒2, 二道校验) */
#define OLD_NUMOBJ_OWNER_OFF     0x000000E4UL  /* +0xE4 SetOwner(恒layer, 二道校验) */
#define OLD_NUMOBJ_LIVE_CNT      0x00B5EFD4UL  /* 活数字计数(0..30, 旁证) */

/* ============================================================
 * ★版本剖面接管（vib_target_old.h）
 * 上面各 OLD_* 的 ACT1 硬编码值在此统一改由版本剖面提供：
 *   ACT1 剖面取值与上面逐字一致（零回归）；ACT5 剖面换成 ACT5 值，
 *   未定位项为 VIB_ANCHOR_UNRESOLVED(0) → 使用方必须跳过。
 * 换版本只改 vib_target_old.h，本文件与引擎代码不动。
 * ============================================================ */
#undef  OLD_ANCHOR_EVENT_SENDER
#define OLD_ANCHOR_EVENT_SENDER  TGT_HOOK_SENDER
#undef  OLD_SCORE_OBJ
#define OLD_SCORE_OBJ            TGT_SCORE_OBJ
#undef  OLD_SCORE_TOTAL_OFF
#define OLD_SCORE_TOTAL_OFF      TGT_SCORE_TOTAL_OFF
#undef  OLD_SCORE_ACCUMULATOR
#define OLD_SCORE_ACCUMULATOR    TGT_HOOK_ACCUM
#undef  OLD_EVENT_BROADCAST
#define OLD_EVENT_BROADCAST      TGT_HOOK_BROADCAST
#undef  OLD_ENC_TABLE_BASE
#define OLD_ENC_TABLE_BASE       TGT_ENC_TABLE_BASE
#undef  OLD_ENC_KEY_ADDR
#define OLD_ENC_KEY_ADDR         TGT_ENC_KEY_ADDR
#undef  OLD_ENC_XOR_KEY
#define OLD_ENC_XOR_KEY          TGT_ENC_KEY_ADDR
#undef  OLD_FINALE_SENDER
#define OLD_FINALE_SENDER        TGT_HOOK_FINALE
#undef  OLD_FIELD_DECODE
#define OLD_FIELD_DECODE         TGT_FIELD_DECODE
#undef  OLD_RESULT_SHAKE
#define OLD_RESULT_SHAKE         TGT_HOOK_RESULT
#undef  OLD_PLAYER_OBJ
#define OLD_PLAYER_OBJ           TGT_PLAYER_OBJ
#undef  OLD_PLAYER_HP_SLOT_OFF
#define OLD_PLAYER_HP_SLOT_OFF   TGT_PLAYER_HP_SLOT_OFF
#undef  OLD_PLAYER_MP_SLOT_OFF
#define OLD_PLAYER_MP_SLOT_OFF   TGT_PLAYER_MP_SLOT_OFF
#undef  OLD_PLAYER_HIT_ENTRY
#define OLD_PLAYER_HIT_ENTRY     TGT_PLAYER_HIT_ENTRY
#undef  OLD_PLAYER_HIT_PUSH
#define OLD_PLAYER_HIT_PUSH      TGT_PLAYER_HIT_PUSH
#undef  OLD_HIT_COUNT
#define OLD_HIT_COUNT            TGT_HIT_COUNT
#undef  OLD_DAMAGE_TOTAL
#define OLD_DAMAGE_TOTAL         TGT_DAMAGE_TOTAL
#undef  OLD_PLAYER_ATTACK_HIT
#define OLD_PLAYER_ATTACK_HIT    TGT_HOOK_ATHIT
#undef  OLD_CAM_SHAKE_APPLY
#define OLD_CAM_SHAKE_APPLY      TGT_HOOK_CAMSHAKE
#undef  OLD_PLAYER_FRENZY_OFF
#define OLD_PLAYER_FRENZY_OFF    TGT_PLAYER_FRENZY_OFF
#undef  OLD_BATTLE_CORE
#define OLD_BATTLE_CORE          TGT_BATTLE_CORE
#undef  OLD_BATTLE_CORE_CTX_OFF
#define OLD_BATTLE_CORE_CTX_OFF  TGT_BATTLE_CORE_CTX_OFF
#undef  OLD_CTX_LAYERMGR_OFF
#define OLD_CTX_LAYERMGR_OFF     TGT_CTX_LAYERMGR_OFF
#undef  OLD_LAYER_VEC_BEGIN_OFF
#define OLD_LAYER_VEC_BEGIN_OFF  TGT_LAYER_VEC_BEGIN_OFF
#undef  OLD_LAYER_VEC_END_OFF
#define OLD_LAYER_VEC_END_OFF    TGT_LAYER_VEC_END_OFF
#undef  OLD_NUMOBJ_VFTABLE
#define OLD_NUMOBJ_VFTABLE       TGT_NUMOBJ_VFTABLE
#undef  OLD_NUMOBJ_CRIT_OFF
#define OLD_NUMOBJ_CRIT_OFF      TGT_NUMOBJ_CRIT_OFF
#undef  OLD_NUMOBJ_TYPE_OFF
#define OLD_NUMOBJ_TYPE_OFF      TGT_NUMOBJ_TYPE_OFF
#undef  OLD_NUMOBJ_OWNER_OFF
#define OLD_NUMOBJ_OWNER_OFF     TGT_NUMOBJ_OWNER_OFF
#undef  OLD_NUMOBJ_LIVE_CNT
#define OLD_NUMOBJ_LIVE_CNT      TGT_NUMOBJ_LIVE_CNT
#undef  OLD_MONSTER_REMAIN
#define OLD_MONSTER_REMAIN       TGT_MONSTER_REMAIN

#pragma pack(pop)

#endif /* VIB_PROTOCOL_OLD_H */