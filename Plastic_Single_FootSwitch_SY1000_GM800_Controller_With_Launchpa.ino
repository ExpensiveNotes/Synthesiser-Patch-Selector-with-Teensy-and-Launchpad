/*  Program Change controller for Guitar Synths.
    Using USB Host to control the Launchpad and recieve MIDI from it
    The program Sends program change messages to both the SY1000 and GM-800 via Serial out on Serial1 and Serial7 TX pins

  Hardware:
  1 External Stomp Foot switch
  Plastic Box
  Teensy 4.1
  USB Host Cable
  Launchpad Mini
  Launchpad Orientation "n" top right

  Launchpads have two distinct sections of pads when sending MIDI to the Teensy. The middle 8x8 grid sends NoteOn messages. The Other pads send CC.

  SY1000 Program Change:
  -- 4 Banks of 100 presets. To send a Program Change from 0 to 99 you have to first send a CC message to chose which bank
  User Presets/Patches
  -- Send CC 0 0 for 1-1 to 25-4   This is where I have my Hex pickup patches
  -- Send CC 0 1 for 26-1 to 50-4   This is where I have my Normal Pickup Patches
  Factory Presets
  -- Send CC 0 2 for 1-1 to 25-4
  -- Send CC 0 3 for 26-1 to 50-4
  For the SY100 you have to send bank number information and then the PC patch number

  GM800 program Changes:
  For the GM800 I only send the scene (I call them patches here as well as scenes) PC value
  I kept the original 100 patches/Scenes and added my own from 100 onwards to 127

*/
//---- Colours ---------------------------------

#define RED 5
#define ORANGE 9
#define BLUE 37
#define GREEN 21
#define BROWN 7
#define YELLOW 13
#define PINK 53
#define CYAN 29
#define BLAK 0

//===- Launchpad Stuff ==================================================================-  Launchpad Stuff
#include "USBHost_t36.h"
USBHost myusb;
MIDIDevice midiLaunchpad(myusb);

//Sysex data to set Launchpad into programmer mode
uint8_t sysdata[] = { 0xF0, 0x00, 0x20, 0x29, 0x02, 0x0D, 0x0E, 0x01, 0xF7 };

//Display Buffer for Launchpad
int displayBuffer[10][10] = {};   //I use a grid of 10x10 to buffer the colours for the pads on the Launchpad

int SYHexOffset = 7;      //Add this to the numbers below to match my SY1000 patch locations. 7 because zero based counting and I start at bank 3
int SYNormalOffset = 71;  //Add this to match my Normal pickup patch locations which start at bank 44
int myMutedGM800Patch = 127; //Patches are called scenes on the GM800
int myMutedSY1000HexPatch = 100;
int myMutedSY1000NormalPatch = 20;

//SY1000 and GM-800 Program Change values for each Pad
//Top 5 lines are SY1000
//Bottom 3 lines are GM-800
byte gridPCValue [8][8] = {         //Each row of Patches corresponds to two banks on the SY1000
  {1, 2, 3, 4, 5, 6, 7, 8},         // Corresponds to 44-1 to 45-4 for Normal Guitar Pick up. Patch 3-1 to 4-4 for Hex Pickup
  {9, 10, 11, 12, 13, 14, 15, 16},  //Corresponds to 46-1 to 47-4 for Normal Guitar Pick up. Patch 5-1 to 6-4 for Hex Pickup
  {17, 18, 19, 20, 21, 22, 23, 24}, //Corresponds to 48-1 to 49-4 for Normal Guitar Pick up. Patch 7-1 to 8-4 for Hex Pickup
  {25, 26, 27, 28, 29, 30, 31, 32}, //Corresponds to 50-1 to 50-4 and 4 spare for Normal Guitar Pick up. Patch 9-1 to 10-4 for Hex Pickup
  {33, 34, 35, 36, 37, 38, 39, 40}, //Corresponds to 11-1 to 12-4 for Hex Pickup. 1/2 not lit on Launchpad though
  {1, 2, 3, 4, 5, 6, 7, 8},         //Corresponds to GM-800 Scene 101 to 108. No banks on GM800
  {9, 10, 11, 12, 13, 14, 15, 16},  //Corresponds to GM-800 Scene 109 to 116
  {17, 18, 19, 20, 21, 22, 23, 24}  //Corresponds to GM-800 Scene 117 to 124
};


