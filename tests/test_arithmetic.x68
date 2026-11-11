*-----------------------------------------------------------
* Title      : Arithmetic and Condition Code Test
* Description: Verifies 68000 arithmetic instructions
*-----------------------------------------------------------
    ORG    $1000

START:
    * Test 1: Addition & Subtraction
    MOVE.L #$00001000,D1
    ADD.L  #$00000500,D1    * D1 = $1500
    SUB.L  #$00000200,D1    * D1 = $1300
    CMP.L  #$00001300,D1
    BNE    FAIL

    * Test 2: Multiplication (16x16 -> 32)
    MOVE.W #12,D2
    MULS   #15,D2           * 12 * 15 = 180 ($B4)
    CMP.L  #180,D2
    BNE    FAIL

    * Test 3: Division (32/16 -> 16 rem, 16 quot)
    MOVE.L #185,D3
    DIVU   #10,D3           * Quot = 18 ($12), Rem = 5 ($0005) -> D3 = $00050012
    CMP.L  #$00050012,D3
    BNE    FAIL

    * Test 3b: Signed Division (DIVS with negative numbers)
    MOVE.L #-100,D3
    DIVS   #7,D3            * -100 / 7: Quot = -14 ($FFF2), Rem = -2 ($FFFE)
    CMP.L  #$FFFEFFF2,D3
    BNE    FAIL

    MOVE.L #-42,D3
    DIVS   #6,D3            * -42 / 6: Quot = -7 ($FFF9), Rem = 0 ($0000)
    CMP.L  #$0000FFF9,D3
    BNE    FAIL

    MOVE.L #-32768,D3
    DIVS   #1,D3            * -32768 / 1: Quot = -32768 ($8000), Rem = 0 ($0000)
    CMP.L  #$00008000,D3
    BNE    FAIL

    * Test 4: Logic & Shifts
    MOVE.L #$F0F0F0F0,D4
    AND.L  #$FF00FF00,D4    * D4 = $F000F000
    LSL.L  #4,D4            * D4 = $000F0000
    CMP.L  #$000F0000,D4
    BNE    FAIL

    * All tests passed!
    LEA    MSG_OK,A1
    MOVE.B #14,D0           * Task 14: Display string
    TRAP   #15
    MOVE.B #9,D0            * Task 9: Halt
    TRAP   #15

FAIL:
    LEA    MSG_FAIL,A1
    MOVE.B #14,D0
    TRAP   #15
    MOVE.B #9,D0
    TRAP   #15

MSG_OK:
    DC.B   'TEST: Arithmetic PASSED',10,0
MSG_FAIL:
    DC.B   'TEST: Arithmetic FAILED',10,0

    END    START
