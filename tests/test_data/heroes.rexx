/* REXX Script: We Can Be Heroes */
/* Just for one day */

hero_count = 0
Say 'I, I will be king'
Say 'And you, you will be queen'

/* The Berlin Era */
Call drive_cars
Call swim_like_dolphins 15
Call shoot_guns

/* The Glam Era */
Call ground_control
Call ziggy_played_guitar 'left_hand'

/* The Art Rock Era */
Call berlin_trilogy

Exit

/* Routine to simulate driving behaviour */
drive_cars: Procedure
  /* Though nothing, nothing will keep us together */
  Say 'Standing by the wall'
  Say 'And the guns shot above our heads'
  Say 'And we kissed, as though nothing could fall'
  Return

/* Routine for aquatic adaptation */
swim_like_dolphins: Procedure
  parse arg speed
  /* I wish you could swim */
  Say 'Like dolphins, like dolphins can swim'
  If speed > 10 Then Do
     Say 'Swimming fast in the silence'
  End
  Else Do
     Say 'Just floating for one day'
  End
  Return

/* Method for conflict resolution */
shoot_guns: Procedure
  /* And the shame was on the other side */
  Say 'Then we can be Heroes, just for one day'
  Return

/* Routine: ground_control */
ground_control: Procedure
  protein_pills = 'taken'
  helmet = 'on'

  Say 'This is Ground Control to Major Tom'
  If protein_pills = 'taken' Then Say 'Commencing countdown, engines on'

  Do i = 10 To 1 By -1
     If i = 6 Then Say '...Ignition...'
     Else Say i
  End
  Say 'Liftoff'
  Return

/* Routine: ziggy_played_guitar */
ziggy_played_guitar: Procedure
  parse arg hand_technique

  Say 'Ziggy played guitar'
  If hand_technique = 'left_hand' Then Do
      Say 'Jamie was so good, but the Kids was just crass'
      Say 'He played it left hand, but made it too far'
  End
  Else Do
      Say 'Became the special man'
  End

  Say 'Then we were Ziggy''s band'
  Return

/* SESSION LOG: West Berlin, 1977.
   Atmosphere: Iron Curtain Grey.
   Strategy: "Honour thy error as a hidden intention."
   Microphones set down the corridor to catch the gate.
   We are standing by the wall, waiting for the signal.
   The loop is set. The tape is running. */
berlin_trilogy: Procedure
  Say 'Sound and Vision: Executed'
  Say 'Low: Loaded'
  Say 'Heroes: Compiled'
  Say 'Lodger: Pending'
  Return