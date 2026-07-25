/*

Author/s: Franco Grassano - Franco Zaccra

MIT License

Copyright (c) 2020 - Yaeltex

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.

*/


#include <extEEPROM.h>
#include <YTXKeyboard.h>
#include <PeriodicTask.h>
#include <Adafruit_NeoPixel.h>
#include <Adafruit_SleepyDog.h>

// #define MIDILIB5

#if defined(MIDILIB5)
#include <USB-MIDI.h>
#else
#include <MIDI.h>
#include <midi_Defs.h>
#endif

#include "headers/Defines.h"
#include "headers/types.h"
#include "headers/modules.h"
#include "headers/memoryHost.h"
#include "headers/AnalogInputs.h"
#include "headers/EncoderInputs.h" 
#include "headers/DigitalInputs.h"
#include "headers/FeedbackClass.h"

//----------------------------------------------------------------------------------------------------
// SPECIAL VARIABLES WITHOUT INIT (persist on RAM memory between resets)
//----------------------------------------------------------------------------------------------------

uint32_t __attribute__ ((section (".noinit"))) cdcMagicData ; //previusly add "noinit" section to linker

const uint8_t __attribute__((section(".metadata_section"), used))  bin_metadata_signature[] = "ytx";
const uint8_t __attribute__((section(".metadata_section"), used))  bin_metadata_target_id = SystemModuleTypes::MAIN_CONTROLLER;
const uint8_t __attribute__((section(".metadata_section"), used))  bin_metadata_target_group = ModuleGroupTypes::SYSTEM_GROUP;
const uint8_t __attribute__((section(".metadata_section"), used))  bin_metadata_version_minor = FW_VERSION_MINOR;
const uint8_t __attribute__((section(".metadata_section"), used))  bin_metadata_version_major = FW_VERSION_MAJOR;

//----------------------------------------------------------------------------------------------------
// GENERAL VARIABLES AND HW DEFINITION
//----------------------------------------------------------------------------------------------------
#if defined(WAIT_FOR_SERIAL)
  bool cdcEnabled = true;
#else
  bool cdcEnabled;
#endif

YTXKeyboard_* YTXKeyboard;

uint8_t currentBank = 0;
bool enableProcessing = false;
bool validConfigInEEPROM = false;
bool autoSelectMode = false;
uint16_t lastComponentInfoId = 0;
uint8_t currentBrightness = 0;
uint8_t banksToUpdateState = 0;

bool testMode = false;
bool testEncoders = false;
bool testEncoderSwitch = false;
bool testAnalog = false;
bool testDigital = false;
bool testMicrosLoop = false;
bool testMidi = false;
bool testSysex = false;

  
bool keyboardEnable = false;
bool rainbowFinished = false;

uint32_t antMicrosLoop; 
volatile uint32_t antMicrosAuxShow;

bool keyboardReleaseFlag = false;
uint32_t millisKeyboardPress = 0;

const uint8_t pinResetSAMD11 = 38;
const uint8_t pinBootModeSAMD11 = 6;
const uint8_t externalVoltagePin = 13;

uint16_t msgCount = 0;
bool countOn = false;

uint32_t antMillisWD = 0;
uint32_t antMillisSaveControllerState = 0;
uint32_t antMillisStateBackup = 0;

PeriodicTask MIDIpullTask; 

//----------------------------------------------------------------------------------------------------
// ANALOG VARIABLES
//----------------------------------------------------------------------------------------------------

AnalogInputs analogHw;

bool decoding14bit = false;
byte nrpnIntervalStep = 5;

//----------------------------------------------------------------------------------------------------
// ENCODER VARIABLES
//----------------------------------------------------------------------------------------------------

EncoderInputs encoderHw;

//----------------------------------------------------------------------------------------------------
// DIGITAL INPUTS VARIABLES
//----------------------------------------------------------------------------------------------------

DigitalInputs digitalHw;

//----------------------------------------------------------------------------------------------------
// FEEDBACK VARIABLES AND OBJECTS
//----------------------------------------------------------------------------------------------------

FeedbackClass feedbackHw;

