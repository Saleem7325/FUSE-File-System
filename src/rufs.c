/*
 *  Copyright (C) 2023 CS416 Rutgers CS
 *	Tiny File System
 *	File:	rufs.c
 *
 */

#define FUSE_USE_VERSION 26

#include <fuse.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <errno.h>
#include <sys/time.h>
#include <libgen.h>
#include <limits.h>

#include "block.h"
#include "rufs.h"

/* The total number of blocks on disk */
// #define BLOCKS (DISK_SIZE / BLOCK_SIZE)

/* The number of bytes needed to store MAX_INUM inodes in the inode region of disk */
#define INODE_BYTES (MAX_INUM * sizeof(struct inode))

/* The number of blocks needed to store MAX_INUM inodes in the inode region of disk */
#define INODE_BLOCKS ((INODE_BYTES % BLOCK_SIZE) == 0 ? (INODE_BYTES / BLOCK_SIZE) : ((INODE_BYTES / BLOCK_SIZE) + 1))

/* The number of Inodes per block */
#define INODES (BLOCK_SIZE / sizeof(struct inode))

/* The number of chars to store in inode_bitmap */
#define IBMAP_BYTES ((MAX_INUM % 8) == 0 ? (MAX_INUM / 8) : ((MAX_INUM / 8) + 1))

/* The number of chars to store in data_bitmap */
#define DBMAP_BYTES ((MAX_DNUM % 8) == 0 ? (MAX_DNUM / 8) : ((MAX_DNUM / 8) + 1))

/* Index of super block */
#define SU_BLK_IDX 0

/* Index of Inode bitmap block */
#define IBMAP_IDX 1

/* Index of data bitmap block */
#define DBMAP_IDX 2

/* Index of start of Inode region */
#define INODE_IDX 3

/* Index of start of data region */
#define DATA_IDX (INODE_IDX + INODE_BLOCKS)

char diskfile_path[PATH_MAX];

/* Declare your in-memory data structures here */

/* Pointer to block, for writing to, reading from, and initializing super block region of disk */
struct superblock *su_blk = NULL;

/* Pointer to block, for writing to, reading from, and initializing an Inode block in Inode region of disk*/
struct inode *inode_blk = NULL;

/* Pointer to block, for writing to, reading from, and initializing Inode bitmap region of disk */
bitmap_t inode_bmap = NULL;

/* Pointer to block, for writing to, reading from, and initializing data block bitmap region of disk */
bitmap_t blk_bmap = NULL;

/*_______________________HELPER FUNCTIONS_______________________*/

int get_avail_ino();
int get_avail_blkno();

int init_data_structures(){
	su_blk = (struct superblock *)malloc(BLOCK_SIZE);
	if(!su_blk){
		perror("Malloc failure: super block initialization\n");
		return -1;
	}

	inode_bmap = (bitmap_t)malloc(BLOCK_SIZE);
	if(!inode_bmap){
		perror("Malloc failure: Inode bitmap initialization\n");
		return -1;
	}

	blk_bmap = (bitmap_t)malloc(BLOCK_SIZE);
	if(!blk_bmap){
		perror("Malloc failure: data block bitmap initialization\n");
		return -1;
	}

	inode_blk = (struct inode *)malloc(BLOCK_SIZE);
	if(!inode_blk){
		perror("Malloc failure: inode block initialization\n");
		return -1;
	}

	return 1;
}

int init_superblock(){
	su_blk = (struct superblock *)malloc(BLOCK_SIZE);
	if(!su_blk){
		perror("Malloc failure: super block initialization\n");
		return -1;
	}

	memset(su_blk, '\0', BLOCK_SIZE);
	su_blk->magic_num = MAGIC_NUM;
	su_blk->max_inum = MAX_INUM;
	su_blk->max_dnum = MAX_DNUM;
	su_blk->i_bitmap_blk = IBMAP_IDX;
	su_blk->d_bitmap_blk = DBMAP_IDX;
	su_blk->i_start_blk = INODE_IDX;
	su_blk->d_start_blk = DATA_IDX;

	return bio_write(0, su_blk);
}

int init_inode_bitmap(){
	inode_bmap = (bitmap_t)malloc(BLOCK_SIZE);
	if(!inode_bmap){
		perror("Malloc failure: Inode bitmap initialization\n");
		return -1;
	}

	memset(inode_bmap, '\0', BLOCK_SIZE);
	return bio_write(su_blk->i_bitmap_blk, inode_bmap);
}

