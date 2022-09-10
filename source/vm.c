#include "vermin.h"


static char* memory = NULL;
static void* open_streams[256];
static int64_t registors[16];
static char* buffer[4096];
static struct
{
    uint32_t main;
    uint8_t id;
    uint8_t size;
    uint16_t subparams;
    uint32_t* pointer;
} current_instruction;

//static bool VERMIN__vm_execute_instruction_NOP()
//{
//    VERMIN_LOG("Instruction NOP\n");
//    return true;
//}

// DEBUG FUNCTIONS

static void print_registors()
{
    printf("Registors: [ ");
    for(int i = 0 ; i < 16 ; i++)
        printf("(R%d %lld) ", i, registors[i]);
    printf("]\n");
}

// DEBUG FUNCTIONS

static int64_t VERMIN__vm_as_number(char* ptr, size_t size)
{
    switch(size)
    {
        case 1  : return (int64_t)(*( (int8_t*) ptr ));
        case 2  : return (int64_t)(*( (int16_t*) ptr ));
        case 4  : return (int64_t)(*( (int32_t*) ptr ));
        case 8  : return (int64_t)(*( (int64_t*) ptr ));
        default : return 0;
    }
    return 0;
}

static bool VERMIN__vm_put_number(char* ptr, size_t size, int64_t value)
{
    switch(size)
    {
        case 1  :  *( (int8_t*)ptr ) = (int8_t)value; break;
        case 2  :  *( (int16_t*)ptr ) = (int16_t)value; break;
        case 4  :  *( (int32_t*)ptr ) = (int32_t)value; break;
        case 8  :  *( (int64_t*)ptr ) = (int64_t)value; break;
        default : return false;
    }
    return true;
}

static int64_t VERMIN__vm_get_registor_value(uint32_t registor)
{
    uint8_t reg_id = ((uint8_t*)&registor)[0];
    uint8_t reg_size = ((uint8_t*)&registor)[1];
    uint8_t reg_offset = ((uint8_t*)&registor)[3];
    char* reg_ptr = ((char*)&registors[reg_id]) + reg_offset;
    return VERMIN__vm_as_number(reg_ptr, reg_size);
}

static bool VERMIN__vm_set_registor_value(uint32_t registor, int64_t value)
{
    uint8_t reg_id = ((uint8_t*)&registor)[0];
    uint8_t reg_size = ((uint8_t*)&registor)[1];
    uint8_t reg_offset = ((uint8_t*)&registor)[3];
    char* reg_ptr = ((char*)&registors[reg_id]) + reg_offset;
    return VERMIN__vm_put_number(reg_ptr, reg_size, value);
}

static size_t VERMIN__vm_set_registor_size(uint32_t registor)
{
    return (size_t) (((uint8_t*)&registor)[1]);
}

static bool VERMIN__vm_execute_instruction_MOV()
{
    uint8_t op = ((uint8_t*)&current_instruction.subparams)[0];
    uint8_t size = ((uint8_t*)&current_instruction.subparams)[1];

    switch(op)
    {
        case VERMIN_OP_REG_TO_REG:
            return VERMIN__vm_set_registor_value(current_instruction.pointer[1], VERMIN__vm_get_registor_value(current_instruction.pointer[2]));
        case VERMIN_OP_CONST_TO_REG:
            return VERMIN__vm_set_registor_value(current_instruction.pointer[1], *((int32_t*)&current_instruction.pointer[2]));
        case VERMIN_OP_REG_MEM_TO_REG:
        {            
            uint64_t offset = (uint64_t)VERMIN__vm_get_registor_value(current_instruction.pointer[2]);
            char* memory_ptr = &memory[offset];
            size_t reg_size = VERMIN__vm_set_registor_size(current_instruction.pointer[1]);
            return VERMIN__vm_set_registor_value(current_instruction.pointer[1], VERMIN__vm_as_number(memory_ptr, reg_size));
        }
        case VERMIN_OP_MEM_TO_REG:
        {
            uint64_t offset = (uint64_t)current_instruction.pointer[2];
            char* memory_ptr = &memory[offset];
            size_t reg_size = VERMIN__vm_set_registor_size(current_instruction.pointer[1]);
            return VERMIN__vm_set_registor_value(current_instruction.pointer[1], VERMIN__vm_as_number(memory_ptr, reg_size));
        }
        case VERMIN_OP_REG_TO_REG_MEM:
        {
            int64_t value = VERMIN__vm_get_registor_value(current_instruction.pointer[2]);
            uint64_t offset = (uint64_t)VERMIN__vm_get_registor_value(current_instruction.pointer[1]);
            char* memory_ptr = &memory[offset];
            size_t reg_size = VERMIN__vm_set_registor_size(current_instruction.pointer[2]);
            return VERMIN__vm_put_number(memory_ptr, reg_size, value);
        }
        default:
            VERMIN_LOG("Unknown MOV OP\n");
            return false;
    }
    return true;
}

