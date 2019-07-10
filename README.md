# MM_VoxLux

Create a display using WS2812B LED strips, a MSGEQ7 IC, and an Arduino Mega controller to create light shows from music.

SOP:
  Setup:
    Initialize LED strips, MSGEQ7 IC, alpha-numeric display, and PBs.
    Load previous display mode from EEPROM
    
  Main:
    Detect if 3.5mm cable is plugged into headphone jack
      If there is a signal source present, then run one of display modes
      If no signal source is present, then run a "screen saver" subroutine (Cylon scanner)
      
    Monitor pushbutton status
      If either PB is pressed, then wake display and monitor for further button presses to change display mode or color mode
