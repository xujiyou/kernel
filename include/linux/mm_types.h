#ifndef _LINUX_MM_TYPES_H
#define _LINUX_MM_TYPES_H

#include <linux/auxvec.h>
#include <linux/types.h>
#include <linux/threads.h>
#include <linux/list.h>
#include <linux/spinlock.h>
#include <linux/prio_tree.h>
#include <linux/rbtree.h>
#include <linux/rwsem.h>
#include <linux/completion.h>
#include <asm/page.h>
#include <asm/mmu.h>

#ifndef AT_VECTOR_SIZE_ARCH
#define AT_VECTOR_SIZE_ARCH 0
#endif
#define AT_VECTOR_SIZE (2*(AT_VECTOR_SIZE_ARCH + AT_VECTOR_SIZE_BASE + 1))

struct address_space;

#if NR_CPUS >= CONFIG_SPLIT_PTLOCK_CPUS
typedef atomic_long_t mm_counter_t;
#else  /* NR_CPUS < CONFIG_SPLIT_PTLOCK_CPUS */
typedef unsigned long mm_counter_t;
#endif /* NR_CPUS < CONFIG_SPLIT_PTLOCK_CPUS */

/*
 * Each physical page in the system has a struct page associated with
 * it to keep track of whatever it is we are using the page for at the
 * moment. Note that we have no way to track which tasks are using
 * a page, though if it is a pagecache page, rmap structures can tell us
 * who is mapping it.
 */
struct page {//页帧，此结构要尽量小，因为页的数目巨大,使用联合就是减小此结构的大小
	//falgs的各个比特位储存了体系结构无关的标志，用于描述页的属性
	unsigned long flags;		/* Atomic flags, some possibly
					 * updated asynchronously:原子标志，有些情况下会异步更新 */
	//_count是一个引用计数，表示内核中引用该页的次数，在其为0时，内核就知道page当前不再使用，因此可删除
	atomic_t _count;		/* Usage count, see below:使用计数，见下文. */
	union {
		//_mapcount表示在页表中有多少项指向该页
		atomic_t _mapcount;	/* Count of ptes mapped in mms,
					 * to show when page is mapped
					 * & limit reverse map searches.
		内存管理子系统中映射的页表项计数，用于表示页是否已经映射，还用于限制逆向映射搜索
					 */
		struct {		/* SLUB:用于slub分配器 */
			u16 inuse;
			u16 objects;
		};
	};
	union {
	    struct {
		//private指向一块私有数据，虚拟内存管理会忽略该数据，
		unsigned long private;		/* Mapping-private opaque data:由映射私有，不透明的数据
					 	 * usually used for buffer_heads：若设置了PagePrivate，通常用于buffer_heads
						 * if PagePrivate set; used for:若设置了PageSwapCache, 则用于swp_entry_t
						 * swp_entry_t if PageSwapCache;若设置PG_buddy,则用于伙伴系统的阶
						 * indicates order in the buddy
						 * system if PG_buddy is set.
						 */
		//mapping指定了页帧所在的地址空间
		struct address_space *mapping;	/* If low bit clear, points to：若低位为0，则指向inode address_space或NULL
						 * inode address_space, or NULL.
						 * If page mapped as anonymous若页映射为匿名内存，则最低位置位，并且该指针指向anon_vma对象
						 * memory, low bit is set, and
						 * it points to anon_vma object:
						 * see PAGE_MAPPING_ANON below.参见下文的PAGE_MAPPING_ANON
						 */
	    };
#if NR_CPUS >= CONFIG_SPLIT_PTLOCK_CPUS
	    spinlock_t ptl;
#endif
	    struct kmem_cache *slab;	/* SLUB: Pointer to slab:指向slab的指针 */
	    struct page *first_page;	/* Compound tail pages:用于复合页的尾页，指向首页 */
	};
	union {
		pgoff_t index;		/* Our offset within mapping：在影射区的偏移量. */
		void *freelist;		/* SLUB: freelist req. slab lock */
	};
	//lru是一个表头，用于在各种链表中维护该页，以便将页按不同的类别分组，最重要的类别是活动与不活动
	struct list_head lru;		/* Pageout list, eg. active_list：换出页列表，例如由zone->lru_lock保护的active_list
					 * protected by zone->lru_lock !
					 */
	/*
	 * On machines where all RAM is mapped into kernel address space,
	 * we can simply calculate the virtual address. On machines with
	 * highmem some memory is mapped into kernel virtual memory
	 * dynamically, so we need a place to store that address.
	 * Note that this field could be 16 bits on x86 ... ;)
	 *
	 * Architectures with slow multiplication can define
	 * WANT_PAGE_VIRTUAL in asm/page.h
	 */
#if defined(WANT_PAGE_VIRTUAL)
	//virtual用于高端内存区域中的页，即无法直接映射到内核内存中的页，用于储存该页的虚拟地址
	void *virtual;			/* Kernel virtual address (NULL if
					   not kmapped, ie. highmem):内核虚拟地址，（若没有影射为NULL，即高端内存） */
#endif /* WANT_PAGE_VIRTUAL */
#ifdef CONFIG_CGROUP_MEM_RES_CTLR
	unsigned long page_cgroup;
#endif
};

