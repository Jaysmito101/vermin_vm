import sys
import io
import os
import re
import math

g_skip_lines = False
g_macro_oc = 0
g_ast = []
g_macors = {}
g_include_dirs = ["./"]
g_data_types = {}

def is_data_type_internal(data_type):
    if data_type == "i1" or data_type == "i2" or data_type =="i4" or data_type == "i8":
        return True
    return False

def place_macro_if_present(current_line):
    global g_macors
    for key, value in g_macors.items():
        if key in current_line:
            current_line = current_line.replace(key, value)
    return current_line

def parse_line(current_line):
    global g_ast
    global g_macors
    global g_include_dirs
    global g_skip_lines
    global g_macro_oc
    current_line = current_line.strip()
    current_line = re.sub(' +', ' ', current_line)
    if len(current_line) < 3:
        return True
    if current_line[0] == "#":
        return True
    if current_line[0] == "%":
        command = current_line[1:current_line.find(" ")]
        if command == "if" or command == "ifdef" or command == "ifndef":
            g_macro_oc += 1
        if current_line == "%endif":
            g_macro_oc -= 1
            if g_macro_oc == 0:
                g_skip_lines = False
        if command == "include":
            include_file_name = current_line[current_line.find(" "):].strip()
            include_file_found = False
            for path in g_include_dirs:
                if os.path.exists(os.path.join(path, include_file_name)):
                    file = open(os.path.join(path, include_file_name), "r")
                    include_file_name = file.read() + "\n"
                    file.close()
                    include_file_found = True
                    break
            if not include_file_found:
                print(f"File {include_file_name} not found")
                return False
            parse_code(include_file_name.splitlines(), current_line[current_line.find(" "):].strip())
        elif command == "ifndef":
            value = current_line[current_line.find(" "):].strip()
            if value in g_macors:
                g_skip_lines = True
        elif command == "ifdef":
            value = current_line[current_line.find(" "):].strip()
            if value not in g_macors:
                g_skip_lines = True
        elif command == "define":
            value = current_line[current_line.find(" "):].strip()
            value_key = value
            value_value = ""
            if " " in value:
                value_key = value[value.find(" "):].strip()
                value_value = value[:value.find(" ")].strip()
            g_macors[value_key] = value_value
        return True
    if g_skip_lines:
            return True    
    curr_inst = current_line.split()
    if curr_inst[0] == "typedef":
        if not len(curr_inst) == 3:
            print(f"Invalid typedef statement")
            return False
        if is_data_type_internal(curr_inst[1]):
            g_data_types[curr_inst[2]] = {"basic" : True, "internal" : curr_inst[1]}
        else:
            curr_data_type = curr_inst[1]
            if curr_data_type not in g_data_types:
                print(f"Unknown data type {curr_data_type}")
                return False
            g_data_types[curr_inst[2]] = g_data_types[curr_data_type]
    elif curr_inst[0] == "struct":
        if len(curr_inst) <= 2:
            print("Invalid strut declaration")
            return False
        struct_name = curr_inst[1]
        data = {"basic" : False, "internal" : []}
        for i in range(2, len(curr_inst)):
            struct_var = curr_inst[i]
            if ":" not in struct_var:
                print(f"Invalid variable declaration in struct {struct_name}")
                return False
            struct_var_name = struct_var[:struct_var.find(":")]
            struct_var_type = struct_var[struct_var.find(":") + 1:]
            struct_var_is_basic = True
            if not is_data_type_internal(struct_var_type):
                if struct_var_type not in g_data_types:
                    print(f"Unknown data type {struct_var_type}")
                    return False
                if g_data_types[struct_var_type]["basic"]:
                    struct_var_type = g_data_types[struct_var_type]["internal"]
                else:
                    struct_var_is_basic = False
            data["internal"].append((struct_var_type, struct_var_name, struct_var_is_basic))
        g_data_types[struct_name] = data
    current_line = place_macro_if_present(current_line)
    return True


def parse_code(lines, file_source):
    global g_ast
    line_count = len(lines)
    current_line_number = 0
    while current_line_number < line_count:
        current_line = lines[current_line_number]
        if not parse_line(current_line):
            print(f"Error on line {current_line_number + 1} in file {file_source}")
            break
        current_line_number += 1

def main():
    file_source = "source.vermin"
    file = open(file_source, "r")
    code = file.read() + "\n"
    file.close()
    lines = code.splitlines()
    parse_code(lines, file_source)

if __name__ == "__main__":
    main()