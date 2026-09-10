/* ============================================================
 * DFO 老版本手柄震动插件 - 采集 DLL (LMA1S-release)
 * 挂载: Loader 远程注入 (LoadLibraryA + VibPluginLoaded 契约)
 * 功能: hook 老版本事件锚点(sub_433890 等) + 轮询评分/怪物计数
 *       -> 写入共享内存环形缓冲 (Local\DfoVibrationShm)
 * 参考: 1.5 版 dfovib.c 骨架 (worker 1s 延迟 + collector 6ms 轮询)
 * ============================================================ */
#include <windows.h>
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0501
#endif
#ifndef WINVER
#define WINVER 0x0501
#endif
#include <stdio.h>
#include <string.h>
#include "hooks_old.h"
#include "vib_protocol_old.h"
#include "common/vib_protocol.h"
#include "common/ini_util.h"

/* ---------------- 共享内存 ---------------- */
static HANDLE  g_hMap = NULL;
static VibShm *g_shm = NULL;
static volatile int g_running = 0;
volatile int g_ringReady = 0;   /* ★v13.17 去 static, hooks_old.c extern 链接(外部可见) */
static volatile int g_workerStarted = 0;
static HANDLE g_hWorker = NULL;
static HANDLE g_hCollector = NULL;
static char g_logPath[MAX_PATH] = "";
static HMODULE g_hDllMod = NULL;

/* 环形缓冲写入(CAS head, 与 1.5 版一致) */
static int ring_push(const VibEvent *ev)
{
    VibRing *r;
    DWORD head, next;
    if (!g_shm || !g_ringReady) return 0;
    r = &g_shm->ring;
    for (;;) {
        head = r->head;
        next = (head + sizeof(VibEvent)) & (VIB_RING_SIZE - 1);
        if (next == r->tail) return 0; /* 满 */
        if (InterlockedCompareExchange((volatile LONG *)&r->head, next, head) == (LONG)head) {
            memcpy(r->data + head, ev, sizeof(VibEvent));
            InterlockedIncrement((volatile LONG *)&g_shm->seq);
            g_shm->lastTick = ev->tick;
            return 1;
        }
    }
}

/* ---- 采集去抖(2026-08-31 实机修复, 2026-09 分级细化) ----
 * 症状: sub_4D2C50 等高频 hook 无条件 push 事件 -> 事件风暴(5亿+) ->
 *       ring 256KB 写满 -> 后续受击/击杀事件全部被丢弃。
 * 修复 v1: 按事件类型 40ms 去抖; 结算类事件(FINALE/胜利/失败/结果)放行(低频必达)。
 * 修复 v2: 战斗高频类(命中飘字/评分加分/评分轮询上升)独立拉长去抖,
 *          上限从 25 条/秒降到 ~8 条/秒 —— 事件量收敛且手感不损(震动
 *          间隔 120ms 仍清晰); 结算/胜负类仍放行(必达)。 */
#define VIB_DEDUP_MS          40   /* 默认去抖(受击/击杀/连击等) */
#define VIB_DEDUP_MS_BATTLE  120   /* 战斗高频: 命中飘字/评分加分 */
#define VIB_DEDUP_MS_SCORE   200   /* 评分轮询上升(最密集源) */
static DWORD s_lastTick[64] = {0};
static DWORD s_lastStr[64]  = {0};

/* ★v13.18 发送统计(30s tick 日志用, 实机分诊: 一眼看出事件有没有发出/哪类发了) */
static volatile DWORD g_statPush = 0;      /* 成功入环事件总数 */
static volatile DWORD g_statFont[6] = {0}; /* FONT 分类发送数: [0]=0x01命中 [1]=0x02受击 [2]=0x04 [3]=0x08 [4]=0x10特殊 [5]=0x20DOT */

