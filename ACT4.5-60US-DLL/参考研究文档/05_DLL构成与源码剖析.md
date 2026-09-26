# 05 DLL 构成与源码剖析

> 覆盖三处源码仓：`DfoVibration_OLD\DLL源码\`（老版 ACT1）、`【90US】…1.5版\DLL源码\`（新版 90US）、`DfoVibration V3版本\DLL源码\`（新版，与 1.5 逐字节相同）。
> 引用带 `文件:行号`；新版逐文件剖析的权威来源是 `DfoVibration_OLD/docs/阶段报告/04_1.5DLL源码剖析.md`（下文简称《04 报告》）。

---

## 一、三个源码仓的关系（先看这张表）

| 仓 | 定位 | 关键文件 |
|---|---|---|
| `1.5版\DLL源码\` | **源**（新版 90US 采集 DLL） | dfovib.c(667) / hooks.c(1114) / hooks.h(80) / hooks_asm.s(280) / common/vib_protocol.h(223) / common/ini_util.* / build.bat / install.bat / memtest*.c / tests/ |
| `V3\DLL源码\` | **与 1.5 版完全相同**（V3 的改动全在宿主） | 同上（md5 逐一相同） |
| `DfoVibration_OLD\DLL源码\` | **派生**（老版 ACT1 采集 DLL + 注入体系） | dfovib_old.c(483) / hooks_old.c(1111) / hooks_old.h(34) / hooks_old_asm.s(355) / vib_protocol_old.h(188) / common/（复用） / loader_main.c(105) / inject_x86.c(115) / version_proxy.c(224) / dinput8_proxy.c(146) / build_old.bat / install_old.bat / tests/ |

**md5 验证（2026-09-09，只读比对）——V3 与 1.5 版五个核心文件逐字节一致：**

| 文件 | md5 |
|---|---|
| dfovib.c | `94cc36add0ebc1f171911d2315884548` |
| hooks.c | `d528ab2699aa9fc62ec1ab3df50d734a` |
| hooks.h | `de9a0efe4a543ed07e2b3c87bb652d0f` |
| hooks_asm.s | `6e58b9c1c7ae16f26e99a95b770925d8` |
| common/vib_protocol.h | `f6aa7c33dce256d6a07dbbe59ba40fee` |

结论：**V3 的 DLL 没有变化**（仍为 1.5 的 DfoVibration.dll），V3 的全部演进在宿主；老版 DLL 是从 1.5 骨架派生的重写系（worker 1s 延迟 + collector 6ms 结构同构，`dfovib_old.c:6` 注释自证「参考 1.5 版 dfovib.c 骨架」）。

**编译产物也相同**：两处的 `DfoVibration.dll`（89,088 B）md5 均为 `4aaf7ae49b03ff833c8d289d3bc3b51f`——不仅源码一致，交付的二进制也是同一个。因此「V3 与 1.5 的 DLL 行为完全一致」是可直接验证的事实，不是推测。

---

## 二、新版 DLL 剖析（`dfovib.c` + `hooks.c` + `hooks_asm.s`）

### 2.1 文件职责
| 文件 | 职责 |
|---|---|
| `dfovib.c` | DLL 入口、共享内存生命周期、worker/collector 线程、TextOutW IAT hook、NPK 字符串扫描、EXE 自动拉起 |
| `hooks.c` | x86 inline hook 引擎（安装/卸载/trampoline）、事件分发 `vib_dispatch_*`、文字分类器、纯内存轮询采集 |
| `hooks_asm.s` | 13 个汇编 stub（保存现场 → 调 C 分发 → 跳 trampoline），GNU as / Intel 语法 / `.code32` |
| `hooks.h` | hook 引擎接口 + `vib_tramp_0..N` 全局变量声明 |
| `common/vib_protocol.h` | 共享内存 ABI 契约（事件枚举/结构/钩子地址表/配置结构） |
| `common/ini_util.*` / `toml.*` | ini/toml 读取 |
| `memtest*.c` | 内存基址验证工具族（研究用，非成品） |
| `tests/` | test_load / test_toml |

### 2.2 导出函数与生命周期（《04 报告》§1）
```c
DfoVibrationLoaded()  // dfovib.c:625 官方启动器调用；CAS 幂等，未启动则 CreateThread
VibPluginLoaded()     // dfovib.c:634 注入器调用（同义）
VibPluginDeinit()     // dfovib.c:639 g_running=0
DllMain(ATTACH): DisableThreadLibraryCalls → CAS → CreateThread(worker) + CreateThread(collector)
DllMain(DETACH): g_running=0 → 等 ≤1500ms → hooks_uninstall + textout_hook_uninstall + shm_close
```
worker 主循环（`dfovib.c:461-622`）：`Sleep(1000)` 避 loader lock → 解析自身模块路径（`GetModuleFileNameA(g_hDllMod)`，NULL 会拿 EXE 路径；手动映射注入兜底 psapi 枚举）→ 日志/ini 名由 DLL 文件名派生 → 每 500ms 读 ini、`enabled` 变化时装/卸 hook、把 hook 地址写 `g_shm->apiVersion[7]` → 每 60s hook 首字节自检 → `autostart_exe()` 拉起面板。
collector（`dfovib.c:181-257`）：6ms 轮询 + `vib_flush_counts()`（把 hook 计数器增量批量发事件）。

### 2.3 hook 引擎（《04 报告》§2）
- **Patch**：`p[0]=0xE9`、`*(DWORD*)(p+1)=stub-target-5`、`p[5..hookLen-1]=0x90`（`hooks.c:334-401`）。
- **变长 hookLen**：`insn_len()`（手写精简反汇编器，`hooks.c:226-296`，覆盖 90/C3/CC/F4…、push/pop、mov、jcc、modrm 展开等）+ `calc_hook_len()`（从 5 字节累加到完整指令边界，`hooks.c:299-308`）。
- **安全**：`first_insn_safe()` 拒绝 `E9/E8/EB/C3/C2` 开头（`hooks.c:311-317`）；`target_writable()` 要求页已提交且可执行（`hooks.c:320-332`）。
- **Trampoline**：一次 `VirtualAlloc(VIB_MAX_HOOKS*32, PAGE_EXECUTE_READWRITE)`，每槽 memcpy 原指令 + `E9` 回跳（`hooks.c:364-384`）。
- **HookCtx**：`{target, patchRel, original[16], hookLen, stub, trampVar, installed}`；追加式安装（7 行为 hook + 6 功能 hook 两批）。
- **stub 栈偏移**（`hooks_asm.s`）：pushfd(4)+pushad(32)=36 → 返回地址 `[esp+36]`、a2 `[esp+44]`、a3 `[esp+48]`、a6 `[esp+60]`。各 stub 参数个数不同、`add esp` 不同（stub0-6 用 16；stub7 用 20；stub8 用返回地址分类；stub11 用 8；stub12 用 12）。事件类型是硬编码立即数，**与 stub 索引无关**（stub5 是 TargetDie 却 push 2）。

### 2.4 事件分发族（`vib_dispatch_*`）
`vib_dispatch_common(this,a2,a3,type)`：VEV_DAMAGE 经伤害对象 `vtable+808` → `+68` 取伤害值（>100000 截断），VEV_RESET_COMBO 用剩余伤害百分比，其余默认。
`vib_dispatch_common_hit(this,a2,a3,a6,type)`：飘字 a6 分类，只做原子计数。
`vib_dispatch_skillhit(this,strength,time,retaddr,type)`：**用返回地址区分调用者**（击杀特写区 0x1CB8xxx/0x1D302xx → VEV_CRIT_SHAKE；相机内部 0x1E268xx/0x1E24Exx/0x2C56xxx → 忽略去重）。
其余：`shake` / `readshake` / `targetdie` / `directshake`。

### 2.5 性能核心：计数聚合 + 12ms 批量冲刷
```c
// hooks.c:150-155 回调只计数
if (a6 & 0x20) InterlockedIncrement(&g_cnt_hp); else if (a6 & 0x10) ...
// hooks.c:174-189 collector 每 12ms
v = InterlockedExchange(&g_cnt_hp, 0);
if (v > 0) vib_collect(VEV_FONT, 0x20, GetTickCount(), (DWORD)v);
```
《04 报告》原话：「**彻底消除高频卡顿，是本项目最关键的性能技巧**」。

### 2.6 内存轮询族
`rank_poll`（`hooks.c:475-514`）：读 `0x04187B54` → 直调 `sub_1DD4D60`/`sub_1DD2100`，等级变化才发。
`poll_kill`（672-686）：`n2500(0x4294CE8)+0x59CC`。
`poll_move`（718-755）：`n964(0x418A458)+4/+8` float 坐标；`vib_decode()` 解移速。
`poll_cam_shake`（896-918）：相机 `0x4158A38+1952` 上升沿。
极限闪避（598-618）：`(*n2500+856)(n2500)` 上升沿，开局延迟 3s。
评分点（621-637）：评分对象 `+3084` 明文增量。
破甲/凌空（549-585）：`FN_1DD3900` 槽值增量，**槽参是整数位模式**。
飘字容器（`dfovib.c:213-245`）：`MEM_G_DAMAGE_FONT=0x0415D2B4` → +8 容器 → +4 头节点；80ms 去抖；节点消失 >500ms → VEV_ACTION_END。

### 2.7 加密解码 `vib_decode`（移速/计数）
```c
// hooks.c:702-716
hi = enc>>16; lo = enc&0xFFFF;
tbl2 = *(DECODE_TBL_BASE(0x42D43E0) + hi*4 + 0x24);
t = *(tbl2 + lo*4 + 0x2114);
v = ((t&0xFFFF)<<16)|(t&0xFFFF);
return v ^ *(adr + offset);   // 表查 XOR 还原
```

### 2.8 共享内存写入（CAS 环）
```c
// dfovib.c:82-111
for(;;){ head=r->head; tail=r->tail;
  if ((DWORD)(head-tail) >= r->capacity) return;      // 满则丢弃（回绕安全）
  pos = head % r->capacity;                            // 跨边界拆两段 memcpy
  if (CAS(&r->head, head+sz, head) == head) break; }
