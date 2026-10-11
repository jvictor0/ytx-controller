#include <cassert>
#include <cstdint>
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <vector>
#include <cmath>
#include <new>
#define STATUS_BLINK 0
#define STATUS_FB_ERROR 0
#define MCP23017_BASE_ADDRESS 0
#define INF_POT_BASE_ADDRESS_A 0
#define INF_POT_BASE_ADDRESS_B 0
#define PROGMEM
#define pgm_read_byte(x) (*(x))
#define KEY_LEFT_ALT 1
#define KEY_LEFT_CTRL 2
#define KEY_LEFT_SHIFT 3
#include "Defines.h"
#include "types.h"
#undef SERIALPRINT
#undef SERIALPRINTLN
#define SERIALPRINT(...)
#define SERIALPRINTLN(...)
namespace statusLEDtypes { constexpr int STATUS_FB_MSG_OUT = 0; }
#include "modules.h"
#include "sysex.h"
static_assert(rot_absolute == 0 && rot_rel_binaryOffset == 1 && rot_rel_complement2 == 2 &&
              rot_rel_signedBit == 3 && rot_rel_signedBit2 == 4 && rot_rel_singleValue == 5 &&
              rot_rel_exponential == 6, "Serialized mode values must remain stable");
static_assert(sizeof(ytxEncoderType) == 56, "Serialized encoder configuration must not grow");

uint32_t nowUs = 0;
uint32_t micros() { return nowUs; }
uint32_t millis() { return nowUs / 1000; }
void delay(unsigned ms) { nowUs += ms * 1000; }
unsigned FreeMemory() { return 100000; }
unsigned resets = 0;
void SelfReset(int) { ++resets; }
struct SPIAdressableBUS { void DisableHWAddress(uint8_t) {} };
struct SPIEndlessPot
{
    uint16_t m_state = 0;
    unsigned m_reads = 0;
    uint32_t m_delay = 0;
    uint16_t readModule() { ++m_reads; nowUs += m_delay; return m_state; }
};
struct SPIGPIOExpander { uint16_t digitalRead() { return 0; } };
#include "EncoderInputsHost.h"

struct Message { uint8_t parameter, value, channel; };
struct FakeMidi
{
    std::vector<Message> m_cc;
    void sendControlChange(uint8_t p, uint8_t v, uint8_t c) { m_cc.push_back({p,v,c}); }
    void sendNoteOff(uint8_t, uint8_t, uint8_t) { assert(false); }
    void sendNoteOn(uint8_t, uint8_t, uint8_t) { assert(false); }
    void sendProgramChange(uint8_t, uint8_t) { assert(false); }
    void sendPitchBend(int16_t, uint8_t) { assert(false); }
} MIDI, MIDIHW;
struct Feedback
{
    unsigned m_changes = 0;
    template<typename... Args> void SetChangeEncoderFeedback(Args...) { ++m_changes; }
    int GetVumeterValue(uint8_t) { return 0; }
} feedbackHw;
struct Digital
{
    void SetButtonVelocity(uint16_t) {}
    unsigned GetButtonVelocity() { return 50; }
    void DigitalAction(uint16_t, uint16_t, bool) { assert(false); }
    unsigned GetDigitalValue(uint16_t) { return 0; }
} digitalHw;
struct Analog { void SendMessage(uint8_t) { assert(false); } } analogHw;
struct Keyboard { void releaseAll() {} void press(uint8_t) { assert(false); } } keyboard, *YTXKeyboard = &keyboard;
bool testEncoderSwitch = false;
bool CheckIfBankShifter(uint8_t, bool) { return false; }
void ScanMidiBufferAndUpdate(uint8_t, bool, uint8_t) {}
bool keyboardReleaseFlag = false, autoSelectMode = false, testEncoders = false, enableProcessing = true;
uint32_t millisKeyboardPress = 0;
uint16_t lastComponentInfoId = 0;
uint8_t currentBank = 0;
int mapl(int a, int b, int c, int d, int e) { return (a-b)*(e-d)/(c-b)+d; }
void SetStatusLED(int, int, int) {}
int GetHardwareID(int, int) { return 0; }
void SendComponentInfo(int, int) {}
ytxConfigurationType configuration{};
ytxConfigurationType* config = &configuration;
ytxEncoderType encoder[12]{};
ytxDigitalType digital[1]{};
ytxAnalogType analog[1]{};
ytxFeedbackType feedback[1]{};
EncoderInputs encoderHw;
using byte = unsigned char;
struct ExternalMemory
{
    ytxEncoderType m_next[12]{};
    void read(uint16_t, byte* target, uint16_t size) { memcpy(target, m_next, size); }
} external, *externalMemory = &external;
struct memoryHost
{
    std::vector<void*> m_allocations;
    std::vector<uint16_t> m_sizes;
    bool m_failResponse = false;
    ~memoryHost() { for (void* p : m_allocations) free(p); }
    void* AllocateRAM(uint16_t size)
    {
        m_sizes.push_back(size);
        if (m_failResponse && size == 12 * sizeof(ExponentialEncoderState)) return nullptr;
        void* p = malloc(size);
        memset(p, 0xa5, size);
        m_allocations.push_back(p);
        return p;
    }
    int bankNow = -1;
    uint16_t eepIndex = 0, bankSize = sizeof(encoder);
    void* bankChunk = encoder;
    struct Descriptor { uint16_t eepBaseAddress = 0, sectionSize = 0; } descriptors[8];
    uint8_t LoadBank(uint8_t);
    uint8_t LoadBankSingleSection(uint8_t, uint8_t, uint16_t, bool);
    void ReadFromEEPROM(uint8_t, uint8_t, uint16_t section, void* target, bool rotaryOnly)
    {
        auto& destination = *static_cast<ytxEncoderType*>(target);
        if (rotaryOnly)
        {
            destination.rotBehaviour = external.m_next[section].rotBehaviour;
            destination.rotaryConfig = external.m_next[section].rotaryConfig;
            destination.rotaryFeedback = external.m_next[section].rotaryFeedback;
        }
        else destination = external.m_next[section];
    }
};
memoryHost* memHost = nullptr;
#include "production.inc"

