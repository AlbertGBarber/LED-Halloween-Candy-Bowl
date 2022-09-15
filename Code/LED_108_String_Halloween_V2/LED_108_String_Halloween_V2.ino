//This code is placed under the MIT license
//Copyright (c) 2021 Albert Barber
//
//Permission is hereby granted, free of charge, to any person obtaining a copy
//of this software and associated documentation files (the "Software"), to deal
//in the Software without restriction, including without limitation the rights
//to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
//copies of the Software, and to permit persons to whom the Software is
//furnished to do so, subject to the following conditions:
//
//The above copyright notice and this permission notice shall be included in
//all copies or substantial portions of the Software.
//
//THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
//IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
//FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
//AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
//LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
//OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
//THE SOFTWARE.

//code intended to run on esp8266, Wemos D1 mini
//requires lastest version of adafruit neopixel library (use the library manager)
#include <PixelStrip.h>
#include <EEPROM.h>
#include <Ticker.h>

//toggles for enabling buttons and EEPROM
#define BUTTONS_ENABLE  false
#define EEPROM_ENABLE   false

//enable to test the RGB order of the pixels
//will cycle the strip through the halloween pallet
//the first color should be purple, if it isn't
//you need to change the stripType var to
//NEO_RGB or NEO_GRB
#define RGBCOLORTEST         false

//total number of effects (change this if you add any!)
#define NUM_EFFECTS     10

//pin connections
#define PIXEL_PIN       D8
#define BUTTON_1        D6
#define BUTTON_2        D7
#define BUTTON_3        D5

//below vars are placeholders for possible bluetooth and mic board connections
#define MIC_IN          A0
#define RX_BT           D1
#define TX_BT           D2

//EEPROM Addresses for settings
//we want to store the brightness, current effect index, and the effect rotation toggle
//so they can be recalled after the mask has been turned off
#define BRIGHTNESS_ADDR 2 //brightness address
#define CUR_EFFECT_ADDR 0 //index of current effect address
#define EFFECT_ROT_ADDR 1 //effect rotaion bool address
#define EEPROM_COM_TIME 3000 //ms

//effects control vars
byte effectIndex = 0; //number of effect that's currently active (will be read from EEPROM later)
boolean effectRota = true; //effects rotation on / off flag
boolean effectsStop = false; //stop all effects flag
boolean direct = true; //use for setting direction of effects
boolean breakCurrentEffect = false; //flag for breaking out of effects that use multiple sub effects / loops

//macro for implementing break for effects with multiple sub effects
#define breakEffectCheck() ({ \
    if( (breakCurrentEffect) ){ \
      (breakCurrentEffect) = false; \
      break; \
    } \
  })

//brightness vars
byte brightnessIndex = 2; //initial brightness, index of brightnessLevels array
//brightness levels array, max is 255, but 100 should be bright enough for amost all cases
//!!WARNING brightness is directly tied to power consumption, the max current per led is 60ma, this is for white at 255 brightness
//if you actually run all the leds at max, the glasses will draw 4.75 amps, this is beyond the rating of the jst connectors
const byte brightnessLevels[] = { 10, 30, 80, 120, 170 };
const byte numBrightnessLevels = SIZE( brightnessLevels );

//Strip definitions
const uint16_t stripLength = 108; //bottom ring is 21 pixels
const uint8_t stripType = NEO_GRB + NEO_KHZ800;
PixelStrip strip = PixelStrip(stripLength, PIXEL_PIN, stripType);

//initilize ticker objects
Ticker EEPROMcommiter; //timer for commiting data to EEPRROM

//segment definitions
//==================================================================================

segmentSection sec0[] = {{0, 26}};
Segment segment0 = { SIZE(sec0), sec0, true };

Segment *mainSegments_arr[] = { &segment0 };
SegmentSet mainSegments = { SIZE(mainSegments_arr), mainSegments_arr };

//----------------------------------------------------------------------------------
//Rough Ring segments
segmentSection ringSec0[] = {{0, 26}};
Segment ringSeg0 = { SIZE(ringSec0), ringSec0, true };

