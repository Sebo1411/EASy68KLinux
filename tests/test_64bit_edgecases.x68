*-----------------------------------------------------------
* Title      : 64-bit Porting Edgecases & Regression Test
* Description: Verifies DIVS with negative numbers, indexed
*              addressing with negative offsets, CCR/SR moves,
*              lowercase INCLUDE & INCBIN, and TRAP 15 formats.
*-----------------------------------------------------------
    ORG    $1000

START:
    * ---------------------------------------------------------
    * Test 1: DIVS with negative dividend
    * ---------------------------------------------------------
    * Test 1a: -100 / 7
    * Quotient = -14 ($FFF2), Remainder = -2 ($FFFE)
    * D1 should be $FFFEFFF2
    MOVE.L #-100,D1
    DIVS   #7,D1
    CMP.L  #$FFFEFFF2,D1
    BNE    FAIL

    * Test 1b: -42 / 6
    * Quotient = -7 ($FFF9), Remainder = 0 ($0000)
    * D1 should be $0000FFF9
    MOVE.L #-42,D1
    DIVS   #6,D1
    CMP.L  #$0000FFF9,D1
    BNE    FAIL

    * Test 1c: -1000 / -10
    * Quotient = 100 ($0064), Remainder = 0 ($0000)
    * D1 should be $00000064
    MOVE.L #-1000,D1
    DIVS   #-10,D1
    CMP.L  #$00000064,D1
    BNE    FAIL

    * ---------------------------------------------------------
    * Test 2: Indexed addressing with negative index register
    * ---------------------------------------------------------
    * Test 2a: d(An, Dn.W) with negative Dn.W
    LEA    TABLE_END,A0
    MOVE.W #-4,D0           * index = -4 bytes
    MOVE.B 0(A0,D0.W),D2    * Should load byte at TABLE_END - 4 = $55
    CMP.B  #$55,D2
    BNE    FAIL

    MOVE.W #-2,D0           * index = -2 bytes
    MOVE.B 0(A0,D0.W),D2    * Should load byte at TABLE_END - 2 = $77
    CMP.B  #$77,D2
    BNE    FAIL

    * Test 2b: d(PC, Dn.W) with negative Dn.W
    MOVE.W #-8,D0
    LEA    PC_ANCHOR,A1
    LEA    PC_ANCHOR(PC,D0.W),A2   * A2 should be A1 - 8
    SUBA.L A1,A2
    CMP.L  #-8,A2
    BNE    FAIL

PC_ANCHOR:
    NOP

    * ---------------------------------------------------------
    * Test 3: MOVE to CCR & SR without memory clobbering
    * ---------------------------------------------------------
    MOVE.L #$11223344,D3
    MOVE.L #$55667788,D4

    * Set CCR flags (X, N, Z, V, C) -> $1F
    MOVE.W #$001F,CCR
    * Verify branch on condition works
    BCS    C_SET
    BRA    FAIL
C_SET:
    BVS    V_SET
    BRA    FAIL
V_SET:
    BEQ    Z_SET
    BRA    FAIL
Z_SET:
    BMI    N_SET
    BRA    FAIL
N_SET:
    * Clear CCR flags
    MOVE.W #$0000,CCR
    BNE    Z_CLEAR
    BRA    FAIL
Z_CLEAR:
    BPL    N_CLEAR
    BRA    FAIL
N_CLEAR:
    * Ensure adjacent registers D3, D4 were untouched
    CMP.L  #$11223344,D3
    BNE    FAIL
    CMP.L  #$55667788,D4
    BNE    FAIL

    * ---------------------------------------------------------
    * Test 4: INCLUDE and INCBIN with lowercase paths
    * ---------------------------------------------------------
    INCLUDE inc_lowercase.x68
    MOVE.W #LOWER_MAGIC,D5
    CMP.W  #$ABCD,D5
    BNE    FAIL

    * Test INCBIN data (4 bytes: $12, $34, $56, $78)
    LEA    BIN_DATA,A3
    MOVE.L (A3),D6
    CMP.L  #$12345678,D6
    BNE    FAIL

    * ---------------------------------------------------------
    * Test 5: TRAP #15 Tasks 3, 17, 20 with negative values
    * ---------------------------------------------------------
    * Task 3: Print signed -1
    MOVE.L #-1,D1
    MOVE.B #3,D0
    TRAP   #15

    * Task 17: Print prefix + signed -12345
    LEA    PREFIX17,A1
    MOVE.L #-12345,D1
    MOVE.B #17,D0
    TRAP   #15

    * Task 20: Print signed -42 formatted width 6
    MOVE.L #-42,D1
    MOVE.B #6,D2
    MOVE.B #20,D0
    TRAP   #15

    * All edgecase tests passed!
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

PREFIX17:
    DC.B   ' [Task17:',0
MSG_OK:
    DC.B   '] TEST: 64-bit Edgecases PASSED',10,0
MSG_FAIL:
    DC.B   'TEST: 64-bit Edgecases FAILED',10,0

    * Data table for indexed addressing tests
TABLE_START:
    DC.B   $11, $22, $33, $44, $55, $66, $77, $88
TABLE_END:

BIN_DATA:
    INCBIN inc_data.bin

    END    START
