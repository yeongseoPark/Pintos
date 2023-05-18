/* vm.c: Generic interface for virtual memory objects. */

#include "threads/malloc.h"
#include "vm/vm.h"
#include "vm/inspect.h"

#include "include/threads/vaddr.h"


/*************************** P3 추가 **************************/
unsigned page_hash(const struct hash_elem *p_, void *aux UNUSED);
bool page_less(const struct hash_elem *a, const struct hash_elem *b, void *aux);
unsigned page_hash(const struct hash_elem *p_, void *aux UNUSED);
bool page_less(const struct hash_elem *a, const struct hash_elem *b, void *aux);
bool page_insert(struct hash *h, struct page *p);
bool page_delete(struct hash *h, struct page *p);

struct list frame_table; /* frame entry 연결리스트로 구성 */
static struct list_elem *start = NULL;	/* frame_table을 순회하기 위한 첫 번째 원소*/


/* Initializes the virtual memory subsystem by invoking each subsystem's
 * intialize codes. */
void
vm_init (void) {
	vm_anon_init ();
	vm_file_init ();
#ifdef EFILESYS  /* For project 4 */
	pagecache_init ();
#endif
	register_inspect_intr ();
	/* DO NOT MODIFY UPPER LINES. */
	/* TODO: Your code goes here. */
	list_init(&frame_table);		// frame table 리스트로 묶어서 관리
	start = list_begin(&frame_table);
}

/* Get the type of the page. This function is useful if you want to know the
 * type of the page after it will be initialized.
 * This function is fully implemented now. */
enum vm_type
page_get_type (struct page *page) {
	int ty = VM_TYPE (page->operations->type);
	switch (ty) {
		case VM_UNINIT:
			return VM_TYPE (page->uninit.type);
		default:
			return ty;
	}
}

/* Helpers */
static struct frame *vm_get_victim (void);
static bool vm_do_claim_page (struct page *page);
static struct frame *vm_evict_frame (void);

/* Create the pending page object with initializer. If you want to create a
 * page, do not create it directly and make it through this function or
 * `vm_alloc_page`. */
bool
vm_alloc_page_with_initializer (enum vm_type type, void *upage, bool writable,
		vm_initializer *init, void *aux) {
	
	ASSERT (VM_TYPE(type) != VM_UNINIT)

	struct supplemental_page_table *spt = &thread_current ()->spt;

	/* Check wheter the upage is already occupied or not. */
	if (spt_find_page (spt, upage) == NULL) { //
		/* TODO: Create the page, fetch the initialier according to the VM type,
		 * TODO: and then create "uninit" page struct by calling uninit_new. You
		 * TODO: should modify the field after calling the uninit_new. */

		/********************  P3 추가 ***********************/
		struct page *page = (struct page *)malloc(sizeof(struct page));

		if (type == VM_ANON || type == (VM_ANON | VM_MARKER_0)){
			uninit_new (page, upage, init, type, aux, anon_initializer);
		}
		else if (type == VM_FILE){
			uninit_new (page, upage, init, type, aux, file_backed_initializer);
		}

		page->writable = writable;

		/* TODO: Insert the page into the spt. */
		return spt_insert_page(spt, page);	
	}
err:
	return false;
}

/* Find VA from spt and return page. On error, return NULL. */
struct page *
spt_find_page (struct supplemental_page_table *spt UNUSED, void *va UNUSED) {
	// struct page *page = NULL;
	/* TODO: Fill this function. 
	 - spt에 va를 키로 전달해 해당 page 리턴.
	 - hash_find()는 hash_elem을 인자로 받아야 하므로 dummy page를 만들어 그것의 가상주소를 va로.
	 - va가 있으면 hash func로 index를 찾아갈 수 있으므로, hash_elem을 찾아 해당 페이지 리턴 가능
	*/
	struct page *page = (struct page *)malloc(sizeof(struct page));	// dummy page
	struct hash_elem *hash_elem;

	page->va = pg_round_down(va);	// 해당 va와 가장 가까운 페이지 시작 주소로 설정

	/* e와 같은 해시값을 갖는 page를 spt에서 찾아 hash_elem 리턴 */
	hash_elem = hash_find(&spt->spt_hash, &page->hash_elem);
	
	free(page);

	return hash_elem != NULL ? hash_entry(hash_elem, struct page, hash_elem): NULL;
}


