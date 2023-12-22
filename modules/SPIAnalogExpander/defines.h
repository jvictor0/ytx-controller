#ifndef defines_h
#define defines_h

// #define SERIAL_DEBUG // comment this line out to not print debug data on the serial bus

#if defined(SERIAL_DEBUG)
  #define SERIALPRINT(a)        {Serial.print(a);     }
  #define SERIALPRINTLN(a)      {Serial.println(a);   }
  #define SERIALPRINTF(a, f)    {Serial.print(a,f);   }
  #define SERIALPRINTLNF(a, f)  {Serial.println(a,f); }
#else
  #define SERIALPRINT(a)        {}
  #define SERIALPRINTLN(a)      {}
  #define SERIALPRINTF(a, f)    {}
  #define SERIALPRINTLNF(a, f)  {}
#endif

//types
typedef struct analogType{
  uint16_t rawValue;
  uint16_t rawValuePrev;
  bool direction;
};

// Constant value definitions
#define ADC_MAX_VALUE   1023

//noise filter
#define ANALOG_INCREASING       0
#define ANALOG_DECREASING       1

#define MUX_A                0            // Mux A identifier
#define MUX_B                1            // Mux B identifier
#define MUX_C                2            // Mux C identifier
#define MUX_D                3            // Mux D identifier
#define MUX_E                4            // Mux E identifier
#define MUX_F                5            // Mux F identifier

#define MUX_A_PIN            11           // Mux A pin
#define MUX_B_PIN            10           // Mux B pin
#define MUX_C_PIN            0            // Mux C pin
#define MUX_D_PIN            2            // Mux D pin
#define MUX_E_PIN            5            // Mux E pin
#define MUX_F_PIN            4            // Mux F pin

#define NUM_MUX              6            // Number of multiplexers to address
#define NUM_MUX_CHANNELS     16           // Number of multiplexing channels
#define MAX_ANALOG_INPUTS    NUM_MUX*NUM_MUX_CHANNELS  

// Address lines for multiplexer
const int _S0 = (4u);
const int _S1 = (3u);
const int _S2 = (8u);
const int _S3 = (9u);
// Input signal of multiplexers
const byte muxPin[NUM_MUX] = {MUX_A_PIN, MUX_B_PIN, MUX_C_PIN, MUX_D_PIN, MUX_E_PIN, MUX_F_PIN};

// Do not change - These are used to have the inputs and outputs of the headers in order
const byte MuxMapping[NUM_MUX_CHANNELS] =   {1,        // INPUT 0   - Mux channel 2
                                             0,        // INPUT 1   - Mux channel 0
                                             3,        // INPUT 2   - Mux channel 3
                                             2,        // INPUT 3   - Mux channel 1
                                             12,       // INPUT 4   - Mux channel 12
                                             13,       // INPUT 5   - Mux channel 14
                                             14,       // INPUT 6   - Mux channel 13
                                             15,       // INPUT 7   - Mux channel 15
                                             7,        // INPUT 8   - Mux channel 7
                                             4,        // INPUT 9   - Mux channel 4
                                             5,        // INPUT 10  - Mux channel 6
                                             6,        // INPUT 11  - Mux channel 5
                                             10,       // INPUT 12  - Mux channel 9
                                             9,        // INPUT 13  - Mux channel 10
                                             8,        // INPUT 14  - Mux channel 8
                                             11};      // INPUT 15  - Mux channel 11


#endif //defines_h