static int is_settlement_type(int type)
{
    switch (type) {
    case VEVO_STAGE_FINALE: case VEVO_VICTORY: case VEVO_DEFEAT:
    case VEVO_SCORE_UP:    case VEVO_RESULT_COME: case VEVO_RANK_UP:
    case VEV_FINAL_KILL: case VEV_CRIT_SHAKE:
    case VEV_RANKING: /* ★v10: 结算评分只在结算演出触发(VICTORY/SCORE_UP/RANK_UP 白名单), 低频必达 → 放行 */
        return 1;
    /* ★v10: 结算评分(RANKING)只在结算演出触发(白名单 VICTORY/RESULT_SCORE_UP/RANK_UP),
     *   低频必达 → 放行不参与去抖; 战斗期 STYLE/TECHNIC 仍走 RATING(丢弃) 不碰它。 */
    default: return 0;
    }
}

/* ★第五轮: VEVO_* 已废弃(宿主不认26+), 结算类改用 VEV_FINAL_KILL/VEV_RANKING/
 * VEV_CRIT_SHAKE 表达(见 hooks_old.c 白名单) —— 本表保持兼容旧枚举不删。 */

/* ★v13.39 新架构对齐(用户 2026-09-08 点名试用): 聚合冲刷模式 ——
 * 对齐 1.5 版 vib_dispatch_common_hit/vib_flush_counts 架构:
 *   hook 上下文(游戏线程)只做原子计数(纳秒级, 零内存操作),
 *   collector 每 6ms 批量冲刷(逐条拆发, count=1 保宿主连击语义,
 *   v13.27 回退教训: count=N 批量语义不采用)。
 * 范围: VEV_FONT 七种主分类(01/02/04/08/10/20/60)进计数器;
 *   罕见组合(07/11/12 等)与全部非 FONT 事件保持直发保真。
 * 去抖/入环/统计逻辑全部保留在 raw 路径(风暴保护不降级)。 */
static volatile LONG g_aggFont[7] = {0};
static const DWORD g_aggFlag[7] = {
    FONT_FLAG_PLAYER_ATTACK, FONT_FLAG_PLAYER_HIT, FONT_FLAG_EFFECT,
    FONT_FLAG_STATE, FONT_FLAG_SPECIAL, FONT_FLAG_HP,
    FONT_FLAG_HP | FONT_FLAG_OTHER };

void vib_collect_raw(int type, DWORD strength, DWORD tick, DWORD count);
int hooks_intact_bad_old(void);

void vib_collect(int type, DWORD strength, DWORD tick, DWORD count)
{
    if (g_ringReady && type == VEV_FONT && count == 1) {
        DWORD fl = strength & (FONT_FLAG_PLAYER_ATTACK | FONT_FLAG_PLAYER_HIT |
                               FONT_FLAG_EFFECT | FONT_FLAG_STATE |
                               FONT_FLAG_SPECIAL | FONT_FLAG_HP | FONT_FLAG_OTHER);
        int i;
        for (i = 0; i < 7; i++) {
            if (fl == g_aggFlag[i]) {
                InterlockedIncrement(&g_aggFont[i]);
                return; /* hook 上下文: 计数即返回 */
            }
        }
        /* 罕见组合: 直发保真 */
    }
    vib_collect_raw(type, strength, tick, count);
}

/* collector 每 6ms 调用: 各计数器批量冲刷(逐条拆发过 raw 去抖) */
void vib_flush_font(void)
{
    int i;
    for (i = 0; i < 7; i++) {
        LONG v = InterlockedExchange(&g_aggFont[i], 0);
        LONG k;
        for (k = 0; k < v; k++) {
            vib_collect_raw(VEV_FONT, g_aggFlag[i], GetTickCount(), 1);
        }
    }
}

