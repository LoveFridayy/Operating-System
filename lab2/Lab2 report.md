# Lab2

组员：杨桑多杰 徐南海 张磊

## 练习1：理解first-fit 连续物理内存分配算法（思考题）

### 设计思想

first-fit算法是一个用来分配连续物理内存的算法，维护一个空闲的块列表，当需要内存时，我们就找到对应的一块内存最大的空闲块，分配给对应的进程。

### 实现方法

将空闲分区链以地址递增的顺序连接；在进行内存分配时，从链首开始顺序查找，直到找到一块分区的大小可以满足需求时，按照该作业的大小，从该分区中分配出内存，将剩下的空闲分区仍然链在空闲分区链中。回收时会按照地址从小到大的顺序插入链表，并且合并与之相邻且连续的空闲内存块。

### 代码分析

***default_init（）***

```c++
static void
default_init(void) { //初始化链表
    list_init(&free_list); //空循环链表
    nr_free = 0; //空闲页总数清0
}

//其中list_init()函数的定义如下
static inline void
list_init(list_entry_t *elm) {
    elm->prev = elm->next = elm;
}
```

**该函数用于初始化存放空闲块的链表**，首先调用***list_init***函数，初始化一个空的双向链表***free_list***，然后定义了空闲块的个数***nr_free***为0。

***default_init_memmap（）***

```c++
static void
default_init_memmap(struct Page *base, size_t n) { //将一段检测到的连续物理内存（从base开始，共n页）加入空闲内存池
    assert(n > 0); //确保加入的内存块有效
    struct Page *p = base; //定义一个指针p，它的类型是指向Page结构体的指针，并将其初始化为指向base所指向的内存地址
    for (; p != base + n; p ++) { //遍历n个结构体
        assert(PageReserved(p)); //首先检查该页面是否为保留页面
        p->flags = p->property = 0; //初始化：清除它们的flags和property字段
        set_page_ref(p, 0); //引用次数清零
    }
    base->property = n; //表示它是一个大小为n的空闲块的起始页
    SetPageProperty(base); //设置base页的标志位，确认空闲块头部的身份
    nr_free += n; // 更新nr_free的值
    if (list_empty(&free_list)) { //if函数用于判断该列表是否为空
        list_add(&free_list, &(base->page_link)); //如果空的话，就将起始页面的链表节点添加到链表中
    } else { 
        list_entry_t* le = &free_list; //首先初始化一个指针指向我们的空闲链表头
        while ((le = list_next(le)) != &free_list) { //一直往后找，找到了合适的位置，也就是地址大小按照顺序排列
            struct Page* page = le2page(le, page_link);
            if (base < page) { //如果新页面的起始地址base小于当前遍历到的页面page的地址，说明找到了合适的插入位置
                list_add_before(le, &(base->page_link));
                break;
            } else if (list_next(le) == &free_list) { //如果不是的话，就在后面链接即可
                list_add(le, &(base->page_link));
            }
        }
    }
}
```

**该函数用于初始化一个空的内存块**，将一段检测到的连续物理内存（从***base***开始，共n页）加入空闲内存池。

首先，先判断n是否大于0，即判断加入内存块的有效性。

然后我们定义指针p，它是指向***Page***结构体的指针，并将其初始化为指向***base***所指向的内存地址，随后遍历n个结构体，检查该页面是否为保留页面，清除它们的flags和property字段，并将引用次数清零，完成初始化。

```c++
//其中set_page_ref函数的功能是将给定Page结构体指针所指向的页面的引用计数为指定的值。
static inline void set_page_ref(struct Page *page, int val) { page->ref = val; }
```

然后，将base页面的***property***属性设置为n即块总数，设置base页的标志位，确认空闲块头部的身份，更新nr_free的值，后面if函数用于判断该列表是否为空：如果为空，，就将起始页面的链表节点添加到链表中；如果不为空，则遍历链表找到合适的位置插入新的页面链表节点。

***default_alloc_pages（）***

