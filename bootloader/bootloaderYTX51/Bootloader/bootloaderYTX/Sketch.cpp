
#include <Arduino.h>

/*End of auto generated code by Atmel studio */

#include <extEEPROM.h>
#include <MIDI.h>
#include <midi_Defs.h>
#include <Adafruit_NeoPixel.h>

#include <midi_UsbTransport.h>
#include "board_definitions.h"

#define STATUS_LED_BRIGHTNESS   64
#define STATUS_LED_PIN			26

#define SAMD11_BOOT_MODE_PIN	6
#define SAMD11_RST_PIN			38

#define RX_MIDI_PIN				0
#define TX_MIDI_PIN				1


static const unsigned sUsbTransportBufferSize = 256;
typedef midi::UsbTransport<sUsbTransportBufferSize> UsbTransport;

UsbTransport sUsbTransport;

struct MySettings : public midi::DefaultSettings
{
  static const bool Use1ByteParsing = false;
  static const unsigned SysExMaxSize = sUsbTransportBufferSize; // Accept SysEx messages up to 256 bytes long.
  static const bool UseRunningStatus = false; // My devices seem to be ok with it.
};

MIDI_CREATE_CUSTOM_INSTANCE(UsbTransport, sUsbTransport, MIDI, MySettings);

// Create a 'MIDI' object using MySettings bound to Serial.
MIDI_CREATE_CUSTOM_INSTANCE(HardwareSerial, Serial, MIDIHW, MySettings);

// STATUS LED
Adafruit_NeoPixel statusLED(1, STATUS_LED_PIN, NEO_GRB + NEO_KHZ800);

uint32_t off = statusLED.Color(0, 0, 0);
uint32_t red = statusLED.Color(STATUS_LED_BRIGHTNESS, 0, 0);
uint32_t green = statusLED.Color(0, STATUS_LED_BRIGHTNESS, 0);
uint32_t blue = statusLED.Color(0, 0, STATUS_LED_BRIGHTNESS);
uint32_t magenta = statusLED.Color(STATUS_LED_BRIGHTNESS/2, 0, STATUS_LED_BRIGHTNESS/2);
uint32_t cyan = statusLED.Color(0, STATUS_LED_BRIGHTNESS/2, STATUS_LED_BRIGHTNESS/2);
uint32_t yellow = statusLED.Color(STATUS_LED_BRIGHTNESS/2, STATUS_LED_BRIGHTNESS/2, 0);
uint32_t white = statusLED.Color(STATUS_LED_BRIGHTNESS/3, STATUS_LED_BRIGHTNESS/3, STATUS_LED_BRIGHTNESS/3);

uint32_t statusLEDColor[5] = {magenta, green, cyan, yellow, red};
uint8_t R=0,G=85,B=190, indexColor = 0;
uint32_t counterForLED=0;


extEEPROM eep(kbits_512, 1, 128);//device size, number of devices, page size

#define BOOT_SIGN_ADDR			5
#define BOOT_SIGN_MASK			1
#define YTX_VALID_CONFIG_ADDR	1


extern uint32_t __sketch_vectors_ptr; // Exported value from linker script
extern void board_init(void);

volatile uint32_t* pulSketch_Start_Address;

#define ADDRESS_SIZE		(sizeof(uint32_t))
#define PAGES_PER_BLOCK		2

uint8_t *sysex_data;

#define SYSEX_ID0  				'y'
#define SYSEX_ID1               't'
#define SYSEX_ID2               'x'

#define REQUEST_RST                 0x12
#define REQUEST_BOOT_MODE           0x13
#define REQUEST_UPLOAD_SELF         0x14
#define REQUEST_UPLOAD_OTHER        0x15
#define REQUEST_FIRM_DATA_UPLOAD    0x16
#define REQUEST_ERASE_EEPROM		0x19

#define STATUS_ACK    1
#define STATUS_NAK    2

#define CMD_ALL_LEDS_OFF        0xF3