/*
  Launchpad Colours
  Guitar: Blues
  Wind:  Yellow
  Brass: Orange
  Organ: Brown
  Synth: Green
  Violins/Strings etc: White/Silver
  Piano: Purple
  Odd: Red
  Vocal: Pink
  Drums: Pale purple 116
*/

//These are the colours that I chose to match my patches/scenes. I use an 8x8 grid to locate pads on the Launchpad

int gridColoursHexPickup [8][8] = {
  {37, 38, 37, 78, 37, 38, 37, 11}, //Sy1000
  {91, 29, 39, 13, 38, 38, 66, 96},
  {25, 38, 37, 5, 26, 38, 37, 73},
  {25, 38, 37, 26, 25, 38, 37, 26},
  {25, 38, 37, 11, 0, 0, 0, 0},
  {13, 3, 49, 9, 25, 11, 79, 116},    //GM800
  {26, 78, 5, 13, 94, 5, 94, 96},
  {14, 2, 1, 2, 25, 73, 9, 13}
};

int gridColoursNormalPickup [8][8] = {
  {37, 38, 37, 110, 37, 38, 37, 66},  //Sy1000
  {37, 45, 46, 11, 37, 38, 37, 13},
  {13, 38, 37, 91, 37, 38, 37, 25},
  {37, 38, 37, 25, 0, 0, 0, 0},
  {0, 0, 0, 0, 0, 0, 0, 0},
  {13, 3, 49, 9, 25, 11, 79, 116},    //GM800
  {26, 78, 5, 13, 94, 5, 94, 96},
  {14, 2, 1, 2, 25, 73, 9, 13}
};

//Not necessary but I like it
int rainbow[8] = { BROWN, RED, ORANGE, YELLOW, GREEN, BLUE, PINK, CYAN };
int rainbowColourIndex = 0;

byte blinkPadCounter = 0; //used to blink current patches

//Synth variables
#define synthSY1000 1
#define synthGM800 2
byte currentSynth = synthSY1000; //1==SY1000. 2==GM-800
bool bothSynthsPlaying = false;
bool SY1000HexPickupMode = true;  //Enables switching between Normal Patches and Hex Patches

//Current Patch pad locations on Launchpad grid for each Synth.
int SY1000x = 2, SY1000y = 8, GM800x = 1, GM800y = 3;   //Initialized to starting patches

//===- Switches ==================================================================- Switches
#include <Button.h>
Button buttonToggleSynths(4); //Socket on plastic box for footswitch

//===- SETUP & Loop ==================================================================- SETUP
void setup() {
  Serial.begin(9600);//For Serial Monitor. Useful when debugging. Not used when playing
  myusb.begin();
  //Set functions to recieve MIDI from Launchpad
  midiLaunchpad.setHandleNoteOn(OnNoteOn);
  midiLaunchpad.setHandleControlChange(OnControlChange);

  // --- Switch ---------------------------------------------------------------
  buttonToggleSynths.begin();

  //--- Serial UART init ---------------------------------------------------------------
  Serial1.begin(31250);  //MIDI baud rate for GM-800  on TX1
  Serial7.begin(31250);  //MIDI baud rate for SY1000  on TX7

  //--- Change Launchpad to programmer mode ------------------------------------------------
  delay(3000);  //wait for launchpad to get ready before sending message
  midiLaunchpad.sendSysEx(9, sysdata, true);

  // --- set the initial values for Launchpad buffer and patches
  initDisplayBuffer();
  initPatches();
}

void loop() {
  myusb.Task();
  midiLaunchpad.read();  //Interact with Launchpad and receive MIDI messages
  checkFootSwitch();
  populateLaunchpadDisplayBuffer(); //set grid values to display on Launchpad
  sendDisplayBufferToLaunchPad();
  delay(100);
}

//=== Initialize ===============================================================================

void initDisplayBuffer() {
  //make all values 0 for black unlit pads
  for (int x = 0; x < 10; x++) {
    for (int y = 0; y < 10; y++) {
      displayBuffer[x][y] = BLAK;  //Turn the pads off
    }
  }
}

void initPatches() {
  //send default patches to Synths
  sendPCChangeToSynth(synthGM800);
  sendPCChangeToSynth(synthSY1000);
}

