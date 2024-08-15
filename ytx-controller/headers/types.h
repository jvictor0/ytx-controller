/*

Author/s: Franco Grassano - Franco Zaccra

MIT License

Copyright (c) 2022 - Yaeltex

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

#ifndef TYPES_H
#define TYPES_H
#include <stdint.h>

#define SIGNATURE_CHAR  0xCF
#define SIGNATURE_ADDR  0

#define COMMENT_LEN     8
#define DEVICE_LEN      15
#define SERIAL_NUM_LEN  9

// COMMS TYPES

enum midiPortsType{
    midi_none,
    midi_usb,
    midi_hw,
    midi_hw_usb
};

enum takeOverTypes{
    takeover_none,
    takeover_pickup,
    takeover_valueScaling
};

// GENERAL CONFIG DATA

typedef struct __attribute__((packed))
{
    struct{
        uint8_t signature;              // BYTE 0
        uint8_t fwVersionMin;           // BYTE 1
        uint8_t fwVersionMaj;           // BYTE 2
        uint8_t hwVersionMin;           // BYTE 3
        uint8_t hwVersionMaj;           // BYTE 4
        uint8_t bootFlag:1;             // BYTE 5 - BIT 0: BOOT FLAG
        uint8_t takeoverMode:2;         // BYTE 5 - BIT 1-2: ANALOG TAKEOVER MODE
        uint8_t rainbowOn:1;            // BYTE 5 - BIT 3: RAINBOW ANIMATION AT STARTUP ENABLED/DISABLED
        uint8_t unused0:1;              // BYTE 5 - BIT 4: unused
        uint8_t remoteBanks:1;          // BYTE 5 - BIT 5: REMOTE BANKS 
        uint8_t saveControllerState:1;  // BYTE 5 - BIT 6: save controller state to eeprom
        uint8_t initialDump:1;          // BYTE 5 - BIT 7: initial dump at start up
        uint16_t qtyMessages7bit;       // BYTES 6-7
        uint16_t qtyMessages14bit;      // BYTES 8-9
        uint16_t pid;                   // BYTES 10-11
        char serialNumber[SERIAL_NUM_LEN+1];    // BYTES 12-21
        char deviceName[DEVICE_LEN+1];          // BYTES 22-37
        uint8_t configVersionMin;               // BYTE 38
        uint8_t configVersionMaj;               // BYTE 39
        // For future implementation
        uint8_t unused1[14];                     // BYTES 40-53
    }board;
    
    struct{
        uint8_t encoder[8];
        uint8_t analog[4][8];
        uint8_t digital[2][8];
        uint8_t feedback[8];
        // For future implementation
        uint8_t unused[16];
    }hwMapping;

    struct{
        uint8_t encoderCount;
        uint8_t analogCount;
        uint16_t digitalCount;
        uint8_t feedbackCount;

        // For future implementation
        uint8_t unused2[5];
    }inputs;
    
    struct{
        uint8_t count;
        uint8_t momToggFlags;      // era uint32_t (franco)
        uint16_t shifterId[MAX_BANKS];
        // For future implementation
        uint8_t unused[16];
    }banks;


    struct{
        uint8_t midiMergeFlags : 4;
        uint8_t valueToColorChannel : 4;
        uint8_t valueToIntensityChannel : 4;
        uint8_t vumeterChannel : 4;
        uint8_t splitModeChannel : 4;
        uint8_t remoteBankChannel : 4;

        // For future implementation
        uint8_t unused[14];
    }midiConfig;

    // For future implementation
    uint8_t unused[32];
}ytxConfigurationType;


typedef struct __attribute__((packed))
{
    const uint8_t PROGMEM colorRangeTable[128][3];
}ytxColorTableType;    

// COLOR TABLE
const uint8_t PROGMEM colorRangeTable[128][3] = {
    //  R       G       B
    {0x00, 0x00, 0x00},         // 00
    {0xf2, 0xc, 0xc},
    {0xf2, 0x58, 0x58},
    {0xf2, 0xa5, 0xa5},
    {0xf2, 0x2c, 0xc},
    {0xf2, 0x6e, 0x58},
    {0xf2, 0xb0, 0xa5},
    {0xf2, 0x4d, 0xc},
    {0xf2, 0x84, 0x58},
    {0xf2, 0xbb, 0xa5},
    {0xf2, 0x6e, 0xc},          // 10
    {0xf2, 0x9a, 0x58},
    {0xf2, 0xc6, 0xa5},
    {0xf2, 0x8f, 0xc},
    {0xf2, 0xb0, 0x58},
    {0xf2, 0xd1, 0xa5},
    {0xf2, 0xb0, 0xc},
    {0xf2, 0xc6, 0x58},
    {0xf2, 0xdc, 0xa5},
    {0xf2, 0xd1, 0xc},
    {0xf2, 0xdc, 0x58},         // 20
    {0xf2, 0xe7, 0xa5},
    {0xf2, 0xf2, 0xc},
    {0xf2, 0xf2, 0x58},
    {0xf2, 0xf2, 0xa5},
    {0xd1, 0xf2, 0xc},
    {0xdc, 0xf2, 0x58},
    {0xd1, 0xf2, 0xa5},
    {0xb0, 0xf2, 0xc},
    {0xc6, 0xf2, 0x58},
    {0xb0, 0xf2, 0xa5},         // 30
    {0x8f, 0xf2, 0xc},
    {0xb0, 0xf2, 0x58},
    {0x8f, 0xf2, 0xa5},
    {0x6e, 0xf2, 0xc},
    {0x9a, 0xf2, 0x58},
    {0x6e, 0xf2, 0xa5},
    {0x4d, 0xf2, 0xc},
    {0x84, 0xf2, 0x58},
    {0x4d, 0xf2, 0xa5},
    {0x2c, 0xf2, 0xc},          // 40
    {0x6e, 0xf2, 0x58},
    {0x2c, 0xf2, 0xa5},
    {0xc, 0xf2, 0xc},
    {0x58, 0xf2, 0x58},
    {0xc, 0xf2, 0xa5},
    {0xc, 0xf2, 0x2c},
    {0x58, 0xf2, 0x6e},
    {0xc, 0xf2, 0xb0},
    {0xc, 0xf2, 0x4d},
    {0x58, 0xf2, 0x84},         // 50
    {0xc, 0xf2, 0xbb},
    {0xc, 0xf2, 0x6e},
    {0x58, 0xf2, 0x9a},
    {0xc, 0xf2, 0xc6},
    {0xc, 0xf2, 0x8f},
    {0x58, 0xf2, 0xb0},
    {0xc, 0xf2, 0xd1},
    {0xc, 0xf2, 0xb0},
    {0x58, 0xf2, 0xc6},
    {0xc, 0xf2, 0xdc},          // 60
    {0xc, 0xf2, 0xd1},
    {0x58, 0xf2, 0xdc},
    {0xc, 0xf2, 0xe7},
    {0xc, 0xf2, 0xf2},
    {0x58, 0xf2, 0xf2},
    {0xc, 0xf2, 0xf2},
    {0xc, 0xd1, 0xf2},
    {0x58, 0xdc, 0xf2},
    {0xc, 0xe7, 0xf2},
    {0xc, 0xb0, 0xf2},          // 70
    {0x58, 0xc6, 0xf2},
    {0xc, 0xdc, 0xf2},
    {0xc, 0x8f, 0xf2},
    {0x58, 0xb0, 0xf2},
    {0xc, 0xd1, 0xf2},
    {0xc, 0x6e, 0xf2},
    {0x58, 0x9a, 0xf2},
    {0xc, 0xc6, 0xf2},
    {0xc, 0x4d, 0xf2},
    {0x58, 0x84, 0xf2},         // 80
    {0xc, 0xbb, 0xf2},
    {0xc, 0x2c, 0xf2},
    {0x58, 0x6e, 0xf2},
    {0xc, 0xb0, 0xf2},
    {0xc, 0xc, 0xf2},
    {0x58, 0x58, 0xf2},
    {0xc, 0xa5, 0xf2},
    {0x2c, 0xc, 0xf2},
    {0x6e, 0x58, 0xf2},
    {0x2c, 0xa5, 0xf2},         // 90
    {0x4d, 0xc, 0xf2},
    {0x84, 0x58, 0xf2},
    {0x4d, 0xa5, 0xf2},
    {0x6e, 0xc, 0xf2},
    {0x9a, 0x58, 0xf2},
    {0x6e, 0xa5, 0xf2},
    {0x8f, 0xc, 0xf2},
    {0xb0, 0x58, 0xf2},
    {0x8f, 0xa5, 0xf2},
    {0xb0, 0xc, 0xf2},          // 100
    {0xc6, 0x58, 0xf2},
    {0xb0, 0xa5, 0xf2},
    {0xd1, 0xc, 0xf2},
    {0xdc, 0x58, 0xf2},
    {0xd1, 0xa5, 0xf2},
    {0xf2, 0xc, 0xf2},
    {0xf2, 0x58, 0xf2},
    {0xf2, 0xa5, 0xf2},
    {0xf2, 0xc, 0xd1},
    {0xf2, 0x58, 0xdc},         // 110
    {0xf2, 0xa5, 0xe7},
    {0xf2, 0xc, 0xb0},
    {0xf2, 0x58, 0xc6},
    {0xf2, 0xa5, 0xdc},
    {0xf2, 0xc, 0x8f},
    {0xf2, 0x58, 0xb0},
    {0xf2, 0xa5, 0xd1},
    {0xf2, 0xc, 0x6e},
    {0xf2, 0x58, 0x9a},
    {0xf2, 0xa5, 0xc6},         // 120
    {0xf2, 0xc, 0x4d},
    {0xf2, 0x58, 0x84},
    {0xf2, 0xa5, 0xbb},
    {0xf2, 0xc, 0x2c},
    {0xf2, 0x58, 0x6e},
    {0xf2, 0xa5, 0xb0},
    {0xf0, 0xf0, 0xf0}
};

// FEEDBACK TYPES
enum feedbackSource
{
    fb_src_no,          //  000
    fb_src_usb,         //  001
    fb_src_hw,          //  010
    fb_src_hw_usb,      //  011
    fb_src_local,       //  100
    fb_src_local_usb,   //  101
    fb_src_local_hw,    //  110
    fb_src_local_usb_hw //  111  
};

enum feedbackLocalBehaviour
{
    fb_lb_on_with_press,
    fb_lb_always_on,
};

enum encoderRotaryFeedbackMode{
    fb_spot,
    fb_mirror,
    fb_fill,
    fb_pivot,
};

// FEEDBACK DATA

// CHEQUEAR CONTRA MAPA DE MEMORIA Y ARI
typedef struct __attribute__((packed))
{
    uint8_t valueToIntensity : 1;       // BYTE 0 - BITS 0: MIDI VALUE TO INTENSITY 
    uint8_t source : 3;                 // BYTE 0 - BITS 1-3: MIDI SOURCE PORT FOR FEEDBACK
    uint8_t message : 4;                // BYTE 0 - BITS 4-7: MIDI MESSAGE FOR FEEDBACK
    uint8_t localBehaviour : 4;         // BYTE 1 - BITS 0-3: BEHAVIOUR FOR LOCAL FEEDBACK
    uint8_t channel : 4;                // BYTE 1 - BITS 4-7: MIDI CHANNEL FOR FEEDBACK
    uint8_t parameterLSB : 7;           // BYTE 2 - BITS 0-6: PARAMETER LSB FOR FEEDBACK
    uint8_t valueToColor : 1;           // BYTE 2 - BITS 7: VALUE TO COLOR ENABLE
    uint8_t parameterMSB : 7;           // BYTE 3 - BITS 0-6: PARAMETER MSB FOR FEEDBACK
    uint8_t lowIntensityOff : 1;        // BYTE 3 - BITS 7: UNUSED
    uint8_t color[3];                   // BYTEs 4-6 - COLOR FOR FEEDBACK
    
    // For future implementation
    uint8_t unused2[2];                 // BYTE 7-8 - UNUSED
}ytxFeedbackType;   


// ENCODER TYPES

enum rotaryModes{
    rot_absolute,                   // 0 - Absolute (MIN - MAX)
    rot_rel_binaryOffset,           // 1 - Binary Offset (positive 065 - 127 / negative 063 - 000)
    rot_rel_complement2,            // 2-  2 Complement (positive 001 - 064 / negative 127 - 065)
    rot_rel_signedBit,              // 3 - Signed Bit (positive 065 - 127 / negative 001 - 063)
    rot_rel_signedBit2,             // 4 - Signed Bit 2 (positive 001 - 063 / negative 065 - 127)
    rot_rel_singleValue             // 5 - Single Value (increment 096 / decrement 097)
};

enum rotaryMessageTypes{
    rotary_msg_none,
    rotary_msg_note,
    rotary_msg_cc,
    rotary_msg_vu_cc,
    rotary_msg_pc_rel,
    rotary_msg_nrpn,
    rotary_msg_rpn,
    rotary_msg_pb,
    rotary_msg_key,
    rotary_msg_msg_size
};

// Values ensure retro compatibility with versions < v0.20
enum encoderRotarySpeed{
    rot_variable_speed_1    = 0,
    rot_variable_speed_2    = 4,
    rot_variable_speed_3    = 5,
    rot_fixed_speed_1       = 1,
    rot_fixed_speed_2       = 2,
    rot_fixed_speed_3       = 3
};

enum rotaryConfigKeyboardParameters
{
    rotary_keyLeft,
    rotary_modifierLeft,
    rotary_keyRight,
    rotary_modifierRight
};

enum rotaryConfigMIDIParameters
{
    rotary_LSB,
    rotary_MSB,
    rotary_minLSB,
    rotary_minMSB,
    rotary_maxLSB,
    rotary_maxMSB
};

enum switchConfigKeyboardParameters
{
    switch_key,
    switch_modifier,
};

enum switchConfigMIDIParameters
{
    switch_parameter_LSB,
    switch_parameter_MSB,
    switch_minValue_LSB,
    switch_minValue_MSB,
    switch_maxValue_LSB,
    switch_maxValue_MSB
};
enum switchModes
{
    switch_mode_none,
    switch_mode_message,
    switch_mode_shift_rot,
    switch_mode_fine,
    switch_mode_2cc,
    switch_mode_quick_shift,
    switch_mode_quick_shift_note,
    switch_mode_velocity
};
enum switchActions
{
    switch_momentary,
    switch_toggle
};
enum switchDoubleClickModes
{
    switch_doubleClick_none,
    switch_doubleClick_2min,
    switch_doubleClick_2center,
    switch_doubleClick_2max
};

enum switchMessageTypes{
    switch_msg_note,
    switch_msg_cc,
    switch_msg_pc,
    switch_msg_pc_m,
    switch_msg_pc_p,
    switch_msg_nrpn,
    switch_msg_rpn,
    switch_msg_pb,
    switch_msg_key
};

// ENCODER DATA

typedef struct __attribute__((packed))
{
    struct{
        uint8_t speed : 3;              // BYTE 0 - BITS 0-1: ENCODER SPEED
        uint8_t hwMode : 3;             // BYTE 0 - BITS 2-4: ABSOLUTE/RELATIVE MODES
        uint8_t unused1 : 2;            // BYTE 0 - BITS 5-7: UNUSED
        // For future implementation
        uint8_t unused2;                // BYTE 1 - UNUSED
    }rotBehaviour;
    struct{
        uint8_t channel : 4;            // BYTE 2 - BITS 0-3: MIDI CHANNEL FOR ROTARY
        uint8_t message : 4;            // BYTE 2 - BITS 4-7: MESSAGE TYPE FOR ROTARY
        uint8_t midiPort : 2;           // BYTE 3 - BITS 0-1: MIDI PORT FOR ROTARY
        uint8_t unused1 : 6;            // BYTE 3 - BITS 2-7: UNUSED
        uint8_t parameter[6];           // BYTES 4-9 - PARAMETER, MIN AND MAX FOR ROTARY
        char comment[COMMENT_LEN+1];    // BYTES 10-19 - ROTARY COMMENT
        // For future implementation
        uint8_t unused2[4];             // BYTES 19-22 - UNUSED
    }rotaryConfig;
    struct{
        uint8_t mode : 4;               // BYTE 23 - BITS 0-3: SWITCH ACTION MODE
        uint8_t doubleClick : 3;        // BYTE 23 - BITS 4-6: SWITCH DOUBLE CLICK ACTION
        uint8_t action : 1;             // BYTE 23 - BIT 7: SWTICH TOGGLE/MOMENTARY
        uint8_t channel : 4;            // BYTE 24 - BITS 0-3: MIDI CHANNEL FOR SWITCH
        uint8_t message : 4;            // BYTE 24 - BITS 4-7: MIDI MESSAGE FOR SWITCH
        uint8_t midiPort : 2;           // BYTE 25 - BITS 0-1: MIDI PORT FOR SWITCH
        uint8_t unused1 : 6;            // BYTE 25 - BITS 2-7: UNUSED
        uint8_t parameter[6];           // BYTES 26-31 - PARAMETER, MIN AND MAX FOR SWITCH
        
        // For future implementation
        uint8_t unused2[4];             // BYTES 32-35 - PARAMETER, MIN and MAX FOR ROTARY
    }switchConfig;
    struct{
        uint8_t channel : 4;            // BYTES 36 - BITS 0-3: MIDI CHANNEL FOR ROTARY FEEDBACK
        uint8_t message : 4;            // BYTES 36 - BITS 4-7: MIDI MESSAGE FOR ROTARY FEEDBACK
        uint8_t source : 3;             // BYTES 37 - BITS 0-2: MIDI SOURCE PORT FOR ROTARY FEEDBACK
        uint8_t mode : 2;               // BYTES 36 - BITS 3-4: RING MODE FOR ROTARY FEEDBACK
        uint8_t unused1 : 3;            // BYTES 37 - BITS 5-7: UNUSED
        uint8_t parameterLSB : 7;       // BYTES 38 - BITS 0-6: PARAMETER LSB FOR ROTARY FEEDBACK
        uint8_t rotaryValueToColor : 1; // BYTES 38 - BIT 7: COLOR SWITCH WITH MIDI MESSAGE
        uint8_t parameterMSB : 7;       // BYTES 39 - BITS 0-6: PARAMETER MSB FOR ROTARY FEEDBACK
        uint8_t valueToIntensity : 1;   // BYTES 39 - BIT 7: MIDI VALUE TO INTENSITY 
        uint8_t color[3];               // BYTES 40-42 - COLOR FOR RING FEEDBACK
        
        // For future implementation
        uint8_t unused4[4];             // BYTES 43-46 - UNUSED
    }rotaryFeedback;
    ytxFeedbackType switchFeedback;     // BYTES 47-55 - SWITCH FEEDBACK
}ytxEncoderType;

////////////////////////////////////////////////////////////////////
// DIGITAL TYPES 

enum digitalMessageTypes{
    digital_msg_none,
    digital_msg_note,
    digital_msg_cc,
    digital_msg_pc,
    digital_msg_pc_m,
    digital_msg_pc_p,
    digital_msg_nrpn,
    digital_msg_rpn,
    digital_msg_pb,
    digital_msg_key
};

enum digitalConfigParameters
{
    digital_LSB,
    digital_MSB,
    digital_minLSB,
    digital_minMSB,
    digital_maxLSB,
    digital_maxMSB
};

enum digitalConfigKeyboardParameters
{
    digital_key,
    digital_modifier,
};

// DIGITAL DATA

typedef struct __attribute__((packed))
{
    struct{
        uint8_t message : 4;                // BYTE 0 - BITS 0-3: MESSAGE FOR DIGITAL
        uint8_t midiPort : 2;               // BYTE 0 - BITS 4-5: MIDI PORT FOR DIGITAL MESSAGE
        uint8_t action : 1;                 // BYTE 0 - BITS 6: DIGITAL TOGGLE/MOMENTARY ACTION
        uint8_t unused1 : 1;                // BYTE 0 - BITS 7: UNUSED
        uint8_t channel : 4;                // BYTE 1 - BITS 0-3: MIDI CHANNEL FOR DIGITAL
        uint8_t unused2 : 4;                // BYTE 1 - BITS 4-7: UNUSED
        uint8_t parameter[6];               // BYTES 2-7 - PARAMETER, MIN AND MAX FOR DIGITAL
        char comment[COMMENT_LEN+1];        // BYTES 8-16 - COMMENT FOR DIGITAL

        // For future implementation
        uint8_t unused3[2];                 // BYTES 17-18 - UNUSED
    }actionConfig;
    ytxFeedbackType feedback;               // BYTES 19-27 - DIGITAL FEEDBACK     

}ytxDigitalType;

////////////////////////////////////////////////////////////////////
// ANALOG TYPES

enum analogMessageTypes{
    analog_msg_none,
    analog_msg_note,
    analog_msg_cc,
    analog_msg_pc,
    analog_msg_pc_m,
    analog_msg_pc_p,
    analog_msg_nrpn,
    analog_msg_rpn,
    analog_msg_pb,
    analog_msg_key
};

enum analogConfigKeyboardParameters
{
    analog_key,
    analog_modifier
};

enum analogConfigMIDIParameters
{
    analog_LSB,
    analog_MSB,
    analog_minLSB,
    analog_minMSB,
    analog_maxLSB,
    analog_maxMSB
};

enum splitModes
{
    normal,
    splitCenter
};
enum deadZone
{
    dz_off,
    dz_on
};

// ANALOG DATA
typedef struct __attribute__((packed))
{
    uint8_t type : 4;                   // BYTE 0 - BITS 0-3: TYPE OF ANALOG CONTROL
    uint8_t message :4;                 // BYTE 0 - BITS 4-7: MESSAGE FOR ANALOG
    uint8_t midiPort : 2;               // BYTE 1 - BITS 0-1: MIDI PORT FOR ANALOG
    uint8_t splitMode : 1;              // BYTE 1 - BITS 2: SPLIT MODE. 1 OR 2 CC
    uint8_t deadZone : 1;               // BYTE 1 - BITS 3: DEAD ZONE 
    uint8_t channel : 4;                // BYTE 1 - BITS 4-7: MIDI CHANNEL FOR ANALOG
    uint8_t parameter[6];               // BYTES 2-7 - PARAMETER, MIN AND MAX FOR ANALOG
    char comment[COMMENT_LEN+1];        // BYTES 8-16 - COMMENT FOR ANALOG

    // For future implementation
    uint8_t unused[2];                  // BYTES 17-18 - UNUSED

    ytxFeedbackType feedback;           // BYTES 19-27 - ANALOG FEEDBACK 
}ytxAnalogType;



// ANALOG DATA
typedef struct __attribute__((packed))
{
    uint16_t lastEncoderValue[32];
    uint16_t lastEncoderSwitchValue[32];
    uint16_t lastDigitalValue[256];
    uint16_t lastAnalogValue[64];
}ytxLastControllerStateType;


////////////////////////////////////////////////////////////////////
// MEM HOST DECLARATION

enum ytxIOBLOCK
{
  Configuration,
  ColorTable,
  Encoder,
  Analog,
  Digital,
  Feedback,
  BLOCKS_COUNT
};

typedef struct __attribute__((packed))
{
  uint16_t sectionSize;      // Size in bytes
  uint16_t sectionCount;     // How many sections
  uint16_t eepBaseAddress;  // Start address in EEPROM for this block
  void * ramBaseAddress;    // Start address in RAM for this block
  bool unique;  
}blockDescriptor;

typedef struct __attribute__((packed))
{
  struct{
    bool memoryNew:1;
    bool versionChange:1;
    bool unused:6;
  }flags;                
  uint8_t lastCurrentBank;      
  uint8_t unused[14];
}genSettingsControllerState;

genSettingsControllerState genSettings;

#define CTRLR_STATE_MIDI_BUFFER_SIZE        (((5*MIDI_BUF_MAX_LEN/EEPROM_PAGE_SIZE)+1)*EEPROM_PAGE_SIZE) 
                                            // 5 = 1 byte for 7 bit midi buffer +
                                            //     2 bytes for 14 bit midi buffer(not currently used) + 
                                            //     2 bytes for "banksToUpdate"
                                            //= 5120 bytes, aligned to page size
#define CTRLR_STATE_MIDI_BUFFER_ADDR        (EEPROM_TOTAL_SIZE-1-CTRLR_STATE_MIDI_BUFFER_SIZE)


#define CTRLR_STATE_ELEMENTS_SIZE           (MAX_BANKS* \
                                            ((sizeof(EncoderInputs::encoderBankData)+sizeof(FeedbackClass::encFeedbackData))*MAX_ENCODER_AMOUNT \
                                            + \
                                             (sizeof(DigitalInputs::digitalBankData)+sizeof(FeedbackClass::digFeedbackData))*MAX_DIGITAL_AMOUNT)) 
                                            //= 12800 bytes, not aligned
#define CTRLR_STATE_ELEMENTS_ADDRESS        (CTRLR_STATE_MIDI_BUFFER_ADDR-CTRLR_STATE_ELEMENTS_SIZE)


#define CTRLR_STATE_GENERAL_SETT_SIZE       sizeof(genSettingsControllerState)
                                            //= 16 bytes, not aligned
#define CTRLR_STATE_GENERAL_SETT_ADDRESS    (CTRLR_STATE_ELEMENTS_ADDRESS-CTRLR_STATE_GENERAL_SETT_SIZE)


#endif // TYPES_H
