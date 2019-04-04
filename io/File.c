#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define BLOCK_SIZE 512
#define NUM_BLOCKS 4096
#define INODE_SIZE 160

int ceiling(float number)
{
    int int_num = (int) number;
    if (number == int_num) return (int) number;
    else                   return (int) (number + 1);
}

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

int writeToFile(FILE* disk, char* data, int inode_id, int size)
{
    char* buffer = (char*) malloc(BLOCK_SIZE);
    char* inodeBuffer = (char*) malloc(BLOCK_SIZE);
    readBlock(disk, inode_id, inodeBuffer);

    /* --- Find where to write --- */
    int current_file_size;
    memcpy(&current_file_size, inodeBuffer, 4);

    int dataBlockOffset = (int) current_file_size / BLOCK_SIZE;
    int last_block_bytes_left = BLOCK_SIZE - (current_file_size % BLOCK_SIZE);
    int remaining_size = size - last_block_bytes_left;

    /* --- Write file data to last block --- */
    int fileBlockNumber;
    memcpy(&fileBlockNumber, (inodeBuffer + 8) + 4 * dataBlockOffset, 4);
    readBlock(disk, fileBlockNumber, buffer);
    if (remaining_size < 0) {
        memcpy(buffer + (current_file_size % BLOCK_SIZE), data, size);
        writeBlock(disk, fileBlockNumber, buffer, BLOCK_SIZE);
    }
    else {
        memcpy(buffer + (current_file_size % BLOCK_SIZE), data, last_block_bytes_left);
        writeBlock(disk, fileBlockNumber, buffer, BLOCK_SIZE);
    }

    /* --- Write file data to new blocks --- */
    if (remaining_size >= 0) {
        data += last_block_bytes_left;
        buffer = (char*) calloc(BLOCK_SIZE, 1);
        int num_new_blocks = (int) (ceiling((float) remaining_size / BLOCK_SIZE));

        for(int i = 1; i <= num_new_blocks; i++) {

            int newDataBlock = find_available_block(disk, 1);
            if (newDataBlock == 0) {
                fprintf(stderr, "%s\n", "No more data blocks available");
                return 0;
            }
            printf("new data block: %d\n\n", newDataBlock);
            memcpy((inodeBuffer + 8) + 4 * (dataBlockOffset + i), &newDataBlock, 4);
            memcpy(buffer, data + i - 1, size);
            if (remaining_size > 0)
                writeBlock(disk, newDataBlock, buffer, remaining_size);

            remaining_size -= BLOCK_SIZE;

        }
    }

    /* --- Update file size and block status --- */
    current_file_size += size;
    memcpy(inodeBuffer, &current_file_size, 4);
    writeBlock(disk, inode_id, inodeBuffer, BLOCK_SIZE);

    free(inodeBuffer);
    free(buffer);
    return 1;
}

int createFile(FILE* disk, char* name, int mode)
{
    char* inode = (char*) malloc(12);

    /* --- Find available blocks --- */
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

    /* --- Insert default inode data --- */
    unsigned int file_size = 0;
    int file_type = mode; // 0 for directory, 1 for flat file
    memcpy(inode + 0, &file_size, 4);
    memcpy(inode + 4, &file_type, 4);
    memcpy(inode + 8, &dataBlock1, 4);

    printf("inode: %d -- ", inode_id);
    printf("data block 1: %d\n\n", dataBlock1);
    writeBlock(disk, inode_id, inode, 12);
    free(inode);

    /* --- Create a dir entry in the given dir --- */
    if (inode_id != 2) {
        // root dir doesn't need the code below
        char* dir_entry = (char*) calloc(32, 1);
        memcpy(dir_entry, &inode_id, 1);
        memcpy(dir_entry + 1, name, strlen(name) + 1);
        writeToFile(disk, dir_entry, 2, 32); // create a dir entry in root
        free(dir_entry);
    }

    return inode_id;
}

int find_file_inode(FILE* disk, int blockNumk, char* name)
{
    return 0;
}

int Write(char* name, char* data) {
    FILE* disk = fopen("vdisk", "rb+");
    char* inodeBuffer = (char*) malloc(BLOCK_SIZE);
    readBlock(disk, 2, inodeBuffer);

    int inode_id = find_file_inode(disk, 2, name);
    writeToFile(disk, data, 3, strlen(data)); // generalize this 3 ot inode_id

    free(inodeBuffer);
    fclose(disk);
    return inode_id;
}

int Mkdir(char* name)
{
    FILE* disk = fopen("vdisk", "rb+");
    int inode_id = createFile(disk, name, 0);
    if (inode_id == 0) return 0;
    fclose(disk);
    return inode_id;
}

int Touch(char* name)
{
    FILE* disk = fopen("vdisk", "rb+");
    int inode_id = createFile(disk, name, 1);
    if (inode_id == 0) return 0;
    fclose(disk);
    return inode_id;
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
    int magic_num = 2019;
    int num_blocks = 4096;
    int num_inodes = 126;
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
    createFile(disk, NULL, 0); // its inode will be in block 2

    fclose(disk);
}

int main()
{
    InitLLFS();
    Touch("sample");
    printf("\n");

    for(int i = 0; i < 50; i++) Write("sample", "helloworld");
    Write("sample", "abcdefghijklmnopqrstuvwxyz");
    Write("sample", "by Jimmy Chen Chen");

    return 0;
}