/* Insert PAGE into spt with validation. */
bool
spt_insert_page (struct supplemental_page_table *spt UNUSED,
		struct page *page UNUSED) {
	// int succ = false;
	/* TODO: Fill this function. */

	return page_insert(&spt->spt_hash, page);
}

void
spt_remove_page (struct supplemental_page_table *spt, struct page *page) {
	vm_dealloc_page (page);
	return true;
}

/* Get the struct frame, that will be evicted. */
static struct frame *
vm_get_victim (void) {
	struct frame *victim = NULL;
	 /* TODO: The policy for eviction is up to you. */
	struct thread *curr = thread_current();
	struct list_elem *e;

	/* initialize start to first element of the frame_table */
	if (!start) {
		start = list_begin(&frame_table);
	}

	for (e = start; e != list_end(&frame_table); e = list_next(e)) {
		victim = list_entry(e, struct frame, frame_elem);
		if (pml4_is_accessed(curr->pml4, victim->page->va)) { // refer bit is 1 , 현제 스레드의 페이지내 victim page의 va의 레퍼 bit 체크.
			pml4_set_accessed(curr->pml4, victim->page->va, 0);
			victim = list_begin(&frame_table); // 양방향 설정?
			start = list_next(e);
		}	
		else 		// refer bit is 0 
			return victim;
	}
	
	/* If no victim is found from start to end, search from beginning to start */
	for (e = list_begin(&frame_table); e != start; e = list_next(e)) {
		victim = list_entry(e, struct frame, frame_elem);
		if (pml4_is_accessed(curr->pml4, victim->page->va)) {
			pml4_set_accessed(curr->pml4, victim->page->va, 0);
			victim = list_begin(&frame_table);
			start = list_next(e);
		}
		else 
			return victim;
	}
	return victim;			// for문을 다 돌아도 victim(레퍼 bit 0)이 안나오면 현재 리턴?
}


/* Evict one page and return the corresponding frame.
 * Return NULL on error.*/
static struct frame *
vm_evict_frame (void) {
	struct frame *victim UNUSED = vm_get_victim ();
	/* TODO: swap out the victim and return the evicted frame. */
	swap_out(victim->page);

	return NULL;
}

/* palloc() and get frame. If there is no available page, evict the page
 * and return it. This always return valid address. That is, if the user pool
 * memory is full, this function evicts the frame to get the available memory
 * space.*/
static struct frame *
vm_get_frame (void) {
	struct frame *frame = NULL;
	frame = (struct frame *)malloc(sizeof(struct frame));
	/* TODO: Fill this function. */

	frame->kva = palloc_get_page(PAL_USER); 	// RAM user pool -> (virtual) kernel VA로 1page 할당

	frame->page = NULL; 	// @@@

	ASSERT (frame != NULL);
	ASSERT (frame->page == NULL);

	if (frame->kva == NULL) {
		PANIC("todo");
		// frame = vm_evict_frame();		// RAM user pool이 없으면 frame에서 evict, 새로 할당
		// frame->page = NULL;
		// return frame;
	}
	// list_push_back(&frame_table, &frame->frame_elem);
	// frame->page = NULL;
	return frame;
}

/* Growing the stack. */
static void
vm_stack_growth (void *addr UNUSED) {
	/* Project 3 */
	void *stack_bottom = pg_round_down(addr);
	size_t req_stack_size = USER_STACK - (uintptr_t)stack_bottom;
	if ((req_stack_size) > (1 << 20)) PANIC("Stack limit exceeded!\n"); // 1MB stack size limit

	/* Allloc page from tested region to previous claimed stack page
	 * 스택에 해당하는 ANON 페이지를 UNINIT으로 만들고 SPT에 넣어준다.
	 * 이후 바로 claim 해서 물리 메모리와 매핑
	*/
	void *new_stack_bottom = stack_bottom;

	while ((uintptr_t) new_stack_bottom > USER_STACK_LIMIT) {

		if (vm_alloc_page(VM_ANON | VM_MARKER_0, new_stack_bottom, 1)) {
			vm_claim_page(new_stack_bottom);
			new_stack_bottom -= PGSIZE;
		}
	}
}

