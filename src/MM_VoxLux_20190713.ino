#include <EEPROM.h>
#include <MSGEQ7.h>
#include <FastLED.h>
#include <Adafruit_LEDBackpack.h>
#include <Adafruit_GFX.h>

#define analogIN A0  //10-bit analog value from MSGEQ7
#define strobeEQ A1  //Strobe output pin to MSGEQ7 to cycle frequency bands
#define resetEQ A2   //Reset output pin to reset MSGEQ7

uint16_t analogVal[10]={0,0,0,0,0,0,0,0,0,0};  //Array to read in analog values from EQ IC

const uint8_t stripsLED = 7;          //7 LED strips, one for each frequency band
const uint8_t numLEDS = 60;           //One LED strip is 60 LEDs/m
const uint8_t halfLEDS = numLEDS / 2; //Half of an LED strip

const uint8_t dataPIN1 = 30; //LED strip data output pin corresponding to 63Hz band
const uint8_t dataPIN2 = 31; //LED strip data output pin corresponding to 160Hz band
const uint8_t dataPIN3 = 32; //LED strip data output pin corresponding to 400Hz band
const uint8_t dataPIN4 = 33; //LED strip data output pin corresponding to 1KHz band
const uint8_t dataPIN5 = 34; //LED strip data output pin corresponding to 2.5KHz band
const uint8_t dataPIN6 = 35; //LED strip data output pin corresponding to 6.25KHz band
const uint8_t dataPIN7 = 36; //LED strip data output pin corresponding to 16KHz band

const uint8_t modePB = 22;  //LED PB to cycle modes
const uint8_t colorPB = 23; //LED PB to cycle color for different modes

/*
  The cntrlMode array is used for controlling the different mode and color combinations for the LED strips.
  To cut down on convoluted code, there are 9 different color settings, only certain color mode are used for certain display modes.  *Add descriptions
  There are 4 different control modes:
    cntrlMode[0] is reserved
    cntrlMode[1] is from the center out
    cntrlMode[2] is in to the center
    cntrlMode[3] is waterfall mode
    cntrlMode[4] is Cylon/Knight Rider or just a Larson Scanner
*/
uint8_t cntrlMode[10];
uint8_t cntrlPntr = 0;
uint8_t cntrlVal = 0;

uint8_t buttonState = 0;      //The current reading from the input pin
uint8_t lastButtonState = 0;  //The previous reading from the input pin
uint8_t reading = 0;

/*
   The following variables are unsigned longs because the time, measured in
   milliseconds, will quickly become a bigger number than can be stored in an int.
*/
unsigned long lastDebounceTime = 0;     //The last time the output pin was toggled
unsigned long debounceDelay = 50;       //The debounce time; increase if the output flickers
unsigned long displayOffDelay = 10000;  //Turn off display after 10s
unsigned long lastUpdateTime = 0;       //The last time the waterfall mode was updated
unsigned long updateTime = 1;           //Waterfall mode update time

CRGB leds[stripsLED][numLEDS];  //Create LED object

Adafruit_AlphaNum4 setDisplay = Adafruit_AlphaNum4();  //Create display object

CMSGEQ7<191, resetEQ, strobeEQ, analogIN> MSGEQ7;  //Create MSGEQ7 object

