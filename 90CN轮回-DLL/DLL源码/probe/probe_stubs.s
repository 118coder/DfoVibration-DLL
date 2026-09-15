# ============================================================
# 90CN 探针 - x86 inline hook stub (GNU as, i686, Intel syntax)
# stub 0-5: 行为回调 (thiscall, a2/a3 栈参) -> probe_behavior
# stub 6:   onShakeInput (thiscall, a2 栈参) -> probe_shake
# stub 7:   damage_font (thiscall, a6=[esp+60] 类型标志) -> probe_font
# 偏移推导 (与老版 hooks_asm.s 相同, 已实机验证):
#   pushfd(4)+pushad(32)=36; push idx(4)=40
#   [esp+40]=ret [esp+44]=a2 [esp+48]=a3 ... [esp+60]=a6
# ============================================================
.intel_syntax noprefix
.code32
.text

.extern _probe_behavior
.extern _probe_shake
.extern _probe_font
.extern _probe_tramp_0
.extern _probe_tramp_1
.extern _probe_tramp_2
.extern _probe_tramp_3
.extern _probe_tramp_4
.extern _probe_tramp_5
.extern _probe_tramp_6
.extern _probe_tramp_7

# ---- stub 0: OnAttack ----
.globl _probe_stub_0
_probe_stub_0:
    pushfd
    pushad
    push 0
    mov eax, [esp+44]
    push eax
    mov eax, [esp+48]
    push eax
    push ecx
    call _probe_behavior
    add esp, 16
    popad
    popfd
    mov eax, _probe_tramp_0
    jmp eax

# ---- stub 1: OnDamage ----
.globl _probe_stub_1
_probe_stub_1:
    pushfd
    pushad
    push 1
    mov eax, [esp+44]
    push eax
    mov eax, [esp+48]
    push eax
    push ecx
    call _probe_behavior
    add esp, 16
    popad
    popfd
    mov eax, _probe_tramp_1
    jmp eax

# ---- stub 2: OnStartBattle ----
.globl _probe_stub_2
_probe_stub_2:
    pushfd
    pushad
    push 2
    mov eax, [esp+44]
    push eax
    mov eax, [esp+48]
    push eax
    push ecx
    call _probe_behavior
    add esp, 16
    popad
    popfd
    mov eax, _probe_tramp_2
    jmp eax

# ---- stub 3: OnResetCombo ----
.globl _probe_stub_3
_probe_stub_3:
    pushfd
    pushad
    push 3
    mov eax, [esp+44]
    push eax
    mov eax, [esp+48]
    push eax
    push ecx
    call _probe_behavior
    add esp, 16
    popad
    popfd
    mov eax, _probe_tramp_3
    jmp eax

# ---- stub 4: OnActionEnd ----
.globl _probe_stub_4
_probe_stub_4:
    pushfd
    pushad
    push 4
    mov eax, [esp+44]
    push eax
    mov eax, [esp+48]
    push eax
    push ecx
    call _probe_behavior
    add esp, 16
    popad
    popfd
    mov eax, _probe_tramp_4
    jmp eax

# ---- stub 5: OnTargetDie ----
.globl _probe_stub_5
_probe_stub_5:
    pushfd
    pushad
    push 5
    mov eax, [esp+44]
    push eax
    mov eax, [esp+48]
    push eax
    push ecx
    call _probe_behavior
    add esp, 16
    popad
    popfd
    mov eax, _probe_tramp_5
    jmp eax

# ---- stub 6: onShakeInput ----
.globl _probe_stub_6
_probe_stub_6:
    pushfd
    pushad
    push 6
    mov eax, [esp+44]
    push eax
    push ecx
    call _probe_shake
    add esp, 12
    popad
    popfd
    mov eax, _probe_tramp_6
    jmp eax

# ---- stub 7: damage_font (a6 = 类型标志) ----
.globl _probe_stub_7
_probe_stub_7:
    pushfd
    pushad
    push 7
    mov eax, [esp+60]
    push eax
    push ecx
    call _probe_font
    add esp, 12
    popad
    popfd
    mov eax, _probe_tramp_7
    jmp eax

.end
