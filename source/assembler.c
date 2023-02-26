#ifdef _WIN32
#define _CRT_SECURE_NO_WARNINGS
#endif

#include "vermin.h"

#define PARSER_TYPE_MARO 0
#define PARSER_TYPE_CODE 1
#define PARSER_TYPE_DATA 2


typedef struct
{
    char name[256];
    char value[256];
} macro_t;

typedef struct
{
    char name[256];
    uint64_t offset;
} label_t;


typedef struct 
{
    uint8_t id;
    uint8_t size;
    uint16_t subparams;
    uint8_t params_count;
    uint32_t params[4];
    size_t offset;
    char name[128];
} instruction_t;


static char buffer[4096];
static char current_line[3][1024];
static size_t instruction_line_count = 0;
static uint8_t current_parser_type = PARSER_TYPE_MARO;
static instruction_t current_instruction;
static bool skip_instruction = false;
static char* data_section = NULL;
static size_t data_section_offset = 0;
static char* final_binary = NULL;
static size_t final_binary_size = 0;
static struct
{
    label_t* data;
    size_t count;
} labels;
static struct 
{
    macro_t* data;
    size_t count;
} macros;
static struct
{
    instruction_t* data;
    size_t count;
} instructions;
static struct
{
    char data[128][256];
    size_t count;
} tmp_strings;


static void VERMIN__assembler_init()
{
    macros.count = 0;
    macros.data = (macro_t*)VERMIN_malloc(sizeof(macro_t) * instruction_line_count);
    memset(macros.data, 0, sizeof(macro_t) * instruction_line_count);

    labels.count = 0;
    labels.data = (label_t*)VERMIN_malloc(sizeof(label_t) * instruction_line_count);
    memset(labels.data, 0, sizeof(label_t) * instruction_line_count);

    instructions.count = 0;
    instructions.data = (instruction_t*)VERMIN_malloc(sizeof(instruction_t) * instruction_line_count);
    memset(instructions.data, 0, sizeof(instruction_t) * instruction_line_count);

    tmp_strings.count = 0;
    memset(tmp_strings.data, 0, sizeof(tmp_strings.data));

    data_section = (char*)VERMIN_malloc(1 * 1024 * 1024 * 16);
    memset(data_section, 0, 1 * 1024 * 1024 * 16);
}

static void VERMIN__assembler_destroy()
{
    VERMIN_free(macros.data);
    VERMIN_free(labels.data);
    VERMIN_free(instructions.data);
    VERMIN_free(data_section);
}

static bool VERMIM__assembler_parse_reg(const string_t code, uint8_t* type, uint32_t* value)
{
    static char temp_buffer[256];
    int reg = atoi(code + 1);
    if(reg >= 0 && reg <= 15)
    {
        *type = VERMIN_TARGET_REG;
        uint8_t* value_ui = (uint8_t*)value;
        value_ui[0] = (uint8_t)reg;
        value_ui[1] = sizeof(uint64_t);
        value_ui[2] = 0;
        value_ui[3] = 0;
        sprintf(temp_buffer, "R%d", reg);
        if(strlen(code) == (strlen(temp_buffer) + 2))
        {
            value_ui[2] = (code[strlen(code) - 2] == 'U') ? 1 : 0;
            char reg_part = code[strlen(code) - 1];
            if(reg_part == 'H')
                value_ui[1] = sizeof(uint32_t);
            else if(reg_part == 'Q')
                value_ui[1] = sizeof(uint16_t);
            else if(reg_part == 'T')
                value_ui[1] = sizeof(uint8_t);
            else
            {
                VERMIN_LOG("Invalid registor portion\n");
                return false;
            }
            value_ui[3] = value_ui[2] * value_ui[1];
        }
    }
    else
    {
        VERMIN_LOG("Invalid register %d\n", reg);
        return false;
    }
    return true;
}

