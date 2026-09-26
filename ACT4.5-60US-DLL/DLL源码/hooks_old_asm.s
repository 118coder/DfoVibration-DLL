# ============================================================
# DFO 老版本震动插件 - x86 inline hook stub (GNU as, i686, Intel syntax)
# 每个 stub: 保存现场 -> 压入事件类型 -> 取出栈参 -> call 分派
#          -> 还原现场 -> 跳转各自 trampoline
#
# 参数偏移（pushfd 4B + pushad 32B = 36 基准）:
#   cdecl 目标 (sub_433890): 进入 [esp+4]=a1(name) [esp+8]=a2 [esp+12]=a3 [esp+16]=a4
#     pushfd+pushad 后: name@[esp+40] a2@[esp+44] a3@[esp+48] a4@[esp+52]
#     push type 后:     name@[esp+44] a2@[esp+48] a3@[esp+52] a4@[esp+56]
#     push a3 后:       name@[esp+48] a2@[esp+52] a3@[esp+56]
#   thiscall 目标 (其余): 进入 ECX=this, [esp+4]=a2 [esp+8]=a3
#     pushfd+pushad 后: a2@[esp+40] a3@[esp+44]
#     push type 后:     a2@[esp+44] a3@[esp+48]
#     push a3 后:       a2@[esp+48] a3@[esp+52]
#
# 分派函数全为 __cdecl: (obj, a2/a3/name, a3, type)
# 压参顺序 = 从右到左: push type -> push 第3参 -> push 第2参 -> push obj -> call
# 修正 (2026-08-31 实机): 旧 stub 参数序反 + obj 缺失, 且 trampoline 重叠,
#   已由 C 侧 idx 定块 + 此文件压序修正解决。
# ============================================================

.intel_syntax noprefix
.code32
.text

.extern _vib_dispatch_event_sender
.extern _vib_dispatch_damage_font
.extern _vib_dispatch_broadcast
.extern _vib_dispatch_accumulate
.extern _vib_dispatch_voice
.extern _vib_dispatch_result_shake
.extern _vib_dispatch_finale
.extern _vib_tramp_0
.extern _vib_tramp_1
.extern _vib_tramp_2
.extern _vib_tramp_3
.extern _vib_tramp_4
.extern _vib_tramp_5
.extern _vib_tramp_10
.extern _vib_tramp_11
.extern _vib_tramp_12
.extern _vib_dispatch_dot_active
.extern _vib_dispatch_battle_event
.extern _vib_dispatch_camera_shake

# ★★2026-09-24 桩尾跳转改为 push dword ptr [tramp] + ret（不破坏任何寄存器）:
#   旧式 `mov eax, _vib_tramp_N / jmp eax` 会把 EAX 冲成 tramp 页地址。函数入口处
#   EAX 通常不是活值, 所以历史上没问题; 但【函数内部】挂钩点一旦在后续指令里用到
#   EAX 就会出事 —— 实机事故: 60US 探针挂 0x498CBB(生成器内), 其后紧跟
#   `mov ecx, eax`(EAX=刚分配的伤害数字对象), 于是构造器的 this 被换成 tramp 页
#   地址, 对象写进 tramp 页把 trampoline 自己覆盖 → 下一次跳入执行垃圾 → 崩溃
#   (异常 0xc0000005, 错误模块 unknown, 偏移 = tramp页+0x48)。
#   `push dword ptr [mem]`(FF 35 imm32) + `ret`(C3) 不需要任何临时寄存器。

# ---- stub 0: sub_433890 统一事件发送器 (cdecl, 无 this)
# 分派: vib_dispatch_event_sender(obj, nameAddr, a3, type)
.globl _vib_stub_0
_vib_stub_0:
    pushfd
    pushad
    push 0                  # type = 0
    mov eax, [esp+52]       # a3
    push eax                # 第3参
    mov eax, [esp+48]       # name (push a3 后 name@[esp+48])
    push eax                # 第2参
    xor eax, eax
    push eax                # 第1参 obj = NULL
    call _vib_dispatch_event_sender
    add esp, 16
    popad
    popfd
    # ★不破坏寄存器: push 内存 + ret 代替旧的 mov eax/jmp eax
    push dword ptr [_vib_tramp_0]
    ret