static bool VERMIN__vm_execute_instruction_ADD()
{
    int64_t value_1 = VERMIN__vm_get_registor_value(current_instruction.pointer[1]);
    int64_t value_2 = 0;
    switch (current_instruction.subparams)
    {
    case VERMIN_OP_REG_TO_REG: value_2 = VERMIN__vm_get_registor_value(current_instruction.pointer[2]); break;
    case VERMIN_OP_CONST_TO_REG: value_2 = (int64_t)(current_instruction.pointer[2]); break;
    case VERMIN_OP_REG_MEM_TO_REG: value_2 = (VERMIN__vm_as_number(&memory[VERMIN__vm_get_registor_value(current_instruction.pointer[2])], VERMIN__vm_set_registor_size(current_instruction.pointer[1]))); break;
    case VERMIN_OP_MEM_TO_REG: value_2 = VERMIN__vm_as_number(&memory[current_instruction.pointer[2]], VERMIN__vm_set_registor_size(current_instruction.pointer[1])); break;
    default:
        VERMIN_LOG("Unknown ADD OP\n");        
        break;
    }
    int64_t result = value_1 + value_2;
    return VERMIN__vm_set_registor_value(current_instruction.pointer[1], result);
}

static bool VERMIN__vm_execute_instruction_SUB()
{
    int64_t value_1 = VERMIN__vm_get_registor_value(current_instruction.pointer[1]);
    int64_t value_2 = 0;
    switch (current_instruction.subparams)
    {
    case VERMIN_OP_REG_TO_REG: value_2 = VERMIN__vm_get_registor_value(current_instruction.pointer[2]); break;
    case VERMIN_OP_CONST_TO_REG: value_2 = (int64_t)(current_instruction.pointer[2]); break;
    case VERMIN_OP_REG_MEM_TO_REG: value_2 = (VERMIN__vm_as_number(&memory[VERMIN__vm_get_registor_value(current_instruction.pointer[2])], VERMIN__vm_set_registor_size(current_instruction.pointer[1]))); break;
    case VERMIN_OP_MEM_TO_REG: value_2 = VERMIN__vm_as_number(&memory[current_instruction.pointer[2]], VERMIN__vm_set_registor_size(current_instruction.pointer[1])); break;
    default:
        VERMIN_LOG("Unknown SUB OP\n");        
        break;
    }
    int64_t result = value_1 - value_2;
    registors[8] = result;
    return VERMIN__vm_set_registor_value(current_instruction.pointer[1], result);
}

static bool VERMIN__vm_execute_instruction_MUL()
{
    int64_t value_1 = VERMIN__vm_get_registor_value(current_instruction.pointer[1]);
    int64_t value_2 = 0;
    switch (current_instruction.subparams)
    {
    case VERMIN_OP_REG_TO_REG: value_2 = VERMIN__vm_get_registor_value(current_instruction.pointer[2]); break;
    case VERMIN_OP_CONST_TO_REG: value_2 = (int64_t)(current_instruction.pointer[2]); break;
    case VERMIN_OP_REG_MEM_TO_REG: value_2 = (VERMIN__vm_as_number(&memory[VERMIN__vm_get_registor_value(current_instruction.pointer[2])], VERMIN__vm_set_registor_size(current_instruction.pointer[1]))); break;
    case VERMIN_OP_MEM_TO_REG: value_2 = VERMIN__vm_as_number(&memory[current_instruction.pointer[2]], VERMIN__vm_set_registor_size(current_instruction.pointer[1])); break;
    default:
        VERMIN_LOG("Unknown MUL OP\n");        
        break;
    }
    int64_t result = value_1 * value_2;
    return VERMIN__vm_set_registor_value(current_instruction.pointer[1], result);
}