```c++
static struct Page *
default_alloc_pages(size_t n) { //根据first-fit算法分配n个连续物理页
    assert(n > 0);
    if (n > nr_free) { //检查n是否大于总空闲页数nr_free
        return NULL;
    }
    struct Page *page = NULL;
    list_entry_t *le = &free_list; //从free_list头部开始遍历
    while ((le = list_next(le)) != &free_list) {
        struct Page *p = le2page(le, page_link); //使用le2page宏从链表节点转换为Page结构体
        if (p->property >= n) { //当每个空闲块的p->property大于等于请求的大小n则停止遍历
            page = p;
            break;
        }
    }
/*
 *  如果找到的块比请求的大，则计算剩余部分的新起始页*p = page + n，设置新块的大小p->property = page->property - n，
 *  将这个更新的更小的块list_add到free_list中，更新总的空闲页数nr_free -= n，清除返回页的标志ClearPageProperty(page)
*/
    if (page != NULL) {
        list_entry_t* prev = list_prev(&(page->page_link));
        list_del(&(page->page_link)); //将块从free_list中删除
        if (page->property > n) {
            struct Page *p = page + n;
            p->property = page->property - n;
            SetPageProperty(p);
            list_add(prev, &(p->page_link));
        }
        nr_free -= n;
        ClearPageProperty(page);
    }
    return page;
}
```

这个函数用来根据first-fit算法分配内存。它查找空闲链表中第一个大于n的内存块。找到后，它会把这个块分成两部分。第一部分用来分配，第二部分留在链表中。如果剩余部分的大小仍然大于n，它会更新这个块的属性并将其放回链表。分配后，系统会减少空闲块的数量，并标记这个块为已分配。

***default_free_pages（）***

```c++
static struct Page *
default_alloc_pages(size_t n) { //根据first-fit算法分配n个连续物理页
    assert(n > 0);
    if (n > nr_free) { //检查n是否大于总空闲页数nr_free
        return NULL;
    }
    struct Page *page = NULL;
    list_entry_t *le = &free_list; //从free_list头部开始遍历
    while ((le = list_next(le)) != &free_list) {
        struct Page *p = le2page(le, page_link); //使用le2page宏从链表节点转换为Page结构体
        if (p->property >= n) { //当每个空闲块的p->property大于等于请求的大小n则停止遍历
            page = p;
            break;
        }
    }
/*
 *  如果找到的块比请求的大，则计算剩余部分的新起始页*p = page + n，设置新块的大小p->property = page->property - n，
 *  将这个更新的更小的块list_add到free_list中，更新总的空闲页数nr_free -= n，清除返回页的标志ClearPageProperty(page)
*/
    if (page != NULL) {
        list_entry_t* prev = list_prev(&(page->page_link));
        list_del(&(page->page_link)); //将块从free_list中删除
        if (page->property > n) {
            struct Page *p = page + n;
            p->property = page->property - n;
            SetPageProperty(p);
            list_add(prev, &(p->page_link));
        }
        nr_free -= n;
        ClearPageProperty(page);
    }
    return page;
}

static void
default_free_pages(struct Page *base, size_t n) { //从base开始释放一个之前分配的大小为n内存块
    assert(n > 0);
    struct Page *p = base;
    for (; p != base + n; p ++) {
        assert(!PageReserved(p) && !PageProperty(p));
        p->flags = 0;
        set_page_ref(p, 0);
    }
/*
    初始化待释放的n个页面的状态，包括清楚标志位flags，引用计数ref清零
*/
    base->property = n;
    SetPageProperty(base); //设置base->property = n和相应的标志位，将其标记为一个新的空闲块。
    nr_free += n; //更新总空闲页数

/*
    将这个新释放的块按地址顺序插入到free_list中
*/
    if (list_empty(&free_list)) {
        list_add(&free_list, &(base->page_link));
    } else {
        list_entry_t* le = &free_list;
        while ((le = list_next(le)) != &free_list) {
            struct Page* page = le2page(le, page_link);
            if (base < page) {
                list_add_before(le, &(base->page_link));
                break;
            } else if (list_next(le) == &free_list) {
                list_add(le, &(base->page_link));
            }
        }
    }
/*
    以下为合并过程，one of the most important keyprogresses,作用为减少内存碎片
*/
    list_entry_t* le = list_prev(&(base->page_link)); //与前一个块合并
/*
 *  检查 free_list 中紧邻它前面的那个块 (p)。如果 p + p->property == base，说明这两个块在物理上是连续的。
 *  此时，将它们的内存合并：p的大小增加n，然后将base块从链表中删除。
*/
    if (le != &free_list) {
        p = le2page(le, page_link);
        if (p + p->property == base) {
            p->property += base->property;
            ClearPageProperty(base);
            list_del(&(base->page_link));
            base = p;
        }
    }

    le = list_next(&(base->page_link)); //与后一个块合并
/*
 *  检查 free_list 中紧邻它后面的那个块 (p)。如果 base + base->property == p，说明它们也是连续的。
 *  此时，将base的大小增加p的大小，然后将p块从链表中删除。
*/
    if (le != &free_list) {
        p = le2page(le, page_link);
        if (base + base->property == p) {
            base->property += p->property;
            ClearPageProperty(p);
            list_del(&(p->page_link));
        }
    }
}
```