/*
 * This struct defines a memory VMM memory area. There is one of these
 * per VM-area/task.  A VM area is any part of the process virtual memory
 * space that has a special rule for the page-fault handlers (ie a shared
 * library, the executable area etc).
 */
struct vm_area_struct {
	struct mm_struct * vm_mm;//反向指针，指向该区域所属的mm_struct实例	/* The address space we belong to. */
	unsigned long vm_start;//起始地址		/* Our start address within vm_mm. */
	unsigned long vm_end;//结束地址		/* The first byte after our end address
					   within vm_mm. */

	/* linked list of VM areas per task, sorted by address */
	struct vm_area_struct *vm_next;//所有实例的链表是通过这个成员实现的

	pgprot_t vm_page_prot;//该区域的访问权限，与内存页的访问权限相同		/* Access permissions of this VMA. */
	unsigned long vm_flags;//描述该区域的标志		/* Flags, listed below. */

	struct rb_node vm_rb;

	/*
	 * For areas with an address space and backing store,
	 * linkage into the address_space->i_mmap prio tree, or
	 * linkage to the list of like vmas hanging off its node, or
	 * linkage of vma in the address_space->i_mmap_nonlinear list.
	 */
	union {
		struct {
			struct list_head list;
			void *parent;	/* aligns with prio_tree_node parent */
			struct vm_area_struct *head;
		} vm_set;

		struct raw_prio_tree_node prio_tree_node;
	} shared;//链表或优先树

	/*
	 * A file's MAP_PRIVATE vma can be in both i_mmap tree and anon_vma
	 * list, after a COW of one of the file pages.	A MAP_SHARED vma
	 * can only be in the i_mmap tree.  An anonymous MAP_PRIVATE, stack
	 * or brk vma (with NULL file) can only be in an anon_vma list.
	 */
    /*用于管理匿名映射的共享页，指向相同页的映射都保存在一个双链表上*/
	struct list_head anon_vma_node;	/* Serialized by anon_vma->lock */
	struct anon_vma *anon_vma;	/* Serialized by page_table_lock */

	/* Function pointers to deal with this struct. */
	struct vm_operations_struct * vm_ops;//指向一堆方法的集合，这些方法用于在区域上执行各种标准操作

	/* Information about our backing store: */
	unsigned long vm_pgoff;		/* Offset (within vm_file) in PAGE_SIZE
					   units, *not* PAGE_CACHE_SIZE */
	struct file * vm_file;//指向file实例，描述了一个被映射的文件		/* File we map to (can be NULL). */
	void * vm_private_data;//私有数据，很少用		/* was vm_pte (shared mem) */
	unsigned long vm_truncate_count;/* truncate_count or restart_addr */

#ifndef CONFIG_MMU
	atomic_t vm_usage;		/* refcount (VMAs shared if !MMU) */
#endif
#ifdef CONFIG_NUMA
	struct mempolicy *vm_policy;	/* NUMA policy for the VMA */
#endif
};

