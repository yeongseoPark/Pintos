#ifndef USERPROG_PROCESS_H
#define USERPROG_PROCESS_H

#include "threads/thread.h"
/* 준코(05/13) */
#include "filesys/file.h"

tid_t process_create_initd(const char *file_name);
tid_t process_fork(const char *name, struct intr_frame *if_);
int process_exec(void *f_name);
int process_wait(tid_t);
void process_exit(void);
void process_activate(struct thread *next);

// ******************************LINE ADDED****************************** //
// Project 2-1 : User Programs - Argument Passing
void argument_stack(char **argv, int argc, struct intr_frame *if_);
// Project 2-2-1 : User Programs - System Call - Basics
struct thread *get_child(int pid);
// *************************ADDED LINE ENDS HERE************************* //
static bool install_page(void *upage, void *kpage, bool writable);

/* 준코(05/13) */
static bool load_segment(struct file *file, off_t ofs, uint8_t *upage,
                         uint32_t read_bytes, uint32_t zero_bytes, bool writable);

struct aux
{
    struct file *file;
    off_t ofs;
    size_t read_bytes;
    size_t zero_bytes;
};

bool lazy_load_segment(struct page *page, void *aux);
#endif /* userprog/process.h */