//=== MIDI From Launchpad =========================================

// === Middle 8x8 pads ================================================

//A pad in the 8x8 grid has been pressed. i.e. A patch has been chosen
void OnNoteOn(byte channel, byte note, byte velocity) {
  if (velocity == 0) return;  //A noteon on event is triggered by pressing and releasing a pad. This ignores the pad release.

  //translate the recieved MIDI note to an xy grid. Launchpads send a unique MIDI note for each pad in the 8x8 grid
  int xPos = noteToX(note); //Based on the orientation of the Launchpad wityh the "N" at the top right
  int yPos = noteToY(note);

  if (yPos > 3) {   //SY1000 pads
    //Sy1000
    SY1000x = xPos;
    SY1000y = yPos;
    currentSynth = synthSY1000;
  } else {
    //GM800
    GM800x = xPos;
    GM800y = yPos;
    currentSynth = synthGM800;
  }
  sendPCChangeToSynth(currentSynth);
}

//From Launchpad - Translate MIDI note to xy position
int noteToX(int noteIndex) {
  //translate MIDI note to x position
  return noteIndex % 10;
}

int noteToY(int noteIndex) {
  //translate MIDI note to y position
  return int(noteIndex / 10);
}

// === Outside Named pads ================================================

//What to do if named pads are pressed.
void OnControlChange(byte channel, byte control, byte value) {
  if (value == 0) return;  //ignore the pad release

  //Top left pad swaps between Hex and Normal Pickup Patches for the SY1000
  if (control == 91) {
    SY1000HexPickupMode = !SY1000HexPickupMode;
  }
  //Top right pad
  if (control == 98) toggleBothOrSingleSynthsPlaying();
}

void toggleBothOrSingleSynthsPlaying() {
  bothSynthsPlaying = !bothSynthsPlaying;
  if (bothSynthsPlaying) {
    sendPCToSY1000();
    sendPCToGM800();
  } else {
    sendPCChangeToSynth(currentSynth);
  }
}

//=== MIDI to Launchpad to set colours =========================================

//change grid for display buffer to Launchpad colours
void populateLaunchpadDisplayBuffer() {
  //clear buffer
  initDisplayBuffer();
  //novation n display. Just for fun - not necessary
  displayBuffer[9][9] = rainbow[rainbowColourIndex];
  rainbowColourIndex++;
  if (rainbowColourIndex > 6) rainbowColourIndex = 0;

  //load Patch Default colours depending on which pickup is being used
  if (SY1000HexPickupMode) {
    loadPatchColoursHexPickup();
    displayBuffer[1][9] = GREEN;
  }  else {
    loadPatchColoursNormalPickup();
    displayBuffer[1][9] = RED;
  }

  //Highlight current patch by showing a green pad on the right
  if (currentSynth == synthSY1000) displayBuffer[9][SY1000y] = GREEN;
  if (currentSynth == synthGM800) displayBuffer[9][GM800y] = GREEN;

  //make them black again half the time to enable flashing
  if (blinkPadCounter  < 3) {
    displayBuffer[SY1000x][SY1000y] = BLAK;
    displayBuffer[9][SY1000y] = BLAK;
    displayBuffer[GM800x][GM800y] = BLAK;
    displayBuffer[9][GM800y] = BLAK;
  }
  blinkPadCounter++;
  if (blinkPadCounter > 6) blinkPadCounter = 0;

  //Show if both synths are playing or just a single one
  if (bothSynthsPlaying) displayBuffer[8][9] = RED;
  else displayBuffer[8][9] = GREEN;
}

void loadPatchColoursNormalPickup() {
  for (byte y = 0; y < 8; y++) {
    for (byte x = 0; x < 8; x++) {
      displayBuffer[y + 1][8 - x] = gridColoursNormalPickup[x][y];
    }
  }
}

void loadPatchColoursHexPickup() {
  for (byte y = 0; y < 8; y++) {
    for (byte x = 0; x < 8; x++) {
      displayBuffer[y + 1][8 - x] = gridColoursHexPickup[x][y];
    }
  }
}

