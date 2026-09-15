#ifndef VIB_90CN_ADDRS_H
#define VIB_90CN_ADDRS_H
/* ============================================================
 * 90CN 客户端地址总表 (DNF.exe, MD5 02f218b6..., ImageBase 0x400000)
 * 由 90US DFO.exe 跨版本静态比对定位 (2026-09-14)
 *
 * 置信度标记:
 *   [S] STATIC-CONFIRMED  函数体逐指令一致 / 多模式交叉一致
 *   [V] VERIFY-RUNTIME    待实机探针验证 (探针会采集数据)
 *   [X] DISABLED-V1       v1 禁用 (无法静态确认, 有崩溃风险)
 * ============================================================ */

/* ---------------- hook 目标函数 ---------------- */
/* 行为事件回调 (事件名表 0x42B3F20, 指针表 0x42B4304; 包装器差值法定位)
 * [4轮定稿-DEAD] 私服行为事件系统死代码: 读表器 0x00CF3FC0 全场零调用;
 * 表 26 项中唯二"活跃"项是空桩 (T01=or eax,-1;ret4 / T02=xor eax,eax;ret4),
 * 被引擎其他路径高频调用但无业务逻辑。主 DLL 不 hook 行为事件, 改走
 * FONT+KILL+RANK+SLOT 组合 (见 CN_DAMAGE_FONT / CN_KILL_COUNT_OFF 等)。 */
#define CN_ON_ATTACK        0x00CF4190  /* [S] US 0x7885A0 (死路径, 不 hook) */
#define CN_ON_DAMAGE        0x00CF4070  /* [S] US 0x788480 (死路径, 不 hook) */
#define CN_ON_TARGET_DIE    0x00CF40B0  /* [S] US 0x7884C0 (死路径, 不 hook) */
#define CN_ON_ACTION_END    0x00CF4350  /* [S] US 0x788760 (死路径, 不 hook) */
#define CN_ON_RESET_COMBO   0x00CF42F0  /* [S] US 0x788700 (死路径, 不 hook) */
#define CN_ON_START_BATTLE  0x00CF4320  /* [S] US 0x788730 (死路径, 不 hook) */

/* 镜头震动输入 (字符串 "onShakeInput" @0x48F25B0 唯一引用)
 * [4轮] hook 安装成功(len=10)但整场零调用 (与行为系统同死); 用户指示镜头震动搁置 */
#define CN_ON_SHAKE_INPUT   0x03349740  /* [S] US 0x2AFADB0 */

/* 伤害飘字创建函数 (每次伤害必经, a6=类型标志)
 * [4轮实证] hook 活跃, a6 位分布与 US 完全一致:
 *   0x01=命中(27) 0x02=受击(5) 0x11=命中+特效(3) 0x12=受击+特效(1) 0x22=DOT(14)
 * -> 主 DLL 核心信号源, 分类逻辑直接平移 US vib_dispatch_common_hit */
#define CN_DAMAGE_FONT      0x013ECEC0  /* [S][4轮实证] US 0xE010E0, 60指令逐条一致 */

/* 相机震动函数簇 [探针 v1.4 实测 2026-09-14]
 * CONVERGE 活跃: 26 次调用全部来自死亡特写函数 0x0252D550 (this==n2500 才触发),
 *   参数恒 (1.5f, 100, 150, fn_ret, 1, 0, 1, 0), [ebp+0x24]=byte 标志;
 *   分类窗口 (生产版 hooks_90cn.c s_critShakeWins): 击杀特写 0x024A3A20~0x024A3C29
 *   (US sub_1CB80A0 同源) + 死亡特写 0x0252D550~0x0252D615 (US sub_1D30270 case15);
 *   忽略窗口: 帧更新/screen 转发 0x02639600~0x0263972B + 收尾 0x02637B60~0x02637BEB
 * SCREEN/DIRECT 实测零调用; READBAR 仅"停止"标志 (a3 低字节 0) -> 三个 hook 搁置不发 */
#define CN_SHAKE_CONVERGE   0x026382C0  /* [S][v1.4实证活跃] US 0x1E25540 */
#define CN_SHAKE_SCREEN     0x026396F0  /* [S][v1.4零调用] US 0x1E268F0 */
#define CN_SHAKE_READBAR    0x02638550  /* [S][v1.4仅停止标志] US 0x1E25760 */
#define CN_SHAKE_DIRECT     0x02639D00  /* [S][v1.4零调用, 写+0xA88 非US的+0x9DC] US 0x1E26F00 */

/* 评分系统函数 (thiscall, 直接调用) */
#define CN_FN_SCORE_DMG     0x025E5490  /* [S] US 0x1DD4D60 取当前伤害 */
#define CN_FN_RANK_LEVEL    0x025E2750  /* [S] US 0x1DD2100 等级 0~8 (0=SSS…8=F, 越小越好) */
#define CN_FN_SLOT_SCORE    0x025E4030  /* [S] US 0x1DD3900 槽场景值(double) */

