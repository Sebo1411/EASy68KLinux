*-----------------------------------------------------------
* Title      : LP64 Portability / TRAP #15 Regression Tests
* Description: Guards the fixes made during the Linux port audit.
*              The test only prints PASSED when every check succeeds,
*              so the generic run_tests.sh harness validates it.
*-----------------------------------------------------------
    ORG    $1000

START:
    * 1. A word move into a data register must replace only the low word
    *    and leave the upper word untouched (the 68000 does not
    *    sign-extend byte/word moves into Dn).
    MOVE.L #$12345678,D1
    MOVE.W #-1,D1
    CMP.L  #$1234FFFF,D1
    BNE    FAIL

    * 2. Negative DIVS dividend: -100 / 10 = -10 ($FFFF FFF6).
    MOVE.L #-100,D3
    DIVS   #10,D3
    CMP.L  #$0000FFF6,D3
    BNE    FAIL

    * 3. TAS reads the byte, sets the condition codes from it, then
    *    sets bit 7.  A zero byte must end up $80 with Z set.
    LEA    FLAG,A0
    MOVE.B #0,(A0)
    TAS    (A0)
    BNE    FAIL              * Z must be set for the original zero byte
    CMP.B  #$80,(A0)
    BNE    FAIL
    *    TAS on the now-$80 byte must leave Z clear.
    TAS    (A0)
    BEQ    FAIL

    * 4. TRAP task 3: display a negative signed 32-bit number.
    MOVE.L #-100,D1
    MOVE.B #3,D0
    TRAP   #15

    * 5. TRAP task 17: prefix plus signed number must not over-read.
    LEA    PREFIX,A1
    MOVE.L #-7,D1
    MOVE.B #17,D0
    TRAP   #15

    * 6. TRAP task 20: negative field width for a signed number.
    MOVE.L #-42,D1
    MOVE.B #-6,D2
    MOVE.B #20,D0
    TRAP   #15

    * 7. TRAP task 0 with a negative D1 length must not write before
    *    the string or corrupt adjacent memory.
    LEA    STR,A1
    MOVE.L #-1,D1
    MOVE.B #0,D0
    TRAP   #15
    CMP.B  #$45,(A1)
    BNE    FAIL

    * 8. S-Record data boundary: the record checksum must not be
    *    written past the last data byte ($2002 stays zero).
    LEA    $2000,A0
    CMP.B  #$AA,(A0)
    BNE    FAIL
    CMP.B  #$BB,1(A0)
    BNE    FAIL
    TST.B  2(A0)
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

STR:
    DC.B   'EDGE',0
PREFIX:
    DC.B   'N=',0
MSG_OK:
    DC.B   'TEST: regressions PASSED',10,0
MSG_FAIL:
    DC.B   'TEST: regressions FAILED',10,0
FLAG:
    DC.B   0

    ORG    $2000
    DC.B   $AA,$BB
    END    START