/* Handle the fault on write_protected page */
static bool
vm_handle_wp (struct page *page UNUSED) {
}

/* Return true on success */
bool
vm_try_handle_fault (struct intr_frame *f UNUSED, void *addr UNUSED,
		bool user UNUSED, bool write UNUSED, bool not_present UNUSED) {
	// struct thread *curr = thread_current();
	struct supplemental_page_table *spt UNUSED = &thread_current()->spt;
	struct page *page = NULL;

	/* TODO: Validate the fault */
	if (is_kernel_vaddr(addr) || addr == NULL)
		return false;

	// 유저 스택을 가리키는 경우: f->rsp에 있는 유저 스택포인터 가져오기
	// void *rsp_stack;
	// if (is_kernel_vaddr(f->rsp)) {
	// 	rsp_stack = &thread_current()->rsp_stack;
	// } else {
	// 	rsp_stack = f->rsp;
	// }

	/* TODO: Your code goes here */
		// 스택 증가로 page fault 해결 가능한지
		if (f->rsp - 8 <= addr && addr <= USER_STACK && USER_STACK - 0x100000 <= addr) {
			vm_stack_growth(thread_current()->stack_bottom - PGSIZE);
			return true;
	}

	// 현재 페이지가 없는 경우
	if (not_present) {
		// spt에서 주소에 맞는 페이지 가져오기
		page = spt_find_page(spt, addr);

		if (page == NULL)
			return false;

		if (vm_do_claim_page(page) == false)   // spt에서 찾은 page로 물리페이지 요청
			return false; 
	}
	return true;
}


/* Free the page.
 * DO NOT MODIFY THIS FUNCTION. */
void
vm_dealloc_page (struct page *page) {
	destroy (page);
	free (page);
}

/* Claim the page that allocate on VA. */
bool
vm_claim_page (void *va UNUSED) {
	struct page *page = NULL;
	/* TODO: Fill this function */
	page = spt_find_page(&thread_current()->spt, va);  // va를 주고, 해당 hash_elem가 속한 page 리턴
	if (page == NULL)
		return false;

	return vm_do_claim_page (page);
}

/* Claim the PAGE and set up the mmu. */
static bool
vm_do_claim_page (struct page *page) {
	struct frame *frame = vm_get_frame ();
	struct thread *t = thread_current ();

	ASSERT(frame != NULL);

	/* Set links */
	frame->page = page;
	page->frame = frame;

	/* TODO: Insert page table entry to map page's VA to frame's PA. */
	if (pml4_get_page (t->pml4, page->va) == NULL && pml4_set_page (t->pml4, page->va, frame->kva, page->writable)){
			return swap_in(page, frame->kva);
	}
	else		// install failed
		return false;
}

// install_page(page->va, frame->kva, page->writable)

/* Initialize new supplemental page table */
void
supplemental_page_table_init (struct supplemental_page_table *spt UNUSED) {
	/**** Project 3 ****/
	hash_init(&spt->spt_hash, page_hash, page_less, NULL);
}