EncoderInputs::encoderBankData banks[12]{}, secondBank[12]{};
EncoderInputs::encoderBankData* bankPointers[] = {banks, secondBank};
EncoderInputs::encoderData hw[12]{};
EncoderInputs::moduleData modules[3]{};
SPIEndlessPot spi[3];
void* modulePointers[] = {&spi[0], &spi[1], &spi[2]};
ExponentialEncoderState response[12];
void EncoderInputs::InitRotaryModule(SPIAdressableBUS*, uint8_t moduleNo)
{
    encodersModule[moduleNo] = &spi[moduleNo];
}

void Setup()
{
    encoderHw = EncoderInputs{};
    memset(encoder, 0, sizeof(encoder));
    memset(banks, 0, sizeof(banks));
    memset(secondBank, 0, sizeof(secondBank));
    memset(hw, 0, sizeof(hw));
    memset(modules, 0, sizeof(modules));
    configuration = {};
    configuration.inputs.encoderCount = 12;
    configuration.banks.count = 2;
    for (unsigned i = 0; i < 3; ++i) { configuration.hwMapping.encoder[i] = E41H; spi[i] = {}; }
    encoderHw.nBanks = 2; encoderHw.nEncoders = 12; encoderHw.nModules = 3;
    encoderHw.begun = true; encoderHw.eBankData = bankPointers; encoderHw.eHwData = hw;
    encoderHw.encMData = modules; encoderHw.encodersModule = modulePointers;
    encoderHw.m_exponentialState = response;
    enableProcessing = true;
    nowUs += 100000;
    for (unsigned i = 0; i < 12; ++i)
    {
        encoder[i].rotBehaviour.hwMode = rot_rel_exponential;
        encoder[i].rotBehaviour.speed = rot_variable_speed_3;
        encoder[i].rotaryConfig.message = rotary_msg_cc;
        encoder[i].rotaryConfig.midiPort = midi_usb;
        encoder[i].rotaryConfig.channel = 8;
        encoder[i].rotaryConfig.parameter[rotary_LSB] = i;
        encoder[i].rotaryConfig.parameter[rotary_maxLSB] = 127;
        hw[i].nextJump = 13;
    }
    encoderHw.ResetExponentialEncoders(nowUs);
    MIDI.m_cc.clear(); MIDIHW.m_cc.clear(); feedbackHw.m_changes = 0;
}