# ---- stub 1: sub_4E6DA0 播报层 (thiscall)
# 分派: vib_dispatch_broadcast(obj, a2, a3, type)
.globl _vib_stub_1
_vib_stub_1:
    pushfd
    pushad
    push 1                  # type = 1
    mov eax, [esp+48]       # a3
    push eax                # 第3参
    mov eax, [esp+48]       # a2 (push a3 后 a2@[esp+48])
    push eax                # 第2参
    push ecx                # 第1参 obj = this
    call _vib_dispatch_broadcast
    add esp, 16
    popad
    popfd
    # ★不破坏寄存器: push 内存 + ret 代替旧的 mov eax/jmp eax
    push dword ptr [_vib_tramp_1]
    ret

# ---- stub 2: sub_4D2C50 加分汇聚 (thiscall)
.globl _vib_stub_2
_vib_stub_2:
    pushfd
    pushad
    push 2                  # type = 2
    mov eax, [esp+48]       # a3
    push eax
    mov eax, [esp+48]       # a2
    push eax
    push ecx
    call _vib_dispatch_accumulate
    add esp, 16
    popad
    popfd
    # ★不破坏寄存器: push 内存 + ret 代替旧的 mov eax/jmp eax
    push dword ptr [_vib_tramp_2]
    ret

# ---- stub 3: sub_4E8520 技巧语音层 (thiscall)
.globl _vib_stub_3
_vib_stub_3:
    pushfd
    pushad
    push 3                  # type = 3
    mov eax, [esp+48]       # a3
    push eax
    mov eax, [esp+48]       # a2
    push eax
    push ecx
    call _vib_dispatch_voice
    add esp, 16
    popad
    popfd
    # ★不破坏寄存器: push 内存 + ret 代替旧的 mov eax/jmp eax
    push dword ptr [_vib_tramp_3]
    ret

# ---- stub 4: sub_4E3B40 结算震动实体 (thiscall)
.globl _vib_stub_4
_vib_stub_4:
    pushfd
    pushad
    push 4                  # type = 4
    mov eax, [esp+48]       # a3
    push eax
    mov eax, [esp+48]       # a2
    push eax
    push ecx
    call _vib_dispatch_result_shake
    add esp, 16
    popad
    popfd
    # ★不破坏寄存器: push 内存 + ret 代替旧的 mov eax/jmp eax
    push dword ptr [_vib_tramp_4]
    ret

# ---- stub 5: sub_4BCA70 地城阶段结算流程 (thiscall)
.globl _vib_stub_5
_vib_stub_5:
    pushfd
    pushad
    push 5                  # type = 5
    mov eax, [esp+48]       # a3
    push eax
    mov eax, [esp+48]       # a2
    push eax
    push ecx
    call _vib_dispatch_finale
    add esp, 16
    popad
    popfd
    # ★不破坏寄存器: push 内存 + ret 代替旧的 mov eax/jmp eax
    push dword ptr [_vib_tramp_5]
    ret

# ---- stub 6: sub_4748D0 攻击命中/伤害应用 (__userpurge: ECX=目标对象 a1,
#      [esp+4]=a4 普通伤害标志==1; 判定 a1[249]==B75A14 = 目标==玩家=被打,
#      否则 目标非玩家=玩家攻击命中怪物)
# 分派: vib_dispatch_attack_hit(obj=目标, a3=0占位, a4, type=6)
.globl _vib_stub_6
_vib_stub_6:
    pushfd
    pushad
    push 6                  # type = 6
    mov eax, [esp+48]       # a5 伤害信息块 (sub_4748D0 __userpurge: a5@[esp+8], pushfd+pushad+push type 后 +40 → [esp+48])
    push eax                # 第3参 a5
    mov eax, [esp+48]       # a4 普通伤害标志 (原 a4@[esp+44], push a5 后 +4 → [esp+48])
    push eax                # 第2参 a4
    push ecx                # 第1参 obj = 被击目标 (ecx 仍=a1)
    call _vib_dispatch_attack_hit
    add esp, 16
    popad
    popfd
    # ★不破坏寄存器: push 内存 + ret 代替旧的 mov eax/jmp eax
    push dword ptr [_vib_tramp_6]
    ret

