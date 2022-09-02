#include "vermin.h"

size_t VERMIN_count_char_in_string(const string_t string, char character)
{
    size_t count = 0; 
    bool is_in_string = false;
    bool was_back_slash = false;
    for(size_t i = 0 ; string[i] != '\0' ; i++ )
    {
        if(!is_in_string)
        {
            if(string[i] == character)
                count++;
            if(!was_back_slash && string[i] == '\"')
                is_in_string = true;
        }
        else
        {
            if(!was_back_slash && string[i] == '\"')
                is_in_string = false;
        }
        if(was_back_slash)
        {
            was_back_slash = false;
            continue;
        }
        if(string[i] == '\\')
            was_back_slash = true;
    }
    return count;
}

string_t* VERMIN_split_string(string_t string, char delim, size_t* count_ptr)
{
    size_t count = VERMIN_count_char_in_string(string, delim) + 1;
    if(count_ptr)
        *count_ptr = count;
    if(count == 0)
        return NULL;
    string_t* parts = (string_t*)VERMIN_malloc(count * sizeof(string_t));
    int parts_index = 0;
    parts[parts_index++] = string;
    bool is_in_string = false;
    bool was_back_slash = false;
    for(size_t i = 0 ; string[i] != '\0' ; i++ )
    {
        if(!is_in_string)
        {
            if(!was_back_slash && string[i] == '\"')
                is_in_string = true;
            if(string[i] == delim)
            {
                string[i] = '\0';
                parts[parts_index++] = string + i + 1;
            }
        }
        else
        {
            if(!was_back_slash && string[i] == '\"')
                is_in_string = false;
        }
        if(was_back_slash)
        {
            was_back_slash = false;
            continue;
        }
        if(string[i] == '\\')
            was_back_slash = true;
    } 

    return parts;
}