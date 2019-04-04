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

int createFile(FILE* disk, int mode)
{
    char* inode = (char*) malloc(12);
    int inode_id = find_available_block(disk, 0);
    if (inode_id == 0) {
        fprintf(stderr, "%s\n", "No more inodes available");
        return 0;
    }
    int dataBlock1 = find_available_block(disk, 1);
    if (dataBlock1 == 0) {
        fprintf(stderr, "%s\n", "No more data blocks available");
        return 0;
    }

    unsigned int file_size = 0;
    int file_type = mode; // 0 for directory, 1 for flat file
    memcpy(inode + 0, &file_size, 4);
    memcpy(inode + 4, &file_type, 4);
    memcpy(inode + 8, &dataBlock1, 4);

    printf("inode: %d -- ", inode_id);
    printf("data block 1: %d\n\n", dataBlock1);
    writeBlock(disk, inode_id, inode, 12);
    free(inode);
    return inode_id;
}

void writeToFile(FILE* disk, char* data, int inode_id, int size)
{
    char* inodeBuffer = (char*) malloc(BLOCK_SIZE);
    readBlock(disk, inode_id, inodeBuffer);

    /* Find where to write */
    int current_file_size;
    memcpy(&current_file_size, inodeBuffer, 4);
    int dataBlockOffset = current_file_size * 8; // Make it more general

    /* Write file data */
    int fileBlockNumber;
    memcpy(&fileBlockNumber, (inodeBuffer + 8) + dataBlockOffset, 4);
    writeBlock(disk, fileBlockNumber, data, size);

    /* Update file size and data block */
    current_file_size += size;
    memcpy(inodeBuffer, &current_file_size, 4);
    writeBlock(disk, inode_id, inodeBuffer, BLOCK_SIZE);

    free(inodeBuffer);
}

void mkdir(char* name)
{
    FILE* disk = fopen("vdisk", "rb+");
    createFile(disk, 0);
    fclose(disk);
}

void touch(char* name)
{
    FILE* disk = fopen("vdisk", "rb+");
    int inode_id = createFile(disk, 1);

    char* dir_entry = (char*) calloc(32, 1);
    memcpy(dir_entry, &inode_id, 1);
    memcpy(dir_entry + 1, name, strlen(name) + 1);

    writeToFile(disk, dir_entry, 2, 32); // create a dir_entry in root

    free(dir_entry);
    fclose(disk);
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
    memset(buffer, 0x3F, 1); // reserved for superblock and bitmap block
    writeBlock(disk, 1, buffer, BLOCK_SIZE);
    free(buffer);

    /* --- Create root directory --- */
    createFile(disk, 0); // its inode will be in block 2

    fclose(disk);
}

int main()
{
    InitLLFS();
    touch("sample");


    return 0;
}
