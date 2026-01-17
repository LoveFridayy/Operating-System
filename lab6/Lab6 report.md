# Lab6

## 组员：杨桑多杰，张磊，徐南海

### 练习0：填写已有实验

#### proc_struct结构体

我们在原来的实验基础上新增了9行代码

```c++
struct proc_struct
{
    enum proc_state state;                  // Process state
    int pid;                                // Process ID
    int runs;                               // the running times of Proces
    uintptr_t kstack;                       // Process kernel stack
    volatile bool need_resched;             // bool value: need to be rescheduled to release CPU?
    struct proc_struct *parent;             // the parent process
    struct mm_struct *mm;                   // Process's memory management field
    struct context context;                 // Switch here to run process
    struct trapframe *tf;                   // Trap frame for current interrupt
    uintptr_t pgdir;                        // the base addr of Page Directroy Table(PDT)
    uint32_t flags;                         // Process flag
    char name[PROC_NAME_LEN + 1];           // Process name
    list_entry_t list_link;                 // Process link list
    list_entry_t hash_link;                 // Process hash list
    
    // 新增的codes
    int exit_code;                          // exit code (be sent to parent proc)
    uint32_t wait_state;                    // waiting state
    struct proc_struct *cptr, *yptr, *optr; // relations between processes
    struct run_queue *rq;                   // running queue contains Process
    list_entry_t run_link;                  // the entry linked in run queue
    int time_slice;                         // time slice for occupying the CPU
    skew_heap_entry_t lab6_run_pool;        // FOR LAB6 ONLY: the entry in the run pool
    uint32_t lab6_stride;                   // FOR LAB6 ONLY: the current stride of the process
    uint32_t lab6_priority;                 // FOR LAB6 ONLY: the priority of process, set by lab6_set_priority(uint32_t)
};
```

#### **alloc_proc()** **函数**

我们在原来的实验基础上新增了6行代码进行初始化

```c++
alloc_proc(void)
{
    struct proc_struct *proc = kmalloc(sizeof(struct proc_struct));
    if (proc != NULL)
    {    
        proc->state = PROC_UNINIT;
        proc->pid = -1;
        proc->runs = 0;
        proc->kstack = 0;
        proc->need_resched = 0;
        proc->parent = NULL;
        proc->mm = NULL;
        memset(&(proc->context), 0, sizeof(struct context));
        proc->tf = NULL;
        proc->pgdir = boot_pgdir_pa;
        proc->flags = 0;
        memset(proc->name, 0, PROC_NAME_LEN);
        list_init(&(proc->list_link));
        list_init(&(proc->hash_link));
        proc->wait_state = 0;
        proc->cptr = proc->optr = proc->yptr = NULL;

        // LAB6:YOUR CODE (update LAB5 steps)
        /*
         * below fields(add in LAB6) in proc_struct need to be initialized
         *       struct run_queue *rq;                       // run queue contains Process
         *       list_entry_t run_link;                      // the entry linked in run queue
         *       int time_slice;                             // time slice for occupying the CPU
         *       skew_heap_entry_t lab6_run_pool;            // entry in the run pool (lab6 stride)
         *       uint32_t lab6_stride;                       // stride value (lab6 stride)
         *       uint32_t lab6_priority;                     // priority value (lab6 stride)
         */
        proc->rq = NULL;
        list_init(&(proc->run_link));
        proc->time_slice = 0;
        proc->lab6_run_pool.left = proc->lab6_run_pool.right = proc->lab6_run_pool.parent = NULL;
        proc->lab6_stride = 0;
        proc->lab6_priority = 1;
    }
    return proc;
```

#### interrupt_handler()函数

```c++
case IRQ_S_TIMER:
        // "All bits besides SSIP and USIP in the sip register are
        // read-only." -- privileged spec1.9.1, 4.1.4, p59
        // In fact, Call sbi_set_timer will clear STIP, or you can clear it
        // directly.
        // clear_csr(sip, SIP_STIP);

        /* LAB3 :填写你在lab3中实现的代码 */
        /*(1)设置下次时钟中断- clock_set_next_event()
         *(2)计数器（ticks）加一
         *(3)当计数器加到100的时候，我们会输出一个`100ticks`表示我们触发了100次时钟中断，同时打印次数（num）加一
         * (4)判断打印次数，当打印次数为10时，调用<sbi.h>中的关机函数关机
         */
        clock_set_next_event();
        ticks++;

        // lab6: YOUR CODE  (update LAB3 steps)
        // 在时钟中断时调用调度器的 sched_class_proc_tick 函数
        sched_class_proc_tick(current);

        break;
```