# ---- stub 7: sub_42DCB0 战斗 UI 加载器 (Combo.img 注册所在函数)
#      进入 = 战斗界面激活/连击横幅就绪 → VEV_COMBO(9) 连击通道
# 分派: vib_dispatch_combo_ui(obj=ecx, 0, 0, type=7)
.globl _vib_stub_7
_vib_stub_7:
    pushfd
    pushad
    push 7                  # type = 7
    xor eax, eax
    push eax                # 第3参 a3 = 0
    push eax                # 第2参 a2 = 0
    push ecx                # 第1参 obj = ecx
    call _vib_dispatch_combo_ui
    add esp, 16
    popad
    popfd
    # ★不破坏寄存器: push 内存 + ret 代替旧的 mov eax/jmp eax
    push dword ptr [_vib_tramp_7]
    ret

# ---- stub 8: sub_470ED0 伤害数字生成咽喉 (cdecl 5 参, 无 this)
#      __cdecl(a1,a2,a3,a4,a5): [esp+4]=a1 [esp+8]=a2 [esp+12]=a3 [esp+16]=a4 [esp+20]=a5
#      pushfd(4)+pushad(32)+push type(4)=40 后:
#        [esp+40]=游戏返回地址 [esp+44]=a1 [esp+48]=a2 [esp+52]=a3 [esp+56]=a4(伤害值) [esp+60]=a5(标志)
# ★v10 建筑区分: 返回地址==0x0058C958 (sub_58C8C0 的调用点 0x58C953 call 的下一条)
#      = 可破坏物/建筑 → 跳过命中通道(直接跳 tramp, 不调 C 不分派)
# 分派(cdecl 5 参压序 从右到左): push type(8) -> push a5 -> push a4 -> push a2(0) -> push a1(obj)
#   调用后 add esp, 24 (6 参 * 4B) —— ★v13.33: 新增第5参 ret=游戏返回地址
#   (来源调用点识别, sa_melee_hit_p2 只找到 9 个静态调用点但 177 条 a5=01
#   无法静态归因, 返回地址动态定案; 新版 vib_dispatch_skillhit 同款手法)
.globl _vib_stub_8
_vib_stub_8:
    pushfd
    pushad
    push 8                  # type = 8
    cmp dword ptr [esp+40], 0x58C958   # 游戏返回地址 = 建筑/可破坏物调用点?
    je .Lskip8                         # 是 → 跳过命中通道(建筑破坏不震)
    mov eax, [esp+40]       # ★v13.33 ret = 调用点下一条地址 (esp0+0)
    push eax                # 第5参 ret
    mov eax, [esp+64]       # a5 标志位 (原 esp0+20, 已 push ret: 20+44=64)
    push eax                # 第4参 a5
    mov eax, [esp+64]       # a4 伤害值 (原 esp0+16, 已 push ret+a5: 16+48=64)
    push eax                # 第3参 a4
    xor eax, eax
    push eax                # 第2参 a2 = 0 (占位)
    mov eax, [esp+60]       # a1 (原 esp0+4, 已 push ret+a5+a4+0: 4+56=60)
    push eax                # 第1参 obj = a1
    call _vib_dispatch_damage_font
    add esp, 24             # 6 参 * 4
    popad
    popfd
    # ★不破坏寄存器: push 内存 + ret 代替旧的 mov eax/jmp eax
    push dword ptr [_vib_tramp_8]
    ret
.Lskip8:
    add esp, 4              # 弹掉 type
    popad
    popfd
    # ★不破坏寄存器: push 内存 + ret 代替旧的 mov eax/jmp eax
    push dword ptr [_vib_tramp_8]
    ret

