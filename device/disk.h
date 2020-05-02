/* Copyright (C) 2017 Daniel Page <csdsp@bristol.ac.uk>
 *
 * Use of this source code is restricted per the CC BY-NC-ND license, a copy of 
 * which can be found via http://creativecommons.org (and should be included as 
 * LICENSE.txt within the associated archive or repository).
 */

#ifndef __DISK_H
#define __DISK_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "PL011.h"

/* Each of the following functions adopts the same approach to
 * reporting success vs. failure, as indicated by the response 
 * produced by the disk: they return an r st.
 * 
 * r <  0 means failure
 * r >= 0 means success
 *
 * Rather than give up immediately if a given request fails, it
 * will (automatically) retry for some fixed number of times.
 */

#define DISK_RETRY   (  3 )

#define DISK_SUCCESS (  0 )
#define DISK_FAILURE ( -1 )

#define BLOCK_NUM 2048
#define INODE_BLOCKS 24
#define DATA_BLOCKS 1000

// query the disk block count
extern int disk_get_block_num();
// query the disk block length
extern int disk_get_block_len();

// write an n-byte block of data x to   the disk at block address a
extern int disk_wr( uint32_t a, const uint8_t* x, int n );
// read  an n-byte block of data x from the disk at block address a
extern int disk_rd( uint32_t a,       uint8_t* x, int n );

// sblock
typedef struct s_block{
	uint32_t inode_count;
	uint32_t root_inode;
} s_block;

// inode struct, stores file metadata
typedef struct inode {
	uint16_t i_ino;          // inode number
	char     *i_type[1];     // file type (directory / file)
	uint8_t  i_nlink;        // number of hard links
	uint32_t i_size;         // file size in bytes
	uint16_t i_blocks;       // file size in blocks
	uint16_t i_addresses[15]; // data block addresses
} inode;


#endif