segmentSection ringSec1[] = {{53, 22}};
Segment ringSeg1 = { SIZE(ringSec1), ringSec1, true };

segmentSection ringSec2[] = {{75, 12}};
Segment ringSeg2 = { SIZE(ringSec2), ringSec2, true };

segmentSection ringSec3[] = {{87, 21}};
Segment ringSeg3 = { SIZE(ringSec3), ringSec3, false };

Segment *ringSegments_arr[] = { &ringSeg0, &ringSeg1, &ringSeg2, &ringSeg3 };
SegmentSet ringSegments = { SIZE(ringSegments_arr), ringSegments_arr };

//==================================================================================

//Define some colors we'll use frequently
const uint32_t white = strip.Color(255, 255, 255);
const uint32_t off = 0;
const uint32_t orangeHalwen = strip.Color(250, 65, 19); //strip.Color(253,119,8);
const uint32_t yellowHalwen = strip.Color(253, 229, 0);
const uint32_t greenHalwen = strip.Color( 43, 208, 17 );
const uint32_t pastelgreen = strip.Color(120, 212, 96); //green
const uint32_t purpleHalwen = strip.Color(174, 3, 255); //strip.Color(137,41,191);
const uint32_t blueHalwen = strip.Color( 16, 124, 126 );
const uint32_t brownOrangHalwen = strip.Color(92, 16, 9);

//main halloween colors used in the effects
uint32_t halloweenPallet[] = { greenHalwen, purpleHalwen, orangeHalwen, pastelgreen, off };
//                              -----0-----  ------1-----  -----2------  ------3---- -4--
//the halloween pallet minus the off
uint32_t halloweenSubPallet[] = { greenHalwen, purpleHalwen, orangeHalwen, pastelgreen };
//                                -----0-----  ------1-----  -----2------  ------3----

uint32_t firePallet[] = { strip.Color(150, 0, 0), strip.Color(255, 0, 0), strip.Color(255, 143, 0), strip.Color(253, 186, 0) }; //strip.Color(255, 255, 100)

byte wavepattern[]  = { 0, 0, 0, 1, 1, 1};
byte wavepattern2[] = { 2, 2, 2, 0, 0, 0 };
byte wavepattern3[] = { 3, 3, 3, 2, 2, 2 };
byte wavepattern4[] = { 2, 2, 2, 1, 1, 1 };
byte halloweenWavePattern[4] = { 0, 1, 2, 3 };

//for simple repeart drawing functions
//using halloween pallet
byte simpleRepeatPattern[5] = {0, 1, 2, 3};
byte simpleRepeatPattern2[] = {0, 0, 0, 4, 4, 1, 1, 1, 4, 4, 2, 2, 2, 4, 4, 3, 3, 3, 4, 4};

byte halloweenStreamerPattern[8] = { 4, 4, 4, 4, 4, 4, 4, 4};
byte halloweenStreamerPatternColors[4] = { 0, 1, 2, 3 };

uint32_t tempRandPallet[4];

//callback routine for committing EEPROM data
//EEPRROM has limited writes, so we want to commit all the writes after a delay
//this allows the data to change while the user is pressing buttons, and we'll only commit
//the final values once they are done
void ICACHE_RAM_ATTR commitEEPROM() {
  EEPROM.commit();
}

//triggered by button 1, stops the current pattern, and switches to the next one, wrapping if needed
//also stores effect index in eeprom
//if button 2 is also being held, turn effects on / off
void ICACHE_RAM_ATTR nextEffect() {
  strip.pixelStripStopPattern = true; //stop the current pattern
  breakCurrentEffect = true; //set flag to break out of current case statement
  //if we are rotating to the next effect, reset all the segment directions
  //and store the next effect index in eeprom
  if (effectRota) {
    resetSegDirections();
    EEPROM.write(CUR_EFFECT_ADDR, (effectIndex + 1) % NUM_EFFECTS);
    //stop any other commit timers and start a new one
    EEPROMcommiter.detach();
    EEPROMcommiter.once_ms(EEPROM_COM_TIME, commitEEPROM);
  }
  //if button 2 is being held, stop/start the effect cycle
  //otherwise increase the index to start the next effect
  if ( digitalRead(BUTTON_2) == LOW) {
    effectsStop = !effectsStop;
  }
}