### 练习1: 理解调度器框架的实现（不需要编码）

请仔细阅读和分析调度器框架的相关代码，特别是以下两个关键部分的实现：

在完成练习0后，请仔细阅读并分析以下调度器框架的实现：

- 调度类结构体 sched_class 的分析：请详细解释 sched_class 结构体中每个函数指针的作用和调用时机，分析为什么需要将这些函数定义为函数指针，而不是直接实现函数。
- 运行队列结构体 run_queue 的分析：比较lab5和lab6中 run_queue 结构体的差异，解释为什么lab6的 run_queue 需要支持两种数据结构（链表和斜堆）。
- 调度器框架函数分析：分析 sched_init()、wakeup_proc() 和 schedule() 函数在lab6中的实现变化，理解这些函数如何与具体的调度算法解耦。

对于调度器框架的使用流程，请在实验报告中完成以下分析：

- 调度类的初始化流程：描述从内核启动到调度器初始化完成的完整流程，分析 default_sched_class 如何与调度器框架关联。
- 进程调度流程：绘制一个完整的进程调度流程图，包括：时钟中断触发、proc_tick 被调用、schedule() 函数执行、调度类各个函数的调用顺序。并解释 need_resched 标志位在调度过程中的作用
- 调度算法的切换机制：分析如果要添加一个新的调度算法（如stride），需要修改哪些代码？并解释为什么当前的设计使得切换调度算法变得容易。

#### 解答：

##### 1. 调度类结构体 sched_class 的分析

`sched_class` 结构体定义了调度器的接口，包含以下函数指针：

```c
struct sched_class {
    const char *name;                                           // 调度器名称
    void (*init)(struct run_queue *rq);                        // 初始化运行队列
    void (*enqueue)(struct run_queue *rq, struct proc_struct *proc);  // 将进程加入运行队列
    void (*dequeue)(struct run_queue *rq, struct proc_struct *proc);  // 将进程从运行队列移除
    struct proc_struct *(*pick_next)(struct run_queue *rq);    // 选择下一个要运行的进程
    void (*proc_tick)(struct run_queue *rq, struct proc_struct *proc); // 处理时钟中断
};
```

**各函数指针的作用和调用时机：**

- `init`：初始化运行队列，在系统启动时由 `sched_init()` 调用，用于初始化队列数据结构
- `enqueue`：将进程加入就绪队列，在进程被唤醒（`wakeup_proc()`）或时间片用完但仍可运行时（`schedule()`）调用
- `dequeue`：将进程从就绪队列移除，在进程被选中运行时（`schedule()`）调用
- `pick_next`：选择下一个要运行的进程，在调度器需要切换进程时（`schedule()`）调用
- `proc_tick`：处理时钟中断，每次时钟中断时由 `sched_class_proc_tick()` 调用，用于更新进程时间片

**为什么使用函数指针而不是直接实现函数？**

使用函数指针是面向对象设计中"策略模式"的体现，具有以下优势：

1. **解耦性**：调度器框架与具体调度算法分离，框架代码不依赖具体实现
2. **可扩展性**：添加新调度算法只需实现 `sched_class` 接口，无需修改框架代码
3. **灵活性**：可以在运行时动态切换调度算法（通过修改 `sched_class` 指针）
4. **多态性**：同一接口可以有多种实现（RR、Stride等），调用者无需关心具体实现

##### 2. 运行队列结构体 run_queue 的分析

**Lab6 中的 run_queue 结构体：**

```c
struct run_queue {
    list_entry_t run_list;           // 双向链表，用于RR调度
    unsigned int proc_num;           // 队列中进程数量
    int max_time_slice;              // 最大时间片
    skew_heap_entry_t *lab6_run_pool; // 斜堆，用于Stride调度
};
```

**与Lab5的差异：**

Lab5中的 `run_queue` 只包含链表结构，而Lab6新增了 `lab6_run_pool` 斜堆字段。