void vib_collect_raw(int type, DWORD strength, DWORD tick, DWORD count)
{
    VibEvent ev;
    DWORD slot;
    DWORD now;
    if (tick == 0) tick = GetTickCount();
    now = tick;
    slot = ((DWORD)type) & 0x3F;
    /* ★v13.18 FONT 去抖子槽细分: 旧逻辑 slot=type&0x3F 使全部 FONT 变体
     * (0x01命中/0x02受击/0x10特殊/0x20DOT)共用 slot10 的 120ms 窗互相吞事件
     * —— 密集战斗中命中流会饿死受击/特殊通道(用户"受击绑命中"现象的 DLL 侧
     * 结构性根因, sa_host_gate_audit_result D1 + 主会话源码通读独立坐实)。
     * 修法: FONT 按标志位分到独立子槽 48-53(事件枚举最大 36, 无冲突);
     * 协议不变(VibEvent 结构不动), 只改 DLL 内部去抖表。 */
    if (type == VEV_FONT) {
        DWORD fl = strength & 0x3F;
        if (fl & FONT_FLAG_PLAYER_HIT)         slot = 49; /* 0x02 受击 */
        else if (fl & FONT_FLAG_PLAYER_ATTACK) slot = 48; /* 0x01 命中 */
        else if (fl & FONT_FLAG_SPECIAL)       slot = 52; /* 0x10 特殊/暴击 */
        else if (fl & FONT_FLAG_HP)            slot = 53; /* 0x20 DOT */
        else if (fl & FONT_FLAG_STATE)         slot = 51; /* 0x08 */
        else if (fl & FONT_FLAG_EFFECT)        slot = 50; /* 0x04 */
        /* 其余保持 slot=10 */
    }
    if (!is_settlement_type(type)) {
        DWORD ms = VIB_DEDUP_MS;
        /* 战斗高频: 命中飘字/评分加分 -> 120ms; 评分轮询上升 -> 200ms;
         * ★第五轮: RANKING(语音层)/COMBO(连击横幅)/TARGET_DIE(6ms 轮询)
         *   也纳入 120ms — 防 sub_4E8520/sub_42DCB0 高频调用漏事件(宿主
         *   这两通道无节流), 防 B6F608 轮询槽值微抖每下降沿误发死怪事件。 */
        if (type == VEV_FONT || type == VEV_RATING
         || type == VEV_RANKING || type == VEV_COMBO
         || type == VEV_TARGET_DIE)
            ms = VIB_DEDUP_MS_BATTLE;
        else if (type == VEVO_SCORE_UP)
            ms = VIB_DEDUP_MS_SCORE;
        /* ★v13.30 命中子槽(slot48)单独 40ms —— 用户实测鬼剑士三段普攻
         * 只有第一击和最后一击震: 段间隔 ~100-200ms 时中间段落在 120ms
         * 窗内被吞(攻速越快越明显)。40ms 仍能合并 stub6+stub8 同刀双源
         * (两源到达间隔微秒级), 多段每段一条 = 按飘字逐条驱动(用户拍板)。
         * ★v13.35 再降 10ms —— 格斗家实测(v13.33 ret 探针): 极速连段间隔
         * 10-39ms 的对多达 786 对, 40ms 窗吞掉 1092/2411 条命中(用户实测
         * "命中只有个别几击"); 10ms 仅合并同帧多目标(0ms 簇), 极速连段
         * 每段放行。宿主侧注入窗(P_FONT_IVL)同步 40→20 兜底。 */
        if (type == VEV_FONT && slot == 48)
            ms = 10;
        if ((DWORD)(now - s_lastTick[slot]) < ms) {
            if (strength > s_lastStr[slot]) s_lastStr[slot] = strength; /* 取峰值 */
            return; /* 去抖合并 */
        }
    }
    s_lastTick[slot] = now;
    s_lastStr[slot] = strength;
    ev.type = (DWORD)type;
    ev.strength = strength;
    ev.tick = tick;
    ev.reserved = count;
    if (ring_push(&ev)) {
        InterlockedIncrement((volatile LONG *)&g_statPush);
        if (type == VEV_FONT) {
            DWORD fl = strength & 0x3F;
            int bi = 0;
            if (fl & FONT_FLAG_PLAYER_HIT) bi = 1;
            else if (fl & FONT_FLAG_PLAYER_ATTACK) bi = 0;
            else if (fl & FONT_FLAG_SPECIAL) bi = 4;
            else if (fl & FONT_FLAG_HP) bi = 5;
            else if (fl & FONT_FLAG_STATE) bi = 3;
            else if (fl & FONT_FLAG_EFFECT) bi = 2;
            g_statFont[bi]++;
        }
    }
}