double Sum()
{
    double sum = 0;
    for (const auto& m : MIDI.m_cc) { assert(m.value != 64); sum += ExponentialRelativeCodec::Decode(m.value); }
    return sum;
}

void InitAndButtons()
{
    Setup();
    memoryHost memory;
    memHost = &memory;
    memcpy(external.m_next, encoder, sizeof(encoder));
    SPIAdressableBUS bus;
    encoderHw.Init(2,12,&bus);
    assert(encoderHw.begun && resets == 0);
    unsigned responseAllocations = 0;
    for (auto size : memory.m_sizes) if (size == 12 * sizeof(ExponentialEncoderState)) ++responseAllocations;
    assert(responseAllocations == 1);
    for (unsigned i = 0; i < 12; ++i)
    {
        assert(encoderHw.m_exponentialState[i].m_codec.m_remainder == 0);
        assert(encoderHw.m_exponentialState[i].m_response.m_initialized);
        assert(encoderHw.m_exponentialState[i].m_response.m_framePulses == 0);
    }
    config->board.saveControllerState = true;
    DumpControllerState();
    assert(MIDI.m_cc.empty());
    encoderHw.Init(0,0,&bus);
    encoderHw.ServiceExponentialEncoders(nowUs);
    assert(!encoderHw.begun && MIDI.m_cc.empty());
    Setup();
    memory.m_failResponse = true;
    encoderHw.Init(1,12,&bus);
    assert(!encoderHw.begun && resets == 1);
    Setup();
    encoder[0].switchConfig.mode = switch_mode_message;
    encoder[0].switchConfig.message = switch_msg_cc;
    encoder[0].switchConfig.midiPort = midi_usb;
    encoder[0].switchConfig.channel = 1;
    encoder[0].switchConfig.parameter[switch_parameter_LSB] = 33;
    encoder[0].switchConfig.parameter[switch_maxValue_LSB] = 127;
    banks[0].switchInputState = true;
    encoderHw.SwitchAction(0,0,1,false);
    assert(MIDI.m_cc.size() == 1 && MIDI.m_cc[0].channel == 2 && MIDI.m_cc[0].parameter == 33);
    assert(response[0].m_response.m_framePulses == 0);
    MIDI.m_cc.clear();
    encoder[0].switchConfig.doubleClick = switch_doubleClick_2max;
    modules[0].state = 0x0f00;
    hw[0].clickCount = 2;
    hw[0].lastSwitchBounce = millis() - DOUBLE_CLICK_WAIT - 1;
    encoderHw.SwitchCheck(0,0);
    assert(banks[0].encoderValue == 127 && hw[0].clickCount == 0);
    encoderHw.ServiceExponentialEncoders(nowUs + 8000);
    assert(MIDI.m_cc.empty());
}

void MotionAndSilence()
{
    Setup();
    for (unsigned speed : {0,1,2,3,4,5})
    {
        encoder[0].rotBehaviour.speed = speed;
        banks[0].encFineAdjust = 1; banks[0].shiftRotaryAction = 1;
        banks[0].buttonSensitivityControlOn = 1; hw[0].switchHWState = 1;
        modules[0].state = 0x11;
        const int direction = encoderHw.DecodeRotaryDirection(0,0);
        assert(direction == 1 && !(modules[0].state & 0x10));
        hw[0].encoderDirection = direction;
        encoderHw.EncoderAction(0,0);
        nowUs += 1000;
    }
    assert(hw[0].nextJump == 13 && hw[0].currentSpeed == 0);
    assert(banks[0].encoderValue == 0 && MIDI.m_cc.empty());
    encoderHw.SendRotaryMessage(0,0,true);
    encoderHw.SendRotaryMessage(0,0,false);
    encoderHw.SendRotaryAltMessage(0,0,true);
    encoderHw.SendRotaryAltMessage(0,0,false);
    encoderHw.SendExponentialRotaryMessage(0,64);
    encoderHw.SetEncoderValue(0,0,45);
    assert(MIDI.m_cc.empty() && banks[0].encoderValue == 45);
    nowUs += 2000;
    encoderHw.ServiceExponentialEncoders(nowUs);
    assert(!MIDI.m_cc.empty());
    assert(std::abs(Sum() + response[0].m_codec.m_remainder - 6.0/1024) < 1e-15);
    for (const auto& m : MIDI.m_cc) assert(m.parameter == 0 && m.channel == 9);
    assert(MIDIHW.m_cc.empty() && banks[0].encoderValue == 45);
    Setup();
    encoder[0].rotaryConfig.channel = 15;
    encoder[0].rotaryConfig.parameter[rotary_LSB] = 97;
    encoder[0].rotaryConfig.midiPort = midi_hw;
    encoderHw.SendExponentialRotaryMessage(0,65);
    assert(MIDI.m_cc.empty() && MIDIHW.m_cc.size() == 1);
    assert(MIDIHW.m_cc[0].channel == 16 && MIDIHW.m_cc[0].parameter == 97);
}

