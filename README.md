<br/>
<p align="center">
    <img src="./logo.png" border="0"></
</p>

<br/>
<p align="center">
  <img alt="GitHub repo size" src="https://img.shields.io/github/repo-size/Jaysmito101/vermin_vm?style=for-the-badge">
  <img alt="Lines of code" src="https://img.shields.io/tokei/lines/github/Jaysmito101/vermin_vm?style=for-the-badge">
  <img alt="GitHub commit activity" src="https://img.shields.io/github/commit-activity/w/Jaysmito101/vermin_vm?style=for-the-badge">
    <br>
    <img alt="Maintenance" src="https://img.shields.io/maintenance/yes/2022?style=for-the-badge">
    <a href="https://patreon.com/jaysmito101"><img src="https://img.shields.io/endpoint.svg?url=https%3A%2F%2Fshieldsio-patreon.vercel.app%2Fapi%3Fusername%3Djaysmito101%26type%3Dpledges&style=for-the-badge" alt="Support me on Patreon" /></a>
</p>
<br/>


# vermin_vm
A Virtual Machine + Assembler written in C

## SPECS: 

### REGISTORS

Naming format: <b>R*</b> [ex: <b>R0</b>, <b>R1</b>, ..., <b>R15</b>]

Each registor is 64 bit in size

Accessing specific parts of registors:

Entire registor : R*    [ex: R0, R1, ...]
Lower 32-bits   : R*LH  [ex: R0LH, R1LH, ...]
Upper 32-bits   : R*UH  [ex: R0UH, R1UH, ...]
Lower 16-bits of Lower 32-bit : R*LQ [ex: R0LQ, R1LQ, ...]
Upper 16-bits of Lower 32-bit : R*UQ [ex: R0UQ, R1UQ, ...]
Lower 8-bits of Lower 16-bit  : R*LT [ex: R0LT, R1LT, ...]
Upper 8-bits of Lower 16-bit  : R*UT [ex: R0UT, R1UT, ...]

Registor representation:

A registor is representated by a 32 bit integer:

| Bits    | Purpose              |
|--------|----------------------|
| 0 - 8   | ID [0 - 15] |
| 9 - 16  | Size [64 (8), 32 (4), 16 (2), 8 (1)]  |
| 17 - 24 | Lower / Upper part [Lower (0), Upper (1)] |
| 25 - 32 | Registor Local offset [8-bit integer] |


Sepcial Registors:
| Registers | Purpose | 
|------|----------------------| 
| R15  |  Instruction pointer | 
| R14  |  Stack start pointer | 
| R13  |  Stack top pointer | 
| R12  |  Heap start pointer | 
| R8 to R11 |  Reserved | 

### MEMORY LAYOUT
|ADDRESS INCREASES DOWNWARDS|
|-----|
| BEGIN |
| R14 | 
| STACK  | 
| R13 | 
| R12 | 
| HEAP  | 
| END | 


# Assembly Instructions:

Each instruction has the following format:
    
    [8-bit id][8-bit size in bytes][16-bit sub params] [32 x n-bit params]

<b>NOTE</b>: First 32 bits are compulsory

*****************************************************

### NOP

ID = 0

Size = 4

SubParams = NULL

*****************************************************

### MOV

ID = 1

Size = 4 + 2 * 4 [ = 12]

SubParams [first 8 bits] = Reg to Reg (0), Const to Reg (1), Reg Memory to Reg (2), Memory to Reg (3), Reg to Reg Memory (4)

SubParams [second 8 bits] = size to move in bytes

Params = 2

Moves data from param 2 to param 1

*****************************************************

### ADD

ID = 2

Size = 4 + 2 * 4 [ = 12]

SubParams = Reg to Reg (0), Const to Reg (1), Reg Memory to Reg (2), Memory to Reg (3)

Params = 2

Adds content of param 2 and 1 (1 + 2) and stores in 1

*****************************************************
### SUB/CMP

ID = 3

Size = 4 + 2 * 4 [ = 12]

SubParams = Reg to Reg (0), Const to Reg (1), Reg Memory to Reg (2), Memory to Reg (3)

Params = 2

Subs content of param 2 and 1 (1 - 2) and stores in 1 and also in R8

*****************************************************

### MUL

ID = 4

Size = 4 + 2 * 4 [ = 12]

SubParams = Reg to Reg (0), Const to Reg (1), Reg Memory to Reg (2), Memory to Reg (3)

Params = 2

Muls content of param 2 and 1 (1 * 2) and stores in 1

*****************************************************
### DIV

ID = 5

Size = 4 + 2 * 4 [ = 12]

SubParams = Reg to Reg (0), Const to Reg (1), Reg Memory to Reg (2), Memory to Reg (3)

Params = 2

Divs content of param 2 and 1 (1 / 2) and stores Quotient 1 and remainder in R8

*****************************************************
### INC

ID = 6

Size = 4 + 4 [ = 8]

SubParams = NULL

Params = 1 [ a registor ]

Increments contents of registor by 1

*****************************************************

### DEC

ID = 7

Size = 4 + 4 [ = 8]

SubParams = NULL

Params = 1 [ a registor ]

Decrements contents of registor by 1

*****************************************************
### XOR

ID = 8

Size = 4 + 2 * 4 [ = 12]

SubParams = Reg to Reg (0), Const to Reg (1), Reg Memory to Reg (2), Memory to Reg (3)

Params = 2

Xors content of param 2 and 1 (1 XOR 2) and stores in 1

*****************************************************
### JMP/(JE or JZ)/(JNE or JNZ)/JGT/JLT/JGE/JLE

ID = 9

Size = 4 + 2 * 4 [ = 12]

SubParams [first 8 bit] = Plain (0), Jump if [R8 = ]Zero or equal (1), Jump if [R8 != ]not Zero or equal (2), Jump if R8 > 0 (3), Jump if R8 < 0 (4), Jump if R8 >= 8 (5), Jump if R8 <= 0 (6)

SubParams [second 8 bit] = To Const (3), To Reg (0), To Reg Mem (1)

Params = 1 64-bit integer [instruction index] or 1 32-bit registor

*****************************************************
### PUSH

ID = 10

Size = 4 + 4 [ = 8]

SubParams = Reg(0), Mem(2) (push PTR), Const(3)

Params = 1

Push item to stack also increments stack top pointer

*****************************************************
### POP

ID = 11
Size = 4 + 4 [ = 8]
SubParams = Reg(0)
Params = 1

Pop item to stack also increments stack top pointer

*****************************************************
### EXT

ID = 12

Size = 4

SubParams = Const (exit code)

Quit VM

*****************************************************
### SYSCALL

ID = 13

Size = 4

SubParams = NULL

*****************************************************

### PRINTREG

ID = 14

Size = 5

SubParams = NULL

Params = 1 (a registor)

NOTE: PRINTREG IS ONLY MEANT TO BE USED FOR DEBUGGING
