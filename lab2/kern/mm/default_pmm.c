      
#include <pmm.h>
#include <list.h>
#include <string.h>
#include <default_pmm.h>

/* In the first fit algorithm, the allocator keeps a list of free blocks (known as the free list) and,
   on receiving a request for memory, scans along the list for the first block that is large enough to
   satisfy the request. If the chosen block is significantly larger than that requested, then it is 
   usually split, and the remainder added to the list as another free block.
   Please see Page 196~198, Section 8.2 of Yan Wei Min's chinese book "Data Structure -- C programming language"
*/
// you should rewrite functions: default_init,default_init_memmap,default_alloc_pages, default_free_pages.
/*
 * Details of FFMA
 * (1) Prepare: In order to implement the First-Fit Mem Alloc (FFMA), we should manage the free mem block use some list.
 *              The struct free_area_t is used for the management of free mem blocks. At first you should
 *              be familiar to the struct list in list.h. struct list is a simple doubly linked list implementation.
 *              You should know howto USE: list_init, list_add(list_add_after), list_add_before, list_del, list_next, list_prev
 *              Another tricky method is to transform a general list struct to a special struct (such as struct page):
 *              you can find some MACRO: le2page (in memlayout.h), (in future labs: le2vma (in vmm.h), le2proc (in proc.h),etc.)
 * (2) default_init: you can reuse the  demo default_init fun to init the free_list and set nr_free to 0.
 *              free_list is used to record the free mem blocks. nr_free is the total number for free mem blocks.
 * (3) default_init_memmap:  CALL GRAPH: kern_init --> pmm_init-->page_init-->init_memmap--> pmm_manager->init_memmap
 *              This fun is used to init a free block (with parameter: addr_base, page_number).
 *              First you should init each page (in memlayout.h) in this free block, include:
 *                  p->flags should be set bit PG_property (means this page is valid. In pmm_init fun (in pmm.c),
 *                  the bit PG_reserved is setted in p->flags)
 *                  if this page  is free and is not the first page of free block, p->property should be set to 0.
 *                  if this page  is free and is the first page of free block, p->property should be set to total num of block.
 *                  p->ref should be 0, because now p is free and no reference.
 *                  We can use p->page_link to link this page to free_list, (such as: list_add_before(&free_list, &(p->page_link)); )
 *              Finally, we should sum the number of free mem block: nr_free+=n
 * (4) default_alloc_pages: search find a first free block (block size >=n) in free list and reszie the free block, return the addr
 *              of malloced block.
 *              (4.1) So you should search freelist like this:
 *                       list_entry_t le = &free_list;
 *                       while((le=list_next(le)) != &free_list) {
 *                       ....
 *                 (4.1.1) In while loop, get the struct page and check the p->property (record the num of free block) >=n?
 *                       struct Page *p = le2page(le, page_link);
 *                       if(p->property >= n){ ...
 *                 (4.1.2) If we find this p, then it' means we find a free block(block size >=n), and the first n pages can be malloced.
 *                     Some flag bits of this page should be setted: PG_reserved =1, PG_property =0
 *                     unlink the pages from free_list
 *                     (4.1.2.1) If (p->property >n), we should re-caluclate number of the the rest of this free block,
 *                           (such as: le2page(le,page_link))->property = p->property - n;)
 *                 (4.1.3)  re-caluclate nr_free (number of the the rest of all free block)
 *                 (4.1.4)  return p
 *               (4.2) If we can not find a free block (block size >=n), then return NULL
 * (5) default_free_pages: relink the pages into  free list, maybe merge small free blocks into big free blocks.
 *               (5.1) according the base addr of withdrawed blocks, search free list, find the correct position
 *                     (from low to high addr), and insert the pages. (may use list_next, le2page, list_add_before)
 *               (5.2) reset the fields of pages, such as p->ref, p->flags (PageProperty)
 *               (5.3) try to merge low addr or high addr blocks. Notice: should change some pages's p->property correctly.
 */


/*
 *物理内存分配的完整流程：
 * 1、启动与初始化：内核启动时，首先调用default_init()，准备好一个空的free_list。
 * 2、构建内存地图：内核通过引导程序（如GRUB）获取物理内存布局信息。对于每一个可用的物理内存段，
 *    内核会调用default_init_memmap()，将其作为一个大的空闲块加入到free_list中。
 * 3、内存分配请求：当内核的其他部分（如进程管理、文件系统缓存）需要内存时，会调用default_alloc_pages(n)。
 * 4、执行分配：管理器在free_list中找到第一个能满足n页需求的块，可能会将其分割，然后返回所需的内存。
 * 5、内存释放：当内存使用完毕后，调用default_free_pages()将其归还。
 * 6、执行释放与合并：管理器将归还的内存块重新加入 free_list，并立即尝试与相邻的空闲块合并，以备未来的大块内存请求。
*/

/*
 *改进思路：
 * 1、改进搜索效率和策略：当前的首次适应算法在分配内存时，总是从链表的头部开始线性搜索，时间复杂度为 O(N)，
 *    其中N是空闲块的数量。当空闲块很多时，这个搜索过程会变得很慢。Google得知可使用下次适应算法或者分离空
 *    闲链表法实现，其中分离空闲链表法最优，其分配速度接近O（1），而极大减少了产生内存碎片。
 * 2、改进碎片管理策略：首次适应算法的主要问题是容易在链表的前部产生大量无法使用的小碎片（外部碎片）。我们
 *    可以使用最佳适应算法(Best-Fit)或者最差适应算法 (Worst-Fit)来减少碎片的产生。
 * 3、采用完全不同的内存管理模型：除了在首次适应算法上进行微调，还可以采用结构上完全不同的算法。如后面challenge
 *    中我们要实现的伙伴系统，或者SLUB分配算法来获取更高的内存管理性能
*/

free_area_t free_area;

#define free_list (free_area.free_list) //一个按物理地址排序的双向链表free_list来管理所有空闲内存块
#define nr_free (free_area.nr_free) //内部计数器，记录了当前系统中的所有空闲页的总数

static void
default_init(void) { //初始化链表
    list_init(&free_list); //空循环链表
    nr_free = 0; //空闲页总数清0
}

static void
default_init_memmap(struct Page *base, size_t n) { //将一段检测到的连续物理内存（从base开始，共n页）加入空闲内存池
    assert(n > 0); //确保加入的内存块有效
    struct Page *p = base;
    for (; p != base + n; p ++) { //遍历n个结构体
        assert(PageReserved(p));
        p->flags = p->property = 0; //初始化：清除它们的flags和property字段
        set_page_ref(p, 0);
    }
    base->property = n; //表示它是一个大小为n的空闲块的起始页
    SetPageProperty(base); //设置base页的标志位，确认空闲块头部的身份
    nr_free += n;
    if (list_empty(&free_list)) {  //遍历free_list，确保加入的新空闲块始终按物理地址从高到低排序
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
}

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

static size_t
default_nr_free_pages(void) {
    return nr_free;
}

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
//这个结构体在
const struct pmm_manager default_pmm_manager = {
    .name = "default_pmm_manager",
    .init = default_init,
    .init_memmap = default_init_memmap,
    .alloc_pages = default_alloc_pages,
    .free_pages = default_free_pages,
    .nr_free_pages = default_nr_free_pages,
    .check = default_check,
};