static bool VERMIM__assembler_parse_reg_or_mem_or_const(const string_t code, uint8_t* type, uint32_t* value)
{
    static char temp_buffer[256];
    if(code[0] == 'R')
        return VERMIM__assembler_parse_reg(code, type, value);
    else if(code[0] == '@')
    {
        *type = VERMIN_TARGET_MEM;
        *value = (uint32_t)tmp_strings.count;
        sprintf(tmp_strings.data[tmp_strings.count++], "%s", (code + 1));
    }
    else if(code[0] == '$')
    {
        *type = VERMIN_TARGET_MEMC;
        *value = (uint32_t)tmp_strings.count;
        sprintf(tmp_strings.data[tmp_strings.count++], "%s", (code + 1));
    }
    else if(code[0] == '&')
    {
        sprintf(temp_buffer, "%s", (code + 1));
        bool success = VERMIM__assembler_parse_reg(temp_buffer, type, value);
        *type = VERMIN_TARGET_REG_MEM;
    }
    else
    {
        *type = VERMIN_TARGET_CONST;
        int64_t val = atoll(code);
        int32_t val32 = (int32_t)val;
        memcpy(value, &val32, sizeof(int32_t));
    }
    return true;
}

static bool VERMIN__assembler_instruction_p1_to_p2_classify(uint8_t p1, uint8_t p2, uint16_t* subparams)
{
    if(p1 == VERMIN_TARGET_REG && p2 == VERMIN_TARGET_REG)
        *subparams = VERMIN_OP_REG_TO_REG;
    else if(p1 == VERMIN_TARGET_CONST && p2 == VERMIN_TARGET_REG)
        *subparams = VERMIN_OP_CONST_TO_REG;
    else if(p1 == VERMIN_TARGET_REG_MEM && p2 == VERMIN_TARGET_REG)
        *subparams = VERMIN_OP_REG_MEM_TO_REG;
    else if(p1 == VERMIN_TARGET_MEM && p2 == VERMIN_TARGET_REG)
        *subparams = VERMIN_OP_MEM_TO_REG;
    else if(p1 == VERMIN_TARGET_MEMC && p2 == VERMIN_TARGET_REG)
        *subparams = VERMIN_OP_MEMC_TO_REG;
    else if(p1 == VERMIN_TARGET_REG && p2 == VERMIN_TARGET_REG_MEM)
        *subparams = VERMIN_OP_REG_TO_REG_MEM;
    else
    {
        VERMIN_LOG("Invalid param combination\n");
        return false;
    }
    return true;
}

static bool VERMIN__assembler_parse_double_param()
{
    current_instruction.params_count = 2;
    if(!(strlen(current_line[1]) > 1 && strlen(current_line[2]) >= 1))
    {
        VERMIN_LOG("Invalid params\n");
        return false;
    }
    uint8_t p1 = VERMIN_TARGET_REG;
    uint8_t p2 = VERMIN_TARGET_REG;
    if(!VERMIM__assembler_parse_reg_or_mem_or_const(current_line[1],  &p1, &current_instruction.params[0])
    || !VERMIM__assembler_parse_reg_or_mem_or_const(current_line[2],  &p2, &current_instruction.params[1]))
    {
        VERMIN_LOG("Failed to parse param\n");    
        return false;
    }
    return VERMIN__assembler_instruction_p1_to_p2_classify(p2, p1, &current_instruction.subparams);
}

static bool VERMIN__assembler_parse_nop()
{
    current_instruction.id = VERMIN_INSTRUCTION_NOP;
    current_instruction.size = sizeof(uint32_t);
    current_instruction.params_count = 0;
    current_instruction.subparams = 0;
    return true;
}

static bool VERMIN__assembler_parse_xor()
{
    current_instruction.id = VERMIN_INSTRUCTION_XOR;
    current_instruction.size = sizeof(uint32_t) * 3;
    bool res = VERMIN__assembler_parse_double_param();
    if(current_instruction.subparams == VERMIN_OP_REG_TO_REG_MEM)
    {
        VERMIN_LOG("Target of operation must be  registor\n");
        return false;
    }
    return res;
}

static bool VERMIN__assembler_parse_mov()
{
    current_instruction.id = VERMIN_INSTRUCTION_MOV;
    current_instruction.size = sizeof(uint32_t) * 3;
    bool res = VERMIN__assembler_parse_double_param();
    if(!res)
        return false;
    uint8_t subparams = (uint8_t)current_instruction.subparams;
    uint8_t* ci_subparams = (uint8_t*)(&current_instruction.subparams);
    ci_subparams[0] = subparams;
    uint8_t mov_size = 4; // sizeof(uint32_t)
    if(strlen(current_line[0]) > 3)
        mov_size = (uint8_t)(current_line[0][3]) - 48;
    if(mov_size != 1 && mov_size != 2 && mov_size != 4 && mov_size != 8)
        mov_size = 4;
    ci_subparams[1] = mov_size;
    return res;
}

