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
	if (spt_find_page (spt, upage) == NULL) {
		/* TODO: Create the page, fetch the initialier according to the VM type,
		 * TODO: and then create "uninit" page struct by calling uninit_new. You
		 * TODO: should modify the field after calling the uninit_new. */

		/* TODO: Insert the page into the spt. */
	
		/********************  P3 추가 ***********************/
		struct page *page = (struct page *)calloc(1, sizeof(struct page));




	
	
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
	struct page *page = (struct page *)malloc(sizeof(struct page));
	struct hash_elem *e;

	page->va = pg_round_down(va);	// 해당 va와 가장 가까운 페이지 시작 주소로 설정

	/* e와 같은 해시값을 갖는 page를 spt에서 찾아 hash_elem 리턴 */
	e = hash_find(&spt->spt_hash, &page->hash_elem);
	free(page);

	return e != NULL ? hash_entry(e, struct page, hash_elem): NULL;
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
	struct frame *frame = (struct frame *)malloc(sizeof(struct frame));
	/* TODO: Fill this function. */

	ASSERT (frame != NULL);
	ASSERT (frame->page == NULL);

	frame->kva = palloc_get_page(PAL_USER);		// RAM user pool -> (virtual) kernel VA로 1page 할당
	if (frame->kva == NULL) {
		PANIC("todo");
		// frame = vm_evict_frame();		// RAM user pool이 없으면 frame에서 evict, 새로 할당
		// frame->page = NULL;
		// return frame;
	}
	list_push_back(&frame_table, &frame->frame_elem);
	frame->page = NULL;
	return frame;
}

/* Growing the stack. */
static void
vm_stack_growth (void *addr UNUSED) {
}

/* Handle the fault on write_protected page */
static bool
vm_handle_wp (struct page *page UNUSED) {
}

/* Return true on success */
bool
vm_try_handle_fault (struct intr_frame *f UNUSED, void *addr UNUSED,
		bool user UNUSED, bool write UNUSED, bool not_present UNUSED) {
	struct supplemental_page_table *spt UNUSED = &thread_current ()->spt;
	struct page *page = NULL;
	/* TODO: Validate the fault */
	/* TODO: Your code goes here */

	return vm_do_claim_page (page);
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
}

/* Free the resource hold by the supplemental page table */
void
supplemental_page_table_kill (struct supplemental_page_table *spt UNUSED) {
	/* TODO: Destroy all the supplemental_page_table hold by thread and
	 * TODO: writeback all the modified contents to the storage. */
}


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