int init_data_bitmap(){
	blk_bmap = (bitmap_t)malloc(BLOCK_SIZE);
	if(!blk_bmap){
		perror("Malloc failure: data block bitmap initialization\n");
		return -1;
	}
	
	memset(blk_bmap, '\0', BLOCK_SIZE);

	// Setting bits for super block, inode bitmap, block bitmap  
	set_bitmap(blk_bmap, SU_BLK_IDX);
	set_bitmap(blk_bmap, su_blk->i_bitmap_blk);
	set_bitmap(blk_bmap, su_blk->d_bitmap_blk);

	// Setting bits for inode region
	uint32_t count = su_blk->i_start_blk;
	while(count < su_blk->d_start_blk){
		set_bitmap(blk_bmap, count++);
	}

	return bio_write(su_blk->d_bitmap_blk, blk_bmap);
}

/*
 * Initializes every inode entry to NULL in all inode region blocks
 * Initializes first inode to root
*/
int init_inode_region(){
	inode_blk = (struct inode *)malloc(BLOCK_SIZE);
	if(!inode_blk){
		perror("Malloc failure: inode block initialization\n");
		return -1;
	}
	memset(inode_blk, '\0', BLOCK_SIZE);

	int ino = get_avail_ino();
	int blk_no = get_avail_blkno();
	if(ino < 0 || blk_no < 0){
		return -1;
	}

	inode_blk[ino].ino = ino;
	inode_blk[ino].valid = 1;
	inode_blk[ino].size = 0;
	inode_blk[ino].type = S_IFDIR;
	inode_blk[ino].direct_ptr[0] = blk_no;

	if(bio_write(INODE_IDX, inode_blk) < 0){
		return -1;
	}
	memset(&inode_blk[ino], '\0', sizeof(struct inode));

	int count = INODE_IDX + 1;
	while(count < INODE_BLOCKS){
		if(bio_write(count++, inode_blk) < 0){
			return -1;
		}
	}

	return 0;
}

int get_inode_block(uint16_t ino){
	return ((ino / INODES) + INODE_IDX);
}

int get_inode_offset(uint16_t ino){
	return (ino % INODES);
}

/*_______________________RUFS FUNCTIONS_______________________*/

/* 
 * Get available inode number from bitmap
 */
int get_avail_ino() {
	// Step 1: Read inode bitmap from disk
	if(bio_read(IBMAP_IDX, inode_bmap) < 0){
		return -1;
	}

	// Step 2: Traverse inode bitmap to find an available slot
	int ino = -1;
	for(int i = 0; i < MAX_INUM; i++){
		if(get_bitmap(inode_bmap, i) == 0){
			ino = i;
			break;
		}
	}

	// Step 3: Update inode bitmap and write to disk
	if(ino == -1){
		return -1;
	}

	set_bitmap(inode_bmap, ino);
	if(bio_write(IBMAP_IDX, inode_bmap) < 0){
		return -1;
	}

	return ino;
}

/* 
 * Get available data block number from bitmap
 */
int get_avail_blkno() {
	// Step 1: Read data block bitmap from disk
	if(bio_read(DBMAP_IDX, blk_bmap) < 0){
		return -1;
	}
	
	// Step 2: Traverse data block bitmap to find an available slot
	int blk = -1;
	for(int i = 0; i < MAX_DNUM; i++){
		if(get_bitmap(blk_bmap, i) == 0){
			blk = i;
			break;
		}
	}

	// Step 3: Update data block bitmap and write to disk 
	if(blk == -1){
		return -1;
	}

	set_bitmap(blk_bmap, blk);
	if(bio_write(DBMAP_IDX, blk_bmap) < 0){
		return -1;
	}

	return blk;
}

/* 
 * inode operations
 */
int readi(uint16_t ino, struct inode *inode) {
  // Step 1: Get the inode's on-disk block number
  // Step 2: Get offset of the inode in the inode on-disk block
  // Step 3: Read the block from disk and then copy into inode structure
	int block = get_inode_block(ino);
	int offset = get_inode_offset(ino);

	if(bio_read(block, inode_blk) < 0){
		return -1;
	}

	memcpy(inode, &inode_blk[offset], sizeof(struct inode));
	return 0;
}