/* DLL 侧调试日志 */
void dll_log2(const char *fmt, ...)
{
    FILE *fp;
    char buf[512];
    va_list ap;
    if (!g_logPath[0]) return;
    va_start(ap, fmt);
    _vsnprintf(buf, sizeof(buf) - 1, fmt, ap);
    va_end(ap);
    fp = fopen(g_logPath, "a");
    if (fp) { fprintf(fp, "[%lu] %s\n", (unsigned long)GetCurrentThreadId(), buf); fclose(fp); }
}

static void dll_log(const char *fmt, ...)
{
    FILE *fp;
    char buf[512];
    va_list ap;
    if (!g_logPath[0]) return;
    va_start(ap, fmt);
    _vsnprintf(buf, sizeof(buf) - 1, fmt, ap);
    va_end(ap);
    fp = fopen(g_logPath, "a");
    if (fp) { fprintf(fp, "[%lu] %s\n", (unsigned long)GetCurrentThreadId(), buf); fclose(fp); }
}

/* ---------------- ★v13.8 移动震动 + 掉血(DOT)震动 轮询 ----------------
 * 均读 B75A14(OLD_PLAYER_OBJ, 我方实证玩家对象指针) 指向的对象 +
 * 第三方偏移(坐标 x=0x13C y=0x140; 血量 当前0x1178/最大0xE48 —— 来自
 * E:\LX\ACT1和85B部分基址.txt, 虽其"人物基址 0xBB3ACC"实测不可用, 但这些
 * 偏移值按游戏对象标准布局推测; IsBadReadPtr 保护, 读不到/异常即不发不崩)。
 */
#define OLD_PLAYER_XOFF  0x13CUL
#define OLD_PLAYER_YOFF  0x140UL
#define OLD_PLAYER_HPOFF 0x1178UL
#define OLD_PLAYER_HPMAX 0xE48UL
#define MOVE_STOP_MS     200UL
static volatile DWORD g_moving = 0;
static volatile DWORD g_lastMoveTick = 0;
static DWORD s_lastHp = 0; /* v13.9: 保留(等 IDA 真 HP 偏移后恢复轮询) */
static void poll_dot_old(void);

/* 移动震动: 读玩家坐标, 位移>=1(像素)发 VEV_MOVE(强度), 停止超时发 0。
 * 复用 1.5 版 hooks.c 标准模式; 强度固定 40(第三方偏移解码不可靠, 不冒险
 * 读速度字段), 宿主 move 走路质感参数(MOVE_PACE/PULSE/GAIN)会进一步成形。 */
