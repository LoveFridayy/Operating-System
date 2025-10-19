# Lab 1

## 一、实验过程

在***tmux***终端中利用`make debug`与`make gdb`指令进入调试界面，输入指令`x/10i $pc`查看即将执行的10条指令，得到如下结果：

```assembly
(gdb) x/10i $pc
=> 0x1000:      auipc  t0,0x0
   0x1004:      addi   a1,t0,32
   0x1008:      csrr   a0,mhartid
   0x100c:      ld     t0,24(t0)
   0x1010:      jr     t0
   0x1014:      unimp
   0x1016:      unimp
   0x1018:      unimp
   0x101a:      0x8000
   0x101c:      unimp
```

由于地址在`0x1010`处的指令会跳转，故实际执行的为以下指令：

```assembly
   0x1000:      auipc   t0,0x0     # t0 = pc + 0 << 12 = 0x1000
   0x1004:      addi    a1,t0,32   # a1 = t0 + 32 = 0x1020
   0x1008:      csrr    a0,mhartid # a0 = mhartid = 0
   0x100c:      ld      t0,24(t0)  # t0 = [t0 + 24] = 0x80000000
   0x1010:      jr      t0		    # 跳转到地址0x80000000
```

之后输入`si`指令进行单步执行，然后使用指令`i r registers`查看寄存器的值，得到如下结果：

```assembly
(gdb) si
0x0000000000001004 in ?? ()
(gdb) i r t0
t0             0x1000   4096
(gdb) si
0x0000000000001008 in ?? ()
(gdb) i r to
Invalid register `to'
(gdb) i r t0
t0             0x1000   4096
(gdb) si
0x000000000000100c in ?? ()
(gdb) i r t0
t0             0x1000   4096
(gdb) si
0x0000000000001010 in ?? ()
(gdb) ir t0
Undefined command: "ir".  Try "help".
(gdb) i r t0
t0             0x80000000       2147483648
(gdb) si
0x0000000080000000 in ?? ()
```

输入`x/10i 0x80000000`，显示跳转到地址为`0x80000000`处继续执行的10条数据。该地址处加载的是作为`bootloader`的`OpenSBI.bin`，作用为加载操作系统内核并启动操作系统的执行。结果如下：

```assembly
(gdb) x/10i 0x80000000
=> 0x80000000: csrr    a6,mhartid              # a6 = mhartid (获取当前硬件线程的ID)
   0x80000004: bgtz    a6,0x80000108           # 如果 a6 > 0，则跳转到0x80000108
   0x80000008: auipc   t0,0x0                # t0 = pc + (0x0 << 12) = 0x80000008
   0x8000000c: addi    t0,t0,1032             # t0 = t0 + 1032 = 0x80000408
   0x80000010: auipc   t1,0x0                # t1 = pc + (0x0 << 12) = 0x80000010
   0x80000014: addi    t1,t1,-16              # t1 = t1 - 16 = 0x80000000
   0x80000018: sd      t1,0(t0)                 # 将t1的值（0x80000000）存储在地址0x80000408处
   0x8000001c: auipc   t0,0x0                # t0 = pc + (0x0 << 12) = 0x8000001c
   0x80000020: addi    t0,t0,1020             # t0 = t0 + 1020 = 0x80000400
   0x80000024: ld      t0,0(t0)                 # t0 = [t0 + 0] = [0x80000400] (从地址0x80000400加载一个双字到t0)
```

接下来进行断点设置，输入指令`b* kern_entry`,内核会在运行到我们设置好的断点处停止

```assembly
(gdb) b* kern_entry
Breakpoint 1 at 0x80200000: file kern/init/entry.S, line 7.
```

此时内核暂停在入口函数的第一条汇编指令处，我们检查汇编代码。

```assembly
(gdb) x/5i 0x80200000
   0x80200000 <kern_entry>:     auipc   sp,0x3    # sp = pc + (0x3 << 12) = 0x80200000 + (0x3 << 12) = 0x80203000
   0x80200004 <kern_entry+4>:   mv      sp,sp     # sp = sp (这条指令实际上没有改变sp的值，可能是为了某些同步/延迟原因)
   0x80200008 <kern_entry+8>:
    j   0x8020000a <kern_init>                    # 无条件跳转到地址0x8020000c
   0x8020000a <kern_init>:      auipc   a0,0x3    # a0 = pc + (0x3 << 12) = 0x8020000c + (0x3 << 12) = 0x8020300c
   0x8020000e <kern_init+4>:    addi    a0,a0,-2  # a0 = a0 - 2 = 0x8020300c - 2 = 0x8020300a