# ---- stub 9: 通知38 MonsterDie 死亡直钩 (★v12, 宿主规则死亡通道)
#      目标 0x004217CE, hookLen=7: C6 85 8A E4 FF FF 00 (mov byte [ebp-1B76h],0)
#      语义: 服务端逐只怪死亡确认(模拟端每怪发一次) → VEV_TARGET_DIE(宿主死亡通道)
#      注意: 0x4217CE 是通知分发器 sub_419420 switch-case 38 跳转目标,
#      (jmp ds:jpt_419474[eax*4] 进入, 非 call) —— [esp+0] 不是返回地址,
#      [esp+4..] 是分发器栈帧的无关数据, 参数不可用也不使用(分派全忽略)。
#      现场靠 pushfd+pushad 完整保存, trampoline 重放原 7B 后回 0x4217CE+7
#      继续执行该 handler, 无损。
#      pushfd(4)+pushad(32)+push type(4)=40: 参数位置仅供占位(不读真实参数)
.globl _vib_stub_9
_vib_stub_9:
    pushfd
    pushad
    push 9                  # type = 9 (死亡直钩)
    mov eax, [esp+52]       # a3
    push eax                # 第3参 a3
    mov eax, [esp+48]       # a2
    push eax                # 第2参 a2
    mov eax, [esp+44]       # a1
    push eax                # 第1参 obj = a1
    call _vib_dispatch_target_die
    add esp, 16             # 4 参 * 4
    popad
    popfd
    # ★不破坏寄存器: push 内存 + ret 代替旧的 mov eax/jmp eax
    push dword ptr [_vib_tramp_9]
    ret

# ---- stub 10: sub_4D10F0 状态tick调度器 (★v13.10 DOT 持续伤害, __usercall)
#      目标 0x004D10F0, hookLen=5 (55 8B EC 6A FF 入口)
#      语义: 每角色 ActiveStatus 状态 tick 调度器, switch(*(a1+4)):
#        case 2 → sub_4D0960(中毒)  case 9 → sub_4CF6B0(出血/燃烧)
#        case 0xB → sub_4CF1E0(状态扣血综合)  三者都最终 call sub_470ED0 生成红字
#      根因: DOT/持续伤害走这三个(不经 sub_470ED0 直达), 现有 stub8 覆盖不到。
#      __usercall(a1@ecx, a2@ebx, a3@edi): a1=受害角色对象(ecx 入口保存于 esi)
#      判定: *(a1+0x30)==dword_B75A14 (玩家归属, 0x30 是角色对象偏移, 非飘字0x3E4)
#            && *(a1+4)∈{2,9,0xB} (DOT 状态码) → vib_dispatch_dot_active → FONT_HP(0x20)
#      pushfd(4)+pushad(32)+push type(4)=40, a1=esi(已由游戏 mov esi,ecx 保存)
#      分派: asm 只取 a1=esi 传 C; 归属/状态码判定全在 C 侧 vib_dispatch_dot_active。
#      偏移核算: pushfd(-4) pushad(-36) push type(-40): esi(在 pushad 块内偏移16)
#        = esp0-20; 当前 esp=esp0-40 → esi@[esp+20]。再 push a3/a2(-48) → esi@[esp+28]。
.globl _vib_stub_10
_vib_stub_10:
    pushfd
    pushad
    push 10                 # type = 10 (DOT 状态tick)
    # pushad 压序: EAX0 ECX4 EDX8 EBX12 ESP16 EBP20 ESI24 EDI28
    # pushfd+pushad 后 esp-36; push type 后 esp-40. ESI=pushad起始+24=esp0-12,
    #   相对当前 esp(esp0-40)=+28 → [esp+28]. 压 a3/a2 各-4 后 → +36.
    mov eax, [esp+28]       # ESI = a1 (受害角色)
    test eax, eax
    jz .Lskip10
    xor ecx, ecx
    push ecx                # 第3参 a3 = 0
    push ecx                # 第2参 a2 = 0
    mov eax, [esp+36]       # ESI = a1 (两 push 后)
    push eax                # 第1参 a1
    call _vib_dispatch_dot_active   # C 侧判定: 归属玩家 + DOT 状态码 → 才发 FONT_HP
    add esp, 16             # 4 参 * 4
    popad
    popfd
    # ★不破坏寄存器: push 内存 + ret 代替旧的 mov eax/jmp eax
    push dword ptr [_vib_tramp_10]
    ret