static void poll_move_old(void)
{
    DWORD now = GetTickCount();
    DWORD player;
    static int s_lx = 0, s_ly = 0;
    int nx, ny, mdx, mdy;
    float xf, yf;
    if (!g_ringReady) return;
    player = *(DWORD *)0x00B75A14UL;
    /* ★v13.8fix: player 原始值先打日志(IDA: B75A14 初始值 0xFFFFFFFF, 城镇/战斗
     * 实值待观测); 无效值(0 / -1 / 小于 0x10000)直接跳过本轮 */
    { static int s_diag = 0; if (!s_diag) { s_diag = 1;
        dll_log("[move] B75A14 raw=0x%08X (%s)", player,
                (player == 0 || player == 0xFFFFFFFF || player < 0x10000) ? "invalid" : "valid"); } }
    /* ★v13.38 双源并行(实测修正: 城镇态 B75A14 有效(0x157F5458)但 +0x13C/0x140
     * 恒定不动(12 条 5s 节流日志同值) —— 城镇对象的这两个偏移不是实时坐标;
     * B75A18 指向的对象坐标实时(探针 18 样本轨迹吻合)。故:
     *   源 A = B75A14 (副本实时坐标, 城镇坐标死)
     *   源 B = B75A18 (城镇实时坐标; 副本态行为待 [mvB] 日志定案)
     * 任一源位移 >=1 即发 VEV_MOVE(40ms 去抖自然合并双源同拍)。 */
    if (IsBadReadPtr((LPCVOID)(player + OLD_PLAYER_XOFF), 8)) return;
    xf = *(float *)(player + OLD_PLAYER_XOFF);
    yf = *(float *)(player + OLD_PLAYER_YOFF);
    nx = (int)xf; ny = (int)yf;
    /* ★v13.8fix: 坐标读值诊断(带频率限制 5s 一次, 观察坐标是否随移动变化) */
    { static DWORD s_diag = 0; DWORD t = GetTickCount();
      if (t - s_diag >= 5000) { s_diag = t;
        dll_log("[move] player=0x%08X xy=(%.1f,%.1f) off=(0x13C,0x140)", player, xf, yf); } }
    mdx = nx - s_lx; if (mdx < 0) mdx = -mdx;
    mdy = ny - s_ly; if (mdy < 0) mdy = -mdy;
    s_lx = nx; s_ly = ny;
    if ((float)(mdx + mdy) >= 1.0f) {   /* 像素级位移 = 移动中 */
        vib_collect(VEV_MOVE, 40, now, 0);
        g_moving = 1;
        g_lastMoveTick = now;
    } else if (g_moving && now - g_lastMoveTick > MOVE_STOP_MS) {
        vib_collect(VEV_MOVE, 0, now, 0);
        g_moving = 0;
    }
    /* ★v13.38 源 B: B75A18 (城镇实时坐标) 独立沿检测。
     * 瞬移保护: 单次位移 >2000px 丢弃并重置基准(切图跳变不误报)。 */
    {
        static float s_blx = 0, s_bly = 0;
        static int   s_binit = 0;
        DWORD pb;
        float bx, by;
        if (IsBadReadPtr((LPCVOID)0x00B75A18UL, 4)) return;
        pb = *(DWORD *)0x00B75A18UL;
        if (pb == 0 || pb == 0xFFFFFFFF || pb < 0x10000) return;
        if (IsBadReadPtr((LPCVOID)(pb + OLD_PLAYER_XOFF), 8)) return;
        bx = *(float *)(pb + OLD_PLAYER_XOFF);
        by = *(float *)(pb + OLD_PLAYER_YOFF);
        { static DWORD s_bdiag = 0; DWORD t = GetTickCount();
          if (t - s_bdiag >= 5000) { s_bdiag = t;
            dll_log("[mvB] obj=0x%08X xy=(%.1f,%.1f)", pb, bx, by); } }
        if (!s_binit) { s_blx = bx; s_bly = by; s_binit = 1; return; }
        {
            float bdx = bx - s_blx, bdy = by - s_bly;
            float bdist = (bdx < 0 ? -bdx : bdx) + (bdy < 0 ? -bdy : bdy);
            s_blx = bx; s_bly = by;
            if (bdist > 2000.0f) return;          /* 切图跳变: 重置基准不发 */
            if (bdist >= 1.0f) {
                vib_collect(VEV_MOVE, 40, now, 0);
                g_moving = 1;
                g_lastMoveTick = now;
            }
            /* 停止事件由源 A 的 g_moving 超时统一处理(共享状态) */
        }
    }
}

