#include "filesys/fat.h"
#include "devices/disk.h"
#include "filesys/filesys.h"
#include "threads/malloc.h"
#include "threads/synch.h"
#include <stdio.h>
#include <string.h>
#include <bitmap.h>


void
fat_init (void) {
	fat_fs = calloc (1, sizeof (struct fat_fs));
	if (fat_fs == NULL)
		PANIC ("FAT init failed");

	// Read boot sector from the disk
	unsigned int *bounce = malloc (DISK_SECTOR_SIZE);
	if (bounce == NULL)
		PANIC ("FAT init failed");
	disk_read (filesys_disk, FAT_BOOT_SECTOR, bounce);
	memcpy (&fat_fs->bs, bounce, sizeof (fat_fs->bs));
	free (bounce);

	// Extract FAT info
	if (fat_fs->bs.magic != FAT_MAGIC)
		fat_boot_create ();
	fat_fs_init ();

	/* Project 4 추가 */
	fat_bitmap = bitmap_create(fat_fs->fat_length);
}

void
fat_open (void) {
	fat_fs->fat = calloc (fat_fs->fat_length, sizeof (cluster_t));
	if (fat_fs->fat == NULL)
		PANIC ("FAT load failed");

	// Load FAT directly from the disk
	uint8_t *buffer = (uint8_t *) fat_fs->fat;
	off_t bytes_read = 0;
	off_t bytes_left = sizeof (fat_fs->fat);
	const off_t fat_size_in_bytes = fat_fs->fat_length * sizeof (cluster_t);
	for (unsigned i = 0; i < fat_fs->bs.fat_sectors; i++) {
		bytes_left = fat_size_in_bytes - bytes_read;
		if (bytes_left >= DISK_SECTOR_SIZE) {
			disk_read (filesys_disk, fat_fs->bs.fat_start + i,
			           buffer + bytes_read);
			bytes_read += DISK_SECTOR_SIZE;
		} else {
			uint8_t *bounce = malloc (DISK_SECTOR_SIZE);
			if (bounce == NULL)
				PANIC ("FAT load failed");
			disk_read (filesys_disk, fat_fs->bs.fat_start + i, bounce);
			memcpy (buffer + bytes_read, bounce, bytes_left);
			bytes_read += bytes_left;
			free (bounce);
		}
	}
}

void
fat_close (void) {
	// Write FAT boot sector
	uint8_t *bounce = calloc (1, DISK_SECTOR_SIZE);
	if (bounce == NULL)
		PANIC ("FAT close failed");
	memcpy (bounce, &fat_fs->bs, sizeof (fat_fs->bs));
	disk_write (filesys_disk, FAT_BOOT_SECTOR, bounce);
	free (bounce);

	// Write FAT directly to the disk
	uint8_t *buffer = (uint8_t *) fat_fs->fat;
	off_t bytes_wrote = 0;
	off_t bytes_left = sizeof (fat_fs->fat);
	const off_t fat_size_in_bytes = fat_fs->fat_length * sizeof (cluster_t);
	for (unsigned i = 0; i < fat_fs->bs.fat_sectors; i++) {
		bytes_left = fat_size_in_bytes - bytes_wrote;
		if (bytes_left >= DISK_SECTOR_SIZE) {
			disk_write (filesys_disk, fat_fs->bs.fat_start + i,
			            buffer + bytes_wrote);
			bytes_wrote += DISK_SECTOR_SIZE;
		} else {
			bounce = calloc (1, DISK_SECTOR_SIZE);
			if (bounce == NULL)
				PANIC ("FAT close failed");
			memcpy (bounce, buffer + bytes_wrote, bytes_left);
			disk_write (filesys_disk, fat_fs->bs.fat_start + i, bounce);
			bytes_wrote += bytes_left;
			free (bounce);
		}
	}
}

void
fat_create (void) {
	// Create FAT boot
	fat_boot_create ();
	fat_fs_init ();

	// Create FAT table
	fat_fs->fat = calloc (fat_fs->fat_length, sizeof (cluster_t));
	if (fat_fs->fat == NULL)
		PANIC ("FAT creation failed");

	// Set up ROOT_DIR_CLST
	fat_put (ROOT_DIR_CLUSTER, EOChain);

	// Fill up ROOT_DIR_CLUSTER region with 0
	uint8_t *buf = calloc (1, DISK_SECTOR_SIZE);
	if (buf == NULL)
		PANIC ("FAT create failed due to OOM");
	disk_write (filesys_disk, cluster_to_sector (ROOT_DIR_CLUSTER), buf);
	free (buf);
}