enum statusLEDtypes
{
    STATUS_FB_NONE,
    STATUS_FB_INIT,
    STATUS_FB_CONFIG_IN,
    STATUS_FB_CONFIG_OUT,
    STATUS_FB_MSG_IN,
    STATUS_FB_MSG_OUT,
    STATUS_FB_EEPROM,
    STATUS_FB_ERROR,
    STATUS_FB_NO_CONFIG,
    STATUS_FB_LAST
};
enum statusLEDstates
{
    STATUS_NONE,
    STATUS_OFF,
    STATUS_BLINK,
    STATUS_ON
};

// STATUS LED
Adafruit_NeoPixel *statusLED;

uint8_t flagBlinkStatusLED;
uint8_t blinkCountStatusLED;
uint8_t statusLEDfbType;
int16_t blinkInterval = 0;
bool lastStatusLEDState;
uint32_t millisStatusPrev;
bool firstTime;
volatile bool fbShowInProgress = false;
bool sendingFbData = false;
volatile bool waitingForAck = false;
volatile bool auxAckReceived = false;
volatile bool auxInitAckTagged = false;
volatile bool auxBurstTransmissionActive = false;
volatile bool auxBurstAckExpected = false;
volatile bool auxQueueOverflowed = false;
volatile bool waitingForRainbow = true;    // At startup, wait for rainbow animation to finish
volatile bool auxResetPending = false;
uint32_t antMicrosAck = 0;

// Burst error handling variables
//
volatile bool burstErrorOccurred = false;
volatile uint8_t burstErrorIndex = 0;
volatile bool receivingErrorIndex = false;
volatile uint8_t errorIndexBytesReceived = 0;
volatile uint8_t errorIndexByte1 = 0;
volatile uint8_t errorIndexByte2 = 0;

const uint32_t off = statusLED->Color(0, 0, 0);
const uint32_t red = statusLED->Color(STATUS_LED_BRIGHTNESS, 0, 0);
const uint32_t green = statusLED->Color(0, STATUS_LED_BRIGHTNESS, 0);
const uint32_t blue = statusLED->Color(0, 0, STATUS_LED_BRIGHTNESS);
const uint32_t magenta = statusLED->Color(STATUS_LED_BRIGHTNESS/2, 0, STATUS_LED_BRIGHTNESS/2);
const uint32_t cyan = statusLED->Color(0, STATUS_LED_BRIGHTNESS/2, STATUS_LED_BRIGHTNESS/2);
const uint32_t yellow = statusLED->Color(STATUS_LED_BRIGHTNESS/2, STATUS_LED_BRIGHTNESS/2, 0);
const uint32_t white = statusLED->Color(STATUS_LED_BRIGHTNESS/3, STATUS_LED_BRIGHTNESS/3, STATUS_LED_BRIGHTNESS/3);
const uint32_t statusLEDColor[statusLEDtypes::STATUS_FB_LAST] = {off, magenta, blue, white, green, yellow, cyan, red, red}; 

uint32_t antMillisPowerChange = 0;
bool powerChangeFlag = false;
bool powerAdapterConnected = false;

// bool bankUpdateFirstTime = false;
  
//----------------------------------------------------------------------------------------------------
// COMMS - MIDI AND SERIAL VARIABLES AND OBJECTS
//----------------------------------------------------------------------------------------------------

#if defined(USBCON)
  #if defined(MIDILIB5)
  USBMIDI_CREATE_DEFAULT_INSTANCE();
  MIDI_CREATE_INSTANCE(HardwareSerial, Serial1, MIDIHW);
  #else

  #include <midi_UsbTransport.h>

  static const unsigned sUsbTransportBufferSize = 1024; //TODO: 256 and 512 doesnt work with kilowhat web. mod size power off 2
  typedef midi::UsbTransport<sUsbTransportBufferSize> UsbTransport;

  UsbTransport sUsbTransport;

  struct USBSettings : public midi::DefaultSettings
  {
    static const bool Use1ByteParsing = false;
    static const unsigned SysExMaxSize = 512; // Accept SysEx messages.
    static const bool UseRunningStatus = false; // My devices seem to be ok with it.
  };

  // USB instance
  MIDI_CREATE_CUSTOM_INSTANCE(UsbTransport, sUsbTransport, MIDI, USBSettings);

  struct DINSettings : public midi::DefaultSettings
  {
    static const bool Use1ByteParsing = false;
    static const unsigned SysExMaxSize = 512; // Accept SysEx messages.
    static const bool UseRunningStatus = false; // My devices seem to be ok with it.
    static const bool OutputClockTransportOnly = true; // DIN out: allow only Clock/Start/Continue/Stop.
  };

  // Create a 'MIDI' object using MySettings bound to Serial1.
  MIDI_CREATE_CUSTOM_INSTANCE(HardwareSerial, Serial1, MIDIHW, DINSettings);

  #endif
