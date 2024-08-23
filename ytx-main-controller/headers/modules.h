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

#ifndef MODULES_H
#define MODULES_H
#include <stdint.h>


enum EncoderModuleTypes{
	ENCODER_NONE,
	E41H,   // DETENTED ENCODERS - HORIZONTAL
	E41V,   // DETENTED ENCODERS - VERTICAL
    E41H_D, // DETENTED ENCODERS - HORIZONTAL
    E41V_D, // DETENTED ENCODERS - VERTICAL
};
enum DigitalModuleTypes{
	DIGITAL_NONE,
	RB41,
	RB42,
	RB82,
	ARC41
};
enum AnalogModuleTypes{
	ANALOG_NONE,
	P41,		// 4 rotary potentiometers
	F41,		// 4 slide potentiometers, 45mm travel
	JAF,		// 2 axis joystick, fixed (return to center)
	JAL,		// 2 axis joystick, loose (no return to center)
    F21100      // 2 slide potentiometers, 100mm travel
};
enum FeedbackModuleTypes{
	FB_NONE,	
	A11,		// Feeback to analog module in MUX 1, position 1	
	A12,		// Feeback to analog module in MUX 1, position 2
	A13,		// Feeback to analog module in MUX 1, position 3
	A14,		// Feeback to analog module in MUX 1, position 4
	A21,		// Feeback to analog module in MUX 2, position 1
	A22,		// Feeback to analog module in MUX 2, position 2
	A23,		// Feeback to analog module in MUX 2, position 3
	A24,		// Feeback to analog module in MUX 2, position 4
	A31,		// Feeback to analog module in MUX 3, position 1
	A32,		// Feeback to analog module in MUX 3, position 2
	A33,		// Feeback to analog module in MUX 3, position 3
	A34,		// Feeback to analog module in MUX 3, position 4
	A41,		// Feeback to analog module in MUX 4, position 1
	A42,		// Feeback to analog module in MUX 4, position 2
	A43,		// Feeback to analog module in MUX 4, position 3
	A44,		// Feeback to analog module in MUX 4, position 4
};

enum FeedbackTypes{
	NONE,
    FB_ENCODER,
    FB_ENC_VUMETER,
    FB_ENC_VAL_TO_COLOR,
    FB_ENC_VAL_TO_INT,
    FB_ENC_SWITCH,
    FB_ENC_2CC,
    FB_ENC_SHIFT,
    FB_ENC_SW_VAL_TO_INT,
    FB_DIGITAL,
    FB_DIG_VAL_TO_INT,
    FB_ANALOG,
    FB_INDEPENDENT,
    FB_BANK_CHANGED,
    FB_BANK_ENCODERS,
    FB_BANK_DIGITAL1,
    FB_BANK_DIGITAL2,
};

enum ButtonConnectTypes{
	BUTTON_DIRECT,
	BUTTON_MATRIX
};

typedef struct ytxNumberOfModules{
    uint8_t encoders;
    uint8_t digital[2]; // 2 digital ports
    uint8_t analog;
    uint8_t feedback;
} ytxNumberOfModules;

ytxNumberOfModules modulesInConfig; 
uint8_t amountOfDigitalInConfig[2]; // 2 Digital ports

typedef struct ytxModuleInputs{
    uint8_t nEncoders;
    uint8_t nDigital;
    uint8_t nAnalog;
    uint8_t nLedsPerControl;
} ytxModuleInputs;  // Add name for typedef

enum e41orientation{
	HORIZONTAL,
	VERTICAL
};

// E41 definition
typedef struct{
    ytxModuleInputs components;
    uint8_t nextAddressPin[3];
    uint8_t encPins[4][2];
    uint8_t encSwitchPins[4]; 
} ytxE41Module;

ytxE41Module defE41module = {
    .components = {4, 0, 0, 16},
    .nextAddressPin = {6, 7, 8},
    .encPins = {
      {0, 1},       // A,B
      {3, 4},   
      {15, 14},  
      {12, 11}   
    },
    .encSwitchPins = { 2, 5, 13, 10 }
}; 

	// RB41 definition
typedef struct{
    ytxModuleInputs components;
    uint8_t nextAddressPin[3];
    uint8_t rb41pins[4];
    bool buttonWiredStyle;
} ytxRB41Module;

ytxRB41Module defRB41module = {
    .components = {0, 4, 0, 1},
    .nextAddressPin = {6, 7, 8},
    .rb41pins = { 0, 1, 2, 3 },
    .buttonWiredStyle = BUTTON_DIRECT
}; 

// RB42 definition
typedef struct{
    ytxModuleInputs components;
    uint8_t nextAddressPin[3];
    uint8_t rb42pins[8];
    bool buttonWiredStyle;
} ytxRB42Module;

ytxRB42Module defRB42module = {
    .components = {0, 8, 0, 1},
    .nextAddressPin = {6, 7, 8},
    .rb42pins = { 0, 1, 2, 3, 4, 5, 9, 10 },
    .buttonWiredStyle = BUTTON_DIRECT
}; 

// RB82 definition

#define RB82_ROWS	4
#define RB82_COLS	4

#define ROWS    0
#define COLS    1

typedef struct{
    ytxModuleInputs components;
    uint8_t nextAddressPin[3];
    uint8_t rb82pins[2][RB82_COLS];		// [rows][cols]
    uint8_t buttonMapping[RB82_ROWS][RB82_COLS];
    bool buttonWiredStyle;
} ytxRB82Module;

ytxRB82Module defRB82module = {
    .components = {0, 16, 0, 1},
    .nextAddressPin = {6, 7, 8},
    .rb82pins = { 	{0, 1, 2, 3 },        // ROWS
    				{15,14,13,12}	},    // COLS
    .buttonMapping = {  {0, 1, 8,  9}, 
                        {2, 3, 10, 11},
                        {4, 5, 12, 13}, 
                        {6, 7, 14, 15}   },
	.buttonWiredStyle = BUTTON_MATRIX
}; 


ytxModuleInputs defP41module{
    .nEncoders = 0,
    .nDigital = 0,
    .nAnalog = 4,
    .nLedsPerControl = 0
};

ytxModuleInputs defF41module{
    .nEncoders = 0,
    .nDigital = 0,
    .nAnalog = 4,
    .nLedsPerControl = 0
};

ytxModuleInputs defF21100module{
    .nEncoders = 0,
    .nDigital = 0,
    .nAnalog = 2,
    .nLedsPerControl = 0
};

ytxModuleInputs defJAFmodule{
    .nEncoders = 0,
    .nDigital = 0,
    .nAnalog = 2,
    .nLedsPerControl = 0
};

ytxModuleInputs defJALmodule{
    .nEncoders = 0,
    .nDigital = 0,
    .nAnalog = 2,
    .nLedsPerControl = 0
};

// // CHANGE TO SAME AS E41 - INITIALIZE IN CLASS
// typedef struct{
// 	// encoder pin connections to MCP23S17
// 	uint8_t buttonPins[RB41_components.nDigital] = { 0, 1, 2, 3 };
// }ytxRB41Module;

// typedef struct{
// 	// encoder pin connections to MCP23S17
// 	uint8_t buttonPins[RB42_components.nDigital] = { 0, 1, 2, 3, 4, 5, 9, 10 };
// }ytxRB42Module;


#endif
