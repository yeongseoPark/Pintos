#ifndef VM_UNINIT_H
#define VM_UNINIT_H
#include "vm/vm.h"

struct page;
enum vm_type;

typedef bool vm_initializer (struct page *, void *aux);

/* Uninitlialized page. The type for implementing the
 * "Lazy loading". */
struct uninit_page {
	/* Initiate the contets of the page */
	vm_initializer *init; // lazy_load_segment
	enum vm_type type;
	void *aux;
	/* Initiate the struct page and maps the pa to the va */
	bool (*page_initializer) (struct page *, enum vm_type, void *kva); // 각 페이지에 맞는 이니셜라이저

	// init과 page_initiaizlier는 fault -> vm_try_handle_fault -> do_claim_page -> swap_in -> uninit_initialize 에서 호출됨
	// uninit_initialize를 언제 uninit page의 swap in으로 설정하냐면, 
	// process_exec -> load -> load_segment -> vm_alloc_initializer -> uninit_new 에서 미리 설정해뒀다가
	// fault가 나면 맨위의 주석을 실행하는거임
};

void uninit_new (struct page *page, void *va, vm_initializer *init,
		enum vm_type type, void *aux,
		bool (*initializer)(struct page *, enum vm_type, void *kva));
#endif