InterlockedIncrement(&g_shm->seq); g_shm->lastTick = ev->tick;
```
**DLL 是纯生产者**：只推进 head，tail 由宿主消费者推进。

### 2.9 TextOutW IAT hook（遗留）
IAT 槽 `0x0571C474`；`MyTextOutW` 复制文本 ≤95 wchar → `classify_font_text()`（SSS/EXCELLENT→100、SS/GREAT→80、S/GOOD→60、CRITICAL→80、破招/BREAK→80、背击/BACK→40、GUARD→50、AIR→20、FINAL→40、COMBO→10、PERFECT→70、ARMOR→30、DODGE→30）→ 命中发事件 → 转发原函数；卸载校验 `*slot==MyTextOutW` 才还原。
⚠ 1.5 研究结论：游戏 UI 走内部位图字体，**TextOutW 实测零调用**，此路径已证伪（代码保留）。

---

## 三、老版 DLL 剖析（`dfovib_old.c` + `hooks_old.c` + `hooks_old_asm.s`）

### 3.1 入口与线程（`dfovib_old.c`）
- 头注释：`挂载: Loader 远程注入 (LoadLibraryA + VibPluginLoaded 契约)`；参考 1.5 骨架。
- 导出只有 `VibPluginLoaded()`（455-461）；`DllMain`（463-484）里 `g_running=1`（v13.7 修复：此前恒 0 导致所有轮询从未运行）+ 双线程。
- worker（355-451）：`Sleep(1000)` → 建共享内存（`CreateFileMappingA(... sizeof(VibShm)+VIB_RING_SIZE)`）→ 日志路径 `DfoVibration_OLD_dll.log` + 2MB 轮转（v13.39）→ `hooks_install_old()` 打印 `hooks installed: N/10` → 每 500ms 打 30s 统计 tick（`push=`/`font a=/h=/sp=/dot=`）+ 60s hook 完整性自检。
- collector（335-352）：6ms 依次 `poll_score_old` / `poll_monsters_old` / `poll_move_old` / `poll_dot_old`（已暂停）/ `poll_hit_counter_old` / `poll_player_hit_old` / `poll_crit_number_old` / `vib_flush_font`。
- `ring_push`（34-51）：与新版同构 CAS，但用 `& (VIB_RING_SIZE-1)` 掩码（要求容量为 2 的幂）。

### 3.2 发送去抖体系（`dfovib_old.c:53-206`）
- `VIB_DEDUP_MS=40` / `_BATTLE=120` / `_SCORE=200`；`is_settlement_type()` 白名单放行。
- **FONT 子槽**（149-158）：0x02→49、0x01→48、0x10→52、0x20→53、0x08→51、0x04→50（其余 slot=type&0x3F）。
- **命中子槽单独 10ms**（179-180，v13.35）。
- 去抖时保留峰值强度（`s_lastStr[slot]`）。
- **v13.39 聚合冲刷**（87-133）：`vib_collect()` 若 `type==VEV_FONT && count==1` 且是七种主分类之一 → `InterlockedIncrement(&g_aggFont[i])` 直接返回；collector 的 `vib_flush_font()` 取增量后**逐条调 `vib_collect_raw`**（count=1 保连击语义）；罕见组合直发保真。
- 统计：`g_statPush` / `g_statFont[6]`。

### 3.3 hook 表与引擎（`hooks_old.c`）
- 13 个目标 + `g_targetLens[13]`（`hooks_old.c:531-546`），idx11（0x790D93）在安装循环里 `continue` 跳过（v13.16 无损改造，注释写明「E9 JMP 改写 + trampoline 重放会崩」）。
- 各 hookLen 构成逐条注释（如 0x433890=6：push ebp+mov ebp,esp+mov eax,[ebp+8]）。
- `target_writable`（548-556）与新版不同：**只要求页已提交且可执行**（不要求可写），真正改权限由 `hook_one` 里的 VirtualProtect 完成——注释记录「要求可写会导致 0/6 全失败（实机验证）」。
- 分派函数见 `hooks_old.h`：`vib_dispatch_event_sender/broadcast/accumulate/voice/result_shake/finale/camera_shake` + 轮询 `poll_score_old/poll_monsters_old/poll_player_hit_old/poll_hit_counter_old/poll_crit_number_old`。
- 加密读取：`enc_key_old()` / `read_enc_dword()` / `enc_slot_ok()`（交接文档 4.2 节）。
- `vib_ptr_ok` 排除模块映像区（`hooks_old.c:989`：`p>=0x401000 && p<0xF3C000` 返回 0）——注意教训 12：对常量地址套堆指针判据会让 LLVM -O2 常量折叠出恒假分支，整段代码被当死代码消除。

### 3.4 `hooks_old_asm.s`（355 行）
与新版同构的 stub 模板，13 个；额外处理返回地址取参（暴击/震屏分类）。老版注释更详尽。

---

## 四、Loader 与代理 DLL（老版专属）

### 4.1 `loader_main.c`（105 行）
- 两种模式：无参 = 常驻轮询（`poll_thread` 每 2s 用 Toolhelp32 找 `DNF.exe`，PID 变化即 `inject_process`）；带参 = 启动器模式（`launch_and_inject(argv[1], dllPath)`）。
- DLL 路径 = loader 同目录的 `DfoVibration_OLD.dll`。

### 4.2 `inject_x86.c`（115 行）
- `inject_process(pid,dllPath)`：`OpenProcess(PROCESS_ALL_ACCESS)` → `GetModuleHandleA("kernel32.dll")`+`GetProcAddress("LoadLibraryA")` → `VirtualAllocEx` 写路径 → `CreateRemoteThread(LoadLibraryA, pRemote)` → `WaitForSingleObject(hThread, 10000)`。
- `launch_and_inject`：`CreateProcessA(CREATE_SUSPENDED)` → 注入 → `ResumeThread`。
- 注释明确：x86→x86；老版 32 位进程，loader 必须 32 位。

### 4.3 代理 DLL
- `version_proxy.c`（224 行）：游戏目录放 `version.dll` 冒充；**转发 System32\version.dll 全部 17 个导出**（绝对路径 `GetSystemDirectoryW`）；延迟 1s `LoadLibraryA("DfoVibration_OLD.dll")`。先例：工作区已接受的 SimSunFontHook 同款。
- `dinput8_proxy.c`（146 行）：同理冒充 `dinput8.dll`，转发真身 6 个导出。
- 原理：Windows DLL 搜索「应用目录优先于 System32」。⚠ 修正（报告 24 §16）：实机发现客户端**原本已有** SimSunFontHook 版 `version.dll`（用户字体依赖），纯新增第二份不可行 → 最终定案是合并职责（字体+震动双职责）或改用宿主 auto_inject；`dinput8.dll` 方案被用户否决。详见 `03_老版本技术路线_ACT1注入式.md` §三②。

---

## 五、协议 ABI 契约（`common/vib_protocol.h`，两版逐字节相同）

```c
#define VIB_SHM_NAME   "Local\\DfoVibrationShm"
#define VIB_SHM_MAGIC  0x564F4656
#define VIB_SHM_VERSION 2
#define VIB_RING_SIZE  (256*1024)

