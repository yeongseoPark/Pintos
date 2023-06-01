#include "filesys/filesys.h"
#include <debug.h>
#include <stdio.h>
#include <string.h>
#include "filesys/file.h"
#include "filesys/filesys.h"
#include "filesys/free-map.h"
#include "filesys/inode.h"
#include "filesys/directory.h"
#include "devices/disk.h"
#include "threads/thread.h"
#include "filesys/fat.h"

/* The disk that contains the file system. */
struct disk *filesys_disk;

static void do_format (void);

/* Initializes the file system module.
 * If FORMAT is true, reformats the file system. */
void filesys_init (bool format) {
	filesys_disk = disk_get (0, 1);
	if (filesys_disk == NULL)
		PANIC ("hd0:1 (hdb) not present, file system initialization failed");

	inode_init ();

#ifdef EFILESYS
	fat_init ();

	if (format)
		do_format ();

	fat_open ();
#else
	/* Original FS */
	free_map_init ();

	if (format)
		do_format ();

	free_map_open ();
#endif
}

/* Shuts down the file system module, writing any unwritten data
 * to disk. */
void filesys_done (void) {
	/* Original FS */
#ifdef EFILESYS
	fat_close ();
#else
	free_map_close ();
#endif
}

/* Creates a file named NAME with the given INITIAL_SIZE.
 * Returns true if successful, false otherwise.
 * Fails if a file named NAME already exists,
 * or if internal memory allocation fails. */
bool filesys_create (const char *name, off_t initial_size) {

	disk_sector_t inode_sector = 0; 

	/* inode와 off_t를 멤버로 가지는 dir구조체
	   inode를 알기 위해서 Link를 알아야 함. 
	   Hard Link / Soft Link 
	   https://www.youtube.com/watch?v=9_KIdQ8abH4 해당영상 14분

	   Link file : 알아보기 쉽게 파일을 저장해놓은 테이블
	   이는 실제 파일이 저장된 filesystem과 연결됨

	   이 Link file과 File system을 inode를 이용하여 연결함
	   inode는 파일이 생성될때, 모든 파일에 unique하게 부여되는 번호.
	   File system의 REF는, 실제 file system을 참조하는 링크파일의 개수
	   이 REF가 0이 되면 OS는 파일을 실제 file system에서 삭제하게 됨

	   하드링크 : file system에 저장된 inode값을 똑같이 참조하는 링크파일을 여러개 만드는 것 
	   - 원본 데이터의 복사본을 만드는 동시에, 파일 시스템에서 같은 값을 참조하게 만듬
	   - 원본이든,복사본이든 어떤 파일이라도 수정이 되면, 참조하고 있는 모든 파일이 수정, 편집이 일어남
		- 그래서, 하드링크를 생성한 이상, 원본과 복사본이라는 개념 자체가 사라지게 됨
		- 같은 inode를 참조(하드링크로 생성된) 파일이 하나 삭제되어도, 다른 파일에는 영향을 주지 않음
			- 수정과 달리, 삭제는 개별 파일처럼 취급 (REF카운트가 0이 되기 전까지 저장된 데이터가 지워지지 않기에)
			- 디렉토리에는 하드링크를 설정할 수 없고, 파일에만 설정 가능

		소프트링크 : 윈도우의 바로가기 같은 것. 
		- 링크파일이 링크파일을 참조하는 방식
		- library.so 파일이 library1.0.so를 참조하다가 , library2.0.so를 참조하게 되는것
	*/
	struct dir *dir = dir_open_root (); // 루트 디렉토리 오픈
	bool success = (dir != NULL
			&& free_map_allocate (1, &inode_sector) // 하나의 free sector를 찾고, 첫번째부분을 inode_sector에 저장 
			&& inode_create (inode_sector, initial_size, 0) // initial size만큼의 inode를 초기화하고, file system disk의 sector에 inode를 씀
			&& dir_add (dir, name, inode_sector)); // dir에 name이란 파일을 더함, file의 inode는 inode_sector안에 있다
	if (!success && inode_sector != 0)
		free_map_release (inode_sector, 1);
	dir_close (dir); // 썼으니 디렉토리 닫아줌

	return success;
}


