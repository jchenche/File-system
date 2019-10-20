#ifndef __diskIO_h__
#define __diskIO_h__

#define BLOCK_SIZE 512
#define NUM_BLOCKS 4096

void readBlock(FILE* disk, int blockNum, char* buffer);
void writeBlock(FILE* disk, int blockNum, char* data);

#endif