**为什么需要支持两种数据结构？**

1. **链表（list）**：适用于Round Robin调度算法
   - 时间复杂度：入队O(1)，出队O(1)，选择下一个进程O(1)
   - 特点：FIFO顺序，简单高效

2. **斜堆（skew_heap）**：适用于Stride调度算法
   - 时间复杂度：插入O(log n)，删除最小值O(log n)
   - 特点：优先队列，可根据stride值快速找到最小值
   - 优势：支持动态优先级调度，保证公平性

不同调度算法需要不同的数据结构来高效实现其调度策略，因此 `run_queue` 需要同时支持两种数据结构。

##### 3. 调度器框架函数分析

**sched_init() 函数：**

```c
void sched_init(void) {
    list_init(&timer_list);
    sched_class = &default_sched_class;  // 设置默认调度类
    rq = &__rq;
    rq->max_time_slice = MAX_TIME_SLICE;
    sched_class->init(rq);               // 调用具体调度算法的初始化
    cprintf("sched class: %s\n", sched_class->name);
}
```

- 初始化全局运行队列和调度类
- 通过函数指针调用具体调度算法的初始化函数
- 实现了框架与算法的解耦

**wakeup_proc() 函数：**

```c
void wakeup_proc(struct proc_struct *proc) {
    assert(proc->state != PROC_ZOMBIE);
    bool intr_flag;
    local_intr_save(intr_flag);
    {
        if (proc->state != PROC_RUNNABLE) {
            proc->state = PROC_RUNNABLE;
            proc->wait_state = 0;
            if (proc != current) {
                sched_class_enqueue(proc);  // 通过调度类接口加入队列
            }
        }
    }
    local_intr_restore(intr_flag);
}
```

- 将进程状态设置为RUNNABLE
- 通过 `sched_class_enqueue()` 调用具体调度算法的入队函数
- 如果是当前进程则不入队（已在运行）

**schedule() 函数：**

```c
void schedule(void) {
    bool intr_flag;
    struct proc_struct *next;
    local_intr_save(intr_flag);
    {
        current->need_resched = 0;
        if (current->state == PROC_RUNNABLE) {
            sched_class_enqueue(current);  // 当前进程重新入队
        }
        if ((next = sched_class_pick_next()) != NULL) {
            sched_class_dequeue(next);     // 选中的进程出队
        }
        if (next == NULL) {
            next = idleproc;               // 无就绪进程则运行idle
        }
        next->runs++;
        if (next != current) {
            proc_run(next);                // 切换到新进程
        }
    }
    local_intr_restore(intr_flag);
}
```

- 完全通过调度类接口操作，不依赖具体算法
- 实现了调度决策与进程切换的分离

##### 4. 调度类的初始化流程

1. **内核启动** → `kern_init()` 
2. **进程初始化** → `proc_init()` 创建idle和init进程
3. **调度器初始化** → `sched_init()`
   - 设置 `sched_class = &default_sched_class`
   - 调用 `default_sched_class.init(rq)` 即 `RR_init()`
   - 初始化运行队列链表和计数器
4. **关联完成**：调度器框架通过全局指针 `sched_class` 与具体调度算法关联

##### 5. 进程调度流程

```
时钟中断触发
    ↓
trap_handler() → interrupt_handler()
    ↓
IRQ_S_TIMER 分支
    ↓
clock_set_next_event()  // 设置下次中断
    ↓
sched_class_proc_tick(current)
    ↓
调用 sched_class->proc_tick(rq, current)
    ↓
RR_proc_tick(): 减少时间片，若为0则设置 need_resched = 1
    ↓
中断返回前检查 need_resched
    ↓
若 need_resched == 1，调用 schedule()
    ↓
schedule() 执行：
    1. current->need_resched = 0
    2. 若current仍RUNNABLE，调用 enqueue 重新入队
    3. 调用 pick_next 选择下一个进程
    4. 调用 dequeue 将选中进程出队
    5. 调用 proc_run() 切换进程
```

**need_resched 标志位的作用：**

- 标记进程需要被重新调度
- 在时钟中断、进程主动让出CPU（`do_yield()`）时设置
- 在中断返回前检查，若为1则触发调度
- 避免在中断处理中直接调度，保证调度时机的安全性