.Lskip10:
    add esp, 4              # 弹掉 type
    popad
    popfd
    # ★不破坏寄存器: push 内存 + ret 代替旧的 mov eax/jmp eax
    push dword ptr [_vib_tramp_10]
    ret

# ---- stub 11: 0x790D93 暴击/破招/背击 真锚点 (★v13.15, sa_monolayer 定案)
#      目标 0x00790D93, hookLen=5 (E8 01 CE FF FF = call sub_470ED0 近调用)
#      语义: sub_78EE00(事件主结算)内部, 唯一权威出口。此指令是 E8 近调用
#        sub_470ED0(伤害数字咽喉), 其 a5 标志在此【一次性组装完毕】+ 归属内嵌。
#        ★v13.13 挂 sub_78EE00 入口失败根因: 入口 0x78EE32/38/42 重写参数区、
#          0x78EE53 清零 → 入口 HIBYTE=0 结构性收不到暴击。必须在此刻读 a5。
#      a5 定位: 0x790D93 是 E8 rel32。到达时(未 pushfd)a5=第5参(最深),
#        子代理定案 a5@[esp+0x10]。stub: pushfd(4)+pushad(32)+push type(4)=40,
#        a5 相对=0x10+40=0x38 → 读 [esp+56]。
#      C 判定: vib_dispatch_battle_event(_, _, a5, 11) 读 a5,
#        玩家暴击=(a5&0x10特殊)&&(a5&0x01玩家攻)&&!(a5&0x02非受击) → FONT_SPECIAL(0x10)。
#      trampoline 重放 5B (E8 rel32) 后跳回 0x790D98 继续执行 sub_470ED0 调用。
.globl _vib_stub_11
_vib_stub_11:
    pushfd
    pushad
    push 11                 # type = 11 (暴击/破招/背击)
    mov eax, [esp+56]       # a5 第5参(最深) @0x790D93时刻[esp+0x10], 3压后 +40 → +56
    push eax                # 第3参 a5 (C 侧做三元组判定)
    xor eax, eax
    push eax                # 第2参 a2 = 0 占位
    xor eax, eax
    push eax                # 第1参 obj = 0 占位 (a5 已含归属, 无需 this)
    call _vib_dispatch_battle_event
    add esp, 16             # 4 参 * 4
    popad
    popfd
    # ★不破坏寄存器: push 内存 + ret 代替旧的 mov eax/jmp eax
    push dword ptr [_vib_tramp_11]
    ret

# ---- stub 12: sub_4E49E0 镜头震动汇聚点 (★v13.21, 68 xref 唯一震屏出口)
#      目标 0x004E49E0, hookLen=6 (55 8B EC 8B 45 0C = push ebp/mov ebp,esp/mov eax,[ebp+arg_4])
#      __thiscall(this=相机, a2=震屏度数, a3=第二参): 进入 ECX=this, [esp+4]=a2, [esp+8]=a3
#      pushfd(4)+pushad(32)+push type(4)=40 后: a2@[esp+44] a3@[esp+48]
#      分派: vib_dispatch_camera_shake(obj=相机, a2=度数, a3, type=12)
#        C 侧: degree==0(停震)不发, 100ms 去重, → VEV_KILL(21) 宿主「释放技能」滑块。
.globl _vib_stub_12
_vib_stub_12:
    pushfd
    pushad
    push 12                 # type = 12 (镜头震动)
    mov eax, [esp+48]       # a3 (push type 后 a3@[esp+48])
    push eax                # 第3参 a3
    mov eax, [esp+48]       # a2 度数 (push a3 后 a2@[esp+48])
    push eax                # 第2参 a2
    push ecx                # 第1参 obj = this(相机)
    call _vib_dispatch_camera_shake
    add esp, 16             # 4 参 * 4
    popad
    popfd
    # ★不破坏寄存器: push 内存 + ret 代替旧的 mov eax/jmp eax
    push dword ptr [_vib_tramp_12]
    ret