enum ytxIOStructure
{
	ID0,
	ID1,
	ID2,
	MESSAGE_STATUS,
	WISH,
	MESSAGE_TYPE,
	REQUEST_ID,
	DATA,
	HEADER_SIZE = DATA
};

uint8_t ack_msg[] =
{
	 SYSEX_ID0, SYSEX_ID1, SYSEX_ID2, STATUS_ACK
};

uint8_t nak_msg[] =
{
	 SYSEX_ID0, SYSEX_ID1, SYSEX_ID2, STATUS_NAK
};

uint32_t PAGE_SIZE, PAGES, MIN_FLASH, MAX_FLASH, ENCODED_BLOCK_SIZE;

uint8_t *stream;
volatile uint8_t sysex_cnt;
uint8_t pagesCnt;
volatile uint8_t rstFlg = 0;

bool forwarderEnable = false;

uint32_t current_number;
uint32_t i, length;
uint8_t command, *ptr_data, *ptr;
uint8_t j;
uint32_t u32tmp;

/*! \brief Decode System Exclusive messages.
 SysEx messages are encoded to guarantee transmission of data bytes higher than
 127 without breaking the MIDI protocol. Use this static method to reassemble
 your received message.
 \param inSysEx The SysEx data received from MIDI in.
 \param outData    The output buffer where to store the decrypted message.
 \param inLength The length of the input buffer.
 \return The length of the output buffer.
 @see encodeSysEx @see getSysExArrayLength
 Code inspired from Ruin & Wesen's SysEx encoder/decoder - http://ruinwesen.com
 */
uint8_t decodeSysEx(uint8_t* inSysEx, uint8_t* outData, uint8_t inLength)
{
	uint8_t count  = 0;
	uint8_t msbStorage = 0;

	for (unsigned i = 0; i < inLength; ++i)
	{
		if ((i % 8) == 0)
		{
			msbStorage = inSysEx[i];
		}
		else
		{
			outData[count++] = inSysEx[i] | ((msbStorage & 1) << 7);
			msbStorage >>= 1;
		}
	}
	return count;
}

uint8_t encodedSysExSize(uint8_t length)
{
	uint8_t outLength  = 0;     // Num bytes in output array.
	uint8_t count = 0;     // Num 7bytes in a block.

	for (unsigned i = 0; i < length; ++i)
	{
		if (count++ == 6)
		{
			outLength  += 8;
			count       = 0;
		}
	}
	
	return outLength + count + (count != 0 ? 1 : 0);
}

/*! \brief Reset microcontroller
 \param void
 \return void
 */


void SelfReset(){
	USBDevice.detach();
	USBDevice.end();
	delay(500);
	NVIC_SystemReset();
}

/*! \brief Decode received block and write to flash 
 \param void
 \return 0 if fails or 1 if success
 */