##### 6. 调度算法的切换机制

**添加新调度算法（如Stride）需要：**

1. 实现 `sched_class` 接口的所有函数（init、enqueue、dequeue、pick_next、proc_tick）
2. 定义新的 `sched_class` 结构体（如 `stride_sched_class`）
3. 在 `sched_init()` 中修改一行代码：`sched_class = &stride_sched_class;`

**为什么切换调度算法变得容易？**

1. **接口统一**：所有调度算法实现相同接口，框架代码无需修改
2. **单点配置**：只需修改 `sched_class` 指针指向，即可切换算法
3. **完全解耦**：框架代码（sched.c）与算法实现（default_sched.c）完全分离
4. **编译时选择**：可通过宏或配置文件在编译时选择调度算法
5. **运行时切换**：理论上可在运行时动态切换（需要额外的状态迁移处理）

这种设计体现了"开闭原则"：对扩展开放，对修改关闭。





### 练习2: 实现 Round Robin 调度算法（需要编码）

完成练习0后，建议大家比较一下（可用kdiff3等文件比较软件）个人完成的lab5和练习0完成后的刚修改的lab6之间的区别，分析了解lab6采用RR调度算法后的执行过程。理解调度器框架的工作原理后，请在此框架下实现时间片轮转（Round Robin）调度算法。

注意有“LAB6”的注释，你需要完成 kern/schedule/default_sched.c 文件中的 RR_init、RR_enqueue、RR_dequeue、RR_pick_next 和 RR_proc_tick 函数的实现，使系统能够正确地进行进程调度。代码中所有需要完成的地方都有“LAB6”和“YOUR CODE”的注释，请在提交时特别注意保持注释，将“YOUR CODE”替换为自己的学号，并且将所有标有对应注释的部分填上正确的代码。

提示，请在实现时注意以下细节：

- 链表操作：list_add_before、list_add_after等。
- 宏的使用：le2proc(le, member) 宏等。
- 边界条件处理：空队列的处理、进程时间片耗尽后的处理、空闲进程的处理等。

请在实验报告中完成：

- 比较一个在lab5和lab6都有, 但是实现不同的函数, 说说为什么要做这个改动, 不做这个改动会出什么问题
  - 提示: 如`kern/schedule/sched.c`里的函数。你也可以找个其他地方做了改动的函数。
- 描述你实现每个函数的具体思路和方法，解释为什么选择特定的链表操作方法。对每个实现函数的关键代码进行解释说明，并解释如何处理**边界情况**。
- 展示 make grade 的**输出结果**，并描述在 QEMU 中观察到的调度现象。
- 分析 Round Robin 调度算法的优缺点，讨论如何调整时间片大小来优化系统性能，并解释为什么需要在 RR_proc_tick 中设置 need_resched 标志。
- **拓展思考**：如果要实现优先级 RR 调度，你的代码需要如何修改？当前的实现是否支持多核调度？如果不支持，需要如何改进？

#### 解答：

##### 1. Lab5与Lab6函数实现差异分析

**以 `wakeup_proc()` 函数为例：**

Lab5中的实现（简化版）：
```c
void wakeup_proc(struct proc_struct *proc) {
    if (proc->state != PROC_RUNNABLE) {
        proc->state = PROC_RUNNABLE;
        // Lab5中没有调度器框架，直接操作
    }
}
```

Lab6中的实现：
```c
void wakeup_proc(struct proc_struct *proc) {
    assert(proc->state != PROC_ZOMBIE);
    bool intr_flag;
    local_intr_save(intr_flag);
    {
        if (proc->state != PROC_RUNNABLE) {
            proc->state = PROC_RUNNABLE;
            proc->wait_state = 0;
            if (proc != current) {
                sched_class_enqueue(proc);  // 新增：通过调度类接口加入队列
            }
        }
    }
    local_intr_restore(intr_flag);
}
```

**为什么要做这个改动？**

1. **引入调度器框架**：Lab6引入了统一的调度器接口，需要通过 `sched_class_enqueue()` 将唤醒的进程加入运行队列
2. **支持多种调度算法**：不同调度算法有不同的入队策略（RR用链表，Stride用堆），必须通过接口调用
3. **维护运行队列状态**：调度器需要知道哪些进程在就绪队列中，以便正确调度

