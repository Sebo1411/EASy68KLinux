*-----------------------------------------------------------
* Title      : EASy68K Hardware Simulation Demo
* Description: Demonstrates 7-Segment Displays and LED Bank
*-----------------------------------------------------------
    ORG    $1000

START:
    * 1. Write 8 digits to 7-segment display ($E00000..$E0000E)
    * Digit 1: '6' = $7D
    MOVE.B #$7D,$E00000
    * Digit 2: '8' = $7F
    MOVE.B #$7F,$E00002
    * Digit 3: '0' = $3F
    MOVE.B #$3F,$E00004
    * Digit 4: '0' = $3F
    MOVE.B #$3F,$E00006
    * Digit 5: '0' = $3F
    MOVE.B #$3F,$E00008
    * Digit 6: '-' = $40
    MOVE.B #$40,$E0000A
    * Digit 7: 'O' = $3F
    MOVE.B #$3F,$E0000C
    * Digit 8: 'K' = $77
    MOVE.B #$77,$E0000E

    * 2. Write alternating pattern to LED bank ($E00010)
    MOVE.B #$A5,$E00010     * Binary 10100101

    * 3. Print message to terminal
    LEA    MSG,A1
    MOVE.B #14,D0
    TRAP   #15

    * 4. Halt simulation
    MOVE.B #9,D0
    TRAP   #15

MSG:
    DC.B   'Hardware Demo initialized: 7-segment and LEDs set!',10,0

    END    START
