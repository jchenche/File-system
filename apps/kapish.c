#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <sys/types.h>
#include "../io/File.h"
#include "../disk/diskIO.h"

#define INPUT_SIZE 512
#define MAX_WORDS 20
#define WORD_SIZE 50

void _touch(int argc, char** argv)
{
    if (argc == 1 || argc == 2) fprintf(stdout, "usage: touch [file name] [path]\n");
    else if (Touch(argv[1], argv[2]) == 0) fprintf(stderr, "%s\n", "Create file unsuccessful.");
}

void _rm(int argc, char** argv)
{
    if (argc == 1 || argc == 2) fprintf(stdout, "usage: rm [file name] [path]\n");
    else if (Rm(argv[1], argv[2]) == 0) fprintf(stderr, "%s\n", "Remove file unsuccessful.");
}

void _mkdir(int argc, char** argv)
{
    if (argc == 1 || argc == 2) fprintf(stdout, "usage: mkdir [directory name] [path]\n");
    else if (Mkdir(argv[1], argv[2]) == 0) fprintf(stderr, "%s\n", "Create directory unsuccessful.");
}

void _rmdir(int argc, char** argv)
{
    if (argc == 1 || argc == 2) fprintf(stdout, "usage: rmdir [directory name] [path]\n");
    else if (Rmdir(argv[1], argv[2]) == 0) fprintf(stderr, "%s\n", "Remove directory unsuccessful.");
}

void _cat(int argc, char** argv)
{
    if (argc == 1 || argc == 2) fprintf(stdout, "usage: cat [file name] [path]\n");
    else {
        int file_size = get_size(argv[1], argv[2]);
        char* buffer = (char*) malloc(file_size);
        int rv = Read(argv[1], buffer, file_size, argv[2]);
        if (rv == 0) fprintf(stderr, "%s\n", "Read file unsuccessful.");
        else {
            for(int i = 0; i < file_size; i++) printf("%c", buffer[i]);
            printf("\n");
        }
        free(buffer);
    }
}

void _append(int argc, char** argv)
{
    if (argc == 1 || argc == 2 || argc == 3) fprintf(stdout, "usage: append [src file name] [dest file name] [path]\n");
    else {
        FILE* fp = fopen(argv[1], "rb");
        if (fp == NULL) {
            fprintf(stderr, "File named %s not found.\n", argv[1]);
            return;
        }
        fseek(fp, 0, SEEK_END);
        int size = ftell(fp);
        char* content = (char*) malloc(size);
        fseek(fp, 0, SEEK_SET);
        fread(content, size, 1, fp);
        int rv = Write(argv[2], content, size, argv[3]);
        if (rv == 0) fprintf(stderr, "%s\n", "Write file unsuccessful.");
        free(content);
        fclose(fp);
    }
}

void _ls(int argc, char** argv)
{
    if (argc == 1) {

        FILE* disk = fopen(PATH_TO_VDISK, "rb+");
        char* inodeBuffer = (char*) malloc(BLOCK_SIZE);
        int file_size;
        readBlock(disk, ROOT_INODE, inodeBuffer);
        memcpy(&file_size, inodeBuffer, 4);

        char* buffer = (char*) malloc(file_size);
        readFromFile(disk, buffer, ROOT_INODE, file_size); // assuming root has inode_id 2
        for(int i = 0; i < file_size; i++) {
            if (buffer[i] == '\0') printf(" ");
            else                   printf("%c", buffer[i]);
        }
        printf("\n");

        free(inodeBuffer);
        free(buffer);
        fclose(disk);

    } else if (argc == 2)
        fprintf(stdout, "usage: ls [directory name] [path] (to read root, just type ls)\n");
    else {
        int file_size = get_size(argv[1], argv[2]);
        char* buffer = (char*) malloc(file_size);
        int rv = Read(argv[1], buffer, file_size, argv[2]);
        if (rv == 0) fprintf(stderr, "%s\n", "Read directory unsuccessful.");
        else {
            for(int i = 0; i < file_size; i++) {
                if (buffer[i] == '\0') printf(" ");
                else                   printf("%c", buffer[i]);
            }
            printf("\n");
        }
        free(buffer);
    }
}