该函数用于释放内存块。它会将释放的内存块添加到空闲链表中。之后，它会检查相邻的空闲块。如果相邻块也是空闲的，它们会被合并成一个更大的块。首先，如果页面的保留属性和页面数量属性都不是初始值，我们就重置这些属性。然后，我们将页面的引用数设置为0。接着，我们更新空闲块的数量。最后，将页面加入空闲链表中，并尝试合并相邻的空闲块。如果释放的页面有空闲的前或后块，会将它们合并成一个更大的空闲块

***default_nr_free_pages（）***

```c++
static size_t
default_nr_free_pages(void) {
    return nr_free;
}
```

该函数用于获取当前的空闲页面的数量。

***basic_check（）、default_check( )***

```c++
static void
basic_check(void) {
    struct Page *p0, *p1, *p2;
    p0 = p1 = p2 = NULL;
    assert((p0 = alloc_page()) != NULL);
    assert((p1 = alloc_page()) != NULL);
    assert((p2 = alloc_page()) != NULL);

    assert(p0 != p1 && p0 != p2 && p1 != p2);
    assert(page_ref(p0) == 0 && page_ref(p1) == 0 && page_ref(p2) == 0);

    assert(page2pa(p0) < npage * PGSIZE);
    assert(page2pa(p1) < npage * PGSIZE);
    assert(page2pa(p2) < npage * PGSIZE);

    list_entry_t free_list_store = free_list;
    list_init(&free_list);
    assert(list_empty(&free_list));

    unsigned int nr_free_store = nr_free;
    nr_free = 0;

    assert(alloc_page() == NULL);

    free_page(p0);
    free_page(p1);
    free_page(p2);
    assert(nr_free == 3);

    assert((p0 = alloc_page()) != NULL);
    assert((p1 = alloc_page()) != NULL);
    assert((p2 = alloc_page()) != NULL);

    assert(alloc_page() == NULL);

    free_page(p0);
    assert(!list_empty(&free_list));

    struct Page *p;
    assert((p = alloc_page()) == p0);
    assert(alloc_page() == NULL);

    assert(nr_free == 0);
    free_list = free_list_store;
    nr_free = nr_free_store;

    free_page(p);
    free_page(p1);
    free_page(p2);
}

// LAB2: below code is used to check the first fit allocation algorithm
// NOTICE: You SHOULD NOT CHANGE basic_check, default_check functions!
static void
default_check(void) {
    int count = 0, total = 0;
    list_entry_t *le = &free_list;
    while ((le = list_next(le)) != &free_list) {
        struct Page *p = le2page(le, page_link);
        assert(PageProperty(p));
        count ++, total += p->property;
    }
    assert(total == nr_free_pages());

    basic_check();

    struct Page *p0 = alloc_pages(5), *p1, *p2;
    assert(p0 != NULL);
    assert(!PageProperty(p0));

    list_entry_t free_list_store = free_list;
    list_init(&free_list);
    assert(list_empty(&free_list));
    assert(alloc_page() == NULL);

    unsigned int nr_free_store = nr_free;
    nr_free = 0;

    free_pages(p0 + 2, 3);
    assert(alloc_pages(4) == NULL);
    assert(PageProperty(p0 + 2) && p0[2].property == 3);
    assert((p1 = alloc_pages(3)) != NULL);
    assert(alloc_page() == NULL);
    assert(p0 + 2 == p1);

    p2 = p0 + 1;
    free_page(p0);
    free_pages(p1, 3);
    assert(PageProperty(p0) && p0->property == 1);
    assert(PageProperty(p1) && p1->property == 3);

    assert((p0 = alloc_page()) == p2 - 1);
    free_page(p0);
    assert((p0 = alloc_pages(2)) == p2 + 1);

    free_pages(p0, 2);
    free_page(p2);

    assert((p0 = alloc_pages(5)) != NULL);
    assert(alloc_page() == NULL);

    assert(nr_free == 0);
    nr_free = nr_free_store;

    free_list = free_list_store;
    free_pages(p0, 5);

    le = &free_list;
    while ((le = list_next(le)) != &free_list) {
        struct Page *p = le2page(le, page_link);
        count --, total -= p->property;
    }
    assert(count == 0);
    assert(total == 0);
}
```

用于对前面的一些功能的检测，包括页面分配，引用计数，空闲页面的链接操作等以及对内存管理系统进行更全面的检查，包括对空闲页面链表的遍历和属性检查、页面分配和释放的各种场景测试。

***结构体default_pmm_manager***