uint8_t write_block(void)
{
	uint8_t success = 0;
	
	//take received checksum and remove from size
	uint8_t rcbChecksum = stream[sysex_cnt - 1];
	sysex_cnt--;
	
	//calculate block checksum
	uint8_t calChecksum = 0;
	
	for(uint32_t i = 0; i < sysex_cnt; i++)
	{
		calChecksum ^= stream[i];
	}
	calChecksum &= 0x7f;
	
	//check equal
	if (calChecksum == rcbChecksum)
	{
		//remove header block
		stream += HEADER_SIZE;
		sysex_cnt -= HEADER_SIZE;
	
		if(sysex_cnt == ENCODED_BLOCK_SIZE)
		{
			//decode payload(without headers and checksum)
			uint32_t decodedSize = decodeSysEx(stream,sysex_data,sysex_cnt);
	
			//take destination address and remove from block size
			uint32_t flashAddress;
			memcpy(&flashAddress,sysex_data,sizeof(uint32_t));
			flashAddress += MIN_FLASH;
	
			//check errors
			if ((decodedSize - ADDRESS_SIZE) == PAGES_PER_BLOCK*PAGE_SIZE)
			{
				//write block
				for (uint32_t pages=0; pages<PAGES_PER_BLOCK; pages++)
				{
					uint32_t targetAddress = flashAddress + pages*PAGE_SIZE;
					
					if((targetAddress+PAGE_SIZE)>MAX_FLASH)
						break;
		
					// Set automatic page write
					NVMCTRL->CTRLB.bit.MANW = 0;

					// Do writes in pages
					volatile uint32_t *dst_addr = (uint32_t *)(targetAddress);
					volatile uint32_t *src_addr = (uint32_t *)(sysex_data + pages*PAGE_SIZE + ADDRESS_SIZE);
		
					// Execute "PBC" Page Buffer Clear
					NVMCTRL->CTRLA.reg = NVMCTRL_CTRLA_CMDEX_KEY | NVMCTRL_CTRLA_CMD_PBC;
					while (NVMCTRL->INTFLAG.bit.READY == 0);
					
					// Fill page buffer
					for (uint32_t i=0; i<(PAGE_SIZE/4); i++)
					{
						dst_addr[i] = src_addr[i];
					}
					
					// Execute "WP" Write Page
					NVMCTRL->CTRLA.reg = NVMCTRL_CTRLA_CMDEX_KEY | NVMCTRL_CTRLA_CMD_WP;
					while (NVMCTRL->INTFLAG.bit.READY == 0);	
				}
				success = 1;
			}
		}
	}
	
	return success;

}

void eeErase(uint8_t chunk, uint32_t startAddr, uint32_t endAddr) {
	chunk &= 0xFC;                //force chunk to be a multiple of 4
	uint8_t data[chunk];
	//SerialUSB.println(F("Erasing..."));
	for (int i = 0; i < chunk; i++) data[i] = 0xFF;
	uint32_t msStart = millis();

	for (uint32_t a = startAddr; a <= endAddr; a += chunk) {
		if ( (a & 0xFFF) == 0 ) SerialUSB.println(a);
		eep.write(a, data, chunk);
	}
	uint32_t msLapse = millis() - msStart;
	SerialUSB.print(F("Erase lapse: "));
	SerialUSB.print(msLapse);
	SerialUSB.println(F(" ms"));
}

void handleSystemExclusiveHW(byte *message, unsigned size)
{
	if(forwarderEnable)
		MIDI.sendSysEx(size-2,&message[1]);
}
void handleSystemExclusiveUSB(byte *message, unsigned size)
{
	uint8_t res = 0;
	uint8_t stayInBootBit = 0;
	
	sysex_cnt = size - 2;
	
	if (sysex_cnt >= HEADER_SIZE)
	{	
		stream = message + 1;
		
		if(stream[ID0] == SYSEX_ID0 && stream[ID1] == SYSEX_ID1 && stream[ID2] == SYSEX_ID2)
		{			
			if (stream[REQUEST_ID] == REQUEST_UPLOAD_SELF)
			{
				forwarderEnable = false;
				
				// Erase the flash memory starting from ADDR 0x8000 to the end of flash.

				// Note: the flash memory is erased in ROWS, that is in block of 4 pages.
				//       Even if the starting address is the last byte of a ROW the entire
				//       ROW is erased anyway.
					
				uint32_t dst_addr = MIN_FLASH; // starting address

				while (dst_addr < MAX_FLASH)
				{
					// Execute "ER" Erase Row
					NVMCTRL->ADDR.reg = dst_addr / 2;
					NVMCTRL->CTRLA.reg = NVMCTRL_CTRLA_CMDEX_KEY | NVMCTRL_CTRLA_CMD_ER;
					while (NVMCTRL->INTFLAG.bit.READY == 0);
					dst_addr += PAGE_SIZE * 4; // Skip a ROW
				}
					
				res = 1;
				pagesCnt = 0;
				MIDI.sendSysEx(sizeof(ack_msg),ack_msg);
			}
			else if (stream[REQUEST_ID] == REQUEST_UPLOAD_OTHER)
			{
				forwarderEnable = true;
				stream[REQUEST_ID] = REQUEST_UPLOAD_SELF;

				MIDIHW.sendSysEx(size-2,&message[1]);
			}	
			else if (stream[REQUEST_ID] == REQUEST_RST)
			{
				//reset device
				rstFlg = 1;
			}
			else if (stream[REQUEST_ID] == REQUEST_FIRM_DATA_UPLOAD)
			{
				if(forwarderEnable)
				{
					MIDIHW.sendSysEx(size-2,&message[1]);
				}
				else
				{
					res = write_block();
							
					if(res)
					{
						MIDI.sendSysEx(sizeof(ack_msg),ack_msg);
								
						//animation here//
					}
					else
					{
						MIDI.sendSysEx(sizeof(nak_msg),nak_msg);
					}	
				}
			}
			else if (stream[REQUEST_ID] == REQUEST_ERASE_EEPROM)
			{
				eeErase(128, 0, 65535);
				
				stayInBootBit &= ~BOOT_SIGN_MASK;
				eep.write(BOOT_SIGN_ADDR, (uint8_t*) &stayInBootBit, sizeof(stayInBootBit));
				delay(1);
				MIDI.sendSysEx(sizeof(ack_msg),ack_msg);
				
				rstFlg = 1;	// Reset device
			}
		}
	}
}

