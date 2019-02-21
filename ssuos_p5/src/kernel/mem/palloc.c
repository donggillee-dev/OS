#include <mem/palloc.h>
#include <bitmap.h>
#include <type.h>
#include <round.h>
#include <mem/mm.h>
#include <synch.h>
#include <device/console.h>
#include <mem/paging.h>
#include <proc/proc.h>

/* Page allocator.  Hands out memory in page-size (or
   page-multiple) chunks.  
   */

/* page struct */
struct kpage{
	uint32_t type;
	uint32_t *vaddr; //해당 페이지의 가상 시작주소값을 가짐
	uint32_t nalloc; //할당 받을 페이지 개수
	pid_t pid;
};


static struct kpage *kpage_list;
//static struct kpage *kpage_list_addr;
static uint32_t page_alloc_index;

/* Initializes the page allocator. */
void init_palloc (void) 
{
	/* Calculate the space needed for the kpage list */
	size_t pool_size = sizeof(struct kpage) * PAGE_POOL_SIZE;

	/* kpage list alloc */
	kpage_list = (struct kpage *)(KERNEL_ADDR);

	/* initialize */
	memset((void*)kpage_list, 0, pool_size);
	page_alloc_index = 0;
	//kpage_list_addr = kpage_list;
}

/* Obtains and returns a group of PAGE_CNT contiguous free pages.
   */
	uint32_t *
palloc_get_multiple (uint32_t page_type, size_t page_cnt)
{
	void *pages = NULL;
	struct kpage *kpage = kpage_list;
	size_t page_idx;
	int i,j = 0;

	if (page_cnt == 0)
		return NULL;

	switch(page_type){
		case HEAP__: //(1)
			for(i = 0; i < 1024; i++) {
				if(kpage->vaddr == NULL) 
					break;
				if(kpage->nalloc == page_cnt && kpage->type == FREE__) {
					pages = (void *)((uint32_t)VKERNEL_HEAP_START - (j+page_cnt)*PAGE_SIZE);
					kpage->type = HEAP__;
					kpage->vaddr = pages;				
					break;
				}
				if(kpage->type == HEAP__ || kpage->type == FREE__)
					j += kpage->nalloc;
				kpage += sizeof(struct kpage);
			}
			if(pages == NULL) {
				pages = (void *)((uint32_t)VKERNEL_HEAP_START - (page_alloc_index+page_cnt) * PAGE_SIZE);
				page_alloc_index += page_cnt;
				kpage->nalloc = page_cnt;
				kpage->vaddr = pages;
				kpage->type = HEAP__;
			}
			memset(pages, 0, PAGE_SIZE*page_cnt);
			break;
		case STACK__:
			for(i = 0; i < 1024; i++) {
				if(kpage->vaddr == NULL) {
					break;
				}
				if(kpage->nalloc == page_cnt && kpage->type == FREE__) {
					pages = (void *)(VKERNEL_STACK_ADDR);
					kpage->pid = cur_process->pid;
					kpage->type = STACK__;
					kpage->vaddr = pages;
					break;
				}
				kpage += sizeof(struct kpage);
			}
			if(pages == NULL) {
				pages = (void *)(VKERNEL_STACK_ADDR);
				page_alloc_index += page_cnt;
				kpage->nalloc = page_cnt;
				kpage->vaddr = pages;
				kpage->type = STACK__;
				kpage->pid = cur_process->pid;
			}
			memset(pages-page_cnt*PAGE_SIZE, 0, PAGE_SIZE*page_cnt);
			break;
		default:
			return NULL;
	}
	return (uint32_t*)pages; 
}

/* Obtains a single free page and returns its address.
   */
	uint32_t *
palloc_get_page (uint32_t page_type) 
{
	return palloc_get_multiple (page_type, 1);
}

/* Frees the PAGE_CNT pages starting at PAGES. */
	void
palloc_free_multiple (void *pages, size_t page_cnt) 
{

	struct kpage *kpage = kpage_list;

	if(pages == NULL || page_cnt == 0)
		return;

	while(1) {
		if(kpage->vaddr == pages && kpage->nalloc == page_cnt) {
			kpage->type = FREE__;
			return;
		}
		kpage += sizeof(struct kpage);
	}
}

