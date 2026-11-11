*-----------------------------------------------------------
* Title      : Subroutine and Stack Test
* Description: Verifies BSR, RTS, LINK, UNLK and recursion
*-----------------------------------------------------------
    ORG    $1000

START:
    * Compute Fibonacci(8):
    * F(0)=0, F(1)=1, F(2)=1, F(3)=2, F(4)=3, F(5)=5, F(6)=8, F(7)=13, F(8)=21 ($15)
    MOVE.L #8,D1
    BSR    FIB
    CMP.L  #21,D0
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

* Fibonacci function: Input in D1, Output in D0
FIB:
    LINK   A6,#-8           * Allocate local stack frame
    MOVEM.L D2/D3,-(SP)     * Save registers

    CMP.L  #1,D1
    BGT    FIB_REC
    MOVE.L D1,D0            * Base case: F(0)=0, F(1)=1
    BRA    FIB_RET

FIB_REC:
    MOVE.L D1,D2            * D2 = N
    SUBQ.L #1,D1            * N - 1
    BSR    FIB
    MOVE.L D0,D3            * D3 = FIB(N-1)

    MOVE.L D2,D1
    SUBQ.L #2,D1            * N - 2
    BSR    FIB              * D0 = FIB(N-2)
    ADD.L  D3,D0            * D0 = FIB(N-1) + FIB(N-2)

FIB_RET:
    MOVEM.L (SP)+,D2/D3     * Restore registers
    UNLK   A6               * Restore stack frame
    RTS

MSG_OK:
    DC.B   'TEST: Subroutines & Recursion PASSED',10,0
MSG_FAIL:
    DC.B   'TEST: Subroutines & Recursion FAILED',10,0

    END    START
