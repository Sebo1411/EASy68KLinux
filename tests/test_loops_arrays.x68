*-----------------------------------------------------------
* Title      : Loops and Arrays Test
* Description: Verifies DBRA loops, post-increment addressing,
*              and memory arrays
*-----------------------------------------------------------
    ORG    $1000

START:
    LEA    ARRAY,A0         * Pointer to array
    CLR.L  D1               * D1 = accumulator sum
    MOVE.W #9,D2            * D2 = loop count - 1 (10 elements)

LOOP:
    ADD.W  (A0)+,D1         * Add array element and increment pointer
    DBRA   D2,LOOP

    * Sum of 1..10 is 55 ($37)
    CMP.L  #55,D1
    BNE    FAIL

    * Test negative indexed addressing: A0 is now at end of ARRAY
    MOVE.W #-2,D3               * Offset -2 bytes (10th element = 10)
    MOVE.W 0(A0,D3.W),D4
    CMP.W  #10,D4
    BNE    FAIL

    MOVE.W #-20,D3              * Offset -20 bytes (1st element = 1)
    MOVE.W 0(A0,D3.W),D4
    CMP.W  #1,D4
    BNE    FAIL

    LEA    MSG_OK,A1
    MOVE.B #14,D0
    TRAP   #15
    MOVE.B #9,D0
    TRAP   #15

FAIL:
    LEA    MSG_FAIL,A1
    MOVE.B #14,D0
    TRAP   #15
    MOVE.B #9,D0
    TRAP   #15

ARRAY:
    DC.W   1, 2, 3, 4, 5, 6, 7, 8, 9, 10

MSG_OK:
    DC.B   'TEST: Loops and Arrays PASSED',10,0
MSG_FAIL:
    DC.B   'TEST: Loops and Arrays FAILED',10,0

    END    START