#else // No USB available, fallback to Serial
MIDI_CREATE_DEFAULT_INSTANCE();
#endif

#define NRPN_TIMEOUT_MS   50

uint16_t dataPacketSize;
bool receivingConfig = 0;
uint32_t antMicrosSysex = 0;

//----------------------------------------------------------------------------------------------------
// COMMS - EEPROM VARIABLES AND OBJECTS
//----------------------------------------------------------------------------------------------------

SPISettings ytxSPISettings(SPI_SPEED_2_M, MSBFIRST, SPI_MODE0);

SPIAdressableBUS *spiBUS[3] = {NULL,NULL,NULL};
const uint8_t spiCS[3] = {2,7,10};

// Arduino core definitions for product name, manufacturer name, and PIDs
extern uint8_t STRING_PRODUCT[];
extern uint8_t STRING_MANUFACTURER[];
extern DeviceDescriptor USB_DeviceDescriptorB;
extern DeviceDescriptor USB_DeviceDescriptor;

extEEPROM externalMemory(kbits_512, 1, EEPROM_PAGE_SIZE);//device size, number of devices, page size

memoryHost *memHost; // Mem host object

// RAM pointers to store 1 bank of config
ytxConfigurationType *config;
ytxDigitalType *digital;
ytxEncoderType *encoder;
ytxAnalogType *analog;
ytxFeedbackType *feedback;
ytxLastControllerStateType *controllerState;
uint8_t* colorTable;

// BUFFER STRUCTURES TO STORE RECEIVED MIDI MESSAGE VALUES AND UPDATE WHEN NECESSARY
// STORED IN EEPROM (IF SIZE CHANGES, MODIFY EEPROM ADDRESSES)
typedef struct __attribute__((packed)){
  uint8_t type : 5;
  uint8_t port : 3;
  uint8_t message : 4;
  uint8_t channel : 4;
  uint8_t parameter;
  uint8_t value;
  uint8_t banksPresent;
  uint8_t banksToUpdate;
}midiMsgBuffer7;

typedef struct __attribute__((packed)){
  uint8_t type : 5;
  uint8_t port : 3;
  uint8_t message : 4;
  uint8_t channel : 4;
  uint16_t parameter;
  uint16_t value;
  uint8_t banksPresent;
  uint8_t banksToUpdate;
}midiMsgBuffer14;

typedef struct __attribute__((packed)){
  uint16_t midiBufferSize7 = 0;
  uint16_t midiBufferSize14 = 0;
  uint16_t lastMidiBufferIndex7 = 0;
  uint16_t lastMidiBufferIndex14 = 0;
}midiListenSettings;

midiListenSettings midiRxSettings;
midiMsgBuffer7 *midiMsgBuf7;
midiMsgBuffer14 *midiMsgBuf14;

void Rainbow(Adafruit_NeoPixel *strip, uint8_t wait) {
  uint16_t i, j;

  for (j = 0; j < 256; j++) {
    for (i = 0; i < strip->numPixels(); i++) {
      strip->setPixelColor(i, Wheel(strip, (i * 1 + j) & 255));
    }
    strip->show();
    delay(wait);
  }
}

// Input a value 0 to 255 to get a color value.
// The colours are a transition r - g - b - back to r.
uint32_t Wheel(Adafruit_NeoPixel *strip, byte WheelPos) {
  if (WheelPos < 85) {
    return strip->Color(WheelPos * 3, 255 - WheelPos * 3, 0);
  }
  else if (WheelPos < 170) {
    WheelPos -= 85;
    return strip->Color(255 - WheelPos * 3, 0, WheelPos * 3);
  }
  else {
    WheelPos -= 170;
    return strip->Color(0, WheelPos * 3, 255 - WheelPos * 3);
  }
}