static bool VERMIN__assembler_parse_two_param_instruction(uint8_t type)
{
    current_instruction.id = type;
    current_instruction.size = sizeof(uint32_t) * 3;
    bool res = VERMIN__assembler_parse_double_param();
    if(current_instruction.subparams == VERMIN_OP_REG_TO_REG_MEM)
    {
        VERMIN_LOG("Target of operation must be  registor\n");
        return false;
    }
    return true;
}

static bool VERMIN__assembler_parse_add()
{
    return VERMIN__assembler_parse_two_param_instruction(VERMIN_INSTRUCTION_ADD);
}

static bool VERMIN__assembler_parse_or()
{
    return VERMIN__assembler_parse_two_param_instruction(VERMIN_INSTRUCTION_OR);
}

static bool VERMIN__assembler_parse_lshift()
{
    return VERMIN__assembler_parse_two_param_instruction(VERMIN_INSTRUCTION_LSHIFT);
}

static bool VERMIN__assembler_parse_rshift()
{
    return VERMIN__assembler_parse_two_param_instruction(VERMIN_INSTRUCTION_RSHIFT);
}

static bool VERMIN__assembler_parse_and()
{
    return VERMIN__assembler_parse_two_param_instruction(VERMIN_INSTRUCTION_AND);
}


static bool VERMIN__assembler_parse_sub()
{
    return VERMIN__assembler_parse_two_param_instruction(VERMIN_INSTRUCTION_SUB);
}

static bool VERMIN__assembler_parse_mul()
{
    return VERMIN__assembler_parse_two_param_instruction(VERMIN_INSTRUCTION_MUL);
}

static bool VERMIN__assembler_parse_div()
{
    return VERMIN__assembler_parse_two_param_instruction(VERMIN_INSTRUCTION_DIV);
}

static bool VERMIN__assembler_parse_inc()
{
    current_instruction.id = VERMIN_INSTRUCTION_INC;
    current_instruction.size = sizeof(uint32_t) * 2;
    current_instruction.params_count = 1;
    if(!(strlen(current_line[1]) > 1))
    {
        VERMIN_LOG("Invalid param\n");
        return false;
    }
    uint8_t p1 = VERMIN_TARGET_REG;
    if(!VERMIM__assembler_parse_reg_or_mem_or_const(current_line[1],  &p1, &current_instruction.params[0]))
    {
        VERMIN_LOG("Failed to parse param\n");    
        return false;
    }
    current_instruction.subparams = 0;
    if(p1 != VERMIN_TARGET_REG)
    {
        VERMIN_LOG("INC can only be used for a registor");
        return false;
    }
    return true;
}

static bool VERMIN__assembler_parse_dec()
{
    current_instruction.id = VERMIN_INSTRUCTION_DEC;
    current_instruction.size = sizeof(uint32_t) * 2;
    current_instruction.params_count = 1;
    if(!(strlen(current_line[1]) > 1))
    {
        VERMIN_LOG("Invalid param\n");
        return false;
    }
    uint8_t p1 = VERMIN_TARGET_REG;
    if(!VERMIM__assembler_parse_reg_or_mem_or_const(current_line[1],  &p1, &current_instruction.params[0]))
    {
        VERMIN_LOG("Failed to parse param\n");    
        return false;
    }
    current_instruction.subparams = 0;
    if(p1 != VERMIN_TARGET_REG)
    {
        VERMIN_LOG("DEC can only be used for a registor");
        return false;
    }
    return true;
}