void LegacyAndUnsupported()
{
    Setup();
    encoder[0].rotBehaviour.hwMode = rot_rel_binaryOffset;
    encoder[0].rotBehaviour.speed = rot_fixed_speed_1;
    modules[0].state = 0x11;
    assert(encoderHw.DecodeRotaryDirection(0,0) == 0);
    assert(encoderHw.DecodeRotaryDirection(0,0) == 0);
    assert(encoderHw.DecodeRotaryDirection(0,0) == 0);
    hw[0].encoderDirection = encoderHw.DecodeRotaryDirection(0,0);
    assert(hw[0].encoderDirection == 1);
    encoderHw.EncoderAction(0,0);
    assert(MIDI.m_cc.size() == 1 && MIDI.m_cc[0].value == 65 && hw[0].nextJump == 1);
    Setup();
    encoder[0].rotaryConfig.message = rotary_msg_note;
    assert(!encoderHw.IsExponentialEncoder(0));
    hw[0].encoderDirection = 1;
    encoderHw.EncoderAction(0,0);
    encoderHw.SendRotaryMessage(0,0,true);
    encoderHw.SendRotaryAltMessage(0,0,true);
    encoderHw.SendExponentialRotaryMessage(0,65);
    encoder[0].rotaryConfig.message = rotary_msg_cc;
    configuration.hwMapping.encoder[0] = E41H_D;
    assert(!encoderHw.IsExponentialEncoder(0));
    encoderHw.EncoderAction(0,0);
    encoderHw.ServiceExponentialEncoders(nowUs + 8000);
    assert(MIDI.m_cc.empty());
}

void FairScanAndDelayedReceipt()
{
    Setup();
    encoderHw.priorityCount = 2;
    encoderHw.priorityList[0] = 0; encoderHw.priorityList[1] = 1;
    encoderHw.priorityTime = millis();
    for (auto& module : spi) module.m_state = 0x11;
    encoderHw.Read();
    for (unsigned i = 0; i < 3; ++i) { assert(spi[i].m_reads == 1); assert(response[4*i].m_response.m_framePulses == 1); }
    encoderHw.Read();
    for (const auto& module : spi) assert(module.m_reads == 1);
    nowUs += 8000;
    encoderHw.ServiceExponentialEncoders(nowUs);
    bool seen[3] = {};
    for (const auto& m : MIDI.m_cc) seen[m.parameter/4] = true;
    assert(seen[0] && seen[1] && seen[2]);
    Setup();
    for (auto& route : encoder) route.rotBehaviour.hwMode = rot_rel_binaryOffset;
    encoderHw.priorityCount = 2;
    encoderHw.priorityList[0] = 0; encoderHw.priorityList[1] = 1;
    encoderHw.priorityTime = millis();
    encoderHw.Read();
    assert(spi[0].m_reads == 1 && spi[1].m_reads == 1 && spi[2].m_reads == 0);
    Setup();
    nowUs = 95100;
    encoderHw.ResetExponentialEncoders(88100);
    spi[0].m_state = 0x11; spi[0].m_delay = 1800;
    encoderHw.Read();
    assert(nowUs == 96900);
    spi[0].m_delay = 0;
    for (uint32_t receipt : {96950,97010,98010,99010,100010,101010,102010,103010,104010})
    { nowUs = receipt; encoderHw.Read(); }
    assert(response[0].m_response.m_framePulses == 10);
    nowUs = 104100;
    encoderHw.ServiceExponentialEncoders(nowUs);
    assert(std::abs(Sum() + response[0].m_codec.m_remainder - 10.0/1024) < 1e-15);
}