# ---- stub 13: ★ACT5 伤害数字生成咽喉 sub_4ACC00 (cdecl 6 参, 无 this)
#      __cdecl(Src, x=a2, y=a3, z=a4, 伤害=a5, 标志=a6)
#      证据: sub_4ACC00 尾 `sub_4AC7B0(alloc480, a5, a6)` → ctor(this,值,标志);
#            ctor 内按 a3(a6) 的位 0x01/0x02/0x04/0x08 选数字精灵、0x10=特殊,
#            与 ACT1 FONT 标志位语义同构。创建者 12 个调用点(含 DOT 三兄弟)。
#      pushfd(4)+pushad(32)+push type(4)=40 后 (记 E=此时esp):
#        E+40=ret E+44=Src E+48=x E+52=y E+56=z E+60=a5(伤害) E+64=a6(标志)
#      分派复用 ACT1 的 vib_dispatch_damage_font(obj,a2,a4=伤害,a5=标志,ret,type):
#        压序 type -> ret -> a6(标志) -> a5(伤害) -> 0 -> Src(obj)
.globl _vib_stub_13
_vib_stub_13:
    pushfd
    pushad
    push 13                 # type = 13 (伤害数字/命中)
    mov eax, [esp+40]       # 游戏返回地址 (E+40)
    push eax                # 第5参 ret
    mov eax, [esp+68]       # a6 标志 (E+64, 已 push ret → +68)
    push eax                # 第4参 a5(标志位)
    mov eax, [esp+68]       # a5 伤害值 (E+60, 已 push ret+a6 → +68)
    push eax                # 第3参 a4(伤害值)
    xor eax, eax
    push eax                # 第2参 a2 = 0 占位
    mov eax, [esp+60]       # Src (E+44, 已 push 4 次 → +60)
    push eax                # 第1参 obj = Src
    call _vib_dispatch_damage_font
    add esp, 24             # 6 参 * 4
    popad
    popfd
    # ★不破坏寄存器: push 内存 + ret 代替旧的 mov eax/jmp eax
    push dword ptr [_vib_tramp_8]
    ret
# ---- stub 14: ★60US 伤害数字生成器 sub_498980 (thiscall, 5 栈参)
#      __thiscall(this=生成器对象, a1=?, a2=?, a3=?, a4=宿主对象, a5=伤害)
#      ★标志位不在栈上: 它是函数内 0x498CC3 的 `mov ecx, eax` —— 由函数体内
#        (0x4989B0 起遍历池) 算出来的值。但 0x498CBB 处的 `mov ecx,[ebp+0x1C]`
#        证明它源自信封: [ebp+0x1C] = 第 6 栈参。
#      ★60US 实测(2026-09-24 定案): a4=宿主对象(数字挂谁身上), a5=伤害,
#        第 6 栈参=标志位(bit0 通用分类 / bit1 受击 / bit4 暴击 / bit5 DOT)。
#      栈布局(pushfd(4)+pushad(32)+push type(4)=40, 记 E=esp):
#        E+40=ret E+44=a1 E+48=a2 E+52=a3 E+56=a4(宿主) E+60=a5(伤害) E+64=a6(标志)
#      与 ACT5 stub13 的 E+44.. 布局【完全一致】(都是 thiscall+5栈参), 直接复用同一压序。
.globl _vib_stub_14
_vib_stub_14:
    pushfd
    pushad
    push 14                 # type = 14 (伤害数字/命中, 60US)
    mov eax, [esp+40]       # 游戏返回地址 (E+40)
    push eax                # 第5参 ret
    mov eax, [esp+68]       # a6 标志 (E+64, 已 push ret → +68)
    push eax                # 第4参 a5(标志位)
    mov eax, [esp+68]       # a5 伤害值 (E+60, 已 push ret+a6 → +68)
    push eax                # 第3参 a4(伤害值)
    xor eax, eax
    push eax                # 第2参 a2 = 0 占位
    mov eax, [esp+60]       # a4 宿主对象 (E+56, 已 push 4 次 → +60)
    push eax                # 第1参 obj = 宿主(数字挂靠对象)
    call _vib_dispatch_damage_font
    add esp, 24             # 6 参 * 4
    popad
    popfd
    # ★不破坏寄存器: push 内存 + ret 代替旧的 mov eax/jmp eax
    push dword ptr [_vib_tramp_8]
    ret

