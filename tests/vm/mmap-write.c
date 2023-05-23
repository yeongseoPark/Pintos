/* Writes to a file through a mapping, and unmaps the file,
   then reads the data in the file back using the read system
   call to verify. */

#include <string.h>
#include <syscall.h>
#include "tests/vm/sample.inc"
#include "tests/lib.h"
#include "tests/main.h"

#define ACTUAL ((void *) 0x10000000)

void
test_main (void)
{
  int handle;
  void *map;
  char buf[1024];

  /* Write file via mmap. */
  CHECK (create ("sample.txt", strlen (sample)), "create \"sample.txt\"");
  CHECK ((handle = open ("sample.txt")) > 1, "open \"sample.txt\"");
  // ACTUAL(addr)에 4096 length만큼 handle(fd) 파일내용을 쓰겠다. -> 첫 주소를 map으로 리턴
  CHECK ((map = mmap (ACTUAL, 4096, 1, handle, 0)) != MAP_FAILED, "mmap \"sample.txt\"");

  memcpy (ACTUAL, sample, strlen (sample));
  munmap (map);                         // 그 자리에 sample을 쓰고, map은 해제            

  /* Read back via read(). */
  read (handle, buf, strlen (sample));  // handle(fd)를 읽어서 buf에 쓰겠다.
  CHECK (!memcmp (buf, sample, strlen (sample)),
         "compare read data against written data");
  close (handle);
}



  // ASSERT (map != NULL);               // @ 
  // printf("map is %p\n", map);           // @
  // read (handle, buf, strlen (sample));// @
  // printf("buf is %s\n", buf);// @
  // printf("handle is %d\n", handle);// @

  // printf("buf is %s\n", buf);         // @ writen (변경)
  // printf("sample is %s\n", sample);   // @ original 