//triggered by button 2, turns effect rotation on / off
//also stores the state in eeprom
//(if rotation is off, the current effect will be repeated continuously)
//if button 1 is also being held, turn effects on / off
void ICACHE_RAM_ATTR effectRotaToggle() {
  if ( digitalRead(BUTTON_1) == LOW) {
    strip.pixelStripStopPattern = true;
    effectsStop = !effectsStop;
  } else {
    effectRota = !effectRota;
    EEPROM.write(EFFECT_ROT_ADDR, effectRota);
    //stop any other commit timers and start a new one
    EEPROMcommiter.detach();
    EEPROMcommiter.once_ms(EEPROM_COM_TIME, commitEEPROM);
  }
}

//triggered by button 3, sets the strip brightness to the next
//also stores the brighness index in eeprom
//brightness level in the brightnessLevels array (wrapping to the start if needed)
void ICACHE_RAM_ATTR brightnessAdjust() {
  brightnessIndex = (brightnessIndex + 1) % numBrightnessLevels;
  strip.setBrightness( brightnessLevels[ brightnessIndex ] );
  EEPROM.write(BRIGHTNESS_ADDR, brightnessIndex);
  //stop any other commit timers and start a new one
  EEPROMcommiter.detach();
  EEPROMcommiter.once_ms(EEPROM_COM_TIME, commitEEPROM);
}

//increments the effect index (wrapping if needed)
void incrementEffectIndex() {
  strip.runRainbowOffsetCycle(false);
  strip.setRainbowOffset(0);
  resetBrightness();
  effectIndex = (effectIndex + 1) % NUM_EFFECTS;
}

//resets all the segments to their default directions (as set in segmentDefs)
void resetSegDirections() {

}

void setup() {
  //initalize the led strip, and set the starting brightness
  strip.begin();

  if (BUTTONS_ENABLE) {
    pinMode(BUTTON_1, INPUT_PULLUP);
    pinMode(BUTTON_2, INPUT_PULLUP);
    pinMode(BUTTON_3, INPUT_PULLUP);
    //because of the way my library currently works, effects occupy the processor until they end
    //so to break out of an effect, or change sytem values, we need to use interrupts
    attachInterrupt(digitalPinToInterrupt(BUTTON_1), nextEffect, FALLING);
    attachInterrupt(digitalPinToInterrupt(BUTTON_2), effectRotaToggle, FALLING);
    attachInterrupt(digitalPinToInterrupt(BUTTON_3), brightnessAdjust, FALLING);
  }

  if (EEPROM_ENABLE && BUTTONS_ENABLE) {
    // EEPROM Initialization
    EEPROM.begin(512);

    //read EEPROM values for current effect, brightness, and effect rotation
    effectRota = EEPROM.read(EFFECT_ROT_ADDR);
    brightnessIndex = EEPROM.read(BRIGHTNESS_ADDR);
    effectIndex = EEPROM.read(CUR_EFFECT_ADDR);
  }

  strip.setBrightness( brightnessLevels[brightnessIndex] );
  strip.show();
  //Serial.begin(115200);

  randomSeed(ESP.getCycleCount()); //generate a random seed
  //fill in our random pallet
  strip.genRandPallet( tempRandPallet, SIZE(tempRandPallet) );
}

//!! If you want to change the main loop code, please read all the comments below and in the loop !!
//To remove an effect, simply change its case # to anything greater than the total number of effects (999 for ex)
//if you want to know about certain effects, please see comments in the library for said effect
//if you want to know how segments work, please see comments in segmentSet.h

