/*
    FAT-12 File System Implementation
    Author: R.A.
    Date: 07/06/2024
*/

#ifndef FAT12_H
#define FAT12_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>


#define BYTES_PER_SECTOR 512
#define SECTORS_PER_CLUSTER 1
#define RESERVED_SECTORS 1
#define NUMBER_OF_FATS 1
#define ROOT_ENTRIES 224
#define SECTORS_PER_FAT 9

#define MAX_FILENAME 8
#define MAX_EXTENSION 3
#define FILE_SIZE_BYTES 4
#define MAX_PASSWORD 5
#define FIRST_BLOCK_NUMBER 2
#define LAST_MODIFIED_DATE 2
#define LAST_MODIFIED_TIME 2
#define CREATION_DATE 2
#define CREATION_TIME 2
#define FILE_ATTRIBUTES 1 // Permissions
#define FILE_TYPE 1 // normal file or directory

#define NUMBER_OF_BLOCKS (4 * 1024 * 1024 / BYTES_PER_SECTOR)


typedef struct {
    uint8_t filename[MAX_FILENAME];
    uint8_t extension[MAX_EXTENSION];
    uint8_t file_size[FILE_SIZE_BYTES];
    uint8_t password[MAX_PASSWORD];
    uint8_t first_block_number[FIRST_BLOCK_NUMBER];
    uint8_t last_modified_date[LAST_MODIFIED_DATE];
    uint8_t last_modified_time[LAST_MODIFIED_TIME];
    uint8_t creation_date[CREATION_DATE];
    uint8_t creation_time[CREATION_TIME];
    uint8_t file_attributes[FILE_ATTRIBUTES];
    uint8_t file_type[FILE_TYPE];
} DirectoryEntry;

// superblock that contains the information such as the number of blocks, block size etc.
typedef struct {
    int block_size;
    int number_of_blocks;
    int root_dir_pos;
    int fat_pos;
    int data_pos;
} SuperBlock;

// fat entry
typedef struct {
    uint16_t entry;
} FatEntry;

/*
    0x000: Cluster is free and available to be allocated
    0x001: Cluster is reserved
    0xFFF: Cluster is the last cluster in the file (EOF)
*/

// disk block
typedef struct {
    uint16_t next_block;
    char* data;
} Block;


typedef struct{
    SuperBlock super_block;
    FatEntry fat[NUMBER_OF_BLOCKS];
    DirectoryEntry root_dir[ROOT_ENTRIES];
    Block data[NUMBER_OF_BLOCKS];
} FileSystem;

#endif