static bool VERMIN__assembler_parse_jmp(uint8_t type)
{
    current_instruction.id = VERMIN_INSTRUCTION_JMP;
    current_instruction.size = sizeof(uint32_t);
    uint8_t* subparams_ui = (uint8_t*)(&current_instruction.subparams);
    subparams_ui[0] = type;
    subparams_ui[1] = VERMIN_TARGET_CONST;
    if(strlen(current_line[1]) > 0)
    {
        if(current_line[1][0] == 'R')
        {
            current_instruction.size *= 2;
            current_instruction.params_count = 1;
            return VERMIM__assembler_parse_reg(current_line[1], &subparams_ui[1], &current_instruction.params[0]);
        }
        else if(current_line[1][0] == '@')
        {
            current_instruction.size *= 3;
            current_instruction.params_count = 2;
            subparams_ui[1] = VERMIN_TARGET_MEM;
            *((uint64_t*)(&current_instruction.params)) = tmp_strings.count;
            sprintf(tmp_strings.data[tmp_strings.count++], "%s", (current_line[1] + 1));
            return true;
        }
        else if(current_line[1][0] == '&')
        {
            current_instruction.size *= 2;
            current_instruction.params_count = 1;            
            bool success = VERMIM__assembler_parse_reg(current_line[1] + 1, &subparams_ui[1], &current_instruction.params[0]);
            subparams_ui[1] = VERMIN_TARGET_REG_MEM;
            return success;
        }
        else
        {
            current_instruction.size *= 3;
            current_instruction.params_count = 2;
            *((uint64_t*)(&current_instruction.params)) = (uint64_t)atoll(current_line[1]);
            return true;
        }
    }
    else
    {
        VERMIN_LOG("Jump destination not given\n");
        return false;
    }
    return true;
}

static bool VERMIN__assembler_parse_push()
{
    current_instruction.id = VERMIN_INSTRUCTION_PUSH;
    current_instruction.size = sizeof(uint32_t) * 2;
    current_instruction.params_count = 1;
    if(strlen(current_line[1]) == 0)
    {
        VERMIN_LOG("Push param missing\n");
        return false;
    }
    uint8_t type = 0;
    bool success = VERMIM__assembler_parse_reg_or_mem_or_const(current_line[1], &type, &current_instruction.params[0]);
    current_instruction.subparams = (uint16_t)type;
    return success;
}

static bool VERMIN__assembler_parse_pop()
{
    current_instruction.id = VERMIN_INSTRUCTION_POP;
    current_instruction.size = sizeof(uint32_t) * 2;
    current_instruction.params_count = 1;
    if(strlen(current_line[1]) == 0)
    {
        VERMIN_LOG("Push param missing\n");
        return false;
    }
    uint8_t type = 0;
    bool success = VERMIM__assembler_parse_reg_or_mem_or_const(current_line[1], &type, &current_instruction.params[0]);
    if(type == VERMIN_TARGET_CONST || type == VERMIN_TARGET_REG_MEM || type == VERMIN_TARGET_MEM || type == VERMIN_TARGET_MEMC)
    {
        VERMIN_LOG("Unsupported pop param\n");
        return false;
    }
    current_instruction.subparams = (uint16_t)type;
    return success;
}

static bool VERMIN__assembler_parse_ext()
{
    current_instruction.id = VERMIN_INSTRUCTION_EXT;
    current_instruction.size = sizeof(uint32_t);
    current_instruction.params_count = 0;
    current_instruction.subparams = 0;
    if(strlen(current_line[1]) > 0)
        current_instruction.subparams = (uint16_t)atoi(current_line[1]);
    return true;
}

static bool VERMIN__assembler_parse_syscall()
{
    current_instruction.id = VERMIN_INSTRUCTION_SYSCALL;
    current_instruction.size = sizeof(uint32_t);
    current_instruction.params_count = 0;
    current_instruction.subparams = 0;
    return true;
}

static bool VERMIN__assembler_parse_printreg()
{
    current_instruction.id = VERMIN_INSTRUCTION_PRINTREG;
    current_instruction.size = sizeof(uint32_t) * 2;
    current_instruction.params_count = 1;
    if(strlen(current_line[1]) == 0)
    {
        VERMIN_LOG("PrintReg param missing\n");
        return false;
    }
    uint8_t type = 0;
    bool success = VERMIM__assembler_parse_reg_or_mem_or_const(current_line[1], &type, &current_instruction.params[0]);
    if(type == VERMIN_TARGET_CONST || type == VERMIN_TARGET_REG_MEM || type == VERMIN_TARGET_MEM || type == VERMIN_TARGET_MEMC)
    {
        VERMIN_LOG("Unsupported pop param\n");
        return false;
    }
    current_instruction.subparams = (uint16_t)type;
    return success;
}


