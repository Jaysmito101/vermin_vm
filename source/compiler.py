import sys
import io
import os
import math

g_lines = []
g_current_line_number = 0
g_ast = {
    "structs": [],
    "functions": []
}

def parse_code():
    global g_lines
    global g_current_line_number
    global g_ast
    line_count = len(g_lines)
    g_current_line_number = 0
    while g_current_line_number < line_count:
        current_line = g_lines[g_current_line_number]
        print(f"Line {g_current_line_number} : {current_line}")
        g_current_line_number += 1

def main():
    global g_lines
    file_source = "source.vermin"
    file = open(file_source, "r")
    code = file.read()
    lines = code.split('\n')
    g_lines = lines
    parse_code()

if __name__ == "__main__":
    main()