void parse_execute(char** tokens, int num_words)
{
    if      (strcmp(*tokens, "init") == 0)  InitLLFS();
    else if (strcmp(*tokens, "touch") == 0)   _touch(num_words, tokens);
    else if (strcmp(*tokens, "rm") == 0)         _rm(num_words, tokens);
    else if (strcmp(*tokens, "mkdir") == 0)   _mkdir(num_words, tokens);
    else if (strcmp(*tokens, "rmdir") == 0)   _rmdir(num_words, tokens);
    else if (strcmp(*tokens, "append") == 0) _append(num_words, tokens);
    else if (strcmp(*tokens, "cat") == 0)       _cat(num_words, tokens);
    else if (strcmp(*tokens, "ls") == 0)         _ls(num_words, tokens);
    else fprintf(stderr, "%s\n", "Wrong command, refer to README.md for commands");
}

char* read_input(FILE* fp)
{
    char* line = malloc(INPUT_SIZE * sizeof(char*));
    if (line == NULL)
    {
        fprintf(stderr, "Memory allocation failed.\n");
        exit(1);
    }
    int c;
    int pos = 0;
    int leading_space_over = 0;

    while (1)
    {

        if (fp == NULL) c = getc(stdin);
        else c = getc(fp);

        if (c == EOF)
        {
            printf("\n");
            return NULL;
        }

        if (c == '\n')
        {
            line[pos] = '\0';
            break;
        }
        else if (c != ' ' || leading_space_over)
        {
            line[pos] = c;
            pos++;
            leading_space_over = 1;
        }

        if (pos >= INPUT_SIZE)
        {
            fprintf(stderr, "Max input size = %d\n", INPUT_SIZE);
            free(line);
            exit(1);
        }
    }

    if (strcmp(line, "exit") == 0) return NULL;

    return line;
}

char** tokenize(char* line, int* num_words)
{
    char** tokens = calloc(INPUT_SIZE, sizeof(char*));
    if (tokens == NULL)
    {
        fprintf(stderr, "Memory allocation failed.\n");
        exit(1);
    }
    char* token = strtok(line, " ");

    while(token != NULL)
    {
        if (*num_words >= MAX_WORDS || strlen(token) >= WORD_SIZE) return NULL;
        tokens[*num_words] = token;
        token = strtok(NULL, " ");
        (*num_words)++;
    }

    return tokens;
}

void wait_for_command()
{
    char* line;
    char** tokens;
    int num_words;

    while(1)
    {   
        num_words = 0;
        printf("? ");

        line = read_input(NULL);
        if (line == NULL) exit(0); /* Control D or exit detected */
        if (strlen(line) == 0) continue; /* Empty input */

        tokens = tokenize(line, &num_words);
        if (tokens == NULL)
        {   
            fprintf(stderr, "ERROR: Max # words = %d and max len words = %d\n", MAX_WORDS, WORD_SIZE);
            continue;
        }

        parse_execute(tokens, num_words);

        free(line);
        free(tokens);
    }

}

void test_commands(char* test_file)
{
    FILE* fp = fopen(test_file, "r");
    if (fp == NULL)
    {
        fprintf(stderr, "Error reading file\n");
        exit(1);
    }

    char* line;
    char** tokens;
    int num_words;

    while(1)
    {   
        num_words = 0;
        printf("? ");

        line = read_input(fp);
        if (line == NULL) break; /* Done reading from file */
        if (strlen(line) == 0) continue; /* Empty input */
        printf("%s\n", line);

        tokens = tokenize(line, &num_words);
        if (tokens == NULL)
        {   
            fprintf(stderr, "ERROR: Max # words = %d and max len words = %d\n", MAX_WORDS, WORD_SIZE);
            continue;
        }

        parse_execute(tokens, num_words);

        free(line);
        free(tokens);
    }

    fclose(fp);
}

int main(int argc, char** argv)
{
    if (argc == 2)
    {
        if (strncmp(argv[1], "--test", 7) == 0) test_commands("test01");
        else fprintf(stdout, "usage: ./kapish --test\n");
    }
    else wait_for_command();
    return 0;
}