void sendDisplayBufferToLaunchPad() {
  for (int x = 0; x < 10; x++) {
    for (int y = 0; y < 10; y++) {
      //MIDI note defines the pad, colour, channel, cable which has to be 1 in programmer mode
      midiLaunchpad.sendNoteOn(gridToMIDINote(x, y), displayBuffer[x][y], 1, 1);  //Colours are sent as MIDI notes. I use channel 1 and cable 1
    }
  }
}

//To Launchpad - Send grid position as a MIDI note
int gridToMIDINote(int x, int y) {
  return x + 10 * y;
}

//=== Hardware ==========================================================

void checkFootSwitch() {
  //If the footswitch has been pressed change current synth
  if (buttonToggleSynths.released()) { //I use released because my footswitch is nomally closed. Otherwise use buttonToggleSynths.pressed()
    //Swap Synths
    if (currentSynth == synthSY1000) {
      currentSynth = synthGM800;
      sendPCChangeToSynth(currentSynth);
    }
    else {
      currentSynth = synthSY1000;
      sendPCChangeToSynth(currentSynth);
    }
  }
}

//=== MIDI: sending PC changes to Synths ==================================================

void sendPCChangeToSynth(int whichSynth) {
  if (whichSynth == 1) sendPCToSY1000();
  else sendPCToGM800();
}

void sendPCToSY1000() {
  if (SY1000HexPickupMode) {
    //SY1000 Send CC0 to set Program Bank
    Serial7.write(0xB0);    //Sending a CC message on channel 1
    Serial7.write(0);       //CC 0 sent. For SY1000 #zero means next byte is bank number
    Serial7.write(0);       //CC Data 0 = Bank 1 on SY1000 - That means Patches in the first half of the user patches

    //Send Program Change for my Hex pickup patches
    Serial7.write(0xC0);  //send PC message on Channel 1
    //The gridPCValue stores Rows of Columns so to keep it straight in my mind I switch  x and y values here!!!
    Serial7.write(gridPCValue[8 - SY1000y][SY1000x - 1] + SYHexOffset); //send program change data
  } else {

    //SY1000 CC to set Program Bank for my normal patches using the guitar's pickups
    Serial7.write(0xB0);    //Send CC message on channel 1
    Serial7.write(0);       //CC # zero means next byte is bank number
    Serial7.write(1);       //CC Data 1 = Bank 2 on SY1000 - That means Patches in the second half of the user patches

    //Program Change
    Serial7.write(0xC0);//send PC message + Channel
    Serial7.write(gridPCValue[8 - SY1000y][SY1000x - 1] + SYNormalOffset); //send program change data
  }

  if (bothSynthsPlaying) return; //don't mute the GM800 when choosing the SY1000 if I want both to be playing

  //Choose muted Patch on GM
  Serial1.write(0xC0);//send PC message on Channel 1
  Serial1.write(myMutedGM800Patch); //send program change data. This number corresponds to a scene/patch where all the sounds are muted
  currentSynth = synthSY1000; //make this synth active
}


void sendPCToGM800() {
  if (SY1000HexPickupMode) {
    //To GM-800 - Using Channel 1
    Serial1.write(0xC0);//send PC message on Channel
    Serial1.write(gridPCValue[8 - GM800y][GM800x - 1] + 99); //send program change data starting at location 101 - which maps to my scene 100

    if (bothSynthsPlaying) return;

    //SY1000 to blank - That is a Normal only patch
    Serial7.write(0xB0);    //Send CC message + channel 1
    Serial7.write(0);       //CC # zero
    Serial7.write(1);       //CC Data = Bank 2 on SY1000

    //Program Change
    Serial7.write(0xC0);//send PC message + Channel
    Serial7.write(100); //send program change data to a Normal input 50-4 so no sound emitted if using Hex pickup
  } else {
    //Normal Pickups
    //To GM800 - Using Channel 1
    Serial1.write(0xC0);//send PC message + Channel
    Serial1.write(gridPCValue[8 - GM800y][GM800x - 1] + 99); //send program change data

    if (bothSynthsPlaying) return;

    //SY1000 to blank - That is a Hex only patch
    Serial7.write(0xB0);    //Send CC message on channel 1
    Serial7.write(0);       //CC # zero
    Serial7.write(1);       //CC Data = Bank 2 on SY1000

    //Program Change
    Serial7.write(0xC0);//send PC message + Channel
    Serial7.write(1); //send program change data
  }
  currentSynth = synthGM800;
}
