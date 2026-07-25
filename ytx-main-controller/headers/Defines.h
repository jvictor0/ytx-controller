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

#ifndef DEFINES_H
#define DEFINES_H

//----------------------------------------------------------------------------------------------------
// DEFINES
//----------------------------------------------------------------------------------------------------

// #define WAIT_FOR_SERIAL
// #define INIT_CONFIG
// #define PRINT_CONFIG
// #define PRINT_MIDI_BUFFER
// #define PRINT_EEPROM
// #define DEBUG_SYSEX
// #define START_ERASE_EEPROM

// #define DISABLE_ENCODER_BANKS
// #define DISABLE_DIGITAL_BANKS
// #define DISABLE_ANALOG_BANKS

#if !defined(INIT_CONFIG)
#define USE_KWHAT_COUNT_BUFFER
#endif

#define FW_VERSION_MAJOR      0
#define FW_VERSION_MINOR      24

#define HW_VERSION_MAJOR      1
#define HW_VERSION_MINOR      0

#define VERSION_0_20_MINOR    20

#define FW_VERSION_ADDR       1
#define HW_VERSION_ADDR       3

#define BOOT_FLAGS_ADDR       5

#define FACTORY_RESET_MASK    (1<<4)

#define MICROCHIP_VID         0x04D8  // Microchip's USB sub-licensing program (https://www.microchip.com/usblicensing)
#define DEFAULT_PID           0xEBCA  // Assigned to Yaeltex

#define RESET_TO_CONTROLLER   0
#define RESET_TO_BOOTLOADER   1

/*
 * If CDC_ENABLE_ADDRESS the controller will reset to enumerate CDC communications
 * CDC_ENABLE_ADDRESS must point to a free SRAM cell that must not
 * be touched from the loaded application.
 * It is the exact previous address to the BOOT_DOUBLE_TAP used by the bootloader
 * YAELTEX NOTE: We found that the last 616 bytes are used by something between the bootloader and the application, but can't find what
 */
#define ENABLE_MAGIC_TRICK                  (0x07738135ul)

#define BOOTLOADER_ENABLE_ADDRESS           (0x20007FFCul) 
#define BOOTLOADER_ENABLE_DATA              (*((volatile uint32_t *) BOOTLOADER_ENABLE_ADDRESS))

#define EEPROM_PAGE_SIZE      128
#define EEPROM_TOTAL_SIZE     65536

#define MAX_BANKS             8

#define DIGITAL_PORT_1        0
#define DIGITAL_PORT_2        1
#define DIGITAL_PORTS         2
#define MODULES_PER_PORT      8
#define MAX_DIGITAL_MODULES   DIGITAL_PORTS*MODULES_PER_PORT

#define MAX_DIGITAL_AMOUNT    256
#define MAX_ENCODER_AMOUNT    64
#define MAX_ANALOG_AMOUNT     128

// Note On and Note Off values
#define NOTE_ON       127
#define NOTE_OFF      0

// LED States
#define OUT_OFF         0
#define OUT_BLINK       1
#define OUT_ON          2

// On and Off labels
#define ON              1
#define OFF             0

// Pull up mode for digital inputs
#define PULLUP          1

#define KEYBOARD_MILLIS             25
#define KEYBOARD_MILLIS_ANALOG      1

#define PRIORITY_ELAPSE_TIME_MS     500

#define SAVE_CONTROLLER_STATE_MS    10000

#define POWER_SUPPLY_MS             500

#define WATCHDOG_CHECK_MS           100
#define WATCHDOG_RESET_NORMAL       2000
#define WATCHDOG_RESET_CONFIG       10000
#define WATCHDOG_CONFIG_CHECK_MS    500
#define WATCHDOG_RESET_PRINT        5000

// #if defined(SERIAL_DEBUG)
#define SERIALPRINT(a)        { if(cdcEnabled){SerialUSB.print(a);}     }
#define SERIALPRINTLN(a)      { if(cdcEnabled){SerialUSB.println(a);}   }
#define SERIALPRINTF(a, f)    { if(cdcEnabled){SerialUSB.print(a,f);}   }
#define SERIALPRINTLNF(a, f)  { if(cdcEnabled){SerialUSB.println(a,f);} }
// #else
// #define SERIALPRINT(a)        {}
// #define SERIALPRINTLN(a)      {}
// #define SERIALPRINTF(a, f)    {}
// #define SERIALPRINTLNF(a, f)  {}
// #endif