static void jump_to_application(void) {

  /* Rebase the Stack Pointer */
  __set_MSP( (uint32_t)(__sketch_vectors_ptr) );

  /* Rebase the vector table base address */
  SCB->VTOR = ((uint32_t)(&__sketch_vectors_ptr) & SCB_VTOR_TBLOFF_Msk);

  /* Jump to application Reset Handler in the application */
  asm("bx %0"::"r"(*pulSketch_Start_Address));
}

static volatile bool main_b_cdc_enable = false;

static volatile bool jump_to_app = false;

/**
 * \brief Check the application startup condition
 *
 */
static void check_start_application(void)
{
  /*
   * Test sketch stack pointer @ &__sketch_vectors_ptr
   * Stay in SAM-BA if value @ (&__sketch_vectors_ptr) == 0xFFFFFFFF (Erased flash cell value)
   */
  if (__sketch_vectors_ptr == 0xFFFFFFFF)
  {
    /* Stay in bootloader */
    return;
  }

  /*
   * Load the sketch Reset Handler address
   * __sketch_vectors_ptr is exported from linker script and point on first 32b word of sketch vector table
   * First 32b word is sketch stack
   * Second 32b word is sketch entry point: Reset_Handler()
   */
  pulSketch_Start_Address = &__sketch_vectors_ptr ;
  pulSketch_Start_Address++ ;

  /*
   * Test vector table address of sketch @ &__sketch_vectors_ptr
   * Stay in SAM-BA if this function is not aligned enough, ie not valid
   */
  if ( ((uint32_t)(&__sketch_vectors_ptr) & ~SCB_VTOR_TBLOFF_Msk) != 0x00)
  {
    /* Stay in bootloader */
    return;
  }

#if defined(BOOT_DOUBLE_TAP_ADDRESS)
  #define DOUBLE_TAP_MAGIC 0x07738135
  if (PM->RCAUSE.bit.POR)
  {
    /* On power-on initialize double-tap */
    BOOT_DOUBLE_TAP_DATA = 0;
  }
  else
  {
    if (BOOT_DOUBLE_TAP_DATA == DOUBLE_TAP_MAGIC)
    {
      /* Second tap, stay in bootloader */
      BOOT_DOUBLE_TAP_DATA = 0;
      return;
    }

    /* First tap */
    BOOT_DOUBLE_TAP_DATA = DOUBLE_TAP_MAGIC;

    /* Wait 0.5sec to see if the user tap reset again */
    delay(500);

    /* Timeout happened, continue boot... */
    BOOT_DOUBLE_TAP_DATA = 0;
  }
#endif

  jump_to_app = true;
}

