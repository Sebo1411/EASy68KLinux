*-----------------------------------------------------------
* Title      : EASy68K 2D Graphics Canvas Demo
* Description: Demonstrates TRAP #15 graphics tasks
*-----------------------------------------------------------
    ORG    $1000

START:
    * 1. Set line color to Cyan ($00FFFF00)
    MOVE.L #$00FFFF00,D1
    MOVE.B #80,D0
    TRAP   #15

    * 2. Set fill color to Dark Blue ($00802010)
    MOVE.L #$00802010,D1
    MOVE.B #81,D0
    TRAP   #15

    * 3. Draw filled background rectangle (X1=20, Y1=20, X2=620, Y2=460)
    MOVE.W #20,D1
    MOVE.W #20,D2
    MOVE.W #620,D3
    MOVE.W #460,D4
    MOVE.B #87,D0
    TRAP   #15

    * 4. Set line color to Yellow ($0000FFFF)
    MOVE.L #$0000FFFF,D1
    MOVE.B #80,D0
    TRAP   #15

    * Set fill color to Red ($000000FF)
    MOVE.L #$000000FF,D1
    MOVE.B #81,D0
    TRAP   #15

    * 5. Draw filled ellipse (center area: 240,160 to 400,320)
    MOVE.W #240,D1
    MOVE.W #160,D2
    MOVE.W #400,D3
    MOVE.W #320,D4
    MOVE.B #88,D0
    TRAP   #15

    * 6. Draw diagonal accent line (X1=40, Y1=40, X2=600, Y2=440)
    MOVE.L #$00FFFFFF,D1    * White line
    MOVE.B #80,D0
    TRAP   #15
    MOVE.W #40,D1
    MOVE.W #40,D2
    MOVE.W #600,D3
    MOVE.W #440,D4
    MOVE.B #84,D0
    TRAP   #15

    * 7. Draw text on canvas (Task 95)
    LEA    TITLE_TXT,A1
    MOVE.W #180,D1
    MOVE.W #80,D2
    MOVE.B #95,D0
    TRAP   #15

    * 8. Terminal message
    LEA    TERM_MSG,A1
    MOVE.B #14,D0
    TRAP   #15

    * 9. Halt
    MOVE.B #9,D0
    TRAP   #15

TITLE_TXT:
    DC.B   'EASy68K Native Linux 2D Graphics Canvas!',0
TERM_MSG:
    DC.B   'Graphics Demo drawn successfully on 640x480 canvas!',10,0

    END    START