//The main loop of the program, works as follows:
//if effectsStop is true, effects are "off", so we won't try to start the next effect
//otherwise we jump to the effect matching the effectIndex value using a switch statment
//we also "clean up" a bit by reseting direct to true and the breakCurrentEffect flag to false
//if we don't find an effect with the effectIndex, we increment the effectIndex until we do
//while an effect is running, button inputs can either lock the effect or skip to the next effect
//if we lock the effect (set effectRota to false), we do not increment effectIndex when the effect ends, essentially restarting the effect (with new colors if they're randomly choosen)
//if the effect is skipped, we set strip.pixelStripStopPattern and breakCurrentEffect to true
//strip.pixelStripStopPattern will stop the current effect, and breakCurrentEffect will break out of the current switch statement if needed (the switch case has more than one effect)
//once the effect ends (either naturally or from a button press), we incremented effectIndex (as long as effectRota is set true)
//and jump to the top of the main loop
void loop() {
  //for testing the order of the RGB bytes in the pixels
  if(RGBCOLORTEST){
    strip.crossFadeCycle(halloweenWavePattern, SIZE(halloweenWavePattern), halloweenPallet, 10, 50, 40);
  }
  if (!effectsStop) { //if effectsStop is true, we won't display any effect
    direct = !direct;
    breakCurrentEffect = false;
    resetBrightness();
    //switch statment contains all effects
    //I'm not going to comment each one, as they're hard to describe
    //if an case has a loop, it generally means the effect will by run multiple times in diff directions
    //these will contain breakEffectCheck(); which will breakout of the case if the effect is skipped by button input
    switch (effectIndex) { //select the next effect based on the effectIndex
      case 0:
        for (int i = 0; i <= 3; i++) {
          breakEffectCheck();
          strip.colorSpinSimple(ringSegments, 4, 0, 0, 4, -1, 2, 1, 1, 50, 90);
          ringSegments.flipSegDirectionEvery(1, true);
        }
        break;
      case 1:
        breakEffectCheck();
        //strip.shiftingSea(firePallet, SIZE(firePallet), 30, 0, 5, 600, 50);
        strip.shiftingSea(halloweenPallet, SIZE(halloweenPallet), 30, 0, 5, 600, 50);
        break;
      case 2:
        breakEffectCheck();
        for (int i = 0; i <= 3; i++) {
          breakEffectCheck();
          direct = !direct;
          strip.segGradientCycleRand(ringSegments, 3, 8, 100, direct, 2, 70);
        }
        break;
      case 3:
        breakEffectCheck();
        strip.doRepeatSimplePattern(wavepattern, SIZE(wavepattern), halloweenPallet, SIZE(halloweenPallet), 20, 15, 30, direct);
        breakEffectCheck();
        direct = !direct;
        strip.doRepeatSimplePattern(wavepattern2, SIZE(wavepattern2), halloweenPallet, SIZE(halloweenPallet), 20, 15, 30, direct);
        breakEffectCheck();
        direct = !direct;
        strip.doRepeatSimplePattern(wavepattern3, SIZE(wavepattern3), halloweenPallet, SIZE(halloweenPallet), 20, 15, 30, direct);
        breakEffectCheck();
        direct = !direct;
        strip.doRepeatSimplePattern(wavepattern4, SIZE(wavepattern4), halloweenPallet, SIZE(halloweenPallet), 20, 15, 30, direct);
        break;
      case 4:
        breakEffectCheck();
        strip.fixedLengthRainbowCycle(stripLength, true, 300, 70);
        break;
      case 5:
        for (int i = 0; i <= 3; i++) {
          breakEffectCheck();
          direct = !direct;
          strip.gradientCycleRand( 4, 25, 350, direct, 40);
        }
        break;
      case 6:
        for (int i = 0; i <= 3; i++) {
          breakEffectCheck();
          strip.rainbow(20);
        }
        break;
      case 7:
        for (int i = 0; i < 3; i++) {
          breakEffectCheck();
          strip.genRandPallet( tempRandPallet, SIZE(tempRandPallet) );
          tempRandPallet[0] = 0;
          strip.shiftingSea(tempRandPallet, SIZE(tempRandPallet), 30, 0, 5, 300, 50);
        }
        break;
      case 8:
        //for (int i = 0; i < 3; i++) {
          //breakEffectCheck();
          //strip.patternSweepRand( 3, -1, 0, 1, 25, false, 0, 2, 60, 150); //rainbow trails
        //}
        break;
      case 9:
        breakEffectCheck();
        strip.patternSweepSetRand( 8, halloweenSubPallet, SIZE(halloweenSubPallet), 0, 0, 0, false, 3, 1, 80, 280 );
        break;
      default:
        break;
    }
    //if effectRota is true we'll advance to the effect index
    if (effectRota) {
      incrementEffectIndex();
    }
    strip.stripOff(); //clear the strip for the next effect
  }

  //unused effects

  //strip.solidRainbowCycle(20, 4);

  //for (int i = 0; i < 2; i++) {
          //breakEffectCheck();
          //direct = !direct;
          //strip.patternSweepRepeatRand(4, 0, 0, 2, 10, false, direct, 0, 0, 1, 70, 150 );
  //}
        
  //setTempBrightness(4);
  //strip.twinkleSet(0, halloweenPallet, SIZE(halloweenPallet), 2, 80, 40, 24000);

  //strip.crossFadeCycle(halloweenWavePattern, SIZE(halloweenWavePattern), halloweenPallet, 10, 50, 40);

  //  //do a steamer of several colors in turn, first loop runs through each color and does a steamer
  //  //inner loop sets the first 4 colors of the streamer pattern to the current color (the rest are 0, ie off);
  // flips the direction each time
  //    boolean dirct = true;
  //    for (int i = 0; i < SIZE(halloweenStreamerPatternColors); i++) {
  //      for (int j = 0; j < 3; j++) {
  //        halloweenStreamerPattern[j] = halloweenStreamerPatternColors[i];
  //      }
  //      strip.doRepeatSimplePattern(halloweenStreamerPattern, SIZE(halloweenStreamerPattern), halloweenPallet, SIZE(halloweenPallet), 7, 20, 30, dirct);
  //      dirct = !dirct;
  //    }
  //strip.simpleStreamer(simpleRepeatPattern, SIZE(simpleRepeatPattern), halloweenPallet, 4, 3, 0, false, 400, 80);


  //    for (int j = 0; j < 256; j++) {
  //      strip.randomColors(off, true, strip.Wheel(j & 255), 80, 50, 80);
  //    }

  //strip.setRainbowOffsetCycle(60, false);
  //strip.runRainbowOffsetCycle(true);
  //strip.patternSweepRand( 5, white, -1, 0, 0, false, 0, 1, 100, 180 * 2 ); //rainbow BG with white leds
  //strip.runRainbowOffsetCycle(false);

  //    strip.stripOff();
  //    for (int i = 0; i < SIZE(halloweenStreamerPatternColors); i++) {
  //      if ((i % 2) == 0 ) {
  //        strip.colorWipe( pallet[ halloweenStreamerPatternColors[i] ], stripLength, 30, true, false, false);
  //      } else {
  //        strip.colorWipe( pallet[ halloweenStreamerPatternColors[i] ], stripLength, 30, false, false, false);
  //      }
  //    }

  //    strip.stripOff();
  //    for (int j = 0; j < 3; j++) {
  //      if ((j % 2) == 0 ) {
  //        strip.colorWipeRainbow(30, stripLength, true, false, false, false);
  //        strip.colorWipe(off, stripLength, 30, false, false, false);
  //      } else {
  //        strip.colorWipeRainbow(30, stripLength, false, false, false, false);
  //        strip.colorWipe(off, stripLength, 30, true, false, false);
  //      }
  //    }

}

//a quick shortening of the random color function, just to reduce the pattern function calls more readable
uint32_t RC() {
  return strip.randColor();
}

//sets the current brightness to index value in brightness array
//does not actually change brightness index, so it can be reset later
void setTempBrightness(int index) {
  if (index < numBrightnessLevels && index > 0) {
    strip.setBrightness(brightnessLevels[index]);
  }
}

//resets brightness to current brightnessIndex
void resetBrightness() {
  strip.setBrightness(brightnessLevels[brightnessIndex]);
}