void
fat_boot_create (void) {
	unsigned int fat_sectors =
	    (disk_size (filesys_disk) - 1)
	    / (DISK_SECTOR_SIZE / sizeof (cluster_t) * SECTORS_PER_CLUSTER + 1) + 1;
	fat_fs->bs = (struct fat_boot){
	    .magic = FAT_MAGIC,
	    .sectors_per_cluster = SECTORS_PER_CLUSTER,
	    .total_sectors = disk_size (filesys_disk),
	    .fat_start = 1,
	    .fat_sectors = fat_sectors,
	    .root_dir_cluster = ROOT_DIR_CLUSTER,
	};
}

void
fat_fs_init (void) {
	/* file system 클러스터 개수(data region)를 계산해 fat_length에 저장 */
	// fat_fs가 전체 파일시스템이라면, fat_boot(boot sector)안에 filesys layout에 대한 정보들이 들어있기 때문에
	// boot sector 정보를 가지고 fat_fs의 data region 시작지점 및 클러스터 개수를 계산하는 식으로 이뤄짐. 
	fat_fs->fat_length = disk_size(filesys_disk) -1 - fat_fs->bs.fat_sectors;	// data_start 에서 얼마나 많은 클러스터가 있는지 
	fat_fs->data_start = fat_fs->bs.fat_start + fat_fs->bs.fat_sectors;			
}

/*----------------------------------------------------------------------------*/
/* FAT handling                                                               */
/*----------------------------------------------------------------------------*/

/* Add a cluster to the chain.
 * If CLST is 0, start a new chain.
 * Returns 0 if fails to allocate a new cluster. */
cluster_t
fat_create_chain (cluster_t clst) {
	/* clst에 지정된 클러스터 뒤에 추가하여 체인 확장. 
	 * 지정된 clst가 0이면 새 체인을 만들고, 새로 할당된 클러스터 번호 반환
	 * 즉, 인자가 0이면 해당 clst 넘버 전의 것을 EOF로 결말짓고 새로운 클러스터를 시작하며,
	 * 인자가 0이 아닌 특정 값(x)면 x 다음의 빈 블록의 넘버를 fat[x-1] = val에 넣음으로써 다음 파일조각으로 정함. 
	*/
	cluster_t new_clst = get_empty_cluster();	// 빈 클러스터를 bitmap에서 가져온다
	if (new_clst == 0) return NULL;
	
	fat_put(new_clst, EOChain);

	if (clst != 0) {
		fat_put(clst, new_clst);
	}
	return new_clst;
}
/* 핀토스에서 EOChain은 큰 값임. -1 X. 따라서 fat_get(i) > 0 에 걸리는 조건은 free block 밖에 없음 */

cluster_t get_empty_cluster() {
	size_t clst = bitmap_scan_and_flip(fat_bitmap, 0, 1, false) + 1;
	if (clst == BITMAP_ERROR) 
		return 0;
	else	
		return (cluster_t) clst;	// type casting
}



/* Remove the chain of clusters starting from CLST.
 * If PCLST is 0, assume CLST as the start of the chain. */
void
fat_remove_chain (cluster_t clst, cluster_t pclst) {
	/* clst부터 pclst까지 제거 */
	/* pclst가 입력됐다면, pclst를 끝으로 만듦 */
	if (pclst)
		fat_put(pclst, EOChain);

	// clst부터 순회하며 FAT에서 할당 해제(0)로 설정
	cluster_t temp_c = clst;
	cluster_t next_c;
	for (; fat_get(temp_c) != EOChain; temp_c = next_c) {
		next_c = fat_get(temp_c);
		fat_put(temp_c, 0);
	}
	fat_put(temp_c, 0);  	// EOChain도 0으로 변경
}

/* Update a value in the FAT table. */
void
fat_put (cluster_t clst, cluster_t val) {
	/*  */
	if (cluster_to_sector(clst - 1) >= disk_size(filesys_disk)) return;

	fat_fs->fat[clst - 1] = val;
}

/* Fetch a value in the FAT table. */
cluster_t
fat_get (cluster_t clst) {
	return fat_fs->fat[clst - 1];
}

/* Covert a cluster # to a sector number. */
disk_sector_t
cluster_to_sector (cluster_t clst) {
	/* TODO: Your code goes here. */
	return fat_fs->data_start + clst;	// 1 sector for 1 cluster. (data region부터 sector 카운트)
}

cluster_t
sector_to_clustor(disk_sector_t sector) {
	cluster_t clst = sector - fat_fs->data_start;

	if (clst < 2) return 0;

	return clst;
}