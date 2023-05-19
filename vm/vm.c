/* vm.c: Generic interface for virtual memory objects. */

#include "threads/malloc.h"
#include "vm/vm.h"
#include "vm/inspect.h"



/* memory management */
struct list frame_table; // frame entry의 리스트로 구성된 frame table
// 비어있는 프레임들이 연결돼있는 연결리스트
// 빈 프레임이 필요할시에 그냥 앞에서 꺼내오면 됨
static struct list_elem *start = NULL; 

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
		vm_initializer *init, void *aux) { // 4번째는 lazy_load_segment

	ASSERT (VM_TYPE(type) != VM_UNINIT)

	struct supplemental_page_table *spt = &thread_current ()->spt;

	/* Check wheter the upage(va) is already occupied or not. */
	if (spt_find_page (spt, upage) == NULL) { 
		/* TODO: Create the page, fetch the initialier according to the VM type,
		 * TODO: and then create "uninit" page struct by calling uninit_new. You
		 * TODO: should modify the field after calling the uninit_new. */
		struct page *page = (struct page*)calloc(1,sizeof(struct page));

	// initializerFunc라는 타입(밑과 같은 형태의 함수 포인터)를 선언
		bool (*initializerFunc)(struct page *, enum vm_type, void *); 
		initializerFunc = NULL;
		// initializerFunc initializer = NULL;	

	// 타입에따라서 initializer의 종류를 설정
		switch(type) {  // 얘네를 안거침
			case VM_ANON:
				initializerFunc = anon_initializer;
				break;

			case VM_FILE:
				initializerFunc = file_backed_initializer;
				break;
		}

		// uninit 페이지를 생성
		uninit_new(page, upage, init, type, aux, initializerFunc); // upage가 곧 va
		// 두번째 인자(va)는 호출시에 lazy_load_segment가 들어간다.
		page->writable = writable;

		if (type == (VM_STACK)) {
			page->stack = true;
		}

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

	struct page *page = (struct page *)calloc(1, sizeof(struct page));
    struct hash_elem *hash_elem;

    page->va = pg_round_down(va); // page->va랑 pg_round

    // pg_round_down()를 사용해 가장 가까운 페이지 경계 주소 찾기
    // -> va가 가리키는 가상 페이지의 시작 포인트(오프셋이 0으로 설정된 va) 반환

    // hash_find()를 사용해 보조 테이블에서 hash_elem 구조체 찾기
    hash_elem = hash_find(&spt->spt_hash, &page->hash_elem);
    
    // 에러 체크 - hash_elem가 비어있는 경우
    if (hash_elem == NULL) {
        // NULL 리턴
        return NULL;
    }
    // hash_elem가 비어있지 않은 경우
    else{
        // hash_elem이 소속되어있는 구조체의 포인터를 반환
        return hash_entry(hash_elem, struct page, hash_elem);
    }
}

/* Insert PAGE into spt with validation. */
bool
spt_insert_page (struct supplemental_page_table *spt UNUSED,
		struct page *page UNUSED) {
    int succ = false;
    
    /* TODO: Fill this function. */
    // hash_find()를 사용해 hash_elem 구조체 찾기
    struct hash_elem *check_page_elem = hash_find(&spt->spt_hash, &page->hash_elem);

	if (hash_find(&spt->spt_hash, &page->hash_elem) != NULL)
		return succ;

    // 에러 체크 - check_page_elem이 NULL인 경우
    // if (!hash_insert(&spt->spt_hash, &page->hash_elem))
    //     return true;
    // else
    //     return false;
	hash_insert(&spt->spt_hash, &page->hash_elem);
	// 성공한 경우 succ값 true로 변환후 반환
	succ = true;
	return succ;

}

void
spt_remove_page (struct supplemental_page_table *spt, struct page *page) {
	vm_dealloc_page (page);
	return true;
}

/* Get the struct frame, that will be evicted. */
static struct frame *
vm_get_victim (void) {
	// struct frame *victim = NULL;
	//     /* TODO: The policy for eviction is up to you. */
    // struct thread *curr = thread_current();
    // struct list_elem *e = start;

    // for (start = e; start != list_end(&frame_table); start = list_next(start)) {
    //     victim = list_entry(start, struct frame, frame_elem);
    //     if (pml4_is_accessed(curr->pml4, victim->page->va))
    //         pml4_set_accessed (curr->pml4, victim->page->va, 0);
    //     else
    //         return victim;
    // }

    // for (start = list_begin(&frame_table); start != e; start = list_next(start)) {
    //     victim = list_entry(start, struct frame, frame_elem);
    //     if (pml4_is_accessed(curr->pml4, victim->page->va))
    //         pml4_set_accessed (curr->pml4, victim->page->va, 0);
    //     else
    //         return victim;
    // }

	// return victim;
}

/* Evict one page and return the corresponding frame.
 * Return NULL on error.*/
static struct frame *
vm_evict_frame (void) {
	struct frame *victim UNUSED = vm_get_victim ();
	/* TODO: swap out the victim and return the evicted frame. */
	// swap_out(victim->page);

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
	// if (is_kernel_vaddr(addr)) {
	// 	return false;
	// }
	ASSERT(!is_kernel_vaddr(addr))

	addr = pg_round_down(addr);

	if (vm_alloc_page_with_initializer(VM_STACK, addr, true, NULL, NULL)) {
		vm_claim_page(addr);
		thread_current()->stack_bottom -= PGSIZE;
	}
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
	if(is_kernel_vaddr(addr))
        return false;
	
    /* ----------------------------------- project3-3_Stack Growth ----------------------------------- */
    // 스택의 증가로 page fault를 해결할 수 있는지 확인
	/*  */
    // if (f->rsp - 8 <= addr && addr <= USER_STACK && USER_STACK - 0x100000 <= addr) { // 0x100000 = 1MB(스택 사이즈 제한)
	// 왜 -8? : so it may cause a page fault 8 bytes below the stack pointer..
	if (USER_STACK - 0x100000 <= thread_current()->rsp_stack - 8 && thread_current()->rsp_stack - 8 <= addr && addr <= thread_current()->stack_bottom) { // 0x100000 = 1MB(스택 사이즈 제한)
        // 스택 증가 함수 호출
        // 주소를 현재 스택의 마지막 주소에서 새롭게 할당받을 크기인 PGSIZE로 넘겨줌
        vm_stack_growth(thread_current()->stack_bottom - PGSIZE);
        return true;
    }

	if (not_present) {
        // 보조 페이지 테이블에서 주소에 맞는 페이지 가져오기
        page = spt_find_page(spt, addr); // addr이 이상해서인지 못찾음 

        // 가져온 페이지가 NULL인 경우
        if (page == NULL)
            // false 리턴
            return false;

        // 페이지에 프레임을 할당하고 mmu 설정
        if (vm_do_claim_page (page) == false)
            // 실패한 경우 false 리턴
            return false;
    }

    // 성공한 경우 true 리턴
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

		여기가 문제였네!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
		-> 위의 install_page를 쓰게되면, swap_in을 거치지 않게 됨
	*/
	if(pml4_get_page(t->pml4, page->va) == NULL && pml4_set_page(t->pml4, page->va, frame->kva, page->writable)){
        // stack의 경우 swap_in 과정이 진행되지 않아도 됨
        if(page->stack == true)
            return true;
        else
            // 페이지 구조체 안의 page_operations 구조체를 통해 swap_int 함수 테이블에 값 넣기
            return swap_in (page, frame->kva);
    }
    // 삽입에 실패한 경우
    else
        // false 리턴
        return false;
}

/* Initialize new supplemental page table -> 구현 필요 
	userprog/process.c의 initd(프로세스가 시작할때)와 __do_fork(프로세스가 포크될때) 호출됨	
*/
void
supplemental_page_table_init (struct supplemental_page_table *spt UNUSED) {
	hash_init(&spt->spt_hash, page_hash, page_less, NULL); // aux 값을 뭘 넣어야 하지?? -> NULL..
	// 왜 init이 안됨?
}

/* Copy supplemental page table from src to dst 
child가 parent의 실행 context를 상속받고자 할때(fork()에서 사용한다는 말임 걍) 사용.

src의 supplemental page table의 각 페이지를 iterate하면서 dst의 supplental page table에 entry의 복사본을 넣음
- uninit page를 만들고 바로 claim 해야 함
*/
bool
supplemental_page_table_copy (struct supplemental_page_table *dst UNUSED,
		struct supplemental_page_table *src UNUSED) {
	struct hash_iterator iter;

	hash_first(&iter, &src->spt_hash);

	while(hash_next(&iter)) {
		struct page *parent_page = hash_entry(hash_cur(&iter), struct page, hash_elem);

	// page->operations->type : 현재 페이지의 타입 
	// page->uninit.type : 초기화 함수로 인자를 넘겨주기 위해 존재하는(uninit에서 변경될)타입

		if (parent_page->operations->type == VM_UNINIT && parent_page->stack == false) { /* 스택이 아닌 Uninit 페이지 */
			if (!vm_alloc_page_with_initializer(parent_page->uninit.type, parent_page->va, parent_page->writable, parent_page->uninit.init, parent_page->uninit.aux)) {
				return false;
			}
		} else { /* parent_page->operations->type이 스택, file_backed, anon -> lazy_load 할 필요가 없음 */
		// 처음부터 ASSERT (VM_TYPE(type) != VM_UNINIT)여기 걸리는게 아니라, 좀 가다가 터짐
			if (!vm_alloc_page_with_initializer(parent_page->uninit.type, parent_page->va, parent_page->writable, NULL, NULL)) {
				return false;
			}

			/* uninit 만들었으니 바로 claim해줌 */		
			if (!vm_claim_page(parent_page->va)) {
				return false;
			}

			struct page *child_page = spt_find_page(dst, parent_page->va);
			memcpy(child_page->frame->kva, parent_page->frame->kva, PGSIZE); // 부모의 물리프레임을 자식에게 복사
		}
	}

	return true;
}

void
destroy_fun (struct hash_elem *e, void *aux UNUSED){
    // 삭제할 페이지 받아오기
    struct page *page = hash_entry (e, struct page, hash_elem);

    // 에러체크 - 가져온 페이지가 NULL일 경우
    ASSERT (page != NULL);

    // destroy()를 사용하여 해당 페이지를 제거
    destroy (page);

    // 사용한 페이지 메모리 반환
    free (page);
}

/* Free the resource hold by the supplemental page table
- process_exit() 이 얘를 부름
 */
void
supplemental_page_table_kill (struct supplemental_page_table *spt UNUSED) {
	/* TODO: Destroy all the supplemental_page_table hold by thread and
	 * TODO: writeback all the modified contents to the storage. */
	// struct hash_iterator i;

	// hash_first(&i, &spt->spt_hash);

	// while(hash_next(&i)) {
	// 	struct page *page = hash_entry(hash_cur(&i), struct page, hash_elem);

	// 	if (page->operations->type == VM_FILE) {
	// 		do_munmap(page->va);
	// 	}
	// }

	// /* destroy_fun을 해시테이블의 모든 element에 적용 */
	// hash_destroy(&spt->spt_hash, destroy_fun);

	// free(&spt->spt_hash.aux);
	hash_clear(&spt->spt_hash, destroy_fun);
}

/* -------- helper function ------------ */


/* 페이지의 가상 주소 값을 해싱을 해주는 함수 */
uint64_t page_hash(const struct hash_elem *p_, void *aux UNUSED)
{
	/* given hash_elem, get 페이지 구조체 첫 주소. 
	 * #define hash_entry(HASH_ELEM, STRUCT, MEMBER)
	 * elem 주소에서 해당 오프셋 차감
	*/
	const struct page *p = hash_entry(p_, struct page, hash_elem);
	return hash_bytes(&p->va, sizeof p->va); 	// Returns a hash of the size bytes starting at buf.
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