int writei(uint16_t ino, struct inode *inode) {
	// Step 1: Get the block number where this inode resides on disk
	// Step 2: Get the offset in the block where this inode resides on disk
	// Step 3: Write inode to disk
 	int block = get_inode_block(ino);
	int offset = get_inode_offset(ino);

	if(bio_read(block, inode_blk) < 0){
		return -1;
	}

	memcpy(&inode_blk[offset], inode, sizeof(struct inode));

	if(bio_write(block, inode_blk) < 0){
		return -1;
	}
	
	return 0;
}


/* 
 * directory operations
 */
int dir_find(uint16_t ino, const char *fname, size_t name_len, struct dirent *dirent) {

  // Step 1: Call readi() to get the inode using ino (inode number of current directory)

  // Step 2: Get data block of current directory from inode

  // Step 3: Read directory's data block and check each directory entry.
  //If the name matches, then copy directory entry to dirent structure

	return 0;
}

int dir_add(struct inode dir_inode, uint16_t f_ino, const char *fname, size_t name_len) {

	// Step 1: Read dir_inode's data block and check each directory entry of dir_inode
	
	// Step 2: Check if fname (directory name) is already used in other entries

	// Step 3: Add directory entry in dir_inode's data block and write to disk

	// Allocate a new data block for this directory if it does not exist

	// Update directory inode

	// Write directory entry

	return 0;
}

int dir_remove(struct inode dir_inode, const char *fname, size_t name_len) {

	// Step 1: Read dir_inode's data block and checks each directory entry of dir_inode
	
	// Step 2: Check if fname exist

	// Step 3: If exist, then remove it from dir_inode's data block and write to disk

	return 0;
}

/* 
 * namei operation
 */
int get_node_by_path(const char *path, uint16_t ino, struct inode *inode) {
	
	// Step 1: Resolve the path name, walk through path, and finally, find its inode.
	// Note: You could either implement it in a iterative way or recursive way

	return 0;
}

/* 
 * Make file system
 */
int rufs_mkfs() {
	// Call dev_init() to initialize (Create) Diskfile
	dev_init(diskfile_path);

	// write superblock information
	// initialize inode bitmap
	// initialize data block bitmap
	// update bitmap information for root directory
	// update inode for root directory
	if(init_superblock() < 0 || 
		init_inode_bitmap() < 0 || 
		init_data_bitmap() < 0 || 
		init_inode_region() < 0
	){
		return - 1;
	}

	return 0;
}


/* 
 * FUSE file operations
 */
static void *rufs_init(struct fuse_conn_info *conn) {
	// Step 1a: If disk file is not found, call mkfs
	// Step 1b: If disk file is found, just initialize in-memory data structures and read superblock from disk
	if(dev_open(diskfile_path) == 0){
		// read super block information
		init_data_structures();
		bio_read(SU_BLK_IDX, su_blk);
	}else{
		rufs_mkfs();
	}

	return NULL;
}

static void rufs_destroy(void *userdata) {
	// Step 1: De-allocate in-memory data structures
	// Step 2: Close diskfile
	if(su_blk){
		free(su_blk);
	}

	if(inode_bmap){
		free(inode_bmap);
	}

	if(blk_bmap){
		free(blk_bmap);
	}

	if(inode_blk){
		free(inode_blk);
	}

	dev_close();
}

static int rufs_getattr(const char *path, struct stat *stbuf) {

	// Step 1: call get_node_by_path() to get inode from path

	// Step 2: fill attribute of file into stbuf from inode

		stbuf->st_mode   = S_IFDIR | 0755;
		stbuf->st_nlink  = 2;
		time(&stbuf->st_mtime);

	return 0;
}

static int rufs_opendir(const char *path, struct fuse_file_info *fi) {

	// Step 1: Call get_node_by_path() to get inode from path

	// Step 2: If not find, return -1

    return 0;
}

static int rufs_readdir(const char *path, void *buffer, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi) {

	// Step 1: Call get_node_by_path() to get inode from path

	// Step 2: Read directory entries from its data blocks, and copy them to filler

	return 0;
}


static int rufs_mkdir(const char *path, mode_t mode) {

	// Step 1: Use dirname() and basename() to separate parent directory path and target directory name

	// Step 2: Call get_node_by_path() to get inode of parent directory

	// Step 3: Call get_avail_ino() to get an available inode number

	// Step 4: Call dir_add() to add directory entry of target directory to parent directory

	// Step 5: Update inode for target directory

	// Step 6: Call writei() to write inode to disk
	

	return 0;
}