void setup()
{
  FastLED.addLeds<NEOPIXEL, dataPIN1>(leds[0], numLEDS);  //Initialize LED strip for 63Hz band
  FastLED.addLeds<NEOPIXEL, dataPIN2>(leds[1], numLEDS);  //Initialize LED strip for 160Hz band
  FastLED.addLeds<NEOPIXEL, dataPIN3>(leds[2], numLEDS);  //Initialize LED strip for 400Hz band
  FastLED.addLeds<NEOPIXEL, dataPIN4>(leds[3], numLEDS);  //Initialize LED strip for 1KHz band
  FastLED.addLeds<NEOPIXEL, dataPIN5>(leds[4], numLEDS);  //Initialize LED strip for 2.5KHz band
  FastLED.addLeds<NEOPIXEL, dataPIN6>(leds[5], numLEDS);  //Initialize LED strip for 6.25KHz band
  FastLED.addLeds<NEOPIXEL, dataPIN7>(leds[6], numLEDS);  //Initialize LED strip for 16KHz band

  FastLED.setDither(0);  //Dithering should smooth out colors on strips to eliminate any flashing effects
  
  for (uint8_t x = 0; x < stripsLED; x++)  //Initialize LED strips so that they're blank
  {
    fill_solid(leds[x], numLEDS, CRGB::Black);
  }
  FastLED.show();

  setDisplay.begin(0x70);     //Initialize display address on comm bus
  setDisplay.clear();         //Make sure display is cleared
  setDisplay.writeDisplay();  //Write command to display

  MSGEQ7.begin();

  pinMode(modePB, INPUT_PULLUP);   //Initialize mode PB (Blue) input with pullup resistors (HIGH=OFF, LOW=ON)
  pinMode(colorPB, INPUT_PULLUP);  //Initialize color PB (Green) input with pullup resistors (HIGH=OFF, LOW=ON)

  cntrlPntr = EEPROM.read(0);
  cntrlVal = EEPROM.read(2);
  cntrlMode[cntrlPntr] = cntrlVal;
}

void loop()
{
  Button();
  AnalogRead();
  DisplayWrite();
  ControlLED();

  FastLED.show();             //Update LED strips
  setDisplay.writeDisplay();  //Write command to display
}

void Button()
{
  bitWrite(reading, 0, digitalRead(modePB));   //Read the state of the mode PB (Blue) into a local variable
  bitWrite(reading, 4, digitalRead(colorPB));  //Read the state of the color PB (Green) into a local variable

  //Check to see if you just pressed the button (i.e. the input went from HIGH to LOW), and you've waited long enough since the last press to ignore any noise

  if (reading != lastButtonState)  //If the PB state changed, due to noise or pressing, then reset the debounce timer
  {
    lastDebounceTime = millis();
  }

  if ((millis() - lastDebounceTime) > debounceDelay)
  {
    // whatever the reading is at, it's been there for longer than the debounce delay, so take it as the actual current state:

    if ((reading & 0B00000001) != (buttonState & 0B00000001))  //If the mode PB (Blue) state has changed
    {
      buttonState = reading;
      if (cntrlMode[9] == 0)
      {
        cntrlMode[9] = 1;
        return;
      }
      if (cntrlMode[9] == 1)
      {
        if (cntrlPntr < 3)
        {
          cntrlPntr++;
        }
        else if (cntrlPntr == 3)
        {
          cntrlPntr = 1;
        }
      }
    }
    
    if ((reading & 0B00010000) != (buttonState & 0B00010000))  //If the color PB (Green) state has changed
    {
      buttonState = reading;
      if (cntrlMode[9] == 0)
      {
        cntrlMode[9] = 1;
        return;
      }
      if (cntrlMode[9] == 1)
      {
        if (cntrlPntr==1)
        {
          if (cntrlMode[1] < 9)
          {
            cntrlMode[1]++;
          }
          else if (cntrlMode[1]==9)
          {
            cntrlMode[1]=1;
          }
        }
        if (cntrlPntr==2)
        {
          if (cntrlMode[2] < 9)
          {
            cntrlMode[2]++;
          }
          else if (cntrlMode[2]==9)
          {
            cntrlMode[2]=1;
          }
        }
        if (cntrlPntr==3)
        {
          if (cntrlMode[3] < 2)
          {
            cntrlMode[3]++;
          }
          else if (cntrlMode[3]==2)
          {
            cntrlMode[3]=1;
          }
        }
        if (cntrlPntr==4)
        {
          if (cntrlMode[4] == 2)
          {
            cntrlMode[4]=4;
          }
          else if (cntrlMode[4]>3 && cntrlMode[4]<9)
          {
            cntrlMode[4]++;
          }
          else if (cntrlMode[4]==9)
          {
            cntrlMode[4]=2;
          }
        }
      }
    }
  }
  
  if(cntrlMode[1]>9 || cntrlMode[1]<1)
  {
    cntrlMode[1]=1;
  }
  if(cntrlMode[2]>9 || cntrlMode[2]<1)
  {
    cntrlMode[2]=1;
  }
  if(cntrlMode[3]>2 || cntrlMode[3]<1)
  {
    cntrlMode[3]=1;
  }
  if(cntrlMode[4]>9 || cntrlMode[4]==3 || cntrlMode[4]<2)
  {
    cntrlMode[4]=2;
  }
  
  if ((millis() - lastDebounceTime) > displayOffDelay)
  {
    cntrlVal=cntrlMode[cntrlPntr];
    cntrlMode[9]=0;
    EEPROM.update(cntrlPntr,0);
    EEPROM.update(cntrlVal,2);
  }
  
  lastButtonState = reading;  // save the reading. Next time through the loop, it'll be the lastButtonState
}