/* Opens the file with the given NAME.
 * Returns the new file if successful or a null pointer
 * otherwise.
 * Fails if no file named NAME exists,
 * or if an internal memory allocation fails. */
struct file *filesys_open (const char *name) {
	struct dir *dir = dir_open_root ();
	struct inode *inode = NULL;

	if (dir != NULL)
		dir_lookup (dir, name, &inode); 
		// 디렉토리 내에서 해당 name을 가진 파일을 찾음, 찾기에 성공시 inode를 채움
	dir_close (dir);

	return file_open (inode); // 주어진 inode에 맞는 파일을 연다
}

/* Deletes the file named NAME.
 * Returns true if successful, false on failure.
 * Fails if no file named NAME exists,
 * or if an internal memory allocation fails. */
bool filesys_remove (const char *name) {
	struct dir *dir = dir_open_root (); // 디렉토리를 열고
	bool success = dir != NULL && dir_remove (dir, name); // 지워줌
	dir_close (dir);

	return success;
}

/* Formats the file system. */
static void do_format (void) {
	printf ("Formatting file system...");

#ifdef EFILESYS
	/* Create FAT and save it to the disk. */
	fat_create ();
	fat_close ();
#else
	free_map_create ();
	if (!dir_create (ROOT_DIR_SECTOR, 16))
		PANIC ("root directory creation failed");
	free_map_close ();
#endif

	printf ("done.\n");
}




// ************************* P4-2 추가 ************************* //
/* name 경로분석*/
/* bitmap에서 inode sector 번호할당*/
/* 할당받은 sector에file_name의디렉터리생성*/
/* 디렉터리 엔트리에file_name의엔트리추가*/
/* 디렉터리 엔트리에‘.’, ‘..’ 파일의엔트리추가*/
bool filesys_create_dir(const char *name)
{
	bool success = false;

	char file_name[NAME_MAX + 1];
	struct dir *dir = parse_path(name, file_name);	// 상위 디렉터리는 dir 변수에 저장되고 파일 이름은 file_name 배열에 저장
	if (dir == NULL) return false;	// parse_path 함수가 NULL을 반환하면 상위 디렉터리가 존재하지 않거나 오류가 발생

	cluster_t inode_cluster = fat_create_chain(0);  //  디렉터리의 inode에 대한 새 클러스터를 생성
	disk_sector_t inode_sector = cluster_to_sector(inode_cluster);

}



struct dir *parse_path(char *path_name, char *file_name)
{
	struct thread *curr = thread_current();
	struct dir *dir;

	char path[512];
	strlcpy(path, path_name, strlen(path_name) + 1);

	if (path == NULL || file_name == NULL) return NULL;
	if (strlen(path) == 0) return NULL;

	if (path[0] == '/')
		dir = dir_open_root();
	else
		dir = dir_reopen(curr->curr_dir);

	char *token, *next_token, *save_ptr;
	token = strtok_r(path, "/", &save_ptr);
	next_token = strtok_r(NULL, "/", &save_ptr);

	struct inode *inode = NULL;
	while (token != NULL && next_token != NULL)
	{
		if (!dir_lookup(dir, token, &inode))
			goto fail;
		if (inode_is_link(inode))
		{
			char *link_name = inode_get_link_name(inode);
			strlcpy(path, link_name, strlen(link_name) + 1);

			strlcat(path, "/", strlen(path) + 2);
			strlcat(path, next_token, strlen(path) + strlen(next_token) + 1);
			strlcat(path, save_ptr, strlen(path) + strlen(save_ptr) + 1);

			dir_close(dir);

			if (path[0] == '/')
				dir = dir_open_root();
			else
				dir = dir_reopen(curr->curr_dir);

			token = strtok_r(path, "/", &save_ptr);
			next_token = strtok_r(NULL, "/", &save_ptr);

			continue;
		}

		if (inode_is_dir(inode) == 0)
			goto fail;

		dir_close(dir);
		dir = dir_open(inode);

		token = next_token;
		next_token = strtok_r(NULL, "/", &save_ptr);
	}

	if (token == NULL)
		strlcpy(file_name, ".", 2);
	else
	{
		if (strlen(token) > NAME_MAX)
			goto fail;

		strlcpy(file_name, token, strlen(token) + 1);
	}
	return dir;

fail:
	dir_close(dir);
	return NULL;
}



