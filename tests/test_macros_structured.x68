* Test MACRO/ENDM and Structured Assembly (IF/ELSE/ENDI, FOR/ENDF, WHILE/ENDW, REPEAT/UNTIL, DBLOOP/UNLESS)
        ORG     $1000

ADD_VAL MACRO
        ADD.\0  \1,\2
        ENDM

START:
        ; 1. MACRO test with size argument \0 and operands \1,\2
        MOVE.L  #12,D0
        ADD_VAL.L #8,D0         ; D0 = 20

        ; 2. IF / ELSE / ENDI test
        IF.L D0 <EQ> #20 THEN
            MOVE.L  #100,D1
        ELSE
            MOVE.L  #0,D1
        ENDI
        CMP.L   #100,D1
        BNE     FAIL

        IF.L D0 <LT> #5 THEN
            BRA     FAIL
        ELSE
            ADD.L   #1,D1       ; D1 = 101
        ENDI
        CMP.L   #101,D1
        BNE     FAIL

        ; 3. FOR / TO / ENDF (1 + 2 + 3 + 4 = 10)
        CLR.L   D5
        FOR.W D1 = #1 TO #4 DO
            ADD.W   D1,D5
        ENDF
        CMP.W   #10,D5
        BNE     FAIL

        ; 4. FOR / DOWNTO / BY / ENDF (6 + 4 + 2 = 12)
        CLR.L   D5
        FOR.W D1 = #6 DOWNTO #1 BY #2 DO
            ADD.W   D1,D5
        ENDF
        CMP.W   #12,D5
        BNE     FAIL

        ; 5. WHILE / DO / ENDW (D2 = 0 -> 5, D5 = 5)
        CLR.L   D2
        CLR.L   D5
        WHILE.W D2 <LT> #5 DO
            ADDQ.W  #1,D2
            ADDQ.W  #1,D5
        ENDW
        CMP.W   #5,D5
        BNE     FAIL

        ; 6. REPEAT / UNTIL (D3 = 4 -> 0, runs 4 times)
        MOVE.W  #4,D3
        CLR.L   D5
        REPEAT
            ADD.W   D3,D5
            SUBQ.W  #1,D3
        UNTIL.W D3 <EQ> #0
        CMP.W   #10,D5          ; 4 + 3 + 2 + 1 = 10
        BNE     FAIL

        ; 7. DBLOOP / UNLESS <F> (D4 starts at 2 -> 2, 1, 0 = 3 iterations)
        CLR.L   D5
        DBLOOP D4 = #2
            ADDQ.W  #1,D5
        UNLESS <F>
        CMP.W   #3,D5
        BNE     FAIL

PASS:
        LEA     MSG_PASS,A1
        MOVE.B  #14,D0
        TRAP    #15
        BRA     DONE

FAIL:
        LEA     MSG_FAIL,A1
        MOVE.B  #14,D0
        TRAP    #15

DONE:
        MOVE.B  #9,D0
        TRAP    #15

MSG_PASS DC.B   'PASSED',13,10,0
MSG_FAIL DC.B   'FAILED',13,10,0

        END     START