static bool VERMIN__assembler_parse_label()
{
    label_t label;
    memset(&label, 0, sizeof(label_t));
    sprintf(label.name, "%s", (current_line[0] + 1));
    if(instructions.count > 0)
        label.offset = instructions.data[instructions.count - 1].offset + instructions.data[instructions.count - 1].size;
    else
        label.offset = 0;
    labels.data[labels.count++] = label;
    skip_instruction = true;
    return true;
}

static bool VERMIN__assembler_apply_macro(char* data)
{
    for(size_t i = 0 ; i < macros.count ; i++)
    {
        if(strcmp(data, macros.data[i].name) == 0)
        {
            strcpy(data, macros.data[i].value);
            return true;
        }
    }
    return false;
}

static bool VERMIN__assembler_parse_macro()
{
    if(!(strlen(current_line[0]) > 0 && strlen(current_line[1]) > 0)) return false;
    VERMIN__assembler_apply_macro(current_line[1]);
    strcpy(macros.data[macros.count].name, current_line[0]);
    strcpy(macros.data[macros.count].value, current_line[1]);
    macros.count++;
    return true;
}

static bool VERMIN__assembler_parse_data()
{
    uint64_t instruction_offset = 0;
    if(instructions.count > 0)
        instruction_offset = instructions.data[instructions.count - 1].offset + instructions.data[instructions.count - 1].size;
    label_t label;
    memset(&label, 0, sizeof(label_t));
    sprintf(label.name, "%s", (current_line[0] + 1));
    uint64_t data_size = 0;
    uint8_t individual_data_size = 0;
    size_t count = 1;
    if(strncmp(current_line[1], "STR", 3) == 0)
    {
        if(strlen(current_line[1]) > 3)
            count = max(1, atoi(current_line[1] + 3));
        size_t length = strlen(current_line[2]);
        if(length > 2)
        {
            current_line[2][length - 1] = '\0';
            data_size = (length - 1) * count;
            for(int i = 0 ; i < count ; i ++)
                memcpy(&data_section[data_section_offset + i * (length - 1)], current_line[2] + 1, length - 1);
        }
    }
    else if(current_line[1][0] == 'I')
    {
        if(strlen(current_line[1]) > 2)
            count = max(1, atoi(current_line[1] + 2));
        individual_data_size = ( (uint8_t)(current_line[1][1]) - 48 );
        data_size = individual_data_size * count;
        char* data = (char*)malloc(data_size);
        memset(data, 0, data_size);
        switch (individual_data_size)
        {
        case 1:
        {
            int8_t value = (int8_t)atoi(current_line[2]);
            for(size_t i = 0 ; i < count ; i++)
                ((int8_t*)data)[i] = value;
            break;
        }
        case 2:
        {
            int16_t value = (int16_t)atoi(current_line[2]);
            for(size_t i = 0 ; i < count ; i++)
                ((int16_t*)data)[i] = value;
            break;
        }
        case 4:
        {
            int32_t value = (int32_t)atoi(current_line[2]);
            for(size_t i = 0 ; i < count ; i++)
                ((int32_t*)data)[i] = value;
            break;
        }
        case 8:
        {
            int64_t value = (int64_t)atoll(current_line[2]);
            for(size_t i = 0 ; i < count ; i++)
                ((int64_t*)data)[i] = value;
            break;
        }
        default:
            VERMIN_LOG("Invalid data size %lld [%s]\n", data_size, current_line);
            return false;
        }
        memcpy(&data_section[data_section_offset], data, data_size);
        free(data);
    }
    else
    {
        VERMIN_LOG("Unknown data type %s\n", current_line[1]);
        return false;
    }
    label.offset = instruction_offset + data_section_offset;
    data_section_offset += data_size;
    if(strlen(current_line[0]) > 1)
        labels.data[labels.count++] = label;
    return true;
}

