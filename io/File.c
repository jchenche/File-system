#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define BLOCK_SIZE 512
#define NUM_BLOCKS 4096
#define ROOT_INODE 2

void readBlock(FILE* disk, int blockNum, char* buffer)
{
    fseek(disk, blockNum * BLOCK_SIZE, SEEK_SET);
    fread(buffer, BLOCK_SIZE, 1, disk);
}

void writeBlock(FILE* disk, int blockNum, char* data)
{
    fseek(disk, blockNum * BLOCK_SIZE, SEEK_SET);
    fwrite(data, BLOCK_SIZE, 1, disk);
}

short find_bit_one(int c)
{
    /* Given a byte, find the first 1 bit starting from the left */
    int mask = 0x100;
    for (short i = 0; i < 8; i++) {
        mask = mask >> 1;
        if ((mask & c) == mask) return i;
    }
    return -1;
}

short find_available_block(FILE* disk, int data_type)
{
    int c, lower_bound, upper_bound;
    short bit_one_num;
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
            buffer[i] = c & (~(0x80 >> bit_one_num)); // set to 0 now
            writeBlock(disk, 1, buffer);
            free(buffer);
            return i * 8 + bit_one_num;
        }
    }

    free(buffer);
    return 0; // means no available blocks
}

void deallocate_block(FILE* disk, short blockNum)
{
    int byte_num = blockNum / 8;
    int bit_num = blockNum % 8;
    char* buffer = (char*) malloc(BLOCK_SIZE);
    readBlock(disk, 1, buffer);
    buffer[byte_num] = (buffer[byte_num]) | (0x80 >> bit_num);
    writeBlock(disk, 1, buffer);
    free(buffer);
}

int writeToFile(FILE* disk, char* data, short inode_id, int size)
{
    char* buffer = (char*) malloc(BLOCK_SIZE);
    char* inodeBuffer = (char*) malloc(BLOCK_SIZE);
    readBlock(disk, inode_id, inodeBuffer);

    /* --- Find where to begin to write --- */
    int current_file_size;
    memcpy(&current_file_size, inodeBuffer, 4);
    int dataBlockOffset = (int) (current_file_size / BLOCK_SIZE);
    short fileBlockNumber;
    memcpy(&fileBlockNumber, (inodeBuffer + 8) + 2 * dataBlockOffset, 2);

    /* Some useful numbers */
    int last_block_bytes_left = BLOCK_SIZE - (current_file_size % BLOCK_SIZE);
    int remaining_size = size - last_block_bytes_left;

    /* --- Write file data to last block --- */
    readBlock(disk, fileBlockNumber, buffer);
    if (remaining_size < 0) {
        memcpy(buffer + (current_file_size % BLOCK_SIZE), data, size);
        writeBlock(disk, fileBlockNumber, buffer);
    }
    else {
        memcpy(buffer + (current_file_size % BLOCK_SIZE), data, last_block_bytes_left);
        writeBlock(disk, fileBlockNumber, buffer);
    }

    /* --- Write file data to new blocks --- */
    if (remaining_size >= 0) {
        data += last_block_bytes_left; // remaining data
        int num_new_blocks = ((int) (remaining_size / BLOCK_SIZE)) + 1;

        for(int i = 1; i <= num_new_blocks; i++) {

            short newDataBlock = find_available_block(disk, 1);
            if (newDataBlock == 0) {
                fprintf(stderr, "%s\n", "No more data blocks available");
                return 0;
            }
            memcpy((inodeBuffer + 8) + 2 * (dataBlockOffset + i), &newDataBlock, 2);

            if (remaining_size > 0) {
                if (i != num_new_blocks) {
                    memcpy(buffer, data, BLOCK_SIZE);
                    writeBlock(disk, newDataBlock, buffer);
                    data += BLOCK_SIZE;
                    remaining_size -= BLOCK_SIZE;
                } else {
                    memcpy(buffer, data, remaining_size);
                    writeBlock(disk, newDataBlock, buffer);
                }
            }

        }
    }

    /* --- Update file size and block status --- */
    current_file_size += size;
    memcpy(inodeBuffer, &current_file_size, 4);
    writeBlock(disk, inode_id, inodeBuffer);

    free(inodeBuffer);
    free(buffer);
    return size;
}

