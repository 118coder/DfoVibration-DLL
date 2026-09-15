# ============================================================
# 90CN 探针 v1.4 (镜头震动专项) - x86 inline hook stub
# 统一 6 参 cdecl 分发: probe_shake_dispatch(obj, a2, a3, a4, retaddr, idx)
# 进入 stub 时: [esp0]=ret [esp0+4]=a2 ... [esp0+20]=a6
# pushfd(4)+pushad(32)+push idx(4) 后 E0=esp0-40:
#   [E0+40]=ret [E0+44]=a2 [E0+48]=a3 [E0+52]=a4 [E0+60]=a6
# 每多 push 一个参数, 后续取参偏移 +4 补偿
# 压参顺序(右->左): idx, retaddr, a4, a3, a2, obj
# ============================================================
.intel_syntax noprefix
.code32
.text

.extern _probe_shake_dispatch
.extern _probe_tramp_0
.extern _probe_tramp_1
.extern _probe_tramp_2
.extern _probe_tramp_3
.extern _probe_tramp_4
.extern _probe_tramp_5

# ---- stub 0: shake_converge 0x026382C0 (汇聚点, 49 调用者) ----
# 取 retaddr + a2/a3/a4 (thiscall: this, a2=强度, a3=时间, ...)
.globl _probe_stub_0
_probe_stub_0:
    pushfd
    pushad
    push 0                      # idx
    mov eax, [esp+40]           # retaddr
    push eax
    mov eax, [esp+56]           # a4
    push eax
    mov eax, [esp+56]           # a3
    push eax
    mov eax, [esp+56]           # a2
    push eax
    push ecx                    # this
    call _probe_shake_dispatch
    add esp, 24
    popad
    popfd
    mov eax, _probe_tramp_0
    jmp eax

# ---- stub 1: shake_screen 0x026396F0 ([shake screen] 转发入口) ----
# 取 a2/a3 (首参 float 强度, movss 已静态确认)
.globl _probe_stub_1
_probe_stub_1:
    pushfd
    pushad
    push 1                      # idx
    push 0                      # retaddr=0
    push 0                      # a4=0
    mov eax, [esp+56]           # a3
    push eax
    mov eax, [esp+56]           # a2
    push eax
    push ecx
    call _probe_shake_dispatch
    add esp, 24
    popad
    popfd
    mov eax, _probe_tramp_1
    jmp eax

# ---- stub 2: readbar_shake 0x02638550 (读条震屏) ----
# 取 a2/a3/a4 (首参=标志 byte, 静态确认)
.globl _probe_stub_2
_probe_stub_2:
    pushfd
    pushad
    push 2                      # idx
    push 0                      # retaddr=0
    mov eax, [esp+56]           # a4
    push eax
    mov eax, [esp+56]           # a3
    push eax
    mov eax, [esp+56]           # a2
    push eax
    push ecx
    call _probe_shake_dispatch
    add esp, 24
    popad
    popfd
    mov eax, _probe_tramp_2
    jmp eax

# ---- stub 3: direct_shake 0x02639D00 (直接写相机震屏字段 +0xa88) ----
# 取 a2 (首参=强度值, 直接写入对象字段)
.globl _probe_stub_3
_probe_stub_3:
    pushfd
    pushad
    push 3                      # idx
    push 0                      # retaddr=0
    push 0                      # a4=0
    push 0                      # a3=0
    mov eax, [esp+60]           # a2
    push eax
    push ecx
    call _probe_shake_dispatch
    add esp, 24
    popad
    popfd
    mov eax, _probe_tramp_3
    jmp eax

# ---- stub 4: damage_font 0x013ECEC0 (FONT 背景, a6 类型标志) ----
# 取 a6 (v1.3 同款偏移 [E0+60], 4 参数占位补偿后 [E4+76])
.globl _probe_stub_4
_probe_stub_4:
    pushfd
    pushad
    push 4                      # idx
    push 0                      # retaddr=0
    push 0                      # a4=0
    push 0                      # a3=0
    mov eax, [esp+76]           # a6
    push eax
    push ecx
    call _probe_shake_dispatch
    add esp, 24
    popad
    popfd
    mov eax, _probe_tramp_4
    jmp eax

# ---- stub 5: 备用 (未挂目标) ----
.globl _probe_stub_5
_probe_stub_5:
    pushfd
    pushad
    push 5
    push 0
    push 0
    push 0
    mov eax, [esp+60]
    push eax
    push ecx
    call _probe_shake_dispatch
    add esp, 24
    popad
    popfd
    mov eax, _probe_tramp_5
    jmp eax

.end