static bool VERMIN__assembler_parse_current_line(size_t line_number)
{
    if(current_line[0][0] == ';' || strlen(current_line[0]) == 0)
        return true;
    if(current_line[0][0] == '#' && strlen(current_line[0]) > 2)
    {
        if(strcmp(current_line[0] + 1, "MACRO") == 0)
            current_parser_type = PARSER_TYPE_MARO;
        else if(strcmp(current_line[0] + 1, "DATA") == 0)
            current_parser_type = PARSER_TYPE_DATA;
        else if(strcmp(current_line[0] + 1, "CODE") == 0)
            current_parser_type = PARSER_TYPE_CODE;
        else
        {
            VERMIN_LOG("Unknown derective %s on line %zu\n", current_line[0], line_number);
            return false;
        }
        return true;
    }
    if(current_parser_type != PARSER_TYPE_MARO)
    {
        VERMIN__assembler_apply_macro(current_line[0]);
        VERMIN__assembler_apply_macro(current_line[1]);
        VERMIN__assembler_apply_macro(current_line[2]);
    }
    if(current_parser_type == PARSER_TYPE_MARO)
        return VERMIN__assembler_parse_macro();
    else if(current_parser_type == PARSER_TYPE_DATA)
        return VERMIN__assembler_parse_data();
    else if(current_parser_type == PARSER_TYPE_CODE)
    {
        bool success = false;
        skip_instruction = false;
        memset(&current_instruction, 0, sizeof(instruction_t));
        if(instructions.count == 0)
            current_instruction.offset = 0;
        else
            current_instruction.offset = instructions.data[instructions.count - 1].offset + instructions.data[instructions.count - 1].size;
        if(current_line[0][0] == '@')            
            success = VERMIN__assembler_parse_label();
        else if(memcmp(current_line[0], "OR", 2) == 0)
            success = VERMIN__assembler_parse_or();
        else if(memcmp(current_line[0], "JE", 2) == 0 || memcmp(current_line[0], "JZ", 2) == 0)
            success = VERMIN__assembler_parse_jmp(VERMIN_JMP_TYPE_JZ);
        else if(memcmp(current_line[0], "JNE", 3) == 0 || memcmp(current_line[0], "JNZ", 3) == 0)
            success = VERMIN__assembler_parse_jmp(VERMIN_JMP_TYPE_JNZ);
        else if(memcmp(current_line[0], "JMP", 3) == 0)
            success = VERMIN__assembler_parse_jmp(VERMIN_JMP_TYPE_JMP);
        else if(memcmp(current_line[0], "JGT", 3) == 0)
            success = VERMIN__assembler_parse_jmp(VERMIN_JMP_TYPE_JGT);
        else if(memcmp(current_line[0], "JLT", 3) == 0)
            success = VERMIN__assembler_parse_jmp(VERMIN_JMP_TYPE_JLT);
        else if(memcmp(current_line[0], "JGE", 3) == 0)
            success = VERMIN__assembler_parse_jmp(VERMIN_JMP_TYPE_JGE);
        else if(memcmp(current_line[0], "JLE", 3) == 0)
            success = VERMIN__assembler_parse_jmp(VERMIN_JMP_TYPE_JLE);
        else if(memcmp(current_line[0], "AND", 3) == 0)
            success = VERMIN__assembler_parse_and();
        else if(memcmp(current_line[0], "NOP", 3) == 0)
            success = VERMIN__assembler_parse_nop();
        else if(memcmp(current_line[0], "MOV", 3) == 0)
            success = VERMIN__assembler_parse_mov();
        else if(memcmp(current_line[0], "ADD", 3) == 0)
            success = VERMIN__assembler_parse_add();
        else if(memcmp(current_line[0], "SUB", 3) == 0 || memcmp(current_line[0], "CMP", 3) == 0)
            success = VERMIN__assembler_parse_sub();
        else if(memcmp(current_line[0], "MUL", 3) == 0)
            success = VERMIN__assembler_parse_mul();
        else if(memcmp(current_line[0], "DIV", 3) == 0)
            success = VERMIN__assembler_parse_div();
        else if(memcmp(current_line[0], "INC", 3) == 0)
            success = VERMIN__assembler_parse_inc();
        else if(memcmp(current_line[0], "DEC", 3) == 0)
            success = VERMIN__assembler_parse_dec();
        else if(memcmp(current_line[0], "XOR", 3) == 0)
            success = VERMIN__assembler_parse_xor();
        else if(memcmp(current_line[0], "POP", 3) == 0)
            success = VERMIN__assembler_parse_pop();
        else if(memcmp(current_line[0], "EXT", 3) == 0)
            success = VERMIN__assembler_parse_ext();
        else if(memcmp(current_line[0], "PUSH", 4) == 0)
            success = VERMIN__assembler_parse_push();
        else if(memcmp(current_line[0], "LSHIFT", 6) == 0)
            success = VERMIN__assembler_parse_lshift();
        else if(memcmp(current_line[0], "RSHIFT", 6) == 0)
            success = VERMIN__assembler_parse_rshift();
        else if(memcmp(current_line[0], "SYSCALL", 6) == 0)
            success = VERMIN__assembler_parse_syscall();
        else if(memcmp(current_line[0], "PRINTREG", 8) == 0)
            success = VERMIN__assembler_parse_printreg();
        else
        {
            VERMIN_LOG("Unknown commad %s on line %zu\n", current_line[0], (line_number + 1));
            return false;
        }
        if(success && !skip_instruction)
        {
            instructions.data[instructions.count++] = current_instruction;
        }
        if(!success)
            VERMIN_LOG("Error on line %zu\n", (line_number + 1));
        return success;
    }
    return true;
}