int readFromFile(FILE* disk, char* data, short inode_id, int size)
{
    char* buffer = (char*) malloc(BLOCK_SIZE);
    char* inodeBuffer = (char*) malloc(BLOCK_SIZE);
    readBlock(disk, inode_id, inodeBuffer);

    /* --- Find where to stop reading --- */
    int current_file_size;
    memcpy(&current_file_size, inodeBuffer, 4);
    if (current_file_size < size) size = current_file_size;
    int lastDataBlock = (int) (size / BLOCK_SIZE);

    /* Read file data from blocks */
    short fileBlockNumber;
    for(int i = 0; i <= lastDataBlock; i++) {

        memcpy(&fileBlockNumber, (inodeBuffer + 8) + 2 * i, 2);
        if (size > 0) {
            if (i != lastDataBlock) {
                readBlock(disk, fileBlockNumber, buffer);
                memcpy(data, buffer, BLOCK_SIZE);
                data += BLOCK_SIZE;
                size -= BLOCK_SIZE;
            } else {
                readBlock(disk, fileBlockNumber, buffer);
                memcpy(data, buffer, size);
            }
        }

    }

    free(inodeBuffer);
    free(buffer);
    return size;
}

int get_file_size(FILE* disk, short inode_id)
{
    char* inodeBuffer = (char*) malloc(BLOCK_SIZE);
    readBlock(disk, inode_id, inodeBuffer);

    int current_file_size;
    memcpy(&current_file_size, inodeBuffer, 4);

    free(inodeBuffer);
    return current_file_size;
}

short find_inode(FILE* disk, char* name, short directory_inode)
{
    /* Find the inode of a file in a given directory */

    int size = get_file_size(disk, directory_inode);
    char* buffer = (char*) malloc(size);
    readFromFile(disk, buffer, directory_inode, size);

    short inode_id = 0;
    for(int i = 0; i < size; ) {
        if (memcmp(buffer + 1, name, strlen(name) + 1) == 0) {
            memcpy(&inode_id, buffer, 1);
            break;
        }
        i += 32;
        buffer += 32;
    }

    return inode_id;
}

int is_flat_file(FILE* disk, short inode_id)
{
    char* inodeBuffer = (char*) malloc(BLOCK_SIZE);
    readBlock(disk, inode_id, inodeBuffer);

    int file_type;
    memcpy(&file_type, inodeBuffer + 4, 4);

    free(inodeBuffer);
    return file_type;
}

short walk_path(FILE* disk, char* _path)
{
    char* path = (char*) malloc(strlen(_path) + 1);
    memcpy(path, _path, strlen(_path) + 1);

    short directory_inode = ROOT_INODE; // start walking from root
    char* token = strtok(path, "/");
    while(token != NULL) {
        directory_inode = find_inode(disk, token, directory_inode);
        if (directory_inode == 0) {
            fprintf(stderr, "Directory named %s doesn't exist in %s\n", token, path);
            free(path);
            return 0;
        }
        if (is_flat_file(disk, directory_inode)) {
            fprintf(stderr, "%s is not a directory\n", token);
            free(path);
            return 0;
        }
        token = strtok(NULL, "/");
    }

    free(path);
    return directory_inode;
}

short find_file_inode(FILE* disk, char* name, char* path)
{
    short directory_inode = walk_path(disk, path); // dir that contains the file
    if (directory_inode == 0) return 0;
    return find_inode(disk, name, directory_inode);
}

short find_file_inode_with_parent(FILE* disk, char* name, char* path, short* parent_dir_inode)
{
    short directory_inode = walk_path(disk, path); // dir that contains the file
    if (directory_inode == 0) return 0;
    *parent_dir_inode = directory_inode;
    return find_inode(disk, name, directory_inode);
}

short Write(char* name, char* data, int size, char* path) {
    FILE* disk = fopen("vdisk", "rb+");

    short inode_id = find_file_inode(disk, name, path);
    if (inode_id == 0) {
        fprintf(stderr, "File %s doesn't exist in %s\n", name, path);
        fclose(disk);
        return 0;
    }
    writeToFile(disk, data, inode_id, size);

    fclose(disk);
    return inode_id;
}

short Read(char* name, char* buffer, int size, char* path) {
    FILE* disk = fopen("vdisk", "rb+");

    short inode_id = find_file_inode(disk, name, path);
    if (inode_id == 0) {
        fprintf(stderr, "File %s doesn't exist in %s\n", name, path);
        fclose(disk);
        return 0;
    }
    readFromFile(disk, buffer, inode_id, size);

    fclose(disk);
    return inode_id;
}

int name_collision(FILE* disk, short directory_inode, char* name)
{
    if (find_inode(disk, name, directory_inode) == 0) {
        return 0;
    } else {
        fprintf(stderr, "There's a name collision with %s\n", name);
        return 1;
    }
}

