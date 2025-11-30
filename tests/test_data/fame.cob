IDENTIFICATION DIVISION.
       PROGRAM-ID. FAME.
       AUTHOR. DAVID-BOWIE-LENNON.

      * Fame makes a man take things over
      * Fame lets him loose, hard to swallow
      * Fame puts you there where things are hollow

       ENVIRONMENT DIVISION.
       CONFIGURATION SECTION.
       SOURCE-COMPUTER. PLASTIC-SOUL-MAINFRAME.
       OBJECT-COMPUTER. YOUNG-AMERICANS-SERVER.

       DATA DIVISION.
       WORKING-STORAGE SECTION.
       01  FAME-LEVEL          PIC 9(3) VALUE 0.
       01  IS-IT-ANY-WONDER    PIC X(3) VALUE 'YES'.
       01  LIMO-STATUS         PIC X(20) VALUE 'WAITING'.

       PROCEDURE DIVISION.
       MAIN-LOGIC.
           DISPLAY 'Fame, makes a man take things over'.
           DISPLAY 'Fame, lets him loose, hard to swallow'.
           DISPLAY 'Fame, puts you there where things are hollow'.
           DISPLAY 'Fame'.

           PERFORM CHECK-FAME-LEVEL.
           PERFORM RIDE-IN-LIMO.
           PERFORM SIGN-AUTOGRAPHS.

           DISPLAY 'Fame, what you get is no tomorrow'.
           DISPLAY 'Fame, what you need you have to borrow'.
           DISPLAY 'Fame, Fame, Fame, Fame'.

           STOP RUN.

       CHECK-FAME-LEVEL.
           IF FAME-LEVEL > 100
               DISPLAY 'Bully for you, chilly for you'
               DISPLAY 'Got to get a rain check on pain'
           ELSE
               DISPLAY 'Is it any wonder I reject you first?'
           END-IF.

       RIDE-IN-LIMO.
      * Fame, what you like is in the limo
           MOVE 'DRIVING' TO LIMO-STATUS.
           DISPLAY 'Driving in the limo to the show'.

       SIGN-AUTOGRAPHS.
      * Fame, it is not your brain, it is just the flame
           ADD 1 TO FAME-LEVEL.
           DISPLAY 'That burns your change to keep you insane'.