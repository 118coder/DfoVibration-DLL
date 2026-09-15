# ============================================================
# 90CN 生产版 - x86 inline hook stub (GNU as, i686, Intel syntax)
# 偏移推导 (探针四轮实机验证, 与 probe_stubs.s 一致):
#   进入 stub 时: [esp0]=ret [esp0+4]=a2 ... [esp0+20]=a6 (thiscall 栈参)
#   pushfd(4)+pushad(32)=36; push type(4) 后 esp=E0=esp0-40:
#   [E0+40]=ret [E0+44]=a2 [E0+48]=a3 [E0+52]=a4 [E0+56]=a5 [E0+60]=a6
#   每额外 push 一个参数, 后续取参偏移 +4 补偿
# cdecl 压参从最右开始, stub 自行 add esp 清栈
# ============================================================
.intel_syntax noprefix
.code32
.text

.extern _vib_dispatch_font_hit
.extern _vib_dispatch_skillhit
.extern _vib_dispatch_shake
.extern _vib_dispatch_readshake
.extern _vib_dispatch_directshake
.extern _vib_dispatch_shake_entry
.extern _vib_dispatch_target_die
.extern _vib_dispatch_probe
.extern _vib_dispatch_notify
.extern _vib_dispatch_diag7
.extern _vib_tramp_0
.extern _vib_tramp_1
.extern _vib_tramp_2
.extern _vib_tramp_3
.extern _vib_tramp_4
.extern _vib_tramp_5
.extern _vib_tramp_6
.extern _vib_tramp_7

# ---- stub 0: damage_font (CN 0x013ECEC0, 每次伤害飘字必经, 四轮实证) ----
# thiscall(this, a2..a11); 取 a2..a5 + a6(类型标志) 交 C 侧
# (v2.7 曾用这些参数做语义诊断: 结论是**显示参数**, 被击者不在飘字层; 保留取参无害)
.globl _vib_stub_0
_vib_stub_0:
    pushfd
    pushad
    push 0                     # type
    mov eax, [esp+60]          # a6
    push eax
    mov eax, [esp+60]          # a5 (push 后滑动补偿)
    push eax
    mov eax, [esp+60]          # a4
    push eax
    mov eax, [esp+60]          # a3
    push eax
    mov eax, [esp+60]          # a2
    push eax
    push ecx                   # this (飘字对象)
    call _vib_dispatch_font_hit
    add esp, 28
    popad
    popfd
    jmp dword ptr [_vib_tramp_0]   # ★ 内存间接跳转: 不碰任何寄存器(eax 必须保留给重放的 switch 跳转)

# ============================================================
# v3.0 通知/命令分发器探针 (stub 1..7)
# 依据(老版 ACT1/ACT4/ACT5 实测可用, 三版共用同一套代码):
#   死亡信号 = hook 客户端**通知分发器的 case38**(MonsterDie):
#     服务端逐只怪发"通知38 MonsterDie" -> 客户端 152-case 跳表分发器
#     (sub_419420, `jmp ds:jpt_419474[eax*4]`, case38 @0x4217CE) -> VEV_TARGET_DIE。
# 本版先**不定 case 号**: hook 7 个候选 switch 站点的 `jmp dword ptr [eax*4+跳表]`(6 字节),
#   跳转前取 eax(= case 号) 做直方图 -> 杀 N 只怪后, 计数恰好 =N 的 case 号即 MonsterDie。
# 取参: 站点处 eax = case 号; pushfd/pushad 不改 eax, 故 pushad 后读 eax 仍正确。
# 重放: 原 6 字节 jmp 由 trampoline 重放 -> 控制权交回该 case 处理体(尾部 jmp 不再到达)。
# ============================================================
.macro NOTIFY_STUB idx
    pushfd
    pushad
    push \idx
    push eax
    call _vib_dispatch_notify
    add esp, 8
    popad
    popfd
.endm

.globl _vib_stub_1
_vib_stub_1:
    NOTIFY_STUB 1
    jmp dword ptr [_vib_tramp_1]   # ★ 内存间接跳转: 不碰任何寄存器(eax 必须保留给重放的 switch 跳转)

.globl _vib_stub_2
_vib_stub_2:
    NOTIFY_STUB 2
    jmp dword ptr [_vib_tramp_2]   # ★ 内存间接跳转: 不碰任何寄存器(eax 必须保留给重放的 switch 跳转)

.globl _vib_stub_3
_vib_stub_3:
    NOTIFY_STUB 3
    jmp dword ptr [_vib_tramp_3]   # ★ 内存间接跳转: 不碰任何寄存器(eax 必须保留给重放的 switch 跳转)

.globl _vib_stub_4
_vib_stub_4:
    NOTIFY_STUB 4
    jmp dword ptr [_vib_tramp_4]   # ★ 内存间接跳转: 不碰任何寄存器(eax 必须保留给重放的 switch 跳转)

.globl _vib_stub_5
_vib_stub_5:
    NOTIFY_STUB 5
    jmp dword ptr [_vib_tramp_5]   # ★ 内存间接跳转: 不碰任何寄存器(eax 必须保留给重放的 switch 跳转)

.globl _vib_stub_6
_vib_stub_6:
    NOTIFY_STUB 6
    jmp dword ptr [_vib_tramp_6]   # ★ 内存间接跳转: 不碰任何寄存器(eax 必须保留给重放的 switch 跳转)

.globl _vib_stub_7
_vib_stub_7:
    NOTIFY_STUB 7
    jmp dword ptr [_vib_tramp_7]   # ★ 内存间接跳转: 不碰任何寄存器(eax 必须保留给重放的 switch 跳转)

.end