short createFile(FILE* disk, char* name, int type, char* path)
{
    /* --- Allocate blocks --- */
    short inode_id = find_available_block(disk, 0);
    if (inode_id == 0) {
        fprintf(stderr, "%s\n", "No more inode blocks available");
        return 0;
    }
    short dataBlock1 = find_available_block(disk, 1);
    if (dataBlock1 == 0) {
        fprintf(stderr, "%s\n", "No more data blocks available");
        deallocate_block(disk, inode_id);
        return 0;
    }

    /* --- Create a dir entry in the given dir --- */
    if ((memcmp(name, "/", 2) != 0)) { // root dir doesn't need the code below
        short directory_inode = walk_path(disk, path);
        if (directory_inode == 0 || name_collision(disk, directory_inode, name)) {
            deallocate_block(disk, inode_id);
            deallocate_block(disk, dataBlock1);
            return 0;
        }
        char* dir_entry = (char*) calloc(32, 1);
        memcpy(dir_entry, &inode_id, 1);
        memcpy(dir_entry + 1, name, strlen(name) + 1);
        writeToFile(disk, dir_entry, directory_inode, 32); // create a dir entry in root. 
        free(dir_entry);
    }

    /* --- Insert default inode data --- */
    char* inode = (char*) malloc(10);
    int file_size = 0;
    int file_type = type; // 0 for directory, 1 for flat file
    memcpy(inode + 0, &file_size, 4);
    memcpy(inode + 4, &file_type, 4);
    memcpy(inode + 8, &dataBlock1, 2);
    writeBlock(disk, inode_id, inode);
    free(inode);

    return inode_id;
}

short deleteFile(FILE* disk, char* name, int type, char* path)
{
    if (memcmp(name, "/", 2) == 0) {
        fprintf(stderr, "%s\n", "Can't delete root directory");
        return 0;
    }

    short parent_dir_inode = ROOT_INODE;
    short inode_id = find_file_inode_with_parent(disk, name, path, &parent_dir_inode);
    if (inode_id == 0) {
        fprintf(stderr, "File %s doesn't exist in %s\n", name, path);
        return 0;
    }

    char* inodeBuffer = (char*) malloc(BLOCK_SIZE);
    int file_size;
    int file_type;
    short fileBlockNumber;
    int lastDataBlock;

    readBlock(disk, inode_id, inodeBuffer);
    memcpy(&file_size, inodeBuffer, 4);
    memcpy(&file_type, inodeBuffer + 4, 4);

    /* Check if it's a directory or a flat file */
    if (type == 0) {
        if (file_type != type) {
            fprintf(stderr, "%s is not a directory\n", name);
            free(inodeBuffer);
            return 0;
        }
        if (file_size != 0) {
            fprintf(stderr, "The directory %s contains files\n", name);
            free(inodeBuffer);
            return 0;
        }
    } else {
        if (file_type != type) {
            fprintf(stderr, "%s is a directory\n", name);
            free(inodeBuffer);
            return 0;
        }
    }

    /* Deallocate the corresponding data blocks and its inode block */
    lastDataBlock = (int) (file_size / BLOCK_SIZE);
    for(int i = 0; i <= lastDataBlock; i++) {
        memcpy(&fileBlockNumber, (inodeBuffer + 8) + 2 * i, 2);
        deallocate_block(disk, fileBlockNumber);
    }
    deallocate_block(disk, inode_id);

    /* Delete the corresponding entry in the parent dir */
    int dir_file_size;
    readBlock(disk, parent_dir_inode, inodeBuffer);
    memcpy(&dir_file_size, inodeBuffer, 4);

    char* buffer = (char*) malloc(dir_file_size);
    readFromFile(disk, buffer, parent_dir_inode, dir_file_size);

    char* temp = buffer;
    for(int i = 0; i < dir_file_size; ) {
        if (memcmp(temp + 1, name, strlen(name) + 1) == 0) {
            memcpy(temp, temp + 32, dir_file_size - 32 * (i + 1)); // delete by shifting left
            break;
        }
        i += 32;
        temp += 32;
    }

    /* Rewrite entries in the parent dir */
    lastDataBlock = (int) (dir_file_size / BLOCK_SIZE);
    for(int i = 1; i <= lastDataBlock; i++) { // deallocate all blocks except the first one
        memcpy(&fileBlockNumber, (inodeBuffer + 8) + 2 * i, 2);
        deallocate_block(disk, fileBlockNumber);
    }

    int zero = 0;
    memcpy(inodeBuffer, &zero, 4); // make the file size 0 for rewrite
    writeBlock(disk, parent_dir_inode, inodeBuffer);

    dir_file_size -= 32;
    writeToFile(disk, buffer, parent_dir_inode, dir_file_size); // rewrite


    free(buffer);
    free(inodeBuffer);
    return inode_id;
}

