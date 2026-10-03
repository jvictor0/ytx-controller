// MIDIUSB ABI and USB boundary used by the verbatim accept/read definitions.
#define MIDI_BUFFER_SIZE 64
#define MIDI_RX 1
struct midiEventPacket_t { uint8_t header, byte1, byte2, byte3; };
struct ring_bufferMIDI {
    midiEventPacket_t midiEvent[MIDI_BUFFER_SIZE];
    volatile uint32_t head, tail;
};
static ring_bufferMIDI midi_rx_buffer = {};
struct MIDI_ {
    void accept();
    uint32_t available();
    midiEventPacket_t read();
};
static MIDI_ midi;
static void (*beforeUSBRecv)() = nullptr;
uint32_t USB_Available(uint32_t ep) { return device.available(ep); }
int USB_Recv(uint32_t ep, void* data, uint32_t len) {
    if (beforeUSBRecv) {
        auto callback = beforeUSBRecv;
        beforeUSBRecv = nullptr;
        callback();
    }
    return device.recv(ep, data, len);
}
