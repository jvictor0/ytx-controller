static const std::array<uint8_t, 4> oldPacket = {{0x0B, 0xB0, 0x10, 0x7F}};
static const std::array<uint8_t, 4> newPacket = {{0x0B, 0xB0, 0x20, 0x40}};

void configure() {
    device.initEndpoints();
    _usbConfiguration = 1;
}
void reconnect() {
    usbd.busReset();
    _usbConfiguration = 0;
    device.initEP(0, USB_ENDPOINT_TYPE_CONTROL);
    configure();
}
void dispatch() {
    if (usbd.out[1].interrupt && usbd.out[1].complete) epHandlers[1]->handleEndpoint();
}
void send(const std::array<uint8_t, 4>& packet) {
    CHECK(usbd.hostSend(1, packet.data(), packet.size()));
    dispatch();
}
void receive(const std::array<uint8_t, 4>& packet) {
    std::array<uint8_t, 4> actual = {};
    CHECK(device.available(1) == 4);
    CHECK(device.recv(1, actual.data(), actual.size()) == 4);
    CHECK(actual == packet);
}
void freshReceive() {
    CHECK(device.available(1) == 0);
    CHECK(usbd.out[1].interrupt);
    CHECK(usbd.in[2].type == 3);
    send(newPacket);
    receive(newPacket);
}
void emptyMidi() {
    // Deliberately dirty unused storage: an empty read must not expose it.
    midi_rx_buffer.midiEvent[midi_rx_buffer.tail] = {0x0B, 0xB0, 0x55, 0x7F};
    const auto packet = midi.read();
    CHECK(packet.header == 0 && packet.byte1 == 0 && packet.byte2 == 0 && packet.byte3 == 0);
}
int main(int argc, char** argv) {
    CHECK(argc == 2);
    const std::string test = argv[1];
    configure();
    const auto initialHandler = epHandlers[1];
    const auto initialBuffer = usbd.out[1].address;
    const auto initialAllocations = allocations;
    if (test == "cold") {
        freshReceive();
        send(oldPacket); receive(oldPacket);
    } else if (test == "reset") {
        send(oldPacket); receive(oldPacket);
        reconnect(); freshReceive();
    } else if (test == "partial") {
        send(oldPacket);
        uint8_t byte;
        CHECK(device.recv(1, &byte, 1) == 1);
        reconnect(); freshReceive();
    } else if (test == "full") {
        send(oldPacket); send(oldPacket);
        CHECK(!usbd.hostSend(1, newPacket.data(), 4));
        reconnect(); freshReceive();
        // Exercise backpressure/release after reconfiguration too.
        send(oldPacket); send(newPacket);
        CHECK(!usbd.hostSend(1, newPacket.data(), 4));
        receive(oldPacket); receive(newPacket);
        send(newPacket); receive(newPacket);
    } else if (test == "pending") {
        CHECK(usbd.hostSend(1, oldPacket.data(), 4));
        // Repeated SET_CONFIGURATION, without EORST clearing the flag.
        configure(); dispatch(); freshReceive();
    } else if (test == "repeat") {
        for (int i = 0; i < 1000; ++i) {
            send(oldPacket);
            if (i % 2) reconnect(); else configure();
            CHECK(epHandlers[1] == initialHandler);
            CHECK(usbd.out[1].address == initialBuffer);
            CHECK(allocations == initialAllocations);
            freshReceive();
        }
    } else if (test == "zlp") {
        reconnect();
        CHECK(usbd.hostSend(1, nullptr, 0)); dispatch();
        freshReceive();
    } else if (test == "full_transfer") {
        reconnect();
        std::array<uint8_t, 256> payload;
        payload.fill(0x5A);
        CHECK(usbd.hostSend(1, payload.data(), payload.size())); dispatch();
        std::array<uint8_t, 512> actual = {};
        CHECK(device.recv(1, actual.data(), actual.size()) == 256);
        CHECK(std::memcmp(actual.data(), payload.data(), 256) == 0);
        CHECK(actual[256] == 0);
        CHECK(device.recv(1, actual.data(), actual.size()) == 0);
        freshReceive();
    } else if (test == "midi_ring_wrap") {
        // Two full transfers wrap the 64-event MIDI ring and exercise the
        // event left in the endpoint when the ring reaches its 63-event cap.
        std::array<uint8_t, 256> payload;
        for (int i = 0; i < 64; ++i) {
            payload[i * 4] = 0x0B; payload[i * 4 + 1] = 0xB0;
            payload[i * 4 + 2] = 0x10; payload[i * 4 + 3] = i;
        }
        reconnect();
        for (int transfer = 0; transfer < 2; ++transfer) {
            CHECK(usbd.hostSend(1, payload.data(), payload.size())); dispatch();
            for (int i = 0; i < 64; ++i) {
                const auto packet = midi.read();
                CHECK(packet.header == 0x0B && packet.byte1 == 0xB0);
                CHECK(packet.byte2 == 0x10 && packet.byte3 == i);
            }
            emptyMidi();
        }
    } else if (test == "data0_reconfigure") {
        for (int priorPackets = 1; priorPackets <= 2; ++priorPackets) {
            configure();
            for (int i = 0; i < priorPackets; ++i) {
                CHECK(usbd.hostSend(1, oldPacket.data(), 4, i % 2)); dispatch();
                receive(oldPacket);
            }
            // SET_CONFIGURATION restarts the host at DATA0 even without EORST.
            configure();
            CHECK(usbd.hostSend(1, newPacket.data(), 4, 0)); dispatch();
            receive(newPacket);
        }
    } else if (test == "irq_recv0" || test == "irq_recv1") {
        if (test == "irq_recv1") { send(oldPacket); receive(oldPacket); }
        send(oldPacket);
        // Deliver a pending reset ISR at the first IRQ-unmask boundary.
        // Before the fix this is inside recv, before its copy/index updates.
        onEnableIRQ = reconnect;
        std::array<uint8_t, 4> data = {};
        auto count = device.recv(1, data.data(), 4);
        CHECK(onEnableIRQ == nullptr);
        CHECK(count == 0 || (count == 4 && data == oldPacket));
        CHECK(irqMask == 0);
        freshReceive();
    } else if (test == "irq_available") {
        send(oldPacket);
        uint8_t byte;
        CHECK(device.recv(1, &byte, 1) == 1);
        onEnableIRQ = reconnect;
        auto count = device.available(1);
        CHECK(count == 0 || count == 3);
        CHECK(onEnableIRQ == nullptr);
        freshReceive();
    } else if (test == "masked") {
        irqMask = 1;
        reconnect(); freshReceive();
        CHECK(irqMask == 1);
    } else if (test == "midi_empty") {
        // A short transfer is consumed without filling the MIDI event ring.
        CHECK(usbd.hostSend(1, oldPacket.data(), 1)); dispatch();
        emptyMidi();
    } else if (test == "midi_reset_race") {
        send(oldPacket);
        beforeUSBRecv = reconnect;
        emptyMidi();
        CHECK(beforeUSBRecv == nullptr);
        send(newPacket);
        auto packet = midi.read();
        CHECK(std::memcmp(&packet, newPacket.data(), 4) == 0);
        emptyMidi();
    } else if (test == "midi_unconfigured") {
        send(oldPacket);
        usbd.busReset(); _usbConfiguration = 0;
        emptyMidi();
    } else {
        CHECK(false);
    }
    CHECK(epHandlers[1] == initialHandler);
    CHECK(allocations == initialAllocations);
}