static int rufs_rmdir(const char *path) {

	// Step 1: Use dirname() and basename() to separate parent directory path and target directory name

	// Step 2: Call get_node_by_path() to get inode of target directory

	// Step 3: Clear data block bitmap of target directory

	// Step 4: Clear inode bitmap and its data block

	// Step 5: Call get_node_by_path() to get inode of parent directory

	// Step 6: Call dir_remove() to remove directory entry of target directory in its parent directory

	return 0;
}

static int rufs_releasedir(const char *path, struct fuse_file_info *fi) {
	// For this project, you don't need to fill this function
	// But DO NOT DELETE IT!
    return 0;
}

static int rufs_create(const char *path, mode_t mode, struct fuse_file_info *fi) {

	// Step 1: Use dirname() and basename() to separate parent directory path and target file name

	// Step 2: Call get_node_by_path() to get inode of parent directory

	// Step 3: Call get_avail_ino() to get an available inode number

	// Step 4: Call dir_add() to add directory entry of target file to parent directory

	// Step 5: Update inode for target file

	// Step 6: Call writei() to write inode to disk

	return 0;
}

static int rufs_open(const char *path, struct fuse_file_info *fi) {

	// Step 1: Call get_node_by_path() to get inode from path

	// Step 2: If not find, return -1

	return 0;
}

static int rufs_read(const char *path, char *buffer, size_t size, off_t offset, struct fuse_file_info *fi) {

	// Step 1: You could call get_node_by_path() to get inode from path

	// Step 2: Based on size and offset, read its data blocks from disk

	// Step 3: copy the correct amount of data from offset to buffer

	// Note: this function should return the amount of bytes you copied to buffer
	return 0;
}

static int rufs_write(const char *path, const char *buffer, size_t size, off_t offset, struct fuse_file_info *fi) {
	// Step 1: You could call get_node_by_path() to get inode from path

	// Step 2: Based on size and offset, read its data blocks from disk

	// Step 3: Write the correct amount of data from offset to disk

	// Step 4: Update the inode info and write it to disk

	// Note: this function should return the amount of bytes you write to disk
	return size;
}

static int rufs_unlink(const char *path) {

	// Step 1: Use dirname() and basename() to separate parent directory path and target file name

	// Step 2: Call get_node_by_path() to get inode of target file

	// Step 3: Clear data block bitmap of target file

	// Step 4: Clear inode bitmap and its data block

	// Step 5: Call get_node_by_path() to get inode of parent directory

	// Step 6: Call dir_remove() to remove directory entry of target file in its parent directory

	return 0;
}

static int rufs_truncate(const char *path, off_t size) {
	// For this project, you don't need to fill this function
	// But DO NOT DELETE IT!
    return 0;
}

static int rufs_release(const char *path, struct fuse_file_info *fi) {
	// For this project, you don't need to fill this function
	// But DO NOT DELETE IT!
	return 0;
}

static int rufs_flush(const char * path, struct fuse_file_info * fi) {
	// For this project, you don't need to fill this function
	// But DO NOT DELETE IT!
    return 0;
}

static int rufs_utimens(const char *path, const struct timespec tv[2]) {
	// For this project, you don't need to fill this function
	// But DO NOT DELETE IT!
    return 0;
}


static struct fuse_operations rufs_ope = {
	.init		= rufs_init,
	.destroy	= rufs_destroy,

	.getattr	= rufs_getattr,
	.readdir	= rufs_readdir,
	.opendir	= rufs_opendir,
	.releasedir	= rufs_releasedir,
	.mkdir		= rufs_mkdir,
	.rmdir		= rufs_rmdir,

	.create		= rufs_create,
	.open		= rufs_open,
	.read 		= rufs_read,
	.write		= rufs_write,
	.unlink		= rufs_unlink,

	.truncate   = rufs_truncate,
	.flush      = rufs_flush,
	.utimens    = rufs_utimens,
	.release	= rufs_release
};


int main(int argc, char *argv[]) {
	int fuse_stat;

	getcwd(diskfile_path, PATH_MAX);
	strcat(diskfile_path, "/DISKFILE");

	fuse_stat = fuse_main(argc, argv, &rufs_ope, NULL);

	return fuse_stat;
}