/* ★★ 怪物死亡 —— 最终采用的 hook 目标 (2026-09-15, v2.6): **受击结算函数内的死亡分支入口**
 *
 * 目标地址 = 0x024C4CB5, 位于 CN 0x024C4B10 (受击/伤害结算, US sub_1CD6D30 同源 93.3%) 内部。
 * 该处的机器码与 US 的死亡判定逐条对应:
 *     CN                                        US (sub_1CD6D30 反编译)
 *     call 0x2246B10 / cmp eax,9 / jne          sub_10716B0(v23) == 9
 *     cmp dword [ebp+0Ch],0 / jg 活着           SHIDWORD(a2) <= 0
 *     jl 0x24C4CB5                              a2 < 0
 *     test edi,edi / jne 活着 (edi = [ebp+8])   a2 == 0 (低 32 位)
 *     ---- 0x024C4CB5 = 剩余HP<=0 的落点 ----
 *       vtable+0x420 取当前HP + 三连清零调用       sub_1C819F0(0)/sub_1C81A50(0)/sub_1C51EC0(0,4)
 *       (含 push 4 / push 0, 与 US 的两参调用完全对应)
 *
 * 为什么 hook 分支而不是函数入口: 入口是**每次受击**都走 (高频, 且需自行判 HP 与类型闸);
 *   0x024C4CB5 只被 0x024C4CAB 那一条 `jl` 跳入 (全 .text 唯一, 已核), 因此**只在剩余HP<=0 时触发**,
 *   语义精确、无需复制游戏的条件判断 (尤其不必调用游戏函数去替代 `sub_2246B10(...)==9` 那道类型闸)。
 * 被击者寄存器: **esi** (0x024C4B38 `mov esi,ecx` 赋值; 到 0x024C4CB5 之前 esi 未被改写)。
 * 签名 (16 字节, 全文件唯一): 8B 16 8B 82 20 04 00 00 8B CE FF D0 6A 00 8B CE
 * 覆盖长度: `mov edx,[esi]`(2) + `mov eax,[edx+0x420]`(6) = 8 字节, 指令边界对齐。 */
#define CN_FN_DEATH_BRANCH  0x024C4CB5  /* [S] 受击结算内"剩余HP<=0"死亡分支 (US 条件逐条对应) */

/* 以下为已否证的死亡候选, 保留作"勿再试"记录 ——
 * v1.9 hook 0x02536B60 / v2.0 hook 0x024A18D0 / v2.2 hook 0x02CC8220 —— **实测全部零触发**,
 *   且 0x5DA0 已被判定实验证实是"释放技能"(轮A 放技能不杀怪 -> 21 次上升;
 *   轮B 只用普攻杀 5 只 -> 0 次上升), 故挂在它上面的任何函数都不可能是击杀信号。
 * v2.5 hook 0x024F2210 (US sub_1D00520 死亡回调同源 97.4%, 玩家 vtable 槽 +0x1FC) ——
 *   **hook 安装成功 (len=6) 但整局零触发**: 该 vtable 槽在 CN 实战死亡路径里不被调用。
 *   教训: **同源度高 ≠ 会被调用**; 必须用实测调用计数验证 (铁律四)。
 * 另: 90US 的 sub_F20BF0/sub_F212A0/sub_1C58AC0 在 CN 里**没有同源实现**
 *   (全 .text 比对最高 70.3%/64.1%/43.8%)。 */
#define CN_FN_ON_DIE        0x024F2210  /* [X] 已否证: v2.5 装成功但零触发 (vtable 槽不走实战) */
#define CN_FN_TARGET_DIE    0x02CC8220  /* [X] 已否证: v2.2 零触发, 且实为释放技能路径 */

/* ---------------- 行为事件注册表 (四轮宽网实测定案: 死代码) ----------------
 * 表 26 项, 每项 = 行为回调函数指针。v1.3 宽网全挂 20 项 + 读表器:
 * - 读表器 0x00CF3FC0 (RD1) 全场零调用 -> 分发路径死
 * - 表项唯二活跃 T01/T02 是空桩 (or eax,-1;ret4 / xor eax,eax;ret4)
 * - 其余 18 项全零调用。行为事件路线彻底废弃, 仅存档备查。 */
#define CN_EVENT_TABLE      0x042B4304  /* [S][4轮定案-DEAD] 行为回调注册表基址 (rdata) */
#define CN_EVENT_TABLE_CNT  26          /* 0x42B4304 .. 0x42B4368 */
#define CN_TBL_READER       0x00CF3FC0  /* [S][4轮实证-零调用] 读表/注册器函数 */


