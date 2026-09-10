#ifndef VIB_HOOKS_OLD_H
#define VIB_HOOKS_OLD_H

#include <windows.h>

/* 安装 6 个老版本锚点 hook, 返回成功数 */
int  hooks_install_old(void);
void hooks_uninstall_old(void);
int  hooks_active_old(void);

/* 分派(汇编 stub 调用, 游戏线程上下文, 必须极短) */
void __cdecl vib_dispatch_event_sender(void *obj, DWORD nameAddr, DWORD a3, int type);
void __cdecl vib_dispatch_broadcast(void *obj, DWORD a2, DWORD a3, int type);
void __cdecl vib_dispatch_accumulate(void *obj, DWORD a2, DWORD a3, int type);
void __cdecl vib_dispatch_voice(void *obj, DWORD a2, DWORD a3, int type);
void __cdecl vib_dispatch_result_shake(void *obj, DWORD a2, DWORD a3, int type);
void __cdecl vib_dispatch_finale(void *obj, DWORD a2, DWORD a3, int type);
void __cdecl vib_dispatch_camera_shake(void *obj, DWORD degree, DWORD a3, int type); /* ★v13.21 */

/* 轮询(collector 调用) */
void poll_score_old(void);
void poll_monsters_old(void);
void poll_player_hit_old(void);   /* ★v13.17 无损受击(玩家HP下降沿, 零patch) */
void poll_hit_counter_old(void);  /* ★v13.18 无损受击主源(B5ED28 被击计数器增量, 零patch) */
void poll_crit_number_old(void);  /* ★v13.18 暴击数字对象轮询探针(默认 LOG_ONLY 标定, 零patch) */

/* 供汇编引用的每-hook trampoline */
extern void *vib_tramp_0;
extern void *vib_tramp_1;
extern void *vib_tramp_2;
extern void *vib_tramp_3;
extern void *vib_tramp_4;
extern void *vib_tramp_5;

#endif /* VIB_HOOKS_OLD_H */