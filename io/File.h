#ifndef __File_h__
#define __File_h__

#define ROOT_INODE 2
#define PATH_TO_VDISK "../disk/vdisk"

// Internal library
short find_bit_one(int c);
short find_available_block(FILE* disk, int data_type);
void  deallocate_block(FILE* disk, short blockNum);
int   writeToFile(FILE* disk, char* data, short inode_id, int size);
int   readFromFile(FILE* disk, char* data, short inode_id, int size);
int   get_file_size(FILE* disk, short inode_id);
short find_inode(FILE* disk, char* name, short directory_inode);
int   is_flat_file(FILE* disk, short inode_id);
short walk_path(FILE* disk, char* _path);
short find_file_inode(FILE* disk, char* name, char* path);
short find_file_inode_with_parent(FILE* disk, char* name, char* path, short* parent_dir_inode);
int   name_collision(FILE* disk, short directory_inode, char* name);
void  file_system_check(FILE* disk);
short createFile(FILE* disk, char* name, int type, char* path);
short deleteFile(FILE* disk, char* name, int type, char* path);

// The API
short Read(char* name, char* buffer, int size, char* path);
short Write(char* name, char* data, int size, char* path);
short Rmdir(char* name, char* path);
short Rm(char* name, char* path);
short Mkdir(char* name, char* path);
short Touch(char* name, char* path);
void  InitLLFS();
int   get_size(char* name, char* path);

#endif