void AnalogRead()
{
  bool newReading = MSGEQ7.read();

  if (newReading)
  {
    analogVal[0] = MSGEQ7.get(MSGEQ7_0);
    analogVal[1] = MSGEQ7.get(MSGEQ7_1);
    analogVal[2] = MSGEQ7.get(MSGEQ7_2);
    analogVal[3] = MSGEQ7.get(MSGEQ7_3);
    analogVal[4] = MSGEQ7.get(MSGEQ7_4);
    analogVal[5] = MSGEQ7.get(MSGEQ7_5);
    analogVal[6] = MSGEQ7.get(MSGEQ7_6);
  }
  for(uint8_t x=0; x<7; x++)
  {
    analogVal[x] = mapNoise(analogVal[x]);
  }
}

//The alphanumeric display is ordered [0][1](Mode/Blue)[2][3](Color/Green)
void DisplayWrite()
{
  if (cntrlMode[9]==1)
  {
    switch (cntrlPntr)
    {
      case 1:
        setDisplay.writeDigitAscii(0, 'C');
        setDisplay.writeDigitAscii(1, 'O');
        break;
      case 2:
        setDisplay.writeDigitAscii(0, 'C');
        setDisplay.writeDigitAscii(1, 'I');
        break;
      case 3:
        setDisplay.writeDigitAscii(0, 'W');
        setDisplay.writeDigitAscii(1, 'a');
        break;
      case 4:
        setDisplay.writeDigitAscii(0, 'K');
        setDisplay.writeDigitAscii(1, 'R');
        break;
    }
    switch (cntrlMode[cntrlPntr])
    {
      case 1:
        setDisplay.writeDigitAscii(2, 'R');
        setDisplay.writeDigitAscii(3, '7');
        break;
      case 2:
        setDisplay.writeDigitAscii(2, 'H');
        setDisplay.writeDigitAscii(3, 'u');
        break;
      case 3:
        setDisplay.writeDigitAscii(2, 'R');
        setDisplay.writeDigitAscii(3, 's');
        break;
      case 4:
        setDisplay.writeDigitAscii(2, 'R');
        setDisplay.writeDigitAscii(3, 'E');
        break;
      case 5:
        setDisplay.writeDigitAscii(2, 'O');
        setDisplay.writeDigitAscii(3, 'R');
        break;
      case 6:
        setDisplay.writeDigitAscii(2, 'Y');
        setDisplay.writeDigitAscii(3, 'E');
        break;
      case 7:
        setDisplay.writeDigitAscii(2, 'G');
        setDisplay.writeDigitAscii(3, 'R');
        break;
      case 8:
        setDisplay.writeDigitAscii(2, 'B');
        setDisplay.writeDigitAscii(3, 'L');
        break;
      case 9:
        setDisplay.writeDigitAscii(2, 'P');
        setDisplay.writeDigitAscii(3, 'U');
        break;
    }
  }
  
  if(cntrlMode[9]==0)
  {
    setDisplay.clear();
  }
}