//----------------------------------------------------------------------------------------------------
// ENCODERS
//----------------------------------------------------------------------------------------------------s

#define IS_ENCODER_ROT_7_BIT(eIndex)   (  encoder[eIndex].rotaryConfig.message == rotary_msg_note   ||  \
                                          encoder[eIndex].rotaryConfig.message == rotary_msg_cc     ||  \
                                          encoder[eIndex].rotaryConfig.message == rotary_msg_vu_cc  ||  \
                                          encoder[eIndex].rotaryConfig.message == rotary_msg_pc_rel  )


#define IS_ENCODER_ROT_FB_7_BIT(eIndex)    (  encoder[eIndex].rotaryFeedback.message == rotary_msg_note   ||  \
                                              encoder[eIndex].rotaryFeedback.message == rotary_msg_cc     ||  \
                                              encoder[eIndex].rotaryFeedback.message == rotary_msg_vu_cc  ||  \
                                              encoder[eIndex].rotaryFeedback.message == rotary_msg_pc_rel )

#define IS_ENCODER_SW_7_BIT(eIndex)    (  encoder[eIndex].switchConfig.mode == switchModes::switch_mode_shift_rot   ?   \
                                         (encoder[eIndex].switchConfig.message == rotary_msg_note   ||    \
                                          encoder[eIndex].switchConfig.message == rotary_msg_cc     ||    \
                                          encoder[eIndex].switchConfig.message == rotary_msg_vu_cc  ||    \
                                          encoder[eIndex].switchConfig.message == rotary_msg_pc_rel)                :   \
                                         (encoder[eIndex].switchConfig.message == switch_msg_note   ||    \
                                          encoder[eIndex].switchConfig.message == switch_msg_cc     ||    \
                                          encoder[eIndex].switchConfig.message == switch_msg_pc     ||    \
                                          encoder[eIndex].switchConfig.message == switch_msg_pc_m   ||    \
                                          encoder[eIndex].switchConfig.message == switch_msg_pc_p ))

#define IS_ENCODER_SW_FB_7_BIT(eIndex)    ( encoder[eIndex].switchConfig.mode == switchModes::switch_mode_shift_rot  ?   \
                                           (encoder[eIndex].switchFeedback.message == rotary_msg_note   ||    \
                                            encoder[eIndex].switchFeedback.message == rotary_msg_cc     ||    \
                                            encoder[eIndex].switchFeedback.message == rotary_msg_vu_cc  ||    \
                                            encoder[eIndex].switchFeedback.message == rotary_msg_pc_rel)              :   \
                                           (encoder[eIndex].switchFeedback.message == switch_msg_note   ||    \
                                            encoder[eIndex].switchFeedback.message == switch_msg_cc     ||    \
                                            encoder[eIndex].switchFeedback.message == switch_msg_pc     ||    \
                                            encoder[eIndex].switchFeedback.message == switch_msg_pc_m   ||    \
                                            encoder[eIndex].switchFeedback.message == switch_msg_pc_p))

#define IS_ENCODER_ROT_14_BIT(eIndex)  (  encoder[eIndex].rotaryConfig.message == rotary_msg_nrpn ||  \
                                          encoder[eIndex].rotaryConfig.message == rotary_msg_rpn  ||  \
                                          encoder[eIndex].rotaryConfig.message == rotary_msg_pb   ||  \
                                          encoder[eIndex].rotaryConfig.message == rotary_msg_key)

#define IS_ENCODER_ROT_FB_14_BIT(eIndex)  ( encoder[eIndex].rotaryFeedback.message == rotary_msg_nrpn  ||  \
                                            encoder[eIndex].rotaryFeedback.message == rotary_msg_rpn   ||  \
                                            encoder[eIndex].rotaryFeedback.message == rotary_msg_pb    ||  \
                                            encoder[eIndex].rotaryFeedback.message == rotary_msg_key)

#define IS_ENCODER_SW_14_BIT(eIndex)   (  encoder[eIndex].switchConfig.mode == switchModes::switch_mode_shift_rot   ?   \
                                         (encoder[eIndex].switchConfig.message == rotary_msg_nrpn   ||  \
                                          encoder[eIndex].switchConfig.message == rotary_msg_rpn    ||  \
                                          encoder[eIndex].switchConfig.message == rotary_msg_pb     ||  \
                                          encoder[eIndex].switchConfig.message == rotary_msg_key)                    :   \
                                         (encoder[eIndex].switchConfig.message == switch_msg_nrpn   ||  \
                                          encoder[eIndex].switchConfig.message == switch_msg_rpn    ||  \
                                          encoder[eIndex].switchConfig.message == switch_msg_pb     ||  \
                                          encoder[eIndex].switchConfig.message == switch_msg_key))

