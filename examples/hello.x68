*-----------------------------------------------------------
* Title      : Hello World
* Written by : EASy68K Linux Test
* Description: Prints Hello World using TRAP #15
*-----------------------------------------------------------
    ORG    $1000
START:
    LEA    MSG,A1
    MOVE.B #14,D0       * Task 14: Display null-terminated string with CR/LF
    TRAP   #15

    MOVE.B #9,D0        * Task 9: Halt simulator
    TRAP   #15

MSG:
    DC.B   'Hello, Linux EASy68K!',0

    END    START