static uint64_t VERMIN__assembler_find_label_offset(const string_t name)
{
    uint64_t offset = UINT64_MAX;
    for(size_t i = 0 ; i < labels.count ; i++)
    {
        if(strcmp(labels.data[i].name, name) == 0)
        {
            offset = labels.data[i].offset;
            break;
        }
    }
    return offset;
}

static bool VERMIN__assembler_place_labels()
{
    for(size_t i = 0 ; i < instructions.count ; i++)
    {
        if(instructions.data[i].id == VERMIN_INSTRUCTION_PUSH || instructions.data[i].id == VERMIN_INSTRUCTION_POP)
        {
            if(instructions.data[i].subparams == VERMIN_TARGET_MEM)
            {
                uint64_t label_offset = VERMIN__assembler_find_label_offset(tmp_strings.data[instructions.data[i].params[0]]);
                if(label_offset == UINT64_MAX)
                {
                    VERMIN_LOG("Label %s not found [PUSH/POP]\n", tmp_strings.data[instructions.data[i].params[0]]);
                    return false;
                }
                uint32_t offset32 = (uint32_t)label_offset;
                instructions.data[i].params[0] = offset32;
            }
        }
        else if(instructions.data[i].id == VERMIN_INSTRUCTION_JMP)
        {
            uint8_t* subparams_ui = (uint8_t*)(&instructions.data[i].subparams);
            if(subparams_ui[1] == VERMIN_TARGET_MEM)
            {
                uint64_t* label_name_index_ptr = ((uint64_t*)(&instructions.data[i].params[0]));
                uint64_t offset = VERMIN__assembler_find_label_offset(tmp_strings.data[*label_name_index_ptr]);
                if(offset == UINT64_MAX)
                {
                    VERMIN_LOG("Label %s not found [JMP]\n", tmp_strings.data[*label_name_index_ptr]);
                    return false;
                }
                *label_name_index_ptr = offset;
                subparams_ui[1] = VERMIN_TARGET_CONST;
            }
        }
        else if(instructions.data[i].id == VERMIN_INSTRUCTION_XOR || instructions.data[i].id == VERMIN_INSTRUCTION_ADD || instructions.data[i].id == VERMIN_INSTRUCTION_SUB || instructions.data[i].id == VERMIN_INSTRUCTION_MUL || instructions.data[i].id == VERMIN_INSTRUCTION_DIV)
        {
            if(instructions.data[i].subparams == VERMIN_OP_MEM_TO_REG || instructions.data[i].subparams == VERMIN_OP_MEMC_TO_REG)
            {
                uint64_t offset = VERMIN__assembler_find_label_offset(tmp_strings.data[instructions.data[i].params[1]]);
                if(offset == UINT64_MAX)
                {
                    VERMIN_LOG("Label %s not found [ADD/SUB/MUL/DIV/XOR]\n", tmp_strings.data[instructions.data[i].params[1]]);
                    return false;
                }
                instructions.data[i].params[1] = (uint32_t)offset;
            }
            if(instructions.data[i].subparams == VERMIN_OP_MEMC_TO_REG)
                instructions.data[i].subparams = VERMIN_OP_CONST_TO_REG;
        }
        else if(instructions.data[i].id == VERMIN_INSTRUCTION_MOV)
        {
            uint8_t* subparams_ui = (uint8_t*)(&instructions.data[i].subparams);
            if(subparams_ui[0] == VERMIN_OP_MEM_TO_REG || subparams_ui[0] == VERMIN_OP_MEMC_TO_REG)
            {
                uint64_t offset = VERMIN__assembler_find_label_offset(tmp_strings.data[instructions.data[i].params[1]]);
                if(offset == UINT64_MAX)
                {
                    VERMIN_LOG("Label %s not found [MOV]\n", tmp_strings.data[instructions.data[i].params[1]]);
                    return false;
                }
                instructions.data[i].params[1] = (uint32_t)offset;
            }
            if(subparams_ui[0] == VERMIN_OP_MEMC_TO_REG)
                subparams_ui[0] = VERMIN_OP_CONST_TO_REG;
        }
    }
    return true;
}

