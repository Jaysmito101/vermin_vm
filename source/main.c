#ifdef _WIN32
#define _CRT_SECURE_NO_WARNINGS
#endif

#include "vermin.h"

static void print_help(const char* message, int exit_code)
{
    if(message) if(strlen(message) > 0) printf("Error : %s\n\n", message);
    printf("Help:\n");
    printf("\tUsage : vermin [OPTION] <INPUT-FILE> [OUTPUT-FILE]\n\n");
    printf("\tOptions:\n");
    printf("\t\tdefault\t:\tExecute the input file\n");
    printf("\t\t   -asm\t:\tAssemble the input file\n");
    printf("\n");    
    if(exit_code != 0)
        exit(exit_code);
}

static const char* input_file = "test.asm";
static const char* output_file = "a.verbin";
static bool assemble = false;
static char buffer[1024];

void VERMIN__parse_option(const char* arg)
{
    if(strcmp(&arg[1], "asm") == 0)
    {
        assemble = true;
        output_file = "a.verasm";
    }
    else
    {
        sprintf(buffer, "Invalid option %s", arg);
        print_help(buffer, EXIT_FAILURE);
    }
}


int main(int argc, char** arg, char** envp)
{

    if(argc == 3)
    {
        if(arg[1][0] == '-' && strlen(arg[1]) > 1)
        {
            VERMIN__parse_option(arg[1]);
            input_file = arg[2];
        }
        else
        {
            input_file = arg[1];
            output_file = arg[2];
        }
    }
    else if(argc == 4)
    {
        VERMIN__parse_option(arg[1]);
        input_file = arg[2];
        output_file = arg[3];
    }
    else if(argc == 2)
        input_file = arg[1];
    else
        print_help("Bad arguments", 1);


    if(assemble)
    {
        FILE *f = fopen(input_file, "rb");
        if(!f)
        {
            sprintf(buffer, "Failed to open file %s", input_file);
            print_help(buffer, EXIT_FAILURE);
        }
        fseek(f, 0, SEEK_END);
        long fsize = ftell(f);
        fseek(f, 0, SEEK_SET);
        char *code = VERMIN_malloc(fsize + 2);
        if(!code) print_help("Failed to allocate memory", EXIT_FAILURE);
        fread(code, fsize, 1, f);
        fclose(f);
        code[fsize - 1] = '\n';
        code[fsize] = 0;    
        printf("Assembling file %s to %s\n\n", input_file, output_file);
        size_t binary_size = 0;
        char* vermin_binary = VERMIN_assemle(code, &binary_size);
        VERMIN_free(code);   
        if(vermin_binary)
        {
            FILE* write_ptr = NULL;
            write_ptr = fopen(output_file, "wb");
            fwrite(vermin_binary, binary_size, 1, write_ptr);
            fclose(write_ptr);
            VERMIN_free(vermin_binary);
        }
    }
    else
    {
        char* vermin_binary = NULL;
        FILE* read_file = NULL;
        read_file = fopen(input_file, "rb");
        if(read_file)
        {
            fseek(read_file, 0, SEEK_END);
            long read_file_fsize = ftell(read_file);
            fseek(read_file, 0, SEEK_SET);
            char* vermin_binary = VERMIN_malloc(read_file_fsize);
            if(!vermin_binary)
                print_help("Failed to allocate memory", EXIT_FAILURE);
            fread(vermin_binary, read_file_fsize, 1, read_file);
            fclose(read_file);
            VERMIN_execute(vermin_binary, read_file_fsize);            
            VERMIN_free(vermin_binary);
        }
        else
        {
            sprintf(buffer, "Failed to open %s", input_file);
            print_help(buffer, EXIT_FAILURE);
        }
    }
    return EXIT_SUCCESS;
}