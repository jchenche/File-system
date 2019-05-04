#ifndef __diskIO_h__
#define __diskIO_h__

void readBlock(FILE* disk, int blockNum, char* buffer);
void writeBlock(FILE* disk, int blockNum, char* data);

#endif
