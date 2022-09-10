#MACRO
#CODE
; print hello world
MOV R0 0
MOV R1 0
MOV R2 $HW
MOV R3 14
SYSCALL
; read string
MOV R5 R13 ; COPY STACK INITIAL STACK TOP
ADD R13 256 ; ALLOCATE 256 BYTES IN STACK FOR THE STRING
MOV R0 1 ; MOVE SYSCALL ID 1 (READ) TO R0
MOV R1 1 ; MOVE STREAM HANDLE 1 (STDIN) TO R1
MOV R2 R5 ; MOVE THE START (PTR) OF THE NEWLY ALLOCATED STRING TO R2
MOV R3 16 ; READ SIZE 16
SYSCALL
; print THE STRING
MOV R0 0
MOV R1 0
MOV R2 R5
MOV R3 14
SYSCALL
SUB R13 256 ; POP OFF THE ALLOCATED STRING FROM THE STACK
EXT
#DATA
@HW STR "Hello World!
"