/* Frees the page at PAGE. */
	void
palloc_free_page (void *page) 
{
	palloc_free_multiple (page, 1);
}


	uint32_t *
va_to_ra (uint32_t *va){
	struct kpage* kpage_pt = kpage_list;
	int i;
	int page_idx = 0;

	if((va != (uint32_t)VKERNEL_STACK_ADDR - PAGE_SIZE) && (va != (uint32_t)VKERNEL_STACK_ADDR - PAGE_SIZE*2)) {
		struct kpage* kpage_pt = kpage_list;
		int i;
		int page_idx = 0;
		for(i = 0; kpage_pt->type != 0; i++) {
			if(kpage_pt->type == 0)
				break;
				if(kpage_pt->vaddr == va) {
					return (uint32_t *)((uint32_t)RKERNEL_HEAP_START + page_idx*PAGE_SIZE);
				}
				page_idx += kpage_pt->nalloc;
			kpage_pt += sizeof(struct kpage);
		}
	}
	else if((va == (uint32_t)VKERNEL_STACK_ADDR - PAGE_SIZE) || (va == (uint32_t)VKERNEL_STACK_ADDR-PAGE_SIZE*2)){
		struct kpage *kpage_pt = kpage_list;
		int i;
		int page_idx = 0;
		for(i = 0; i < 1024; i++) {
			if(kpage_pt->type == 0) {
				break;
			}
			if(va == kpage_pt->vaddr && cur_process->pid == kpage_pt->pid) {
				return (uint32_t *)((uint32_t)RKERNEL_HEAP_START + page_idx * PAGE_SIZE);
			}
			page_idx += kpage_pt->nalloc;
			kpage_pt += sizeof(struct kpage);
		}
	}
}

	uint32_t *
ra_to_va (uint32_t *ra){
	struct kpage *kpage = kpage_list;
	int index = ((uint32_t)ra - RKERNEL_HEAP_START)/PAGE_SIZE;
	int i, cnt = 0;
	
	if(ra == RKERNEL_HEAP_START)
		return (uint32_t *)kpage->vaddr;
	for(i = 0; i < 1024; i++) {
		if(kpage->type == 0)
			break;
		cnt += kpage->nalloc;
		if((cnt-index) == 1) {
			return (uint32_t *)((uint32_t)kpage->vaddr - PAGE_SIZE);
		}
		else if(cnt == index) {
			kpage += sizeof(struct kpage);
			if(kpage->type == STACK__) {
				return (uint32_t *)((uint32_t)VKERNEL_STACK_ADDR - kpage->nalloc*PAGE_SIZE);
			}
			else if(kpage->type == HEAP__) {
				return (uint32_t *)kpage->vaddr;
			}
		}
		kpage += sizeof(struct kpage);
	}
}

void palloc_pf_test(void)
{
	uint32_t *one_page1 = palloc_get_page(HEAP__);
	uint32_t *one_page2 = palloc_get_page(HEAP__);
	uint32_t *two_page1 = palloc_get_multiple(HEAP__,2);
	uint32_t *three_page;
	printk("one_page1 = %x\n", one_page1); 
	printk("one_page2 = %x\n", one_page2); 
	printk("two_page1 = %x\n", two_page1);

	printk("=----------------------------------=\n");
	palloc_free_page(one_page1);
	palloc_free_page(one_page2);
	palloc_free_multiple(two_page1,2);

	one_page1 = palloc_get_page(HEAP__);
	two_page1 = palloc_get_multiple(HEAP__,2);
	one_page2 = palloc_get_page(HEAP__);

	printk("one_page1 = %x\n", one_page1);
	printk("one_page2 = %x\n", one_page2);
	printk("two_page1 = %x\n", two_page1);

	printk("=----------------------------------=\n");
	three_page = palloc_get_multiple(HEAP__,3);

	printk("three_page = %x\n", three_page);
	palloc_free_page(one_page1);
	palloc_free_page(one_page2);
	palloc_free_multiple(two_page1,2);
	palloc_free_multiple(three_page, 3);
}