static bool VERMIN__vm_execute_instruction_DIV()
{
    int64_t value_1 = VERMIN__vm_get_registor_value(current_instruction.pointer[1]);
    int64_t value_2 = 0;
    switch (current_instruction.subparams)
    {
    case VERMIN_OP_REG_TO_REG: value_2 = VERMIN__vm_get_registor_value(current_instruction.pointer[2]); break;
    case VERMIN_OP_CONST_TO_REG: value_2 = (int64_t)(current_instruction.pointer[2]); break;
    case VERMIN_OP_REG_MEM_TO_REG: value_2 = (VERMIN__vm_as_number(&memory[VERMIN__vm_get_registor_value(current_instruction.pointer[2])], VERMIN__vm_set_registor_size(current_instruction.pointer[1]))); break;
    case VERMIN_OP_MEM_TO_REG: value_2 = VERMIN__vm_as_number(&memory[current_instruction.pointer[2]], VERMIN__vm_set_registor_size(current_instruction.pointer[1])); break;
    default:
        VERMIN_LOG("Unknown DIV OP\n");        
        break;
    }
    int64_t result = value_1 / value_2;
    registors[8] = value_1 % value_2;
    return VERMIN__vm_set_registor_value(current_instruction.pointer[1], result);
}

static bool VERMIN__vm_execute_instruction_INC()
{
    int64_t value_1 = VERMIN__vm_get_registor_value(current_instruction.pointer[1]);
    return VERMIN__vm_set_registor_value(current_instruction.pointer[1], value_1 + 1);
}

static bool VERMIN__vm_execute_instruction_DEC()
{
    int64_t value_1 = VERMIN__vm_get_registor_value(current_instruction.pointer[1]);
    return VERMIN__vm_set_registor_value(current_instruction.pointer[1], value_1 - 1);
}

static bool VERMIN__vm_execute_instruction_XOR()
{
    int64_t value_1 = VERMIN__vm_get_registor_value(current_instruction.pointer[1]);
    int64_t value_2 = 0;
    switch (current_instruction.subparams)
    {
    case VERMIN_OP_REG_TO_REG: value_2 = VERMIN__vm_get_registor_value(current_instruction.pointer[2]); break;
    case VERMIN_OP_CONST_TO_REG: value_2 = (int64_t)(current_instruction.pointer[2]); break;
    case VERMIN_OP_REG_MEM_TO_REG: value_2 = (VERMIN__vm_as_number(&memory[VERMIN__vm_get_registor_value(current_instruction.pointer[2])], VERMIN__vm_set_registor_size(current_instruction.pointer[1]))); break;
    case VERMIN_OP_MEM_TO_REG: value_2 = VERMIN__vm_as_number(&memory[current_instruction.pointer[2]], VERMIN__vm_set_registor_size(current_instruction.pointer[1])); break;
    default:
        VERMIN_LOG("Unknown XOR OP\n");        
        break;
    }
    int64_t result = value_1 ^ value_2;
    return VERMIN__vm_set_registor_value(current_instruction.pointer[1], result);
    return true;
}

