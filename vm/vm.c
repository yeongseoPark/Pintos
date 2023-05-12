/* vm.c: Generic interface for virtual memory objects. */

#include "threads/malloc.h"
#include "vm/vm.h"
#include "vm/inspect.h"


/* memory management */
struct list frame_table; // frame entry의 리스트로 구성된 frame table
// 비어있는 프레임들이 연결돼있는 연결리스트
// 빈 프레임이 필요할시에 그냥 앞에서 꺼내오면 됨
static struct list_elem *start = NULL; // frame_table의 시작점. 여기 선언하는게 맞는지 모르겠음

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
	list_init(&frame_table); // 가상 메모리 초기화와 함께, 빈 프레임들을 저장한 frame_table 초기화
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
 * `vm_alloc_page`. 
 * 페이지 구조체 할당하고, 타입에 따른 initializer 세팅하고
 * 제어권 유저 프로그램에게 돌려줌
 * */
bool
vm_alloc_page_with_initializer (enum vm_type type, void *upage, bool writable,
		vm_initializer *init, void *aux) {

	ASSERT (VM_TYPE(type) != VM_UNINIT)

	struct supplemental_page_table *spt = &thread_current ()->spt;

	/* Check wheter the upage(va) is already occupied or not. */
	if (spt_find_page (spt, upage) == NULL) {
		/* TODO: Create the page, fetch the initialier according to the VM type,
		 * TODO: and then create "uninit" page struct by calling uninit_new. You
		 * TODO: should modify the field after calling the uninit_new. */
		struct page *page = (struct page*)malloc(sizeof(struct page));

	// initializerFunc라는 타입(밑과 같은 형태의 함수 포인터)를 선언
		typedef bool (*initializerFunc)(struct page *, enum vm_type, void *); 
		initializerFunc initializer = NULL;	

	// 타입에따라서 initializer의 종류를 설정
		switch(type) { 
			case VM_ANON:
				initializer = anon_initializer;
				break;

			case VM_FILE:
				initializer = file_backed_initializer;
				break;
		}

		// uninit 페이지를 생성
		uninit_new(page, upage, init, type, aux, initializer); // upage가 곧 va
		// 두번째 인자(va)는 호출시에 lazy_load_segment가 들어간다.

		page->writable = writable;

		/* TODO: Insert the page into the spt. */
		return spt_insert_page(spt, page);
	}
err:
	return false;
}

/* Find VA from spt and return page. On error, return NULL. -> 구현 필요 */
struct page *
spt_find_page (struct supplemental_page_table *spt UNUSED, void *va UNUSED) {
	/* TODO: Fill this function. */
	// va를 가지고 어떻게 해당하는 hash_elem을 찾지??
	// dummy page를 만들고, 그것의 가상주소를 va로 만든후에 그 페이지의 hash_elem을 넣는다고?? 그러면, 해당 가상주소에 두개의 페이지가 있는거 아님??
	// struct page *dummy_page = (struct page*)malloc(sizeof(struct page));
	// dummy_page->va = va;

	struct page *page = (struct page*)malloc(sizeof(struct page)); 
	page->va = pg_round_down(va);
	struct hash_elem *elem;

	elem = hash_find(spt->spt_hash, &page->hash_elem); // va값(해시의 키값)을 가지고 hash_elem을 찾기에 이게 가능한듯
	free(page);

	if (elem == NULL) {
		return NULL;
	}
	
	return hash_entry(elem, struct page, hash_elem);
}

/* Insert PAGE into spt with validation. -> 구현 필요 */
bool
spt_insert_page (struct supplemental_page_table *spt UNUSED,
		struct page *page UNUSED) {
	// int succ = false;
	/* check virtual address does not exist in given supplemental page table */
	// if (hash_find(spt->virtual_entry_set, page->vm_entry)) { // 이미 있음
	// 	return succ;
	// }
	/* VA가 주어진 SPT에 있는지 확인도 그냥 hash_insert로 해주면 될듯 */

	/* TODO: Fill this function.  */
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

	return victim;
}

/* Evict one page and return the corresponding frame.
 * Return NULL on error.*/
