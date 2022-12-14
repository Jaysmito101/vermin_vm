#MACRO
NUM_FACTORIALS 6
#CODE
JMP @MAIN
; FACTORIAL FUNC
@FACTORIAL_FUNC
POP R5 ; THE RETURN ADDRESS TO BE POPPED BY RETURNEE
POP R0 ; THE PARAM
PUSH R0 ; PUSH THE PARAM BACK IN THE STACK AS PARAMS ARE TO BE POPPED OF FROM CALLER FUNCTION
MOV R1 1
@FACTORIAL_FUNC.WHILE.1.CONDITION
MOV R8 R0
JLE @FACTORIAL_FUNC.WHILE.1.END
JMP @FACTORIAL_FUNC.WHILE.1.BODY
@FACTORIAL_FUNC.WHILE.1.BODY
MUL R1 R0
DEC R0
JMP @FACTORIAL_FUNC.WHILE.1.CONDITION
@FACTORIAL_FUNC.WHILE.1.END
PUSH R1 ; PUSH RESULT
JMP R5 ; RETURN
; NUM TO STR FUNC
@NUM_TO_STR
MOV R2 R13
SUB R2 24
MOV R0 &R2 ; THE BUFFER
ADD R2 8
MOV R3 &R2 ; THE NUMBER
MOV R5 R0
MOV R6 R0
XOR R1 R1
@NUM_TO_STR.WHILE.1.CONDITION
MOV R8 R3
JLE @NUM_TO_STR.WHILE.1.END
JMP @NUM_TO_STR.WHILE.1.BODY
@NUM_TO_STR.WHILE.1.BODY
; EXTRACT THE DIGIT
DIV R3 10
ADD R8 48
MOV R8LT R8
MOV &R0 R8LT
INC R0
JMP @NUM_TO_STR.WHILE.1.CONDITION
@NUM_TO_STR.WHILE.1.END
XOR R1LT R1LT
MOV &R0 R1LT
SUB R6 R0
MUL R6 -1
DEC R0
; REVERSE THE NUMBER
@NUM_TO_STR.WHILE.2.CONDITION
MOV R1 R0
SUB R1 R5
JLE @NUM_TO_STR.WHILE.2.END
JMP @NUM_TO_STR.WHILE.2.BODY
@NUM_TO_STR.WHILE.2.BODY
MOV R1LT &R0
MOV R1UT &R5
MOV &R0 R1UT
MOV &R5 R1LT
DEC R0
INC R5
JMP @NUM_TO_STR.WHILE.2.CONDITION
@NUM_TO_STR.WHILE.2.END
POP R0 ; GET RETURN ADDRESS
PUSH R6 ; PUSH RETURN VALUE
JMP R0
@MAIN
@FOR.1.INITIALIZATION
XOR R0 R0
PUSH R0 
@MAIN.FOR.1.CONDITION
MOV R0 R13
SUB R0 8
MOV R0 &R0
SUB R0 NUM_FACTORIALS
JGT @MAIN.FOR.1.END
JMP @MAIN.FOR.1.BODY
@MAIN.FOR.1.UPDATION
MOV R0 R13
SUB R0 8
MOV R1 &R0
INC R1
MOV &R0 R1
JMP @MAIN.FOR.1.CONDITION
@MAIN.FOR.1.BODY
MOV R1 R13
SUB R1 8
MOV R1 &R1
PUSH R1
; PUSH RETURN ADDRESS TO STACK AND CALL FUNCTION
MOV R0 R15
ADD R0 44
PUSH R0
JMP @FACTORIAL_FUNC
POP R1
POP R2
; PREPARE PARAMS FOR NUM_TO_STR CALL
MOV R0 R13
ADD R13 256
PUSH R0
PUSH R1
; PUSH RETURN ADDRESS TO STACK AND CALL FUNCTION
MOV R0 R15
ADD R0 44
PUSH R0
JMP @NUM_TO_STR
POP R5 ; POP OF RETURN VALUE (LENGTH OF STRING)
; POP OF FUNCTION PARAMS FROM STACK AFER FUNCTION CALL
POP R1
POP R0
SUB R13 256
; PRINT THE NUMBER FROM BUFFER (R13)
MOV R0 0
MOV R1 0 
MOV R2 R13
MOV R3 R5
SYSCALL
; PRINT THE NEWLINE (ASCII 10)
; HERE 10 IS ASCII FOR '\n'
MOV R0LT 10
MOV &R13 R0LT
MOV R0 0
MOV R1 0 
MOV R2 R13
MOV R3 1
SYSCALL
JMP @MAIN.FOR.1.UPDATION
@MAIN.FOR.1.END
POP R0
EXT
#DATA
