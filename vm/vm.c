/* vm.c: Generic interface for virtual memory objects. */

#include "threads/malloc.h"
#include "vm/vm.h"
#include "vm/inspect.h"
#include "lib/kernel/hash.h"
/* 준코(05/13) */
#include "include/threads/thread.h"
#include "userprog/process.h"
#include "threads/vaddr.h"
#include "threads/mmu.h"


/* memory management */
struct list frame_table; // frame entry의 리스트로 구성된 frame table
// 비어있는 프레임들이 연결돼있는 연결리스트
// 빈 프레임이 필요할시에 그냥 앞에서 꺼내오면 됨
/* 준코(05/13) 함수 선언 */
unsigned page_hash(const struct hash_elem *p_, void *aux UNUSED);
bool page_less(const struct hash_elem *a, const struct hash_elem *b, void *aux);
bool page_insert(struct hash *h, struct page *p);
bool page_delete(struct hash *h, struct page *p);

struct list frame_table;
static struct list_elem *start = NULL;
/* 끝 */

/* Initializes the virtual memory subsystem by invoking each subsystem's
 * intialize codes. */
/* 준코 */
void vm_init(void)
{
	vm_anon_init();
	vm_file_init();
#ifdef EFILESYS /* For project 4 */
	pagecache_init();
#endif
	register_inspect_intr();
	/* DO NOT MODIFY UPPER LINES. */
	/* TODO: Your code goes here. */
	// list_init(&frame_table); //수정
	// start = list_begin(&frame_table);
}

/* Get the type of the page. This function is useful if you want to know the
 * type of the page after it will be initialized.
 * This function is fully implemented now. */
enum vm_type
page_get_type(struct page *page)
{
	int ty = VM_TYPE(page->operations->type);
	switch (ty)
	{
	case VM_UNINIT:
		return VM_TYPE(page->uninit.type);
	default:
		return ty;
	}
}

/* Helpers */
static struct frame *vm_get_victim(void);
static bool vm_do_claim_page(struct page *page);
static struct frame *vm_evict_frame(void);

/* Create the pending page object with initializer. If you want to create a
 * page, do not create it directly and make it through this function or
 * `vm_alloc_page`. */
/* 준코(05/13) */
bool vm_alloc_page_with_initializer(enum vm_type type, void *upage, bool writable,
									vm_initializer *init, void *aux)
{
	ASSERT(VM_TYPE(type) != VM_UNINIT)

	struct supplemental_page_table *spt = &thread_current()->spt;

	/* Check wheter the upage is already occupied or not. */
	if (spt_find_page(spt, upage) == NULL)
	{
		/* TODO: Create the page, fetch the initialier according to the VM type,
		 * TODO: and then create "uninit" page struct by calling uninit_new. You
		 * TODO: should modify the field after calling the uninit_new. */

		/* TODO: Insert the page into the spt. */
		/* 준코(05/13) */
		struct page *page = (struct page *)malloc(sizeof(struct page));

		typedef bool (*page_initializer)(struct page *, enum vm_type, void *);
		page_initializer initializer = NULL;

		switch(VM_TYPE(type))
		{
			case VM_ANON:
				initializer = anon_initializer;
				break;
			case VM_FILE:
				initializer = file_backed_initializer;
				break;
		}

		uninit_new(page, upage, init, type, aux, initializer);
		page->writable = writable;

		if (type == VM_STACK)
		page->stack = true;

		return spt_insert_page(spt, page);
	}
err:
	return false;
}


/* 준코 : UNUSED 지움,  */
struct page *
spt_find_page(struct supplemental_page_table *spt UNUSED, void *va UNUSED)
{	
	struct page *page = (struct page *)malloc(sizeof(struct page));
	struct hash_elem *hash_elem;

	page->va = pg_round_down(va);

	hash_elem = hash_find(&spt->page_table, &page->hash_elem);

	if (hash_elem == NULL){
		return NULL;
	}
	else{
		return hash_entry(hash_elem, struct page, hash_elem);
	}
}
/* 준코(05/20) */
// struct page *
// page_lookup(const void *address)
// {
// 	struct page *page = (struct page*)malloc(sizeof(struct page));
// 	struct hash_elem *e;

// 	page->va = pg_round_down(address);
	
// 	e = hash_find(&thread_current()->spt.page_table, &page->hash_elem);
// 	free(page);
	
// 	return e != NULL ? hash_entry(e, struct page, hash_elem) : NULL;	

// }

/* 준코(05/18) */

bool spt_insert_page(struct supplemental_page_table *spt UNUSED,
					 struct page *page UNUSED)
{		
		int succ = false;

		struct hash_elem *check_page_elem = hash_find(&spt->page_table, &page->hash_elem);

		if(check_page_elem != NULL)
		return succ;
		