static struct frame *
vm_evict_frame (void) {
	struct frame *victim UNUSED = vm_get_victim ();
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
static struct frame *
vm_get_frame (void) {
	struct frame *frame = (struct frame*)malloc(sizeof(struct frame));
	/* TODO: Fill this function. */
	ASSERT (frame != NULL);
	ASSERT (frame->page == NULL);

	frame->kva = palloc_get_page(PAL_USER);		// RAM user pool -> (virtual) kernel VA로 1page 할당

	// 물리프레임을 얻고, 그에 대응되는 kernel가상주소 리턴,
	// PAL_USER가 set되면 USER_POOL에서 가져온다는데, 그말인즉슨 메모리의 공간이 유저pool과 kernel pool로 구분돼있는건가??
	// 아니면 그냥 물리프레임에 USER_POOL임을 표시만??
	// -> 메모리의 공간이 유저풀과 커널풀로 반반씩 나눠져있는게 맞음(pool 구조체의 주석에 써있음)
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

/* Return true on success
	addr에 해당하는 페이지를 찾아서, 물리 프레임과 연결시켜줌	
 */
bool
vm_try_handle_fault (struct intr_frame *f UNUSED, void *addr UNUSED,
		bool user UNUSED, bool write UNUSED, bool not_present UNUSED) {
	struct supplemental_page_table *spt UNUSED = &thread_current ()->spt;
	struct page *page = NULL;
	/* TODO: Validate the fault */
	/* TODO: Your code goes here */
	/* spt_find_page로 spt에서 페이지 찾아옴 */
	page = spt_find_page(spt, addr);

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

/* Claim the PAGE and set up the mmu.
	피지컬 프레임을 요구.
	1. vm_get_frame으로 빈프레임을 얻는다
	2. MMU를 세팅해서 Virtual address와 physical address의 매핑을 추가함
	3. 반환값은 함수 동작이 성공적이었는가, 아닌가를 알려줘야 함
 */
static bool
vm_do_claim_page (struct page *page) {
	struct frame *frame = vm_get_frame ();
	struct thread *t = thread_current ();

	ASSERT(frame != NULL);

	/* Set links */
	frame->page = page;
	page->frame = frame;

	/* TODO: Insert page table entry to map page's VA to frame's PA. 
		user virtual주소와 kernel virtual주소의 매핑
		두번째인자인 kpage(여기서는 frame->kva)는 palloc_get_page로 유저풀에서 얻어온 것이어야 함
		실제로 그러함(vm_get_frame을 봐라!)
		본 페이지 테이블인 pml4에 upage와 kpage(즉 물리주소)의 연결을 수행하는 함수가 instal_page
	*/
	if (pml4_get_page (t->pml4, page->va) == NULL && pml4_set_page (t->pml4, page->va, frame->kva, page->writable)){
			return swap_in(page, frame->kva);
	}
	else		// install failed
		return false;
}

/* Initialize new supplemental page table -> 구현 필요 
	userprog/process.c의 initd(프로세스가 시작할때)와 __do_fork(프로세스가 포크될때) 호출됨	
*/
void
supplemental_page_table_init (struct supplemental_page_table *spt UNUSED) {
	hash_init(&spt->spt_hash, page_hash, page_less, NULL); // aux 값을 뭘 넣어야 하지?? -> NULL..
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

/* -------- helper function ------------ */

/* 페이지의 가상 주소 값을 해싱을 해주는 함수 */
unsigned page_hash(const struct hash_elem *p_, void *aux UNUSED)
{
	/* given hash_elem, get 페이지 구조체 첫 주소. 
	 * #define hash_entry(HASH_ELEM, STRUCT, MEMBER)
	 * elem 주소에서 해당 오프셋 차감
	*/
	const struct page *p = hash_entry(p_, struct page, hash_elem);
	return hash_bytes(&p->va, sizeof(p->va)); 	// Returns a hash of the size bytes starting at buf.
}
/* 해시 테이블 내의 두 페이지 요소에 대해 페이지의 주소값을 비교 
	a가 b보다 작으면 true, a가 b보다 같거나 크면 false
*/
bool page_less(const struct hash_elem *a, const struct hash_elem *b, void *aux)
{
	const struct page *p_a = hash_entry(a, struct page, hash_elem);
	const struct page *p_b = hash_entry(b, struct page, hash_elem);
	return p_a->va < p_b ->va;
}

/* p에 들어있는 hash_elem 구조체를 인자로 받은 해시테이블에 삽입 */
bool page_insert(struct hash *h, struct page *p) 
{
	if (!hash_insert(h, &p->hash_elem))
		return true;
	else	
		return false;
}

/* p에 들어있는 hash_elem 구조체를 인자로 받은 해시테이블에서 삭제 */
bool page_delete(struct hash *h, struct page *p)
{
	if (hash_delete(h, &p->hash_elem)) 
		return true;
	else	
		return false;		
}