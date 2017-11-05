#ifndef _LINUX_VMALLOC_H
#define _LINUX_VMALLOC_H

#include <linux/spinlock.h>
#include <asm/page.h>		/* pgprot_t */

struct vm_area_struct;

/* bits in vm_struct->flags */
#define VM_IOREMAP	0x00000001	/* ioremap() and friends */
#define VM_ALLOC	0x00000002	/* vmalloc() */
#define VM_MAP		0x00000004	/* vmap()ed pages */
#define VM_USERMAP	0x00000008	/* suitable for remap_vmalloc_range */
#define VM_VPAGES	0x00000010	/* buffer for pages was vmalloc'ed */
/* bits [20..32] reserved for arch specific ioremap internals */

/*
 * Maximum alignment for ioremap() regions.
 * Can be overriden by arch-specific value.
 */
#ifndef IOREMAP_MAX_ORDER
#define IOREMAP_MAX_ORDER	(7 + PAGE_SHIFT)	/* 128 pages */
#endif
//每个用vmalloc分配的子区域，都对应于内存中一个该结构的实例
struct vm_struct {
	/* keep next,addr,size together to speedup lookups */
	struct vm_struct	*next;//next使得内核将vmalloc区域中所有的子区域保存在一个单链表中
	void			*addr;//子区域在虚拟地址空间的起始地址
	unsigned long		size;//表示子区域的长度
	unsigned long		flags;//储存了有关该内存区的标志集合，可选值如下
                            //VM_ALLOC指定由vmalloc产生的子区域
                            //VM_MAP用于表示将现存的pages集合映射到连续的虚拟地址空间中
                            //VM_IOREMAP表示将随机的物理内存区映射到vmalloc区域中
	struct page		**pages;//指针数组，是指向page指针的数组
	unsigned int		nr_pages;//指定pages中数组项的数目，即涉及的内存页数目
	unsigned long		phys_addr;//在使用了VM_IOREMAP标志时才需要
	void			*caller;
};

/*
 *	Highlevel APIs for driver use
 *///vmalloc用于分配在虚拟内存中连续但在物理内存中不一定连续的内存，size表示所需内存区的长度,其长度单位是字节
extern void *vmalloc(unsigned long size);
extern void *vmalloc_user(unsigned long size);
extern void *vmalloc_node(unsigned long size, int node);
extern void *vmalloc_exec(unsigned long size);
extern void *vmalloc_32(unsigned long size);
extern void *vmalloc_32_user(unsigned long size);
extern void *__vmalloc(unsigned long size, gfp_t gfp_mask, pgprot_t prot);
extern void *__vmalloc_area(struct vm_struct *area, gfp_t gfp_mask,
				pgprot_t prot);
extern void vfree(const void *addr);

extern void *vmap(struct page **pages, unsigned int count,
			unsigned long flags, pgprot_t prot);
extern void vunmap(const void *addr);

extern int remap_vmalloc_range(struct vm_area_struct *vma, void *addr,
							unsigned long pgoff);
void vmalloc_sync_all(void);
 
/*
 *	Lowlevel-APIs (not for driver use!)
 */

static inline size_t get_vm_area_size(const struct vm_struct *area)
{
	/* return actual size without guard page */
	return area->size - PAGE_SIZE;
}

extern struct vm_struct *get_vm_area(unsigned long size, unsigned long flags);//此函数是__get_vm_area的前端，负责参数准备工作
extern struct vm_struct *get_vm_area_caller(unsigned long size,
					unsigned long flags, void *caller);
extern struct vm_struct *__get_vm_area(unsigned long size, unsigned long flags,
					unsigned long start, unsigned long end);//此函数是下面函数的前端，负责参数准备工作
extern struct vm_struct *get_vm_area_node(unsigned long size,
					  unsigned long flags, int node,
					  gfp_t gfp_mask);//根据子区域的长度信息，该函数试图在虚拟的vmalloc空间中找到一个适当的位置
extern struct vm_struct *remove_vm_area(const void *addr);//将一个现存的子区域从vmalloc地址空间删除，addr是待删除子区域的虚拟起始地址

extern int map_vm_area(struct vm_struct *area, pgprot_t prot,
			struct page ***pages);
extern void unmap_kernel_range(unsigned long addr, unsigned long size);

/* Allocate/destroy a 'vmalloc' VM area. */
extern struct vm_struct *alloc_vm_area(size_t size);
extern void free_vm_area(struct vm_struct *area);

/*
 *	Internals.  Dont't use..
 */
extern rwlock_t vmlist_lock;
extern struct vm_struct *vmlist;//vm_area实例组成的一个链表，管理着vmllooc区域中已经建立的各个子区域，此为表头

extern const struct seq_operations vmalloc_op;

#endif /* _LINUX_VMALLOC_H */