void setup()
{		
	check_start_application();
	
	// EEPROM INITIALIZATION
	uint8_t eepStatus = eep.begin(extEEPROM::twiClock400kHz); //go fast!
	
	// YAELTEX BOOT SIGN - IF BOOT SIGNATURE IN EEPROM IS SET TO 1 REMAIN IN BOOTLOADER TO ACCEPT SYSEX FIRMWARE UPDATE
	bool bootSignPresent = false;
	// TEST I2C EEPROM
	uint8_t stayInBoot = 0;
	
	pinMode(RX_MIDI_PIN, INPUT_PULLUP);
	pinMode(TX_MIDI_PIN, OUTPUT);

	uint16_t bytesR = eep.read(BOOT_SIGN_ADDR, (uint8_t*) &stayInBoot, sizeof(stayInBoot));
		
	if(stayInBoot & BOOT_SIGN_MASK)
	{
		bootSignPresent = true;
		stayInBoot &= ~BOOT_SIGN_MASK;
		eep.write(BOOT_SIGN_ADDR, (uint8_t*) &stayInBoot, sizeof(stayInBoot));
	}

	// Write to MIDI OUT pin (TX) a low state, if it is looped back to the MIDI IN pin (RX) reset to 
	digitalWrite(TX_MIDI_PIN, LOW);
	delay(50);
	if(digitalRead(RX_MIDI_PIN) == LOW){
		delay(100);
		if(digitalRead(RX_MIDI_PIN) == LOW){
			bootSignPresent = true;
		}
	}

	/* Jump in application if condition is satisfied */

	if (!bootSignPresent && jump_to_app) 
	{
		jump_to_application();
	}
	
	
	USBDevice.init();
	USBDevice.attach();
	  
	SerialUSB.begin(115200);
	
	pinMode(SAMD11_BOOT_MODE_PIN,OUTPUT);
	pinMode(SAMD11_RST_PIN,OUTPUT);
	
	digitalWrite(SAMD11_BOOT_MODE_PIN,LOW);
	digitalWrite(SAMD11_RST_PIN,LOW);
	delay(1);
	digitalWrite(SAMD11_RST_PIN,HIGH);
	delay(1);
	

	uint32_t pageSizes[] = { 8, 16, 32, 64, 128, 256, 512, 1024 };
	PAGE_SIZE = pageSizes[NVMCTRL->PARAM.bit.PSZ];
	PAGES = NVMCTRL->PARAM.bit.NVMP;
	MIN_FLASH = (uint32_t)&__sketch_vectors_ptr;
	MAX_FLASH = PAGE_SIZE * PAGES;
	
	//allocate decoded data array
	sysex_data = new uint8_t[PAGE_SIZE*PAGES_PER_BLOCK + ADDRESS_SIZE];
	
	//calculate encoded data size
	ENCODED_BLOCK_SIZE = encodedSysExSize(PAGE_SIZE*PAGES_PER_BLOCK + ADDRESS_SIZE);
	
    // Begin MIDI USB port and set handler for Sysex Messages
    MIDI.begin(MIDI_CHANNEL_OMNI); // Se inicializa la comunicación MIDI por USB.
    MIDI.setHandleSystemExclusive(handleSystemExclusiveUSB);
    MIDI.turnThruOff();
	
	MIDIHW.begin(MIDI_CHANNEL_OMNI); // Se inicializa la comunicación MIDI hacia el SAMD11.
	MIDIHW.setHandleSystemExclusive(handleSystemExclusiveHW);
	MIDIHW.turnThruOff();

    
	// STATUS LED
	statusLED.begin();
	statusLED.setBrightness(STATUS_LED_BRIGHTNESS);
}


void loop() {
	
    MIDI.read();
	MIDIHW.read();
	
	if(SerialUSB.available())
	{
		uint8_t data = SerialUSB.read();	
	}
	
	if(++counterForLED >= 50000)
	{
		counterForLED = 0;
		
		statusLED.clear(); // Set all pixel colors to 'off'
		statusLED.setPixelColor(0, statusLEDColor[indexColor++]);
		statusLED.show();
		
		indexColor %= 5;
	}
	
	//reset system if set
	if(rstFlg)
	{
		SelfReset();
	}
}
