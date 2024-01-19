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

//noise filter
#define ANALOG_INCREASING       0
#define ANALOG_DECREASING       1

enum{
  MUX_A,
  MUX_B,
  MUX_C,
  MUX_D,
  MUX_E,
  MUX_F,
  MUX_COUNT
};

#define MUX_S0_PIN              4              // Mux selector 0 pin on arduino wiring(boards.txt)
#define MUX_S1_PIN              3              // Mux selector 1 pin on arduino wiring(boards.txt)
#define MUX_S2_PIN              8              // Mux selector 2 pin on arduino wiring(boards.txt)
#define MUX_S3_PIN              9              // Mux selector 3 pin on arduino wiring(boards.txt)
const byte muxSelectorsWiring[] = {MUX_S0_PIN,MUX_S1_PIN,MUX_S2_PIN,MUX_S3_PIN};

#define MUX_SELECTION_LINES     sizeof(muxSelectorsWiring)
#define NUM_MUX_CHANNELS        (1<<MUX_SELECTION_LINES)    // Number of multiplexing channels
#define MAX_ANALOG_INPUTS       MUX_COUNT*NUM_MUX_CHANNELS  


#define ADC_CHANNEL_A            11           // ADC channel A on microcontroller
#define ADC_CHANNEL_B            10           // ADC channel B on microcontroller
#define ADC_CHANNEL_C            0            // ADC channel C on microcontroller
#define ADC_CHANNEL_D            2            // ADC channel D on microcontroller
#define ADC_CHANNEL_E            5            // ADC channel E on microcontroller
#define ADC_CHANNEL_F            4            // ADC channel F on microcontroller
// Input signal of multiplexers
const byte acdChannel[MUX_COUNT] = {ADC_CHANNEL_A, ADC_CHANNEL_B, ADC_CHANNEL_C, ADC_CHANNEL_D, ADC_CHANNEL_E, ADC_CHANNEL_F};

#define MUX_A_PIN               19           // Mux A pin on arduino wiring(boards.txt)
#define MUX_B_PIN               25           // Mux B pin on arduino wiring(boards.txt)
#define MUX_C_PIN               15           // Mux C pin on arduino wiring(boards.txt)
#define MUX_D_PIN               14           // Mux D pin on arduino wiring(boards.txt)
#define MUX_E_PIN               17           // Mux E pin on arduino wiring(boards.txt)
#define MUX_F_PIN               18           // Mux F pin on arduino wiring(boards.txt)
// Input signal of multiplexers
const byte adcWiring[MUX_COUNT] = {MUX_A_PIN,MUX_B_PIN,MUX_C_PIN,MUX_D_PIN,MUX_E_PIN,MUX_F_PIN};

// Do not change - These are used to have the inputs and outputs of the PCB headers in order
const byte muxMapping[NUM_MUX_CHANNELS] =   {1,        // INPUT 0   - Mux channel 2
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