		else{
			hash_insert(&spt->page_table, &page->hash_elem);
			succ = true;
			return succ;
		}
}



void spt_remove_page(struct supplemental_page_table *spt, struct page *page)
{
	vm_dealloc_page(page);
}

/* Get the struct frame, that will be evicted. */
static struct frame *
vm_get_victim(void)
{
	struct frame *victim = NULL;
	/* TODO: The policy for eviction is up to you. */
	return victim;
}

/* Evict one page and return the corresponding frame.
 * Return NULL on error.*/
static struct frame *
vm_evict_frame(void)
{
	struct frame *victim = vm_get_victim();
	/* TODO: swap out the victim and return the evicted frame. */
	return NULL;
}

/* palloc() and get frame. If there is no available page, evict the page
 * and return it. This always return valid address. That is, if the user pool
 * memory is full, this function evicts the frame to get the available memory
 * space.
 * palloc_get_page로 유저 풀에서 새 물리 페이지를 얻기
 * 유저풀에서 성공적으로 페이지를 얻으면, 프레임을 할당하고, 멤버를 초기하하고 리턴하기
 * 구현후, 모든 user space page는 이 함수를 통해 할당해야 함
 * page allocation fail은 일단 PANIC("todo")로 두기
 * */

/* 준코 */
static struct frame *vm_get_frame(void)
{
	struct frame *frame = (struct frame *)malloc(sizeof(struct frame));



	frame->kva = palloc_get_page(PAL_USER);

	if (frame->kva == NULL)
	{
		PANIC("todo");
		// free(frame);
		// frame = vm_evict_frame();
		/* swap out if page allocation fails - 나중에 구현*/
	}
	list_push_back(&frame_table, &frame->frame_elem);
	start = &frame->frame_elem;
	frame->page = NULL;
	ASSERT(frame != NULL);
	ASSERT(frame->page == NULL);	
	
	return frame;
}

/* Growing the stack. */
static void
vm_stack_growth(void *addr UNUSED)
{
	if (vm_alloc_page(VM_STACK, addr, 1)){
		vm_claim_page(addr);
		thread_current()->stack_bottom = PGSIZE;
	}
}

/* Handle the fault on write_protected page */
static bool
vm_handle_wp(struct page *page UNUSED)
{
}

/* Return true on success */
/* 준코(05/13) */
bool vm_try_handle_fault(struct intr_frame *f UNUSED, void *addr UNUSED,
						 bool user UNUSED, bool write UNUSED, bool not_present UNUSED)
{
	struct supplemental_page_table *spt = &thread_current()->spt;
	struct page *page = NULL;
	/* TODO: Validate the fault */
	/* TODO: Your code goes here */

	if(is_kernel_vaddr(addr) || addr == NULL)
		return false;

	if (f->rsp - 8 <= addr && addr <= USER_STACK && USER_STACK - 0x100000 <= addr){
		vm_stack_growth(thread_current()->stack_bottom - PGSIZE);
		return true;
	}

	/* 준코(05/15) */
	
	if (not_present){
		page = spt_find_page(spt, addr);

		if (page == NULL) {
			return false;
			}
			
		}
		if(vm_do_claim_page(page) == false)
			return false;
	if (write && !page->writable)
		return false;

	return true;

}
	// struct page* page = spt_find_page(spt, addr);

	// if(page == NULL)
	// 	return false;

	// return vm_do_claim_page(page);


/* Free the page.
 * DO NOT MODIFY THIS FUNCTION. */
void vm_dealloc_page(struct page *page)
{
	destroy(page);
	free(page);
}

/* Claim the page that allocate on VA. */
/* 준코(05/12) */
bool vm_claim_page(void *va UNUSED)
{
	struct page *page = NULL;
	ASSERT(is_user_vaddr(va));

	struct thread *curr = thread_current();
	page = spt_find_page(&thread_current()->spt, va);

	if (page == NULL)
	{
		return false;
	}
	return vm_do_claim_page(page);
	
}

/* Claim the PAGE and set up the mmu.
	피지컬 프레임을 요구.
	1. vm_get_frame으로 빈프레임을 얻는다
	2. MMU를 세팅해서 Virtual address와 physical address의 매핑을 추가함
	3. 반환값은 함수 동작이 성공적이었는가, 아닌가를 알려줘야 함
 */

