// Host model for SAM D21 USB OUT admission, not USB bus/wire timing.
#include <array>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <string>

#define CHECK(expr) do { if (!(expr)) { \
    std::cerr << __LINE__ << ": " << #expr << "\n"; std::exit(1); \
} } while (false)

static uint32_t irqMask = 0;
static void (*onEnableIRQ)() = nullptr;
uint32_t __get_PRIMASK() { return irqMask; }
void __disable_irq() { irqMask = 1; }
void __enable_irq() {
    irqMask = 0;
    if (onEnableIRQ) {
        auto callback = onEnableIRQ;
        onEnableIRQ = nullptr;
        callback();
    }
}
void __ISB() {}
static unsigned allocations = 0;
void* tracked_malloc(size_t size) { ++allocations; return std::malloc(size); }

#define USB_ENDPOINT_TYPE_CONTROL 0
#define USB_ENDPOINT_TYPE_BULK 2
#define USB_ENDPOINT_TYPE_INTERRUPT 3
#define USB_ENDPOINT_OUT(addr) ((addr) | 0x00)
#define USB_ENDPOINT_IN(addr) ((addr) | 0x80)

struct USBDevice_SAMD21G18x {
    struct Bank {
        uint8_t type = 0;
        uint32_t size = 0, count = 0, multi = 0;
        void* address = nullptr;
        bool ready = false, interrupt = false, complete = false, dataToggle = false;
    };
    std::array<Bank, 8> out, in;
    void epBank0SetType(uint32_t ep, uint8_t type) {
        out.at(ep).type = type;
        // DS40001882G 32.6.2.2: disabling an endpoint clears its IRQs.
        if (type == 0) {
            out.at(ep).interrupt = false;
            out.at(ep).complete = false;
        }
    }
    void epBank1SetType(uint32_t ep, uint8_t type) { in.at(ep).type = type; }
    void epBank0SetSize(uint32_t ep, uint32_t size) { out.at(ep).size = size; }
    void epBank1SetSize(uint32_t ep, uint32_t size) { in.at(ep).size = size; }
    void epBank0SetAddress(uint32_t ep, void* address) { out.at(ep).address = address; }
    void epBank1SetAddress(uint32_t ep, void* address) { in.at(ep).address = address; }
    void epBank0ResetDataToggle(uint32_t ep) { out.at(ep).dataToggle = false; }
    void epBank0SetReady(uint32_t ep) { out.at(ep).ready = true; }
    void epBank0ResetReady(uint32_t ep) { out.at(ep).ready = false; }
    void epBank1ResetReady(uint32_t ep) { in.at(ep).ready = false; }
    void epBank0EnableTransferComplete(uint32_t ep) { out.at(ep).interrupt = true; }
    void epBank0DisableTransferComplete(uint32_t ep) { out.at(ep).interrupt = false; }
    void epBank0SetMultiPacketSize(uint32_t ep, uint32_t n) { out.at(ep).multi = n; }
    void epBank0SetByteCount(uint32_t ep, uint32_t n) { out.at(ep).count = n; }
    uint32_t epBank0ByteCount(uint32_t ep) { return out.at(ep).count; }
    bool epBank0IsTransferComplete(uint32_t ep) { return out.at(ep).complete; }
    void epBank0AckTransferComplete(uint32_t ep) { out.at(ep).complete = false; }
    void busReset() {
        // DS40001882G 32.6.2.4: SRAM survives; non-EP0 EPCFG and
        // endpoint interrupt enables/flags do not. Do NOT zero descriptors.
        for (size_t ep = 0; ep < out.size(); ++ep) {
            if (ep != 0) out[ep].type = in[ep].type = 0;
            out[ep].interrupt = in[ep].interrupt = false;
            out[ep].complete = in[ep].complete = false;
        }
    }
    bool hostSend(uint32_t ep, const uint8_t* data, size_t size, int dataPid = -1) {
        auto& bank = out.at(ep);
        if (bank.type != 3 || bank.ready || !bank.address) return false;
        // A duplicate PID is ACKed without delivering data (32.6.2.7).
        // Most cases omit PID; the configuration case supplies host DATA0.
        if (dataPid >= 0 && dataPid != bank.dataToggle) return true;
        CHECK(bank.size == 64);
        CHECK(size <= bank.multi);
        if (size) std::memcpy(bank.address, data, size);
        const size_t packets = size ? (size + bank.size - 1) / bank.size : 1;
        if (packets % 2) bank.dataToggle = !bank.dataToggle;
        bank.count = size;
        bank.complete = bank.ready = true;
        return true;
    }
};
static USBDevice_SAMD21G18x usbd;
static uint8_t udd_ep_in_cache_buffer[8][64];
static uint8_t udd_ep_out_cache_buffer[8][64];
static uint32_t _usbConfiguration = 0;
static bool cdcEnabled = false;
static uint32_t EndPoints[] = {0, USB_ENDPOINT_TYPE_BULK | USB_ENDPOINT_OUT(0),
    USB_ENDPOINT_TYPE_BULK | USB_ENDPOINT_IN(0), 0};
struct USBDeviceClass {
    void initEP(uint32_t ep, uint32_t config);
    void initEndpoints();
    uint32_t recv(uint32_t ep, void* data, uint32_t len);
    uint32_t available(uint32_t ep);
    // Only handler-backed endpoints are exercised; reject fallback use.
    void armRecv(uint32_t) { CHECK(false); }
};
static USBDeviceClass device;