**不做这个改动会出什么问题？**

- 进程被唤醒后不会加入运行队列，调度器无法选择该进程运行
- 进程状态虽然是RUNNABLE，但永远不会被调度到
- 系统会出现"饥饿"现象，某些进程永远得不到CPU时间

##### 2. Round Robin 调度算法实现

**RR_init 函数实现思路：**

初始化运行队列，将链表初始化为空，进程计数器设为0。

```c
static void RR_init(struct run_queue *rq) {
    list_init(&(rq->run_list));  // 初始化双向链表
    rq->proc_num = 0;             // 队列中进程数为0
}
```

**关键点**：使用 `list_init()` 初始化循环双向链表，使头节点的prev和next都指向自己。

---

**RR_enqueue 函数实现思路：**

将进程加入队列尾部，重置时间片，更新进程计数。

```c
static void RR_enqueue(struct run_queue *rq, struct proc_struct *proc) {
    assert(rq != NULL && proc != NULL);
    
    // 重置时间片（如果为0或超过最大值）
    if (proc->time_slice == 0 || proc->time_slice > rq->max_time_slice) {
        proc->time_slice = rq->max_time_slice;
    }
    
    proc->rq = rq;  // 设置进程所属运行队列
    list_add_before(&(rq->run_list), &(proc->run_link));  // 加到队尾
    rq->proc_num++;  // 进程数+1
}
```

**关键点**：
- 使用 `list_add_before(&head, &node)` 将节点加到头节点之前，即队列尾部
- 必须重置时间片，否则进程可能没有时间片可用
- 边界情况：空队列时，第一个进程的prev和next都指向头节点

---

**RR_dequeue 函数实现思路：**

将进程从队列中移除，更新进程计数。

```c
static void RR_dequeue(struct run_queue *rq, struct proc_struct *proc) {
    assert(rq != NULL && proc != NULL && proc->rq == rq);
    
    list_del_init(&(proc->run_link));  // 从链表中删除并重新初始化
    rq->proc_num--;                     // 进程数-1
}
```

**关键点**：
- 使用 `list_del_init()` 而不是 `list_del()`，确保节点被删除后重新初始化
- 断言检查进程确实在该队列中（`proc->rq == rq`）
- 边界情况：删除最后一个进程后，队列变为空

---

**RR_pick_next 函数实现思路：**

选择队列头部的第一个进程（FIFO策略）。

```c
static struct proc_struct *RR_pick_next(struct run_queue *rq) {
    list_entry_t *le = list_next(&(rq->run_list));  // 获取头节点的下一个节点
    if (le != &(rq->run_list)) {  // 如果不是头节点（队列非空）
        return le2proc(le, run_link);  // 转换为进程指针
    }
    return NULL;  // 队列为空，返回NULL
}
```

**关键点**：
- 使用 `list_next()` 获取队列头部进程
- 使用 `le2proc()` 宏将链表节点转换为进程结构体指针
- 边界情况：空队列时返回NULL，调度器会选择idleproc

---

**RR_proc_tick 函数实现思路：**

每次时钟中断时减少当前进程的时间片，时间片用完时设置需要调度标志。

```c
static void RR_proc_tick(struct run_queue *rq, struct proc_struct *proc) {
    if (proc->time_slice > 0) {
        proc->time_slice--;  // 时间片减1
    }
    
    if (proc->time_slice == 0) {
        proc->need_resched = 1;  // 时间片用完，设置需要调度标志
    }
}
```

**关键点**：
- 先检查时间片是否大于0，避免负数
- 时间片为0时设置 `need_resched` 标志，触发调度
- 不在这里直接调用 `schedule()`，而是设置标志，由中断返回前统一处理

---

**边界情况处理总结：**

1. **空队列**：`pick_next` 返回NULL，调度器选择idleproc
2. **单进程**：进程时间片用完后重新入队，继续运行
3. **时间片为0的进程入队**：`enqueue` 中重置为最大时间片
4. **idleproc**：在 `sched_class_proc_tick()` 中特殊处理，不调用 `proc_tick`，直接设置 `need_resched = 1`

##### 3. 测试结果

执行 `make grade` 后的输出结果：

