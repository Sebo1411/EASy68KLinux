*-----------------------------------------------------------
* Title      : TRAP #15 Tasks Test
* Description: Verifies multiple TRAP #15 services
*-----------------------------------------------------------
    ORG    $1000

START:
    * Task 8: Get time in hundredths of seconds since midnight
    MOVE.B #8,D0
    TRAP   #15
    TST.L  D1
    BEQ    FAIL             * Time should be non-zero during the day

    * Task 3: Display signed number in D1.L (positive and negative)
    MOVE.L #12345,D1
    MOVE.B #3,D0
    TRAP   #15

    MOVE.L #-12345,D1
    MOVE.B #3,D0
    TRAP   #15

    * Task 17: Display string then signed number
    LEA    MSG_T17,A1
    MOVE.L #-1,D1
    MOVE.B #17,D0
    TRAP   #15

    * Task 20: Display signed number formatted with width
    MOVE.L #-999,D1
    MOVE.B #8,D2
    MOVE.B #20,D0
    TRAP   #15

    * Task 14: Display string
    LEA    MSG_OK,A1
    MOVE.B #14,D0
    TRAP   #15

    * Task 9: Halt simulator
    MOVE.B #9,D0
    TRAP   #15

FAIL:
    LEA    MSG_FAIL,A1
    MOVE.B #14,D0
    TRAP   #15
    MOVE.B #9,D0
    TRAP   #15

MSG_T17:
    DC.B   ' [T17:',0
MSG_OK:
    DC.B   '] TEST: TRAP 15 PASSED',10,0
MSG_FAIL:
    DC.B   'TEST: TRAP 15 FAILED',10,0

    END    START