void SameMillisService()
{
    Setup();
    nowUs += 100;
    encoderHw.ResetExponentialEncoders(nowUs);
    spi[0].m_state = 0x0f11;
    encoderHw.Read();
    spi[0].m_state = 0x0f00;
    nowUs += 7900;
    encoderHw.Read();
    encoderHw.ServiceExponentialEncoders(nowUs);
    assert(MIDI.m_cc.empty());
    const unsigned reads = spi[0].m_reads;
    nowUs += 100;
    encoderHw.Read();
    assert(spi[0].m_reads == reads);
    encoderHw.ServiceExponentialEncoders(nowUs);
    assert(!MIDI.m_cc.empty());
    assert(std::abs(Sum() + response[0].m_codec.m_remainder - 1.0/1024) < 1e-15);
}

void RoutesAndDisabled()
{
    Setup();
    memoryHost memory;
    memHost = &memory;
    memcpy(external.m_next, encoder, sizeof(encoder));
    external.m_next[0].rotaryConfig.channel = 2;
    external.m_next[0].rotaryConfig.parameter[rotary_LSB] = 77;
    hw[0].encoderDirection = 1;
    encoderHw.EncoderAction(0,0);
    memory.LoadBank(1);
    assert(!MIDI.m_cc.empty());
    for (const auto& m : MIDI.m_cc) assert(m.channel == 9 && m.parameter == 0);
    assert(response[0].m_response.m_framePulses == 0 && response[0].m_codec.m_remainder == 0);
    MIDI.m_cc.clear();
    encoderHw.EncoderAction(0,0);
    external.m_next[0].rotaryConfig.channel = 4;
    memory.LoadBankSingleSection(0, ytxIOBLOCK::Encoder, 0, true);
    for (const auto& m : MIDI.m_cc) assert(m.channel == 3 && m.parameter == 77);
    assert(!MIDI.m_cc.empty());
    MIDI.m_cc.clear();
    encoder[0].switchConfig.mode = switch_mode_quick_shift;
    encoder[0].switchConfig.parameter[switch_parameter_LSB] = 1;
    banks[0].switchInputState = true;
    hw[0].encoderDirection = 1;
    encoderHw.EncoderAction(0,0);
    external.m_next[0].rotaryConfig.channel = 6;
    encoderHw.SwitchAction(0,0,1,false);
    assert(hw[0].thisEncoderBank == 1);
    assert(!MIDI.m_cc.empty());
    for (const auto& m : MIDI.m_cc) assert(m.channel == 5 && m.parameter == 77);
    MIDI.m_cc.clear();
    encoderHw.EncoderAction(0,0);
    enableProcessing = false;
    encoderHw.ServiceExponentialEncoders(nowUs + 8000);
    enableProcessing = true;
    encoderHw.ServiceExponentialEncoders(nowUs + 16000);
    assert(MIDI.m_cc.empty());
    encoderHw.begun = false; encoderHw.m_exponentialState = nullptr;
    encoderHw.ServiceExponentialEncoders(nowUs);
    encoderHw.ResetExponentialEncoders(nowUs);
    encoderHw.FlushExponentialEncoders(nowUs);
    assert(!encoderHw.IsExponentialEncoder(0));
}
int main()
{
    InitAndButtons(); MotionAndSilence(); LegacyAndUnsupported(); FairScanAndDelayedReceipt(); SameMillisService(); RoutesAndDisabled();
    std::printf("MAIN production integration: init/allocation, dump/buttons, motion/legacy, silence/routes, disabled, same-millis service, three-module fairness and ten delayed receipts passed; state = %zu bytes.\n", sizeof(ExponentialEncoderState));
}
