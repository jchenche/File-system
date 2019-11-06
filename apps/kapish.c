#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <pwd.h>
#include "../io/File.h"
#include "../disk/diskIO.h"

#define INPUT_SIZE 512
#define MAX_WORDS 20
#define WORD_SIZE 50
#define TEST_FILE "tests.txt"

void _init(int argc, char** argv);
void _touch(int argc, char** argv);
void _rm(int argc, char** argv);
void _mkdir(int argc, char** argv);
void _rmdir(int argc, char** argv);
void _append(int argc, char** argv);
void _cat(int argc, char** argv);
void _ls(int argc, char** argv);
void _clear(int argc, char** argv);

char* command_str[] = {
    "init",
    "touch",
    "rm",
    "mkdir",
    "rmdir",
    "append",
    "cat",
    "ls",
    "clear"
};
void (*command_func[]) (int, char**) = {
    &_init,
    &_touch,
    &_rm,
    &_mkdir,
    &_rmdir,
    &_append,
    &_cat,
    &_ls,
    &_clear
};
int num_commands()
{
    return sizeof(command_str) / sizeof(char*);
}

void _init(int argc, char** argv)
{
    InitLLFS();
}

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
    if (argc == 1 || argc == 2 || argc == 3)
        fprintf(stdout, "usage: append [src file name] [dest file name] [path]\n");
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

void _clear(int argc, char** argv)
{
	pid_t p = fork();
	if (p == 0) execvp(*argv, argv); /* Child */
	else if (p > 0) wait(NULL);
}

void parse_execute(char** tokens, int num_words)
{
    for(int i = 0; i < num_commands(); i++) {
        if (strcmp(*tokens, command_str[i]) == 0) {
            (*command_func[i])(num_words, tokens);
            return;
        }
    }
    fprintf(stderr, "%s\n", "Wrong command, refer to README.md for commands");
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

    while (1)
    {
        if (fp == NULL) c = getc(stdin);
        else            c = getc(fp);

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

        line[pos++] = c;

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

void test_commands()
{
    FILE* fp = fopen(TEST_FILE, "r");
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
        if (strncmp(argv[1], "--test", 7) == 0) test_commands();
        else fprintf(stdout, "usage: ./kapish (or ./kapish --test)\n");
    }
    else if (argc > 2) fprintf(stdout, "usage: ./kapish (or ./kapish --test)\n");
    else wait_for_command();
    return 0;
}
