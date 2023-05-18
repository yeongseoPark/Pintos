#ifndef VM_FILE_H
#define VM_FILE_H
#include "filesys/file.h"
#include "vm/vm.h"

struct page;
enum vm_type;

struct file_page {
   void* zeroed_start; // mmap -> zero로 채운 값들의 시작점(file_backed)

   struct file *mapped_file; // 연결된 파일

   // 파일을 읽고자하는 시작점
   off_t offset;
};

void vm_file_init (void);
bool file_backed_initializer (struct page *page, enum vm_type type, void *kva);
void *do_mmap(void *addr, size_t length, int writable,
		struct file *file, off_t offset);
void do_munmap (void *va);
#endif