/* ---------------- 全局变量 ---------------- */
#define CN_N2500_PTR        0x052B5BB0  /* [S][4轮实证] US 0x4294CE8 玩家对象 (门控有效, vtable=0x0433376C) */
#define CN_RANK_OBJ_PTR     0x051B0C14  /* [S][4轮实证] US 0x4187B54 评分对象 (dmg 累积 138253, level 8->0) */
#define CN_MEM_G_DAMAGE_FONT 0x05182204 /* [S] US 0x415D2B4 飘字管理器 */
#define CN_CAM_OBJ_PTR      0x0517DBA0  /* [S] US 0x4158A38 相机对象 */
#define CN_N964_POS         0x051C3204  /* [X] 三轮实测全程 0.0 无 POS 日志, 此地址错; 位置改走 n2500+0xD0 */
#define CN_ENC_TABLE_BASE   0x052F7450  /* [S] US 0x42D43E0 XOR 解密表基址 */
#define CN_UI_MGR_PTR       0x051B2764  /* [S] US 0x4189AC0 UI管理器 (暴击分离已休眠, 仅探针观测) */
#define CN_FONT_COUNTER     0x05182200  /* [S] US 0x415D2B0 飘字计数器 (=FONT-4) */

/* GDI TextOutW IAT 槽 (来自 PE 导入表, GDI32.dll)
 * [4轮实证] hook 工作正常但 351 条输出全是 UI 零散字符(数字/冒号/括号),
 * CN 战斗文本走自绘位图字体不过 GDI -> 中文关键词路线作废, 主 DLL 不挂 TextOutW */
#define CN_TEXTOUTW_IAT     0x041A2090  /* [S][4轮-作废] US 0x0571C474 */

/* ---------------- 结构偏移 ---------------- */
#define CN_DAMAGE_OBJ_VT_OFF  836       /* [S] US 808 (0x328->0x344) 伤害对象vtable */
#define CN_DAMAGE_VALUE_OFF   68        /* [S] 不变 */
#define CN_VT_SLOT_OFF        0xC54     /* [S][4轮实证] US 0xBD4 槽号 0 有效, fn=0x024A1A30 */
#define CN_KILL_COUNT_OFF     0x5DA0    /* [S][4轮实证 + 0915 订正] US 0x59CC.
                                         * ★不是击杀计数: 实测为 0/1 脉冲标志 (日志中 +1 与
                                         * +4294967295 成对交替), 上升沿 = 释放技能一次
                                         * -> VEV_KILL -> 宿主【释放技能】滑块 idx9。
                                         * 90US 亦早已澄清 n2500[5747] = 技能释放, 非击杀
                                         * (依据: 研究文档/事件采集实施记录合集.md v12/v14)。
                                         * 怪物死亡是独立信号, 见 CN_FN_TARGET_DIE 上方说明 */
#define CN_SCORE_FINAL_KILL   3084      /* [S][4轮实证] US 不变 (0xC0C, 累积 0->7656->0 循环) */
/* [4轮实证-NDIFF] 玩家 n2500 对象字段: +0xD0 = 玩家 X 坐标 (float, ~8910 像素级,
 * 跑动时变化); +0x000 = vtable 0x0433376C; 前 0xF0 内每 2s 变化集中在 0x00-0xF0 */
#define CN_PL_XPOS_OFF      0xD0        /* [V->4轮实证] 玩家 X 坐标 (float), 主 DLL 暂未用 */
#define CN_MEM_CONTAINER_OFF  8         /* [X] 四轮 SNAP cont=0 mgr+8 给垃圾, 此偏移错 (仅 US) */
#define CN_MEM_HEAD_OFF       4         /* [X] 同上, node=0 (仅 US) */

/* [V] 以下待实机探针标定, v1 不启用 */
#define CN_VT_DODGE_OFF       0xFFFF    /* US 0x358, vtable 已漂移(+0x80@3028槽), 未确认 */
#define CN_VT_COMBO_OFF       0xFFFF    /* US 0x62C, 未确认 (死代码, 不用) */
#define CN_MOVE_SPEED_OFF     0x9A8     /* US 0x9A8, [V] v6.2 启用速度映射 (US poll_move 同款):
                                           * 解码失败回退 40; PROBE[5s] MOVE dec= 输出解码值供标定 */
#define CN_CAM_SHAKE_STR      1952      /* US 1952, 未确认 (仅探针观测) */

/* ---------------- PE 布局 (校验用) ---------------- */
#define CN_TEXT_LO   0x00401000
#define CN_TEXT_HI   0x041A1346
#define CN_RDATA_LO  0x041A2000
#define CN_RDATA_HI  0x05097000
#define CN_DATA_LO   0x05097000
#define CN_DATA_HI   0x06251238

#endif /* VIB_90CN_ADDRS_H */