static bool VERMIN__vm_execute_instruction_JMP()
{
    uint8_t jump_type = ((uint8_t*)(&current_instruction.subparams))[0];
    uint8_t jump_target_type = ((uint8_t*)(&current_instruction.subparams))[1];
    bool jump_condition = false;
    switch(jump_type)
    {
        case VERMIN_JMP_TYPE_JMP: jump_condition = true; break;
        case VERMIN_JMP_TYPE_JNZ: jump_condition = (registors[8] != 0); break;
        case VERMIN_JMP_TYPE_JZ: jump_condition = (registors[8] == 0); break;
        case VERMIN_JMP_TYPE_JGT: jump_condition = (registors[8] > 0); break;
        case VERMIN_JMP_TYPE_JLT: jump_condition = (registors[8] < 0); break;
        case VERMIN_JMP_TYPE_JGE: jump_condition = (registors[8] >= 0); break;
        case VERMIN_JMP_TYPE_JLE: jump_condition = (registors[8] <= 0); break;
        default:
            VERMIN_LOG("Unknown JUMP type\n");
            return false;
    }
    uint64_t jump_target = 0;
    switch(jump_target_type)
    {
        case VERMIN_TARGET_CONST: jump_target = *(uint64_t*)(current_instruction.pointer + 1); break;
        case VERMIN_TARGET_REG: jump_target = (uint64_t)VERMIN__vm_get_registor_value(current_instruction.pointer[1]); break;
        case VERMIN_TARGET_REG_MEM: jump_target = (VERMIN__vm_as_number(&memory[VERMIN__vm_get_registor_value(current_instruction.pointer[2])], VERMIN__vm_set_registor_size(current_instruction.pointer[1]))); break;
        default:
            VERMIN_LOG("Unknown JUMP target type\n");
            return false;
    }
    if(jump_condition)
        registors[15] = (int64_t)jump_target;
    return true;
}

static bool VERMIN__vm_execute_instruction_PUSH()
{
    size_t size = 0;
    int64_t value = 0;
    switch(current_instruction.subparams)
    {
        case VERMIN_TARGET_REG: size = VERMIN__vm_set_registor_size(current_instruction.pointer[1]); value = VERMIN__vm_get_registor_value
        (current_instruction.pointer[1]); break;
        case VERMIN_TARGET_MEM: size = sizeof(uint64_t); value = (int64_t)(current_instruction.pointer[1]); break;
        case VERMIN_TARGET_CONST: size = sizeof(uint32_t); value = (int64_t)(*(int32_t*)(&current_instruction.pointer[1])); break;
        default:
            VERMIN_LOG("Unknown PUSH target");
            return false;
    }
    registors[13] += size;
    return VERMIN__vm_put_number(&memory[registors[13] - size], size, value);
}

static bool VERMIN__vm_execute_instruction_POP()
{
    size_t reg_size = VERMIN__vm_set_registor_size(current_instruction.pointer[1]);
    int64_t result = VERMIN__vm_as_number(memory + registors[13], reg_size);
    registors[13] -= reg_size;
    return VERMIN__vm_set_registor_value(current_instruction.pointer[1], result);
}

static bool VERMIN__vm_execute_instruction_EXT()
{
    return false;
}

static bool VERMIN__vm_execute_instruction_SYSCALL()
{
    switch(registors[0])
    {
        case VERMIN_SYSCALL_WRITE:
        {
            void* handle = open_streams[registors[1]];
            size_t size = registors[3];
            char* data_ptr = memory + registors[2];
            fwrite(data_ptr, 1, size, handle);
            break;
        }
        case VERMIN_SYSCALL_READ:
        {
            void* handle = open_streams[registors[1]];
            size_t size = registors[3];
            char* data_ptr = memory + registors[2];
            fread(data_ptr, 1, size, handle);
            break;
        }
        case VERMIN_SYSCALL_FOPEN:
        {
            char* path = memory + registors[3];
            int64_t* handle_ptr = (int64_t*)(memory + registors[1]);
            *handle_ptr = 0;
            for(int i = 0 ; i < sizeof(open_streams) / sizeof(open_streams[0]) ; i++)
            {
                if(open_streams[i] == NULL)
                {
                    *handle_ptr = i;
                    break;
                }
            }
            if(*handle_ptr == 0) 
            {
                VERMIN_LOG("Streams slot full\n");
                return false;
            }
            open_streams[*handle_ptr] = fopen(path, registors[2] == 0 ? "wb" : "rb");
            break;
        }
        case VERMIN_SYSCALL_FCLOSE:
        {
            if(registors[1] <= 1)
            {
                VERMIN_LOG("Cannot closed reserved streams\n");
                return false;
            }
            if(open_streams[registors[1]] == NULL)
            {
                VERMIN_LOG("Invalid stream handle\n");
                return false;
            }
            fclose(open_streams[registors[1]]);
            open_streams[registors[1]] = NULL;
            break;
        }
        default:
            VERMIN_LOG("Unknown syscall %d\n", registors[0]);
            return false;
    }

    return true;
}