```c++
const struct pmm_manager default_pmm_manager = {
    .name = "default_pmm_manager",
    .init = default_init,
    .init_memmap = default_init_memmap,
    .alloc_pages = default_alloc_pages,
    .free_pages = default_free_pages,
    .nr_free_pages = default_nr_free_pages,
    .check = default_check,
};
```

这个结构体用于内存管理相关的功能，其中包含了多个函数指针和一个字符串成员：

- `.name = "default_pmm_manager"`：用于标识这个内存管理器的名称。
- `.init = default_init`：函数指针，用于初始化内存管理器的某些状态。
- `.init_memmap = default_init_memmap`：函数指针，用于设置内存页面的初始状态。
- `.alloc_pages = default_alloc_pages`：函数指针，指向一个用于分配页面的函数。
- `.free_pages = default_free_pages`：函数指针，指向一个用于释放页面的函数。
- `.nr_free_pages = default_nr_free_pages`：函数指针，指向一个用于获取空闲页面数量的函数。
- `.check = default_check`：函数指针，用于内存分配情况的检查。

### 过程分析

 物理内存分配的完整流程：
 * 1、启动与初始化：内核启动时，首先调用default_init()，准备好一个空的free_list。
 * 2、构建内存地图：内核通过引导程序（如GRUB）获取物理内存布局信息。对于每一个可用的物理内存段，
 *    内核会调用default_init_memmap()，将其作为一个大的空闲块加入到free_list中。
 * 3、内存分配请求：当内核的其他部分（如进程管理、文件系统缓存）需要内存时，会调用default_alloc_pages(n)。
 * 4、执行分配：管理器在free_list中找到第一个能满足n页需求的块，可能会将其分割，然后返回所需的内存。
 * 5、内存释放：当内存使用完毕后，调用default_free_pages()将其归还。
 * 6、执行释放与合并：管理器将归还的内存块重新加入 free_list，并立即尝试与相邻的空闲块合并，以备未来的大块内存请求。



### 改进空间

 * 1、改进搜索效率和策略：当前的首次适应算法在分配内存时，总是从链表的头部开始线性搜索，时间复杂度为 O(N)，其中N是空闲块的数量。当空闲块很多时，这个搜索过程会变得很慢。Google得知可使用下次适应算法或者分离空闲链表法实现，其中分离空闲链表法最优，其分配速度接近O（1），而极大减少了产生内存碎片。
 * 2、改进碎片管理策略：首次适应算法的主要问题是容易在链表的前部产生大量无法使用的小碎片（外部碎片）。我们可以使用最佳适应算法(Best-Fit)或者最差适应算法 (Worst-Fit)来减少碎片的产生。
 * 3、采用完全不同的内存管理模型：除了在首次适应算法上进行微调，还可以采用结构上完全不同的算法。如后面challenge中我们要实现的伙伴系统，或者SLUB分配算法来获取更高的内存管理性能



## 练习2：实现 Best-Fit 连续物理内存分配算法(需要编程)

1. 在 `best_fit_init_memmap` 函数中，程序需要把一段连续物理页框登记成一个大的空闲块，并按地址顺序插到全局空闲链表，因此需要逐页清零，然后按地址有序插入。
   详细代码如下：

  ```c
static void best_fit_init_memmap(struct Page *base, size_t n) {
    assert(n > 0);
    struct Page *p = base;
    for (; p != base + n; p ++) {
        assert(PageReserved(p));
        p->flags = 0;           // 清空所有标志
        set_page_ref(p, 0);     // 引用计数清零
        ClearPageProperty(p);   // 清除页面属性
    }
    base->property = n;
    SetPageProperty(base);
    nr_free += n;
    if (list_empty(&free_list)) {
        list_add(&free_list, &(base->page_link));
    } else {
        list_entry_t* le = &free_list;
        while ((le = list_next(le)) != &free_list) {
            struct Page* page = le2page(le, page_link);
            if (base < page) {
                // 找到第一个地址大于 base 的页
                list_add_before(le, &(base->page_link));
                break;
            }else if (list_next(le) == &free_list) {
                // 到达链表尾部
                list_add(le, &(base->page_link));
            }
        }
    }
}
  ```