struct mm_struct {
    //每个内存区域都是vm_area_struct的实例
	struct vm_area_struct * mmap;//以链表的形式排序虚拟内存区域列表		/* list of VMAs */
	struct rb_root mm_rb;//以红黑树的形式排序虚拟内存区域
	struct vm_area_struct * mmap_cache;	/* last find_vma result */
	unsigned long (*get_unmapped_area) (struct file *filp,
				unsigned long addr, unsigned long len,
				unsigned long pgoff, unsigned long flags);//这个方法在mmap区域中为新映射找到适当的位置
	void (*unmap_area) (struct mm_struct *mm, unsigned long addr);
	unsigned long mmap_base;//内存映射区域的基地址    /* base of mmap area */
	unsigned long task_size;//储存了对应进程的地址空间长度		/* size of task vm space */
	unsigned long cached_hole_size; 	/* if non-zero, the largest hole below free_area_cache */
	unsigned long free_area_cache;		/* first hole of size cached_hole_size or larger */
	pgd_t * pgd;
	atomic_t mm_users;			/* How many users with user space? */
	atomic_t mm_count;			/* How many references to "struct mm_struct" (users count as 1) */
	int map_count;				/* number of VMAs */
	int core_waiters;
	struct rw_semaphore mmap_sem;
	spinlock_t page_table_lock;		/* Protects page tables and some counters */

	struct list_head mmlist;		/* List of maybe swapped mm's.	These are globally strung
						 * together off init_mm.mmlist, and are protected
						 * by mmlist_lock
						 */

	/* Special counters, in some configurations protected by the
	 * page_table_lock, in other configurations by being atomic.
	 */
	mm_counter_t _file_rss;
	mm_counter_t _anon_rss;

	unsigned long hiwater_rss;	/* High-watermark of RSS usage */
	unsigned long hiwater_vm;	/* High-water virtual memory usage */

	unsigned long total_vm, locked_vm, shared_vm, exec_vm;
	unsigned long stack_vm, reserved_vm, def_flags, nr_ptes;
    /*各个段的起始地址与结束地址*/
	unsigned long start_code, end_code, start_data, end_data;//可执行代码占用的虚拟地址空间地址，开始和结束标记
	unsigned long start_brk, brk, start_stack;//堆的其实地址，结束地址brk，brk会变，栈的起始地址
	unsigned long arg_start, arg_end, env_start, env_end;//参数列表和环境列表都位于栈中最高的区域

	unsigned long saved_auxv[AT_VECTOR_SIZE]; /* for /proc/PID/auxv */

	cpumask_t cpu_vm_mask;

	/* Architecture-specific MM context */
	mm_context_t context;

	/* Swap token stuff */
	/*
	 * Last value of global fault stamp as seen by this process.
	 * In other words, this value gives an indication of how long
	 * it has been since this task got the token.
	 * Look at mm/thrash.c
	 */
	unsigned int faultstamp;
	unsigned int token_priority;
	unsigned int last_interval;

	unsigned long flags; /* Must use atomic bitops to access the bits */

	/* coredumping support */
	struct completion *core_startup_done, core_done;

	/* aio bits */
	rwlock_t		ioctx_list_lock;	/* aio lock */
	struct kioctx		*ioctx_list;
#ifdef CONFIG_MM_OWNER
	/*
	 * "owner" points to a task that is regarded as the canonical
	 * user/owner of this mm. All of the following must be true in
	 * order for it to be changed:
	 *
	 * current == mm->owner
	 * current->mm != mm
	 * new_owner->mm == mm
	 * new_owner->alloc_lock is held
	 */
	struct task_struct *owner;
#endif

#ifdef CONFIG_PROC_FS
	/* store ref to file /proc/<pid>/exe symlink points to */
	struct file *exe_file;
	unsigned long num_exe_file_vmas;
#endif
};

#endif /* _LINUX_MM_TYPES_H */