#define IS_ENCODER_SW_FB_14_BIT(eIndex)    (encoder[eIndex].switchConfig.mode == switchModes::switch_mode_shift_rot   ?   \
                                           (encoder[eIndex].switchFeedback.message == rotary_msg_nrpn   ||  \
                                            encoder[eIndex].switchFeedback.message == rotary_msg_rpn    ||  \
                                            encoder[eIndex].switchFeedback.message == rotary_msg_pb)                    :   \
                                           (encoder[eIndex].switchFeedback.message == switch_msg_nrpn   ||  \
                                            encoder[eIndex].switchFeedback.message == switch_msg_rpn    ||  \
                                            encoder[eIndex].switchFeedback.message == switch_msg_pb     ||  \
                                            encoder[eIndex].switchFeedback.message == switch_msg_key))
    
#define MAX_ENCODER_MODS      16

// #define FAST_SPEED_MILLIS  4
// #define MID4_SPEED_MILLIS  6
// #define MID3_SPEED_MILLIS  8
// #define MID2_SPEED_MILLIS  10
// #define MID1_SPEED_MILLIS  16

#define DOUBLE_CLICK_WAIT     200
#define SWITCH_DEBOUNCE_WAIT  20
#define LONG_CLICK_WAIT       1000             // Change to this when implementing long press
// #define LONG_CLICK_WAIT       DOUBLE_CLICK_WAIT   // to prevent long press being ignored

// Millisecond thresholds to calculate non-detent encoder speed
#define ENCODER_MAX_SPEED   6
#define ACCEL_VARIATIONS    3
uint8_t nonDetentMillisSpeedThresholds[][ENCODER_MAX_SPEED] =  {{30, 25, 20, 15, 12, 8},
                                                                {35, 30, 25, 20, 15, 10},
                                                                {40, 35, 30, 25, 18, 14}};
#define FAST_SPEED_MILLIS  8
#define MID4_SPEED_MILLIS  12
#define MID3_SPEED_MILLIS  15
#define MID2_SPEED_MILLIS  20
#define MID1_SPEED_MILLIS  25

// Millisecond thresholds to calculate detented encoder speed
uint8_t detentMillisSpeedThresholds[][ENCODER_MAX_SPEED] = {{40, 20, 16, 14, 12, 10},
                                                            {50, 25, 20, 16, 14, 12},
                                                            {55, 35, 30, 26, 20, 16}};
#define D_FAST_SPEED_MILLIS    10
#define D_MID4_SPEED_MILLIS    20
#define D_MID3_SPEED_MILLIS    30
#define D_MID2_SPEED_MILLIS    40
#define D_MID1_SPEED_MILLIS    50

#define SLOW_SPEED_COUNT        2

// Value that each speed adds to current encoder value
uint8_t encoderAccelSpeed[][ENCODER_MAX_SPEED] =   {{1, 2, 3, 3, 4, 5},
                                                    {1, 2, 3, 4, 6, 7},
                                                    {1, 2, 3, 4, 6, 7}};

#define SLOW_SPEED        1
#define MID1_SPEED        2
#define MID2_SPEED        3
#define MID3_SPEED        5
#define MID4_SPEED        8
#define FAST_SPEED        10


//----------------------------------------------------------------------------------------------------
// ANALOG
//----------------------------------------------------------------------------------------------------

#define IS_ANALOG_7_BIT(aIndex)    (  analog[aIndex].message == analogMessageTypes::analog_msg_note   ||  \
                                      analog[aIndex].message == analogMessageTypes::analog_msg_cc     ||  \
                                      analog[aIndex].message == analogMessageTypes::analog_msg_pc     ||  \
                                      analog[aIndex].message == analogMessageTypes::analog_msg_pc_m   ||  \
                                      analog[aIndex].message == analogMessageTypes::analog_msg_pc_p  )