/* 掉血(DOT)震动: ★v13.9 暂停轮询 — 0x1178 偏移实测读出垃圾值(3.3亿恒定),
 * 等待 IDA 定位真 HP 偏移; DOT 掉血震动已改走 stub8 FONT_HP 飘字事件通道
 * (事件驱动: 中毒/燃烧每跳伤害必生成 HP 飘字, 见 hooks_old.c v13.9)。 */
static void poll_dot_old(void)
{
    (void)0; /* 轮询暂停: 偏移未定案, 防垃圾值波动误发 */
}


/* ---------------- collector: 6ms 轮询 ---------------- */
static DWORD WINAPI collector_thread(LPVOID p)
{
    (void)p;
    while (g_running) {
        poll_score_old();
        poll_monsters_old();
        poll_move_old();   /* ★v13.8 移动震动 */
        poll_dot_old();    /* ★v13.8 掉血(DOT)震动 */
        poll_hit_counter_old(); /* ★v13.18 无损受击主源(B5ED28 被击计数器增量, hooks_old.c 提供)
                                 * ★v13.23 顺序: 必须先于 poll_player_hit_old —— 受击与 HP 掉落
                                 * 同拍到达时, 先更新 g_lastPlayerHitTick 佐证, HP 沿关联门才放行 */
        poll_player_hit_old(); /* ★v13.17 无损受击(玩家HP下降沿+飘字区分门, hooks_old.c 提供) */
        poll_crit_number_old(); /* ★v13.18 暴击数字对象轮询探针(LOG_ONLY 标定, hooks_old.c 提供) */
        vib_flush_font();   /* ★v13.39 聚合冲刷: FONT 计数器批量拆发(新架构对齐) */
        Sleep(6);
    }
    return 0;
}