typedef struct { DWORD type, strength, tick, reserved; } VibEvent;   // 16 B
typedef struct { volatile DWORD head, tail; DWORD capacity; BYTE data[VIB_RING_SIZE]; } VibRing;
typedef struct { DWORD magic, version, gamePid, flags;
                 volatile DWORD seq, lastTick; DWORD apiVersion[7]; VibRing ring; } VibShm;
```

**VEV_* 事件枚举（0-25，注释照抄）**：ATTACK=0、DAMAGE=1、TARGET_DIE=2、RESET_COMBO=3、START_BATTLE=4、ACTION_END=5、SHAKE_INPUT=6、HP_LOST=7、RATING=8（评分特效）、COMBO=9、**FONT=10（strength=a6 标志位）**、RANKING=11（等级 2~8）、KILLPOINT=12、DODGE=13、CRIT=14、BREAK=15、BACK=16、FINAL_KILL=17、AERIAL=18、ARMOR_BREAK=19、BUFF_STACK=20、KILL=21（释放技能）、SHAKE_SCREEN=22、MOVE=23、SKILL_HIT=24、CRIT_SHAKE=25。

**FONT_FLAG a6 位**（含 v13.29 老版语义改道注释）：0x01 玩家攻击 / 0x02 玩家受击 / 0x04 怪物 DOT 跳字（历史名"装备特效"）/ 0x08 状态类（v13.20 起回复也走此位）/ 0x10 特殊 / 0x20 HP 数值飘字（v13.29 起仅玩家侧 DOT）/ 0x40 其他。

**其他**：`g_hookTargets[7]` 行为 hook 地址表；`DAMAGE_OBJ_VT_OFF=808` / `DAMAGE_VALUE_OFF=68`；内存模式锚点 `MEM_G_DAMAGE_FONT=0x0415D2B4` / `MEM_G_AI_MANAGER=0x0415C5D8`；`VibSettings`/`VibConfig`（enabled/autostart/各 gain/maxStrength/decayMs/padMap）；**遗留**：`VibMapEntry`/`padMap[16]`/XID_* 按钮枚举是早期"DLL 内做映射"版本的遗物，1.5 版 DLL 只采集不收发输入。

---

## 六、`vib_protocol_old.h` 扩展（老版专属，188 行）

- **VEVO_* 26-36**：STAGE_FINALE/VICTORY/DEFEAT/SCORE_UP/RESULT_COME/RANK_UP/SCORE_SCROLL/REWARD/BOSS_DEAD/FONT_HIT/FONT_COMBO。⚠ 第五轮已废弃（宿主不认 26+），实际结算类改用 VEV 白名单表达；枚举保留兼容。
- **锚点宏**（注释本身就是一部简史）：
  - `OLD_ANCHOR_EVENT_SENDER 0x00433890`；
  - `OLD_ENC_TABLE_BASE 0x00CFD458` / `OLD_ENC_KEY_ADDR 0x00CFD45C`（注释详述运行时随机密钥与 v13.18 事故）；
  - `OLD_PLAYER_OBJ 0x00B75A14`；
  - `OLD_PLAYER_HP_SLOT_OFF 0x1168` / MP `0x1170`（注释完整记录推翻 0x1828 的推理链与 GetHP thunk 铁证）；
  - `OLD_HIT_COUNT 0x00B5ED28` / `OLD_DAMAGE_TOTAL 0x00B5ED24`（双重门控说明；后者**实测恒 0，勿用于强度分级**）；
  - `OLD_FIELD_DECODE 0x00402030`（槽解算器语义）；
  - `OLD_CAM_SHAKE_APPLY 0x004E49E0`（镜头震动汇聚点，入口 6B 普通指令合规）；
  - `OLD_PLAYER_COND_OFF 0x42C`（**警告：动作/状态汇总，勿用于判定**）；
  - `OLD_PLAYER_FRENZY_OFF 0x2A14`（CNFrenzy 指针，血之狂暴）；
  - 暴击数字对象链（B5DCE0 → +0x200010 → +0xB0 → vector；vftable `0x99D21C`；+0x1AC 暴击动画指针）；
  - 镜头/CC/条件码/结算窗口等（详见文件内注释）。

---

## 七、构建与安装

| | 新版 | 老版 |
|---|---|---|
| 工具链 | LLVM-MinGW（UCRT, x86），`gcc -m32 -O2 -Wall -Wextra` + GNU as 处理 `.s`（Intel 语法、`.code32`） | 同（`build_old.bat`，i686） |
| 产物 | `release\DfoVibration.dll` | `release\DfoVibration_OLD.dll` + `DfoVibration_OLD_Loader.exe` + 代理 DLL |
| 安装 | `install.bat`：DLL+ini → `%GAME%\us_extend_dll\`；面板 exe → 游戏根目录；toml 存在不覆盖 | `install_old.bat`：DLL+Loader 拷到游戏目录；代理方案则放 `version.dll`/`dinput8.dll` |
| 坑 | `build.bat` 引用旧布局 `dll\` 与 `third_party\`（imgui/dx9），与当前平铺结构不一致，直接跑会失败（V3 `DLL源码/README.md` 原话：「需按当前结构调整路径或补齐 third_party」）；`-O2` unused warning 会让 pwsh 报 NativeCommandError 但产物成功（看 `Built:` 行） | 源码目录含全角字符（`【90US】——`）会让 GNU ld/MSVC 炸 "Invalid argument" → robocopy 到纯 ASCII 路径构建（教训 17） |

---

## 八、`memtest*.c` 与 `tests/` 是什么

`memtest.c`~`memtest5.c` 是**内存基址验证/字段定位的调试 DLL 族**（研究工具，非成品）：轮询单基址 + 调游戏函数解引用；对象区快照扫描记录"偏移+旧值+新值"；按偏移统计变化频次；**标记文件机制**（在 `us_extend_dll\` 建 `MARK_WALK/ATTACK/SCORE/STOP.txt`，DLL 检测到即写 `==标记==` 日志并删文件，与手动操作时序对齐）；关注字段表定期输出 hex+float。老版本换客户端时可改基址/偏移/日志路径直接复用这套方法论。
`tests/`：`test_load.c`（演示 LoadLibrary + VibPluginLoaded 流程）、`test_toml` 等。

---

## 九、来源

- 新版逐文件剖析（行号级）：`DfoVibration_OLD/docs/阶段报告/04_1.5DLL源码剖析.md`
- 老版源码：`DfoVibration_OLD/DLL源码/`（本文引用行号以该目录文件为准）
- 协议与锚点宏：`DLL源码/common/vib_protocol.h`、`DLL源码/vib_protocol_old.h`
- 两版引擎分叉：`阶段报告/40_新旧版架构完整对比.md` 第二节
- V3 DLL 无改动：md5 比对（见第一节表）