short Rmdir(char* name, char* path)
{
    FILE* disk = fopen("vdisk", "rb+");
    short inode_id = deleteFile(disk, name, 0, path);
    fclose(disk);
    if (inode_id == 0) return 0;
    return inode_id;
}

short Rm(char* name, char* path)
{
    FILE* disk = fopen("vdisk", "rb+");
    short inode_id = deleteFile(disk, name, 1, path);
    fclose(disk);
    if (inode_id == 0) return 0;
    return inode_id;
}

short Mkdir(char* name, char* path)
{
    FILE* disk = fopen("vdisk", "rb+");
    short inode_id = createFile(disk, name, 0, path);
    fclose(disk);
    if (inode_id == 0) return 0;
    return inode_id;
}

short Touch(char* name, char* path)
{
    FILE* disk = fopen("vdisk", "rb+");
    short inode_id = createFile(disk, name, 1, path);
    fclose(disk);
    if (inode_id == 0) return 0;
    return inode_id;
}

int get_size(char* name, char* path) // Testing function
{
    FILE* disk = fopen("vdisk", "rb+");
    short inode_id = find_file_inode(disk, name, path);
    if (inode_id == 0) {
        fprintf(stderr, "File %s doesn't exist in %s\n", name, path);
        fclose(disk);
        return 0;
    }
    char* inodeBuffer = (char*) malloc(BLOCK_SIZE);
    readBlock(disk, inode_id, inodeBuffer);
    int current_file_size;
    memcpy(&current_file_size, inodeBuffer, 4);
    free(inodeBuffer);
    fclose(disk);
    return current_file_size;
}

void InitLLFS()
{
    /* --- Initialize --- */
    FILE* disk = fopen("vdisk", "wb");
    char* init = calloc(BLOCK_SIZE * NUM_BLOCKS, 1);
    fwrite(init, BLOCK_SIZE*NUM_BLOCKS, 1, disk);
    free(init);
    fclose(disk);

    disk = fopen("vdisk", "rb+");
    char* buffer;

    /* --- Block 0 --- */
    buffer = (char*) malloc(BLOCK_SIZE);
    int magic_num = 2019;
    int num_blocks = 4096;
    int num_inodes = 126;
    memcpy(buffer + 0, &magic_num, 4);
    memcpy(buffer + 4, &num_blocks, 4);
    memcpy(buffer + 8, &num_inodes, 4);
    writeBlock(disk, 0, buffer);
    free(buffer);

    /* --- Block 1 --- */
    buffer = (char*) malloc(BLOCK_SIZE);
    for (int i = 0; i < BLOCK_SIZE; i++) buffer[i] = (char) 0xFF;
    memset(buffer, 0x3F, 1); // reserved for superblock and bitmap block
    writeBlock(disk, 1, buffer);
    free(buffer);

    /* --- Create root directory --- */
    createFile(disk, "/", 0, NULL); // its inode_id will be ROOT_INODE

    fclose(disk);
}

int main()
{
    printf("\n");
    InitLLFS();

    Mkdir("var", "/");
    Touch("first_file.txt", "/var/tmp/");
    char* path = "/var";
    Touch("sample_file", path);
    Touch("hello", path);

    printf("\n");

    FILE* fp = fopen("sample", "rb");
    fseek(fp, 0, SEEK_END);
    int size = ftell(fp);
    char* content = (char*) malloc(size);
    fseek(fp, 0, SEEK_SET);
    fread(content, size, 1, fp);

    Rm("var", "/");

    Write("sample_file", content, size, path);
    Write("sample_file", "- by Jimmy Chen Chen", 20, path);
    Rm("sample_file", path);

    int file_size = get_size("sample_file", path);
    char* buffer = (char*) malloc(file_size + 1);
    Read("sample_file", buffer, file_size, path);
    buffer[file_size] = '\0';
    printf("%s\n", buffer);


    Rmdir("var", "/");
    

    Touch("sample_file2", path);
    Write("sample_file2", "Inserted after deletion!!!", 26, path);

    file_size = get_size("sample_file2", path);
    buffer = (char*) malloc(file_size + 1);
    Read("sample_file2", buffer, file_size, path);
    buffer[file_size] = '\0';
    printf("%s\n", buffer);


    free(buffer);
    free(content);
    fclose(fp);
    return 0;
}