#define IS_ANALOG_14_BIT(aIndex)   (    analog[aIndex].message == analogMessageTypes::analog_msg_nrpn   ||  \
                                        analog[aIndex].message == analogMessageTypes::analog_msg_rpn    ||  \
                                        analog[aIndex].message == analogMessageTypes::analog_msg_pb     ||  \
                                        analog[aIndex].message == analogMessageTypes::analog_msg_key   )

#define IS_ANALOG_FB_7_BIT(aIndex)    (  analog[aIndex].feedback.message == analogMessageTypes::analog_msg_note   ||  \
                                         analog[aIndex].feedback.message == analogMessageTypes::analog_msg_cc     ||  \
                                         analog[aIndex].feedback.message == analogMessageTypes::analog_msg_pc     ||  \
                                         analog[aIndex].feedback.message == analogMessageTypes::analog_msg_pc_m   ||  \
                                        analog[aIndex].feedback.message == analogMessageTypes::analog_msg_pc_p)

#define IS_ANALOG_FB_14_BIT(aIndex)   (   analog[aIndex].feedback.message == analogMessageTypes::analog_msg_nrpn   ||   \
                                           analog[aIndex].feedback.message == analogMessageTypes::analog_msg_rpn   ||   \
                                           analog[aIndex].feedback.message == analogMessageTypes::analog_msg_pb    ||   \
                                          analog[aIndex].feedback.message == analogMessageTypes::analog_msg_key  )

#define ANALOG_MUXES              8
#define ANALOG_PER_MUX            16
#define ANALOG_MODULES_PER_MUX    8
#define ANALOG_PER_MODULES        ANALOG_PER_MUX/ANALOG_MODULES_PER_MUX
#define MAX_ANALOG_MODULES        ANALOG_MUXES*ANALOG_MODULES_PER_MUX

// set low and high limits to adjust for VCC and GND noise
#define RAW_THRESHOLD       5

#define ANALOG_INCREASING       0
#define ANALOG_DECREASING       1

#define SPLIT_DEAD_ZONE      24
#define NORMAL_DEAD_ZONE      16

#define NOISE_THRESHOLD_RAW_14BIT  20   // 20 - works great
#define NOISE_THRESHOLD_RAW_7BIT   10   // 10 - works great

#define FILTER_SIZE_ANALOG   4

#define MUX_A                0            // Mux A identifier
#define MUX_B                1            // Mux B identifier
#define MUX_C                2            // Mux C identifier
#define MUX_D                3            // Mux D identifier

#define MUX_A_PIN            A4           // Mux A pin
#define MUX_B_PIN            A3           // Mux B pin
#define MUX_C_PIN            A1           // Mux C pin
#define MUX_D_PIN            A2           // Mux D pin

#define NUM_MUX              4            // Number of multiplexers to address
#define NUM_MUX_CHANNELS     16           // Number of multiplexing channels
#define MAX_NUMBER_ANALOG   NUM_MUX*NUM_MUX_CHANNELS       

#define PRESCALER_4     0x000
#define PRESCALER_8     0x100
#define PRESCALER_16    0x200
#define PRESCALER_32    0x300
#define PRESCALER_64    0x400
#define PRESCALER_128   0x500
#define PRESCALER_256   0x600
#define PRESCALER_512   0x700

#define RESOL_12BIT     0x00
#define RESOL_10BIT     0x20
#define RESOL_8BIT      0x30

//----------------------------------------------------------------------------------------------------
// DIGITAL
//----------------------------------------------------------------------------------------------------

#define IS_DIGITAL_7_BIT(dIndex)   (  digital[dIndex].actionConfig.message == digitalMessageTypes::digital_msg_note   ||  \
                                      digital[dIndex].actionConfig.message == digitalMessageTypes::digital_msg_cc     ||  \
                                      digital[dIndex].actionConfig.message == digitalMessageTypes::digital_msg_pc   ||  \
                                      digital[dIndex].actionConfig.message == digitalMessageTypes::digital_msg_pc_m   ||  \
                                      digital[dIndex].actionConfig.message == digitalMessageTypes::digital_msg_pc_p   )

#define IS_DIGITAL_14_BIT(dIndex)  (  digital[dIndex].actionConfig.message == digitalMessageTypes::digital_msg_nrpn   ||  \
                                      digital[dIndex].actionConfig.message == digitalMessageTypes::digital_msg_rpn  ||  \
                                      digital[dIndex].actionConfig.message == digitalMessageTypes::digital_msg_pb   ||  \
                                      digital[dIndex].actionConfig.message == digitalMessageTypes::digital_msg_key )

