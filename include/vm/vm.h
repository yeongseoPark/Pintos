#ifndef VM_VM_H
#define VM_VM_H
#include <stdbool.h>
#include "threads/palloc.h"
#include <hash.h>
#include "include/threads/vaddr.h" // pg_round_down
// #include <process.c> // install_page


enum vm_type {
	/* page not initialized */
	VM_UNINIT = 0,
	/* page not related to the file, aka anonymous page */
	VM_ANON = 1,
	/* page that realated to the file */
	VM_FILE = 2,
	/* page that hold the page cache, for project 4 */
	VM_PAGE_CACHE = 3,

	/* Bit flags to store state */

	/* Auxillary bit flag marker for store information. You can add more
	 * markers, until the value is fit in the int. */
	VM_MARKER_0 = (1 << 3),
	VM_MARKER_1 = (1 << 4),

	/* DO NOT EXCEED THIS VALUE. */
	VM_MARKER_END = (1 << 31),
};

#include "vm/uninit.h"
#include "vm/anon.h"
#include "vm/file.h"
#ifdef EFILESYS
#include "filesys/page_cache.h"
#endif

struct page_operations;
struct thread;

#define VM_TYPE(type) ((type) & 7)

/* ----------- load segment 에서 사용하기 위해 추가 */
struct info_aux {
	struct file *file;
	off_t offset;
	uint32_t read_bytes;
	uint32_t zero_bytes;
};


/* The representation of "page".
 * This is kind of "parent class", which has four "child class"es, which are
 * uninit_page, file_page, anon_page, and page cache (project4).
 * DO NOT REMOVE/MODIFY PREDEFINED MEMBER OF THIS STRUCTURE. */
struct page {
	const struct page_operations *operations;
	void *va;              /* Address in terms of user space */
	struct frame *frame;   /* Back reference for frame */

	/* Your implementation */
	/* ㄴ 뭘 구현하라는거임? -> vm_entry, 즉 hash element를 넣는건가? */
	/* 각 페이지는 supplemental page table에 들어가야 하기에 hash_elem 선언 */
	struct hash_elem *vm_entry;
	bool writable;

	/* Per-type data are binded into the union.
	 * Each function automatically detects the current union */
	union {
		struct uninit_page uninit;
		struct anon_page anon;
		struct file_page file;
#ifdef EFILESYS
		struct page_cache page_cache;
#endif
	};
};

/* The representation of "frame" -> physical memory 
   frame 구조체 자체는 프로세스의 커널 가상 주소에 위치(커널의 시작이랑 물리메모리 시작이랑 직접 연결돼있어서 이를통해 물리메모리에서의 위치를 알 수 있는듯?)
*/
struct frame {
	void *kva; // kernel virtual address
	struct page *page; // page structure -> 프레임과 연결된 가상 주소의 페이지

	/* 멤버 추가가능 */
	struct list_elem frame_elem; // vm.c의 frame_table에 들어갈 elem
};

/* The function table for page operations.
 * This is one way of implementing "interface" in C.
 * Put the table of "method" into the struct's member, and
 * call it whenever you needed. */
struct page_operations {
	bool (*swap_in) (struct page *, void *);
	bool (*swap_out) (struct page *);
	void (*destroy) (struct page *); // 페이지가 VM_FILE이라면, file.c의 file_backed_destroy 호출됨
	enum vm_type type;
};

#define swap_in(page, v) (page)->operations->swap_in ((page), v)
#define swap_out(page) (page)->operations->swap_out (page)
#define destroy(page) \
	if ((page)->operations->destroy) (page)->operations->destroy (page)

/* Representation of current process's memory space.
 * We don't want to force you to obey any specific design for this struct.
 * All designs up to you for this. */
struct supplemental_page_table {
	struct hash *virtual_entry_set;  // hash table 선언
};

#include "threads/thread.h"
void supplemental_page_table_init (struct supplemental_page_table *spt);
bool supplemental_page_table_copy (struct supplemental_page_table *dst,
		struct supplemental_page_table *src);
void supplemental_page_table_kill (struct supplemental_page_table *spt);
struct page *spt_find_page (struct supplemental_page_table *spt,
		void *va);
bool spt_insert_page (struct supplemental_page_table *spt, struct page *page);
void spt_remove_page (struct supplemental_page_table *spt, struct page *page);

void vm_init (void);
bool vm_try_handle_fault (struct intr_frame *f, void *addr, bool user,
		bool write, bool not_present);

#define vm_alloc_page(type, upage, writable) \
	vm_alloc_page_with_initializer ((type), (upage), (writable), NULL, NULL)
bool vm_alloc_page_with_initializer (enum vm_type type, void *upage,
		bool writable, vm_initializer *init, void *aux);
void vm_dealloc_page (struct page *page);
bool vm_claim_page (void *va);
enum vm_type page_get_type (struct page *page);

/* 보조 함수 추가 */
unsigned page_hash(const struct hash_elem *p_elem, void *aux UNUSED);
bool page_less(const struct hash_elem *a , const struct hash_elem *b, void *aux);
bool page_insert(struct hash *h, struct page *p);
bool page_delete(struct hash* h, struct page *p);

#endif  /* VM_VM_H */
