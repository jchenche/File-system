#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define BLOCK_SIZE 512
#define NUM_BLOCKS 4096
#define INODE_SIZE 160

void readBlock(FILE* disk, int blockNum, char* buffer)
{
    fseek(disk, blockNum * BLOCK_SIZE, SEEK_SET);
    fread(buffer, BLOCK_SIZE, 1, disk);
}

void writeBlock(FILE* disk, int blockNum, char* data, int size)
{
    fseek(disk, blockNum * BLOCK_SIZE, SEEK_SET);
    fwrite(data, size, 1, disk);
}

int find_bit_one(int c)
{
    /* Given a byte, find the first 1 bit starting from the left */
    int mask = 0x100;
    for (int i = 0; i < 8; i++) {
        mask = mask >> 1;
        if ((mask & c) == mask) return i;
    }
    return -1;
}

int find_available_block(FILE* disk, int data_type)
{
    int c, bit_one_num, lower_bound, upper_bound;
    char* buffer = (char*) malloc(BLOCK_SIZE);
    readBlock(disk, 1, buffer);

    // 0 for metadata, 1 for filedata
    if (data_type == 0) {
        // first 16 bytes (128 bits) are for metadata blocks
        lower_bound = 0;
        upper_bound = 16;
    } else {
        lower_bound = 16;
        upper_bound = BLOCK_SIZE;
    }

    for (int i = lower_bound; i < upper_bound; i++) {
        c = buffer[i];
        if ((bit_one_num = find_bit_one(c)) != -1) {
            printf("location: %d\n", BLOCK_SIZE + i);
            buffer[i] = c & (~(0x80 >> bit_one_num)); // set to 0 now
            writeBlock(disk, 1, buffer, BLOCK_SIZE);
            return i * 8 + bit_one_num;
        }
    }

    return 0; // means no available blocks
}

void createFile(FILE* disk, int mode)
{
    char* inode = (char*) malloc(12);
    unsigned int inode_id = find_available_block(disk, 0);
    if (inode_id == 0) {
        fprintf(stderr, "%s\n", "No more inodes available");
        exit(1);
    }
    unsigned int dataBlock1 = find_available_block(disk, 1);
    if (dataBlock1 == 0) {
        fprintf(stderr, "%s\n", "No more data blocks available");
        exit(1);
    }

    unsigned int file_size = 0;
    unsigned int file_type = mode; // 0 for directory, 1 for flat file
    memcpy(inode + 0, &file_size, 4);
    memcpy(inode + 4, &file_type, 4);
    memcpy(inode + 8, &dataBlock1, 4);

    printf("inode: %d -- ", inode_id);
    printf("data_block: %d\n", dataBlock1);
    writeBlock(disk, inode_id, inode, 12);
    free(inode);
}

void InitLLFS()
{
    char* buffer;

    /* --- Initialize --- */
    FILE* disk = fopen("vdisk", "rb+");
    char* init = calloc(BLOCK_SIZE * NUM_BLOCKS, 1);
    fwrite(init, BLOCK_SIZE*NUM_BLOCKS, 1, disk);
    free(init);

    /* --- Block 0 --- */
    buffer = (char*) malloc(BLOCK_SIZE);
    unsigned int magic_num = 2019;
    unsigned int num_blocks = 4096;
    unsigned int num_inodes = 126;
    memcpy(buffer + 0, &magic_num, 4);
    memcpy(buffer + 4, &num_blocks, 4);
    memcpy(buffer + 8, &num_inodes, 4);
    writeBlock(disk, 0, buffer, 12);
    free(buffer);

    /* --- Block 1 --- */
    buffer = (char*) malloc(BLOCK_SIZE);
    for (int i = 0; i < BLOCK_SIZE; i++) buffer[i] = (char) 0xFF;
    memset(buffer, 0x3f, 1); // reserved for superblock and bitmap block
    writeBlock(disk, 1, buffer, BLOCK_SIZE);
    free(buffer);

    /* --- Create root directory --- */
    createFile(disk, 0);

    fclose(disk);
}

int main()
{
    InitLLFS();


    return 0;
}