#define IS_DIGITAL_FB_7_BIT(dIndex)    (  digital[dIndex].feedback.message == digitalMessageTypes::digital_msg_note   ||  \
                                         digital[dIndex].feedback.message == digitalMessageTypes::digital_msg_cc     ||  \
                                         digital[dIndex].feedback.message == digitalMessageTypes::digital_msg_pc    ||  \
                                         digital[dIndex].feedback.message == digitalMessageTypes::digital_msg_pc_m    ||  \
                                        digital[dIndex].feedback.message == digitalMessageTypes::digital_msg_pc_p    )

#define IS_DIGITAL_FB_14_BIT(dIndex)  (  digital[dIndex].feedback.message == digitalMessageTypes::digital_msg_nrpn   ||   \
                                         digital[dIndex].feedback.message == digitalMessageTypes::digital_msg_rpn   ||   \
                                         digital[dIndex].feedback.message == digitalMessageTypes::digital_msg_pb    ||  \
                                        digital[dIndex].feedback.message == digitalMessageTypes::digital_msg_key )

#define N_DIGITAL_MODULES           1
#define N_DIGITAL_INPUTS_X_MOD      4

#define VELOCITY_SESITIVITY_OFF     0xFF

#define NUM_DIGITAL_INPUTS          N_DIGITAL_MODULES*N_DIGITAL_INPUTS_X_MOD

#define DIGITAL_SCAN_INTERVAL       36

//----------------------------------------------------------------------------------------------------
// FEEDBACK
//----------------------------------------------------------------------------------------------------

// STATUS LED
// Which pin on the Arduino is connected to the NeoPixels?
// On a Trinket or Gemma we suggest changing this to 1
#define STATUS_LED_PIN            PIN_LED_TXL

// How many NeoPixels are attached to the Arduino?
#define N_STATUS_PIXEL            1

#define NUM_STATUS_LED            0

#define MAX_WAIT_MORE_DATA_MS     5
#define MAX_WAIT_MORE_DATA_TOTAL_MS 20
#define EXTERNAL_FEEDBACK         true

// ELEMENT FEEDBACK
#define FEEDBACK_UPDATE_BUFFER_SIZE   256 // = 256 dig + (32 rot + 32 enc) switch (analog has no fb yet)
#define MSG_BUFFER_AUX                128
#define FB_MAX_FRAMES_PER_UPDATE      8
#define USB_MIDI_MESSAGES_PER_LOOP     8
#define AUX_SHOW_TIMEOUT_US            250000UL
#define AUX_INIT_ACK_TIMEOUT_US         5000UL
#define AUX_BOOT_INIT_ACK_TIMEOUT_US  100000UL
#define AUX_BOOT_INIT_MAX_RETRIES          50
#define AUX_INIT_MAX_RETRIES               20
#define AUX_INIT_MAX_RESET_ATTEMPTS         3
#define AUX_RESET_BOOT_GRACE_US        500000UL
#define AUX_COMMAND_ACK_TIMEOUT_US     100000UL
#define AUX_COMMAND_MAX_RETRIES             3
#define AUX_RAINBOW_TIMEOUT_US        9000000UL
#define AUX_RAINBOW_MAX_RETRIES             2
#define AUX_MEMORY_ERROR_CONFIRM_US     250000UL
#define AUX_MEMORY_RECOVERY_MAX_RESETS          2
#define AUX_MEMORY_RECOVERY_STABLE_US      500000UL
#define AUX_QUEUE_FULL_BACKOFF_US           25000UL
#define AUX_BURST_ACK_TIMEOUT_US              5000UL

// COMMANDS
#define ACK_CMD                 0xAA
#define NEW_FRAME_BYTE          0xF0
#define BURST_INIT              0xF1
#define BURST_END               0xF2
#define CMD_ALL_LEDS_OFF        0xF3
#define CMD_ALL_LEDS_ON         0xF4
#define INIT_VALUES             0xF5
#define CHANGE_BRIGHTNESS       0xF6
#define END_OF_RAINBOW          0xF7
#define CHECKSUM_ERROR          0xF8
#define SHOW_REQUEST_DIRTY      0xF9
#define SHOW_END                0xFA
#define CMD_RAINBOW_START       0xFB
#define RESET_HAPPENED          0xFC
#define AUX_MEMORY_ERROR        0xFD
#define AUX_QUEUE_FULL          0xFE
#define END_OF_FRAME_BYTE       0xFF
#define SHOW_REQUEST_ALL        0xEF
#define SHOW_GRANT_DIRTY        0xEE
#define SHOW_GRANT_ALL          0xED