# ============================================================
# ★60US 伤害通道定位探针（log-only, 定案后移除）
# 4 个候选函数共用一个 C 分派 vib_dispatch_probe(id, ecx, ret, a1..a5)。
# 栈布局（pushfd(4)+pushad(32) 后, 记 E=esp）:
#   E+36=ret  E+40..E+56=a1..a5(栈参)  E+24=ecx(thiscall 的 this)
# 压参序（cdecl 右到左）: a5→a4→a3→a2→a1→ret→ecx→id
#   每次 push 后窗口 +4, 六个栈参全部落在 [esp+56]；ecx 在 6 次 push 后 [esp+48]
# ============================================================
.extern _vib_dispatch_probe
.extern _vib_dispatch_probe_regs
.extern _vib_probe_tramp_0
.extern _vib_probe_tramp_1
.extern _vib_probe_tramp_2
.extern _vib_probe_tramp_3

# ---- probe stub 0 ★寄存器捕获模式（v3）: inline 伤害出口 @0x45D19F ----
# 钩点进入时: eax=伤害值, ecx=暴击标志(movzx 后), ebx=攻击者Equipment, ebp=帧指针
# 原指令 7 字节(56 51 68 imm32 = push esi;push ecx;push imm32)由 trampoline 重放。
# pushfd(4)+pushad(32) 后 E=esp: ebp@E+8, ebx@E+16, ecx@E+24, eax@E+28
# 分派 cdecl(id, damage, crit, attacker, frame): 压序 frame→attacker→crit→damage→id
.globl _vib_probe_stub_0
_vib_probe_stub_0:
    pushfd
    pushad
    push dword ptr [esp+8]      # frame (ebp)
    push dword ptr [esp+20]     # attacker (ebx, 16+4)
    push dword ptr [esp+32]     # crit (ecx, 24+8)
    push dword ptr [esp+40]     # damage (eax, 28+12)
    push 0                      # probe id = 0
    call _vib_dispatch_probe_regs
    add esp, 20                 # 5 参 * 4
    popad
    popfd
    # ★不破坏寄存器: push 内存 + ret 代替旧的 mov eax/jmp eax
    push dword ptr [_vib_probe_tramp_0]
    ret

# ---- 通用栈捕获模式（备用, stub 1..3; 当前剖面 UNRESOLVED 不安装但符号保留） ----
# pushfd+pushad 后 E=esp: ret@E+36, a1..a5@E+40..56, ecx@E+24
.macro PROBE_STUB id, tramp
    pushfd
    pushad
    push dword ptr [esp+56]     # a5
    push dword ptr [esp+56]     # a4
    push dword ptr [esp+56]     # a3
    push dword ptr [esp+56]     # a2
    push dword ptr [esp+56]     # a1
    push dword ptr [esp+56]     # ret (E+36)
    push dword ptr [esp+48]     # ecx (E+24, 6 push 后)
    push \id                    # probe id
    call _vib_dispatch_probe
    add esp, 32                 # 8 参 * 4
    popad
    popfd
    # ★不破坏寄存器: push 内存 + ret 代替旧的 mov eax/jmp eax
    #  (宏参数是符号 = 内存操作数; 宏体同样必须零寄存器占用, 否则宏被别人复用时重蹈覆辙)
    push dword ptr [\tramp]
    ret
.endm

.globl _vib_probe_stub_1
_vib_probe_stub_1:
    PROBE_STUB 1, _vib_probe_tramp_1

.globl _vib_probe_stub_2
_vib_probe_stub_2:
    PROBE_STUB 2, _vib_probe_tramp_2

.globl _vib_probe_stub_3
_vib_probe_stub_3:
    PROBE_STUB 3, _vib_probe_tramp_3