static bool VERMIN__vm_execute_instruction()
{
    switch(current_instruction.id)
    {
        case VERMIN_INSTRUCTION_NOP     : return true; /* VERMIN__vm_execute_instruction_NOP(); */
        case VERMIN_INSTRUCTION_MOV     : return VERMIN__vm_execute_instruction_MOV();   
        case VERMIN_INSTRUCTION_ADD     : return VERMIN__vm_execute_instruction_ADD();   
        case VERMIN_INSTRUCTION_SUB     : return VERMIN__vm_execute_instruction_SUB();   
        case VERMIN_INSTRUCTION_MUL     : return VERMIN__vm_execute_instruction_MUL();   
        case VERMIN_INSTRUCTION_DIV     : return VERMIN__vm_execute_instruction_DIV();   
        case VERMIN_INSTRUCTION_INC     : return VERMIN__vm_execute_instruction_INC();   
        case VERMIN_INSTRUCTION_DEC     : return VERMIN__vm_execute_instruction_DEC();   
        case VERMIN_INSTRUCTION_XOR     : return VERMIN__vm_execute_instruction_XOR();   
        case VERMIN_INSTRUCTION_JMP     : return VERMIN__vm_execute_instruction_JMP();   
        case VERMIN_INSTRUCTION_PUSH    : return VERMIN__vm_execute_instruction_PUSH();       
        case VERMIN_INSTRUCTION_POP     : return VERMIN__vm_execute_instruction_POP();   
        case VERMIN_INSTRUCTION_EXT     : return VERMIN__vm_execute_instruction_EXT();   
        case VERMIN_INSTRUCTION_SYSCALL : return VERMIN__vm_execute_instruction_SYSCALL();       

        default:
            VERMIN_LOG("Unknown instruction %d\n", current_instruction.id);
            return false;
    }
    return true;
}

bool VERMIN_execute(char* vermin_binary, size_t binary_size)
{
    memory = VERMIN_malloc(binary_size + VERMIN_STACK_SIZE + VERMIN_HEAP_SIZE);    
    memset(memory, 0, binary_size + VERMIN_STACK_SIZE + VERMIN_HEAP_SIZE);
    memset(registors, 0, sizeof(registors));
    memset(open_streams, 0, sizeof(open_streams));
    memcpy(memory, vermin_binary, binary_size);
    char* current_ptr = (char*)memory;
    char* begin_ptr = (char*)memory;
    char* end_ptr = (memory + binary_size);
    registors[12] = binary_size + VERMIN_STACK_SIZE;
    registors[13] = binary_size;
    registors[14] = binary_size;
    registors[15] = 0;
    open_streams[0] = stdout;
    open_streams[1] = stdin;
    int64_t instruction_ptr_prev = registors[15];
    int c = 0;
    while((size_t)registors[15] < binary_size)
    {
        current_ptr = begin_ptr + registors[15];
        current_instruction.main = *((uint32_t*)current_ptr);
        current_instruction.id = *((uint8_t*)current_ptr);
        current_instruction.size = *((uint8_t*)current_ptr + 1);
        current_instruction.subparams = *((uint16_t*)current_ptr + 1);
        current_instruction.pointer = (uint32_t*)current_ptr;
        if(!VERMIN__vm_execute_instruction())   break;
        if(registors[15] == instruction_ptr_prev)
            registors[15] += current_instruction.size;
        instruction_ptr_prev = registors[15];
        //print_registors();
    }
    VERMIN_free(memory);
    return true;
}