/* uninit.c: Implementation of uninitialized page.
 *
 * All of the pages are born as uninit page. When the first page fault occurs,
 * the handler chain calls uninit_initialize (page->operations.swap_in).
 * The uninit_initialize function transmutes the page into the specific page
 * object (anon, file, page_cache), by initializing the page object,and calls
 * initialization callback that passed from vm_alloc_page_with_initializer
 * function.
 * */

#include "vm/vm.h"
#include "vm/uninit.h"

static bool uninit_initialize (struct page *page, void *kva);
static void uninit_destroy (struct page *page);

/* DO NOT MODIFY this struct */
static const struct page_operations uninit_ops = {
	.swap_in = uninit_initialize,
	.swap_out = NULL,
	.destroy = uninit_destroy,
	.type = VM_UNINIT,
};

/* DO NOT MODIFY this function */
void
uninit_new (struct page *page, void *va, vm_initializer *init,
		enum vm_type type, void *aux,
		bool (*initializer)(struct page *, enum vm_type, void *)) {
	ASSERT (page != NULL);

	*page = (struct page) {
		.operations = &uninit_ops,
		.va = va,
		.frame = NULL, /* no frame for now */
		.uninit = (struct uninit_page) {
			.init = init,
			.type = type,
			.aux = aux,
			.page_initializer = initializer,
		}
	};
}

/* 
	인자로 받은 타입에 맞게 페이지를 초기화
	Initalize the page on first fault -> uninit type의 swap_in 함수가 얘임
   vm/anon.c의 vm_anon_init과 anon_initializer를 필요하면 수정할 수도 있다는데??

   page fault() -> vm_try_handle_fault() -> vm_do_claim_page() -> swap_in() -> uninit_initialize() ->
   각 타입에 맞는 initializer와 vm_init()호출 
 */
static bool
uninit_initialize (struct page *page, void *kva) {
	struct uninit_page *uninit = &page->uninit; // 페이지 구조체 내부의 union 안의 uninit

	/* Fetch first, page_initialize may overwrite the values */
	vm_initializer *init = uninit->init; // lazy_load_segment
	void *aux = uninit->aux;

	/* TODO: You may need to fix this function. */
	/* 터짐 포인트 5/15 밤 */
	return uninit->page_initializer (page, uninit->type, kva) && // 각 페이지type에 맞게 초기화함수 호출
		(init ? init (page, aux) : true);
}

/* Free the resources hold by uninit_page. Although most of pages are transmuted
 * to other page objects, it is possible to have uninit pages when the process
 * exit, which are never referenced during the execution.
 * PAGE will be freed by the caller. */
static void
uninit_destroy (struct page *page) {
	struct uninit_page *uninit UNUSED = &page->uninit;
	/* TODO: Fill this function.
	 * TODO: If you don't have anything to do, just return. */
	struct info_aux *aux = (struct info_aux*)(uninit->aux);
	file_close(&aux->file);
}