2. 在 `best_fit_alloc_pages` 函数中，需要采用 best-fit 策略，分配连续 n 页物理内存，最后返回首页描述符。
   由于与 first-fit 算法相比，best-fit 需要遍历全表，并从其中找出最小但可以使用的块，因此需要新增一个临时变量 `min_size`，并修改原有逻辑。
     详细代码如下：

  ```c
static struct Page *best_fit_alloc_pages(size_t n) {
    assert(n > 0);
    if (n > nr_free) {
        return NULL;
    }
    struct Page *page = NULL;
    list_entry_t *le = &free_list;
    size_t min_size = nr_free + 1;    // 初始化为一个极大值

    while ((le = list_next(le)) != &free_list) {
        struct Page *p = le2page(le, page_link);
        if (p->property >= n && p->property < min_size) {
            page = p;
            min_size = p->property;   // 记录最小满足块
        }
    }
  
    if (page != NULL) {
        list_entry_t* prev = list_prev(&(page->page_link));
        list_del(&(page->page_link));
        if (page->property > n) {
            struct Page *p = page + n;
            p->property = page->property - n;
            SetPageProperty(p);
            list_add(prev, &(p->page_link));
        }
        nr_free -= n;
        ClearPageProperty(page);
    }
    return page;
}
  ```

3. 在 `best_fit_free_pages` 函数中，程序需要从 base 开始回收曾经分配的 n 页，接着插回空闲链表并立即前后向合并，从而保持地址有序与最大连续。
   详细代码如下：

  ```c
  static void best_fit_free_pages(struct Page *base, size_t n) {
      assert(n > 0);
      struct Page *p = base;
      for (; p != base + n; p ++) {
          assert(!PageReserved(p) && !PageProperty(p));
          p->flags = 0;
          set_page_ref(p, 0);
      }
      base->property = n;       // 当前块大小
      SetPageProperty(base);    // 标记为首页
      nr_free += n;             // 更新总空闲页数

      if (list_empty(&free_list)) {
          list_add(&free_list, &(base->page_link));
      } else {
          list_entry_t* le = &free_list;
          while ((le = list_next(le)) != &free_list) {
              struct Page* page = le2page(le, page_link);
              if (base < page) {
                  list_add_before(le, &(base->page_link));
                  break;
              } else if (list_next(le) == &free_list) {
                  list_add(le, &(base->page_link));
              }
          }
      }

      list_entry_t* le = list_prev(&(base->page_link));
      if (le != &free_list) {
          p = le2page(le, page_link);
          if (p + p->property == base) {        // 若地址连续
              p->property += base->property;    // 合并大小
              ClearPageProperty(base);
              list_del(&(base->page_link));
              base = p;                         // 合并之后 base 指向前块
          }
      }

      le = list_next(&(base->page_link));
      if (le != &free_list) {
          p = le2page(le, page_link);
          if (base + base->property == p) {
              base->property += p->property;
              ClearPageProperty(p);
              list_del(&(p->page_link));
          }
      }
  }
  ```

---

整体来看，程序是这样对物理内存进行分配和释放的：

1. 初始化
   程序首先使用 `best_fit_init` 函数将链表置空，接着 `best_fit_init_memmap` 函数则把一段连续物理页框登记为空闲：

   - 每页 `flags/ref` 清零，非首页 `property=0`
   - 首页 `property=n` 并置位 `PG_property`
   - 按页框物理地址从小到大插入 `free_list`，从而保证链表始终有序。

2. 分配内存
   在遍历内存时，程序中的 `best_fit_alloc_pages` 函数使用 best-fit 策略遍历有序链表，从而找到满足 `size>=n` 且最小的块：

   - 若整块>n，把剩余部分拆出来重新挂回链表（更新其 `property`）
   - 把分配出去的首页 `PG_reserved=1, PG_property=0`，`nr_free-=n`
   - 最后返回首页。

3. 释放内存
   在释放内存时，程序中的 `best_fit_free_pages` 函数进行了这些操作：

   - 把 base 及之后 n 页的每页 `flags/ref` 清零，首页设 `property=n` 并置  `PG_property`。

   - 按物理地址插回有序链表。

   - 立即检查前、后相邻块是否物理连续，是就合并：

     - 若前块结束地址等于 base 地址，则将前块 `property+=n`，最后摘除当前块。
     - 若 base 结束地址等于后块地址，则将当前块 `property+=后块`，最后摘除后块。

     合并后链表仍保持地址递增，保证下次分配能正确找到连续区。



## Challenge3：硬件的可用物理内存范围的获取方法(思考题)

1. **手动探测物理内存：**一种操作上比较简单的方式是逐块地访问内存，记录下可用内存的起止范围。例如，逐个向内存块中写入测试数据，并读取；同时设计相应的异常捕获机制。当系统尝试访问超出物理内存范围的地址时，引发异常（如段错误等），OS需要能够捕捉到异常，以此来确定内存的边界。
2. **利用外设间接探测：**通过与外围设备交互来间接推测物理内存范围：可以通过配置DMA控制器，指定内存地址和数据长度，让DMA执行内存操作。若数据传输失败，则该内存地址可能无效或不存在。