/* Copy supplemental page table from src to dst */
bool
supplemental_page_table_copy (struct supplemental_page_table *dst UNUSED,
		struct supplemental_page_table *src UNUSED) {
	
	struct hash_iterator iter;	
	hash_first(&iter, &src->spt_hash);

	while (hash_next(&iter)) {
		// 복사할 hash_elem과 연결된 page를 찾아 해당 page의 구조체 가져오기
		struct page *parent_page = hash_entry(hash_cur(&iter), struct page, hash_elem);

		// Q. src spt에 언제 va가 들어갔지??
		if (parent_page->operations->type == VM_UNINIT) {		// 아직 한번도 접근되지 않은, UNINIT 상태의 parent_page를 fork하는 경우
			if (!vm_alloc_page_with_initializer(parent_page->uninit.type, parent_page->va, parent_page->writable,
											parent_page->uninit.init, parent_page->uninit.aux)) {
				return false;
			}
		}
		else {
			if (!vm_alloc_page_with_initializer(parent_page->uninit.type, parent_page->va, parent_page->writable,
												NULL, NULL)) {
				return false;
			}

			if(!vm_claim_page(parent_page->va))
				return false;

			// 부모 페이지를 복사할 자식 페이지 찾기
			struct page *child_page = spt_find_page(dst, parent_page->va);
			// 자식 페이지에 실제 복사
			memcpy(child_page->frame->kva, parent_page->frame->kva, PGSIZE);
		}
	}	
	return true;
}


void
hash_action_destroy (struct hash_elem *e, void *aux UNUSED) {
	// 삭제할 페이지 가져오기
	struct page *page = hash_entry (e, struct page, hash_elem);
	// error - 가져온 페이지가 NULL
	ASSERT (page != NULL);
	// destroy로 해당 페이지 제거
	
	destroy (page);
	// 사용한 페이지 메모리 반환
	free (page);
}

// #define destroy(page) \
// 	if ((page)->operations->destroy) (page)->operations->destroy (page)

/* Free the resource hold by the supplemental page table */
void
supplemental_page_table_kill (struct supplemental_page_table *spt UNUSED) {
	/* TODO: Destroy all the supplemental_page_table hold by thread and
	 * TODO: writeback all the modified contents to the storage. */
	struct hash_iterator iter;
	
	hash_first(&iter, &spt->spt_hash);
	while (hash_next(&iter)) {
		struct page *page = hash_entry(hash_cur(&iter), struct page, hash_elem);
		if (page->operations->type == VM_FILE) {
			do_munmap(page->va);
		}
	}

	hash_destroy(&spt->spt_hash, hash_action_destroy);

	free(spt->spt_hash.aux);
}


/* Initializes I for iterating hash table H.

   Iteration idiom:

   struct hash_iterator i;

   hash_first (&i, h);
   while (hash_next (&i))
   {
   struct foo *f = hash_entry (hash_cur (&i), struct foo, elem);
   ...do something with f...
   }

   Modifying hash table H during iteration, using any of the
   functions hash_clear(), hash_destroy(), hash_insert(),
   hash_replace(), or hash_delete(), invalidates all
   iterators. */

// /* A hash table iterator. */
// struct hash_iterator {
// 	struct hash *hash;          /* The hash table. */
// 	struct list *bucket;        /* Current bucket. */
// 	struct hash_elem *elem;     /* Current hash element in current bucket. */
// };




/*************************** P3 추가 **************************/
unsigned page_hash(const struct hash_elem *p_, void *aux UNUSED)
{
	/* given hash_elem, get 페이지 구조체 첫 주소. 
	 * #define hash_entry(HASH_ELEM, STRUCT, MEMBER)
	 * elem 주소에서 해당 오프셋 차감
	*/
	const struct page *p = hash_entry(p_, struct page, hash_elem);
	return hash_bytes(&p->va, sizeof(p->va)); 	// Returns a hash of the size bytes starting at buf.
}

/* 해시테이블 내 두 페이지 요소의 페이지 주소값 비교 */
bool page_less(const struct hash_elem *a, const struct hash_elem *b, void *aux)
{
	const struct page *p_a = hash_entry(a, struct page, hash_elem);
	const struct page *p_b = hash_entry(b, struct page, hash_elem);
	return p_a->va < p_b ->va;
}

bool page_insert(struct hash *h, struct page *p) 
{
	if (!hash_insert(h, &p->hash_elem))
		return true;
	else	
		return false;
}

bool page_delete(struct hash *h, struct page *p)
{
	if (hash_delete(h, &p->hash_elem)) 
		return true;
	else	
		return false;		
}