#define SHOW_KIND_NONE             0
#define SHOW_KIND_DIRTY            1
#define SHOW_KIND_ALL              2


#define ENCODER_CHANGE_FRAME            0x00
#define ENCODER_DOUBLE_FRAME            0x01
#define ENCODER_VUMETER_FRAME           0x02
#define ENCODER_SWITCH_CHANGE_FRAME     0x03
#define DIGITAL1_CHANGE_FRAME           0x04
#define DIGITAL2_CHANGE_FRAME           0x05
#define ANALOG_CHANGE_FRAME             0x06
#define BANK_CHANGE_FRAME               0x07
#define ENCODER_BLEND_FRAME             0x08

// BRIGHTNESS
#define BRIGHTNESS_WOP                    25
#define BRIGHTNESS_WOP_32_ENC             18
#define BRIGHTNESS_WITH_POWER             60
#define BANK_OFF_BRIGHTNESS_FACTOR_WP    2/3
#define BANK_OFF_BRIGHTNESS_FACTOR_WOP   2/3

// BLINK INTERVALS
#define STATUS_MIDI_BLINK_INTERVAL            15
#define STATUS_CONFIG_BLINK_INTERVAL          100
#define STATUS_ERROR_BLINK_INTERVAL           200
#define STATUS_CONFIG_ERROR_BLINK_INTERVAL    1000

#define SPOT_SIZE               26
#define S_SPOT_SIZE             14
#define FILL_SIZE               14
#define PIVOT_SIZE              14
#define MIRROR_SIZE             14
        
#define STATUS_LED_BRIGHTNESS   180

#define R_INDEX                 0
#define G_INDEX                 1
#define B_INDEX                 2

#define BANK_INDICATOR_COLOR_R  0xDA
#define BANK_INDICATOR_COLOR_G  0xA5
#define BANK_INDICATOR_COLOR_B  0x20

#define COLOR_RANGE_0           0
#define COLOR_RANGE_1           3
#define COLOR_RANGE_2           7
#define COLOR_RANGE_3           15
#define COLOR_RANGE_4           31
#define COLOR_RANGE_5           63
#define COLOR_RANGE_6           126
#define COLOR_RANGE_7           127

#define MIN_INTENSITY   32
#define MAX_INTENSITY   255

#define NO_SHIFTER          false
#define IS_SHIFTER          true
#define NO_BANK_UPDATE      false
#define BANK_UPDATE         true
#define NO_COLOR_CHANGE     false
#define COLOR_CHANGE        true
#define NO_VAL_TO_INT       false
#define VAL_TO_INT          true
#define NO_BLINK            0

#define NO_QSTB_LOAD        false
#define QSTB_LOAD           true

//----------------------------------------------------------------------------------------------------
// COMMS - SERIAL - MIDI
//----------------------------------------------------------------------------------------------------

// SysEx commands

#define MIDI_USB                  0
#define MIDI_HW                   1

#define MIDI_MERGE_FLAGS_USB_USB  0x01
#define MIDI_MERGE_FLAGS_USB_HW   0x02
#define MIDI_MERGE_FLAGS_HW_USB   0x04
#define MIDI_MERGE_FLAGS_HW_HW    0x08

#define MIDI_BUF_MAX_LEN          1000

#define SPI_SPEED_1_M       1000000
#define SPI_SPEED_1_5_M     1500000
#define SPI_SPEED_2_M       2000000

/*! Enumeration of MIDI types */
enum MidiTypeYTX: uint8_t
{
    InvalidType           = 0x0,    ///< For notifying errors
    NRPN                  = 0x6,    ///< NRPN
    RPN                   = 0x7,    ///< RPN
    NoteOff               = 0x8,    ///< Note Off
    NoteOn                = 0x9,    ///< Note On
    AfterTouchPoly        = 0xA,    ///< Polyphonic AfterTouch
    ControlChange         = 0xB,    ///< Control Change / Channel Mode
    ProgramChange         = 0xC,    ///< Program Change
    AfterTouchChannel     = 0xD,    ///< Channel (monophonic) AfterTouch
    PitchBend             = 0xE,    ///< Pitch Bend
};

#endif
