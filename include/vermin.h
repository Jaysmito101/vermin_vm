#ifndef VERMIN_H
#define VERMIN_H

// vermin macros


#ifndef VERMIN_DISABLE_LOGGING
#define VERMIN_LOG(...) printf(__VA_ARGS__);
#else
#define VERMIN_LOG(...)
#endif

#define VERMIN_STACK_SIZE (1 * 1024 * 1024 * 16)
#define VERMIN_HEAP_SIZE (1 * 1024 * 1024 * 64)

#define VERMIN_INSTRUCTION_NOP      0
#define VERMIN_INSTRUCTION_MOV      1
#define VERMIN_INSTRUCTION_ADD      2
#define VERMIN_INSTRUCTION_SUB      3
#define VERMIN_INSTRUCTION_MUL      4
#define VERMIN_INSTRUCTION_DIV      5
#define VERMIN_INSTRUCTION_INC      6
#define VERMIN_INSTRUCTION_DEC      7
#define VERMIN_INSTRUCTION_XOR      8
#define VERMIN_INSTRUCTION_JMP      9
#define VERMIN_INSTRUCTION_PUSH     10
#define VERMIN_INSTRUCTION_POP      11
#define VERMIN_INSTRUCTION_EXT      12
#define VERMIN_INSTRUCTION_SYSCALL  13

#define VERMIN_TARGET_REG           0
#define VERMIN_TARGET_REG_MEM       1
#define VERMIN_TARGET_MEM           2
#define VERMIN_TARGET_CONST         3
#define VERMIN_TARGET_MEMC          4

#define VERMIN_JMP_TYPE_JMP         0
#define VERMIN_JMP_TYPE_JZ          1
#define VERMIN_JMP_TYPE_JNZ         2
#define VERMIN_JMP_TYPE_JGT         3
#define VERMIN_JMP_TYPE_JLT         4
#define VERMIN_JMP_TYPE_JGE         5
#define VERMIN_JMP_TYPE_JLE         6

#define VERMIN_OP_REG_TO_REG        0
#define VERMIN_OP_CONST_TO_REG      1
#define VERMIN_OP_REG_MEM_TO_REG    2
#define VERMIN_OP_MEM_TO_REG        3
#define VERMIN_OP_REG_TO_REG_MEM    4
#define VERMIN_OP_MEMC_TO_REG       5

#define VERMIN_SYSCALL_WRITE      0
#define VERMIN_SYSCALL_READ       1
#define VERMIN_SYSCALL_FOPEN      2  
#define VERMIN_SYSCALL_FCLOSE     3

// std includes
#include <stdint.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>


// general
#define VERMIN_malloc(x) malloc(x)
#define VERMIN_free(x) free(x)
#define VERMIN_strstartswith(str, pre) return strncmp(pre, str, strlen(pre))


#define VERMIN_DEFINE_PAIR(name, type1, type2) \
    typedef struct \
    { \
        type1 a; \
        type2 b; \
    } name; 

// string
typedef char* string_t;

size_t VERMIN_count_char_in_string(const string_t string, char character);
string_t* VERMIN_split_string(string_t string, char delim, size_t* count);

// assembler

char* VERMIN_assemle(char* code, size_t* binary_size);

// vm

bool VERMIN_execute(char* vermin_binary, size_t read_file_size);

#endif