void ControlLED()
{
  switch(cntrlPntr)
  {
    case 1:
      for (uint8_t x = 0; x < stripsLED; x++)
      {
        uint16_t led_length = map(analogVal[x], 0, 1023, 0, halfLEDS);
        uint8_t start = halfLEDS - led_length;
        uint8_t finish = halfLEDS + led_length;

        for (uint8_t y = 0; y < numLEDS; y++)
        {
          leds[x][y].fadeToBlackBy(128);
        }
        for (uint8_t y = start; y < finish; y++)
        {
          leds[x][y] = ColorSet(analogVal[x], x);
        }
      }
      break;
    case 2:
      for (uint8_t x = 0; x < stripsLED; x++)
      {
        uint16_t led_length = map(analogVal[x], 0, 1023, 0, halfLEDS);
        uint8_t finish = numLEDS - led_length;

        for (uint8_t y = 0; y < numLEDS; y++)
        {
          leds[x][y].fadeToBlackBy(128);
        }
        for (uint8_t y = 0; y < led_length; y++)
        {
          leds[x][y] = ColorSet(analogVal[x], x);
        }
        for (uint8_t y = numLEDS - 1; y >= finish; y--)
        {
          leds[x][y] = ColorSet(analogVal[x], x);
        }
      }
      break;
    case 3:
      if(millis() - lastUpdateTime > updateTime)
      {
        for (uint8_t x = 0; x < stripsLED; x++)
        {
          for (uint8_t y = 0; y < halfLEDS - 1; y++)
          {
            leds[x][y] = leds[x][y + 1];
          }

          leds[x][halfLEDS - 1] = ColorSet(analogVal[x], x);

          for (uint8_t y = numLEDS - 1; y > halfLEDS; y--)
          {
            leds[x][y] = leds[x][y - 1];
          }

          leds[x][halfLEDS] = ColorSet(analogVal[x], x);
        }
      lastUpdateTime=millis();
      }
      break;
    if(cntrlPntr==4)
    {
    //for()
    }
  }
}

/*
  32-bit color value to pass back to control loop
  If single LED at start of control loop is lit then try:
    if(c<128)
    {
      led_color=CRGB::Black;
      return led_color;
    }
    else
    {
      *case statement
    }
*/
uint32_t ColorSet(uint16_t c, uint8_t i)
{
  CRGB led_color;
  uint16_t color=0;
  
  switch(cntrlMode[cntrlPntr])
  {
    case 1:
      color=map(c,0,1023,0,7);
      switch(color)
      {
        case 0:
          led_color = CRGB::Black;
          break;
        case 1:
          led_color = CRGB::Purple;
          break;
        case 2:
          led_color = CRGB::BlueViolet;
          break;
        case 3:
          led_color = CRGB::Blue;
          break;
        case 4:
          led_color = CRGB::Green;
          break;
        case 5:
          led_color = CRGB::Yellow;
          break;
        case 6:
          led_color = CRGB::Orange;
          break;
        case 7:
          led_color = CRGB::Red;
          break;
      }
      break;
    case 2:
      color=map(c,0,1023,0,255);
      led_color=(color,255,255);
      break;
    case 3:
      switch(i)
      {
        case 0:
          led_color=CHSV(HUE_RED,255,255);
          break;
        case 1:
          led_color=CHSV(HUE_ORANGE,255,255);
          break;
        case 2:
          led_color=CHSV(HUE_YELLOW,255,255);
          break;
        case 3:
          led_color=CHSV(HUE_GREEN,255,255);
          break;
        case 4:
          led_color=CHSV(HUE_AQUA,255,255);
          break;
        case 5:
          led_color=CHSV(HUE_BLUE,255,255);
          break;
        case 6:
          led_color=CHSV(HUE_PURPLE,255,255);
          break;
      }
      break;
    case 4:
      led_color=CHSV(HUE_RED,255,255);
      break;
    case 5:
      led_color=CHSV(HUE_ORANGE,255,255);
      break;
    case 6:
      led_color=CHSV(HUE_YELLOW,255,255);
      break;
    case 7:
      led_color=CHSV(HUE_GREEN,255,255);
      break;
    case 8:
      led_color=CHSV(HUE_BLUE,255,255);
      break;
    case 9:
      led_color=CHSV(HUE_PURPLE,255,255);
      break;
  }
  
  return led_color;
}