/* ---------------- worker ---------------- */
static DWORD WINAPI worker_thread(LPVOID param)
{
    (void)param;
    /* 延迟: 避开 DllMain loader lock 窗口 */
    Sleep(1000);

    /* 建共享内存 */
    {
        HANDLE h = CreateFileMappingA(INVALID_HANDLE_VALUE, NULL,
                                      PAGE_READWRITE, 0,
                                      sizeof(VibShm) + VIB_RING_SIZE,
                                      VIB_SHM_NAME);
        if (h) {
            VibShm *m = (VibShm *)MapViewOfFile(h, FILE_MAP_ALL_ACCESS, 0, 0,
                                                sizeof(VibShm) + VIB_RING_SIZE);
            if (m) {
                if (m->magic != VIB_SHM_MAGIC) {
                    memset(m, 0, sizeof(VibShm) + VIB_RING_SIZE);
                    m->magic = VIB_SHM_MAGIC;
                    m->version = VIB_SHM_VERSION;
                    m->gamePid = GetCurrentProcessId();
                    m->ring.capacity = VIB_RING_SIZE;
                    m->ring.head = 0;
                    m->ring.tail = 0;
                }
                g_shm = m;
                g_ringReady = 1;
            }
            g_hMap = h;
        }
    }

    /* 日志路径: 与 DLL 同目录 DfoVibration_OLD_dll.log */
    {
        char *slash;
        GetModuleFileNameA(g_hDllMod ? g_hDllMod : GetModuleHandleA(NULL), g_logPath, MAX_PATH);
        slash = strrchr(g_logPath, '\\');
        if (slash) {
            *(slash + 1) = 0;
            _snprintf(slash + 1, MAX_PATH - (slash - g_logPath) - 1, "DfoVibration_OLD_dll.log");
        }
    }
    /* ★v13.39 日志轮转(新架构基建): >2MB 归档 .old 重新开始 */
    {
        HANDLE h = CreateFileA(g_logPath, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
        if (h != INVALID_HANDLE_VALUE) {
            DWORD sz = GetFileSize(h, NULL);
            CloseHandle(h);
            if (sz > 2 * 1024 * 1024) {
                char oldp[MAX_PATH];
                _snprintf(oldp, sizeof(oldp) - 1, "%s.old", g_logPath);
                DeleteFileA(oldp);
                MoveFileA(g_logPath, oldp);
            }
        }
    }
    dll_log("========== worker start (OLD) pid=%lu ==========", (unsigned long)GetCurrentProcessId());

    /* 装 hook (老版本锚点 10 个: 9 采集 + 1 死亡直钩 0x4217CE) */
    {
        int n = hooks_install_old();
        dll_log("hooks installed: %d/10", n);
    }

    /* 周期: hook 完整性检查 + 状态日志(★v13.18: 移除误导性 enabled 字段——
     * g_iniPath 从未赋值, ini_load_config 恒不执行, 旧日志打的 enabled 是
     * 未初始化栈垃圾且 DLL 侧无任何 enabled 门控, 实机排障曾被误导。
     * 换成发送统计: push=成功入环总数, font a/h/sp/dot=各分类发送数,
     * 一眼看出事件有没有发出、哪类发了。) */
    for (;;) {
        static DWORD s_log = 0;
        DWORD now;
        if (!g_running) break;
        now = GetTickCount();
        if (now - s_log >= 30000) {
            s_log = now;
            dll_log("tick hooks=%d shm_ok=%d push=%lu font a=%lu h=%lu sp=%lu dot=%lu",
                    hooks_active_old(), g_ringReady,
                    (unsigned long)g_statPush,
                    (unsigned long)g_statFont[0], (unsigned long)g_statFont[1],
                    (unsigned long)g_statFont[4], (unsigned long)g_statFont[5]);
        }
        /* ★v13.6 基址验证日志已删除(v13.9): 第三方基址(BB3ACC/B9C1C8/B9D2AC)
         * 实测全 0 已证伪; 有用的是 [move]/[dot] 诊断(B75A14 实证有效)。 */
        /* ★v13.39 hook 完整性自检(新架构基建): 60s 一次, 首字节应为 E9 */
        {
            static DWORD s_chk = 0;
            DWORD t = GetTickCount();
            if (t - s_chk >= 60000) {
                int bad = hooks_intact_bad_old();
                if (bad) dll_log("[hook] WARN %d hooks overwritten (runtime self-protection?)", bad);
                s_chk = t;
            }
        }
        Sleep(500);
    }
    return 0;
}

/* ---------------- 导出契约 ---------------- */
__declspec(dllexport) void __cdecl VibPluginLoaded(void)
{
    if (InterlockedCompareExchange((volatile LONG *)&g_workerStarted, 1, 0) == 0) {
        g_hWorker = CreateThread(NULL, 0, worker_thread, NULL, 0, NULL);
        g_hCollector = CreateThread(NULL, 0, collector_thread, NULL, 0, NULL);
    }
}

BOOL WINAPI DllMain(HINSTANCE hDll, DWORD reason, LPVOID reserved)
{
    (void)reserved;
    if (reason == DLL_PROCESS_ATTACH) {
        g_hDllMod = hDll;
        DisableThreadLibraryCalls(hDll);
        /* ★v13.7 修复: g_running 从此置 1(此前恒 0 → worker/collector 主循环
         * 启动即 break, 所有轮询类功能(基址验证/评分/怪物/血蓝)从未运行;
         * hook 类是直钩瞬间执行不依赖它所以一直能震) */
        g_running = 1;
        if (InterlockedCompareExchange((volatile LONG *)&g_workerStarted, 1, 0) == 0) {
            g_hWorker = CreateThread(NULL, 0, worker_thread, NULL, 0, NULL);
            g_hCollector = CreateThread(NULL, 0, collector_thread, NULL, 0, NULL);
        }
    } else if (reason == DLL_PROCESS_DETACH) {
        g_running = 0;
        hooks_uninstall_old();
        if (g_shm) { UnmapViewOfFile(g_shm); g_shm = NULL; }
        if (g_hMap) { CloseHandle(g_hMap); g_hMap = NULL; }
    }
    return TRUE;
}