static bool VERMIN__assembler_tokenize_line(const string_t line_original, size_t line_number)
{
    sprintf(buffer, "%s", line_original);    
    size_t word_count = 0;
    string_t* words = VERMIN_split_string(buffer, ' ', &word_count);
    current_line[0][0] = current_line[1][0] = current_line[2][0] = '\0';
    for (int i = 0, j = 0; i < word_count && j < 3; i++)
        if (strlen(words[i]) > 0)
            sprintf(current_line[j++], "%s", words[i]);
    return VERMIN__assembler_parse_current_line(line_number);
}

static bool VERMIN__assembler_prepare_final_binary()
{
    uint32_t instruction_binary[64];
    final_binary = NULL;
    final_binary_size = 0;
    size_t instruction_size = 0;
    if(instructions.count > 0)
        instruction_size = instructions.data[instructions.count - 1].offset + instructions.data[instructions.count - 1].size;    
    final_binary_size = instruction_size + data_section_offset;
    final_binary = (char*)VERMIN_malloc(final_binary_size);
    memset(final_binary, 0, final_binary_size);
    size_t offset = 0;
    for(int i = 0 ; i < instructions.count ; i++)        
    {
        memset(instruction_binary, 0, sizeof(instruction_binary));
        ((uint8_t*)(&instruction_binary[0]))[0] = instructions.data[i].id;
        ((uint8_t*)(&instruction_binary[0]))[1] = instructions.data[i].size;
        ((uint16_t*)(&instruction_binary[0]))[1] = instructions.data[i].subparams;
        if(instructions.data[i].params_count > 0)
            memcpy(&instruction_binary[1], &instructions.data[i].params[0], instructions.data[i].size - sizeof(uint32_t));
        memcpy(final_binary + offset, instruction_binary, instructions.data[i].size);
        offset += instructions.data[i].size;
    }
    memcpy(final_binary + instruction_size, data_section, data_section_offset);
    return true;
}

char* VERMIN_assemle(char* code, size_t* binary_size)
{
    size_t lines_count = 0;
    string_t* lines = VERMIN_split_string(code, '\n', &lines_count);
    instruction_line_count = lines_count;
    VERMIN__assembler_init();
    int i = 0;
    for(i = 0 ; i < lines_count; i++)
        if (!VERMIN__assembler_tokenize_line(lines[i], i))
            break;
    final_binary = NULL;
    *binary_size = 0;
    while(i == lines_count) // using while insted of if just use break in middle in case of an error
    {
        bool success = false;
        if(!VERMIN__assembler_place_labels())
            break;
        if(!VERMIN__assembler_prepare_final_binary())
            break;
        printf("Success!\n");
        break;
    }
    VERMIN__assembler_destroy();
    VERMIN_free(lines);    
    *binary_size = final_binary_size;
    return final_binary;
}