```

可以看到在`kern_entry`之后，紧接着就是`kern_init`,输入`continue`执行直到断点，debug输出如下：

```assembly
OpenSBI v0.4 (Jul  2 2019 11:53:53)
   ____                    _____ ____ _____
  / __ \                  / ____|  _ \_   _|
 | |  | |_ __   ___ _ __ | (___ | |_) || |
 | |  | | '_ \ / _ \ '_ \ \___ \|  _ < | |
 | |__| | |_) |  __/ | | |____) | |_) || |_
  \____/| .__/ \___|_| |_|_____/|____/_____|
        | |
        |_|

Platform Name          : QEMU Virt Machine
Platform HART Features : RV64ACDFIMSU
Platform Max HARTs     : 8
Current Hart           : 0
Firmware Base          : 0x80000000
Firmware Size          : 112 KB
Runtime SBI Version    : 0.1

PMP0: 0x0000000080000000-0x000000008001ffff (A)
PMP1: 0x0000000000000000-0xffffffffffffffff (A,R,W,X)
```

这说明OpenSBI此时已经启动。然后输入指令`break kern_init`，输出如下：

```assembly
(gdb) b* kern_init
Breakpoint 3 at 0x8020000a: file kern/init/init.c, line 8.
```

输入`continue`，接着输入`disassemble kern_init`查看反汇编代码：

```assembly
=> 0x000000008020000a <+0>:     auipc   a0,0x3                       # a0 = pc + (0x3 << 12)，即a0 = 0x8020000a + 0x3000 = 0x8020300a
   0x000000008020000e <+4>:     addi    a0,a0,-2                     # a0 = a0 - 4，即a0 = 0x8020300a - 2 = 0x80203008
   0x0000000080200012 <+8>:     auipc   a2,0x3                       # a2 = pc + (0x3 << 12)，即a2 = 0x80200018 + 0x3000 = 0x80203018
   0x0000000080200016 <+12>:    addi    a2,a2,-10                    # a2 = a2 - 12，即a2 = 0x80203018 - 10 = 0x80203008
   0x000000008020001a <+16>:    addi    sp,sp,-16                    # sp = sp - 16 (在堆栈上分配16字节的空间)
   0x000000008020001c <+18>:    li      a1,0                         # a1 = 0 (立即加载0到a1寄存器)
   0x000000008020001e <+20>:    sub     a2,a2,a0                     # a2 = a2 - a0，即a2 = 0x80203008 - 0x80203008 =  0
   0x0000000080200020 <+22>:    sd      ra,8(sp)                     # 将返回地址(ra)存储到堆栈的sp+8位置
   0x0000000080200022 <+24>:    jal     ra,0x802004b6 <memset>       # 跳转到memset函数，并设置返回地址(ra)
   0x0000000080200026 <+28>:    auipc   a1,0x0                       # a1 = pc + (0x0 << 12)，即a1 = 0x80200026
   0x000000008020002a <+32>:    addi    a1,a1,1186                   # a1 = a1 + 1186，即a1 = 0x80200026 + 1186 = 0x802004c8
   0x000000008020002e <+36>:    auipc   a0,0x0                       # a0 = pc + (0x0 << 12)，即a0 = 0x8020002e
   0x0000000080200032 <+40>:    addi    a0,a0,1210                   # a0 = a0 + 1210，即a0 = 0x80200030 + 1232 = 0x802004e8 
   0x0000000080200036 <+44>:    jal     ra,0x80200056 <cprintf>      # 跳转到cprintf函数，并设置返回地址(ra)
   0x000000008020003a <+48>:    j       0x8020003a <kern_init+48>    # 跳转到地址0x8020003a处的指令
```

这个函数最后一个指令是`j 0x8020003a <kern_init+48>`，也就是跳转到自己，所以代码会在这里一直循环下去。输入`continue`，debug窗口出现以下输出：

```assembly
(THU.CST) os is loading ...
```



## 二、问题回答

### 练习1：

1.`la sp, bootstacktop`:将 `bootstacktop` 的地址加载到栈指针寄存器`sp` 中。目的为让CPU 就知道栈在哪里。

2.`tail kern_init`:利用尾调用指令`tail` 让执行流直接跳转到 C 函数 `kern_init`，并且不再返回。目的为进入OS的入口，解放寄存器 `sp` 。

### 练习2：

1.**RISC-V** 硬件加电后最初执行的几条指令位于`0x1000`到`0x1010`。

2.完成的功能有：

+ `auipc t0,0x0`：用于加载一个20bit的立即数，`t0`中保存的数据是`(pc)+(0<<12)`。用于PC相对寻址。

+ `addi a1,t0,32`：将`t0`加上`32`，赋值给`a1`。

+ `csrr a0,mhartid`：读取状态寄存器`mhartid`，存入`a0`中。`mhartid`为正在运行代码的硬件线程的整数ID。

+ `ld t0,24(t0)`：双字，加载从`t0+24`地址处读取8个字节，存入`t0`。

+ `jr t0`：寄存器跳转，跳转到寄存器指向的地址处（此处为`0x80000000`）。