/* 준코(05/12) */
static bool
vm_do_claim_page(struct page *page)
{
	ASSERT(page != NULL);
	struct thread *current = thread_current();
	struct frame *frame = vm_get_frame(); 
	ASSERT(frame != NULL);

	/* Set links */
	frame->page = page;
	page->frame = frame;

	if(pml4_get_page(current->pml4, page->va) == NULL && pml4_set_page(current->pml4, page->va, frame->kva, page->writable)){
		if(page->stack == true)
		return true;
	else
		return swap_in(page, frame->kva);
	}
	else
		return false;


}

	/* TODO: Insert page table entry to map page's VA to frame's PA.
		user virtual주소와 kernel virtual주소의 매핑
		두번째인자인 kpage(여기서는 frame->kva)는 palloc_get_page로 유저풀에서 얻어온 것이어야 함
		실제로 그러함(vm_get_frame을 봐라!)
		본 페이지 테이블인 pml4에 upage와 kpage(즉 물리주소)의 연결을 수행하는 함수가 instal_page
	*/
	// if (install_page(page->va, frame->kva, page->writable)) {
	// 	return swap_in (page, frame->kva); // 이건 뭐고
	// }

	// if (clock_elem != NULL){
	// 	list_insert(clock_elem, &frame->elem);
	// }
	// else{
	// 	list_push_back(&frame_list, &frame.elem);
	// }




/* Initialize new supplemental page table -> 구현 필요
	userprog/process.c의 initd(프로세스가 시작할때)와 __do_fork(프로세스가 포크될때) 호출됨
*/
/* 준코(05/12) : UNUSED 지움 */
/* page_table 정의, spt의 page_table 변경 */
void supplemental_page_table_init(struct supplemental_page_table *spt UNUSED)
{
	hash_init(&spt->page_table, page_hash, page_less, NULL);
}

/* Copy supplemental page table from src to dst */
/* 준코 */
bool supplemental_page_table_copy(struct supplemental_page_table *dst UNUSED,
								  struct supplemental_page_table *src UNUSED)
{
	struct hash_iterator iter;
	hash_first(&iter, &src->page_table);

	while(hash_next(&iter)){
		struct page *parent_page = hash_entry(hash_cur(&iter), struct page, hash_elem);
		enum vm_type parent_type = page_get_type(parent_page);
		void* upage = parent_page->va;
		bool writable = parent_page->writable;
		vm_initializer *init = parent_page->uninit.init;
		void* aux = parent_page->uninit.aux;

		if(parent_page->operations->type == VM_UNINIT){
			if(!vm_alloc_page_with_initializer(parent_type,upage,writable, init, aux))
			{
				return false;
			}
		}
		else{
			if(!vm_alloc_page(parent_type, upage, writable)){
				return false;
			}

			if(!vm_claim_page(upage))
			{
				return false;
			}

			struct page *child_page = spt_find_page(dst, upage);
			memcpy(child_page->frame->kva, parent_page->frame->kva, PGSIZE);
			}
		}

		return true;
	}


/* Free the resource hold by the supplemental page table */
/* 준코(05/13) */
void supplemental_page_table_kill(struct supplemental_page_table *spt UNUSED)
{
	struct hash_iterator iter;
	hash_first(&iter, &spt->page_table);
	while(hash_next(&iter)){
		struct page *target = hash_entry(hash_cur(&iter), struct page, hash_elem);
		if(target->operations->type == VM_FILE){
			do_munmap(target->va);
		}
	}
	hash_destroy(&spt->page_table, hash_destructor);
}

/* -------- helper function ------------ */

/* 페이지의 가상 주소 값을 해싱을 해주는 함수 */
unsigned page_hash(const struct hash_elem *p, void *aux UNUSED)
{
	const struct page *page = hash_entry(p, struct page, hash_elem);

	// hash_bytes 함수로 해시값 얻어냄
	/* 준코(05/13) */
	return hash_bytes(&page->va, sizeof(page->va));
}

/* 해시 테이블 내의 두 페이지 요소에 대해 페이지의 주소값을 비교
	a가 b보다 작으면 true, a가 b보다 같거나 크면 false
*/
bool page_less(const struct hash_elem *a, const struct hash_elem *b, void *aux)
{
	const struct page *page_a = hash_entry(a, struct page, hash_elem);
	const struct page *page_b = hash_entry(b, struct page, hash_elem);

	return page_a->va < page_b->va;

	/* 준코 */
	// if (page_a->va < page_b->va)
	// {
	// 	return true;
	// }
	// else
	// {
	// 	return false;
	// }
}

/* p에 들어있는 hash_elem 구조체를 인자로 받은 해시테이블에 삽입 */
bool page_insert(struct hash *h, struct page *p)
{
	if (!hash_insert(h, &p->hash_elem))
	{
		return true; // hash_insert는 성공하면 null pointer 반환
	}
	else
	{
		return false; // 이미 해당 element 있으면 걔를 반환
	}
}

/* p에 들어있는 hash_elem 구조체를 인자로 받은 해시테이블에서 삭제 */
bool page_delete(struct hash *h, struct page *p)
{
	if (hash_delete(h, &p->hash_elem))
	{
		return true; // 지우려고 하는 element가 있으면 지우고, 이것을 반환
	}
	else
	{
		return false; // 지우려고 하는 element가 테이블에 없으면 null pointer 반환
	}
}