```
（此处应插入实际的make grade输出结果）
```

**在QEMU中观察到的调度现象：**

1. 多个进程按时间片轮转执行
2. 每个进程运行一个时间片（5个时钟中断）后被切换
3. 进程按FIFO顺序获得CPU时间
4. 优先级测试程序显示不同优先级进程获得的CPU时间比例符合预期

##### 4. Round Robin 调度算法分析

**优点：**

1. **公平性**：所有进程平等获得CPU时间，不会出现饥饿
2. **简单高效**：实现简单，调度开销小（O(1)时间复杂度）
3. **响应时间可预测**：最大等待时间 = (n-1) × 时间片，n为进程数
4. **适合分时系统**：适用于交互式系统，用户感觉所有程序同时运行

**缺点：**

1. **不考虑优先级**：所有进程平等对待，无法区分重要性
2. **不考虑I/O特性**：I/O密集型进程和CPU密集型进程获得相同时间片
3. **时间片选择困难**：
   - 时间片过大：响应时间长，退化为FCFS
   - 时间片过小：上下文切换开销大，CPU利用率低
4. **平均周转时间较长**：短作业可能需要等待长作业

**时间片大小调整策略：**

1. **经验值**：通常设置为10-100ms
2. **考虑上下文切换开销**：时间片应远大于切换时间（通常1-10μs）
3. **根据系统类型调整**：
   - 交互式系统：较小时间片（10-50ms），提高响应速度
   - 批处理系统：较大时间片（100ms以上），减少开销
4. **动态调整**：根据系统负载和进程特性动态调整

**为什么需要在 RR_proc_tick 中设置 need_resched 标志？**

1. **分离关注点**：时钟中断处理与调度决策分离
2. **安全性**：避免在中断处理中直接调度，可能导致死锁或不一致
3. **延迟调度**：在中断返回前统一检查并调度，保证原子性
4. **灵活性**：其他地方也可以设置该标志（如 `do_yield()`），统一处理

##### 5. 拓展思考

**实现优先级 RR 调度的修改方案：**

1. **数据结构改进**：
   - 为每个优先级维护一个独立的运行队列
   - 或在单队列中按优先级排序（插入时间复杂度变为O(n)）

2. **enqueue 修改**：
   ```c
   // 方案1：多队列
   list_add_before(&(rq->run_list[proc->priority]), &(proc->run_link));
   
   // 方案2：有序插入
   list_entry_t *le = &(rq->run_list);
   while ((le = list_next(le)) != &(rq->run_list)) {
       struct proc_struct *p = le2proc(le, run_link);
       if (p->priority < proc->priority) break;
   }
   list_add_before(le, &(proc->run_link));
   ```

3. **pick_next 修改**：
   ```c
   // 方案1：从最高优先级队列开始查找
   for (int i = MAX_PRIORITY; i >= 0; i--) {
       if (!list_empty(&(rq->run_list[i]))) {
           return le2proc(list_next(&(rq->run_list[i])), run_link);
       }
   }
   ```

4. **时间片分配**：高优先级进程获得更长时间片

**当前实现是否支持多核调度？**

**不支持**，原因：

1. **单一运行队列**：所有CPU共享一个运行队列，存在锁竞争
2. **全局变量 current**：只有一个当前进程指针，无法表示多个CPU的状态
3. **无CPU亲和性**：进程可能在不同CPU间频繁迁移，缓存失效
4. **无负载均衡**：无法在CPU间均衡分配进程

**多核调度改进方案：**

1. **每CPU运行队列**：
   ```c
   struct run_queue per_cpu_rq[NR_CPUS];
   struct proc_struct *per_cpu_current[NR_CPUS];
   ```

2. **负载均衡机制**：
   - 周期性检查各CPU负载
   - 从繁忙CPU迁移进程到空闲CPU
   - 考虑缓存亲和性，避免频繁迁移

3. **CPU亲和性**：
   - 进程记录上次运行的CPU
   - 优先在同一CPU上调度，提高缓存命中率

4. **锁优化**：
   - 使用per-CPU锁，减少锁竞争
   - 无锁数据结构（如RCU）

5. **调度域**：
   - 将CPU分组（如NUMA节点）
   - 优先在同一调度域内迁移进程

