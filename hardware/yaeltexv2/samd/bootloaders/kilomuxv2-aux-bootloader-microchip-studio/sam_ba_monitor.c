/*
  Copyright (c) 2015 Arduino LLC.  All right reserved.
  Copyright (c) 2015 Atmel Corporation/Thibaut VIARD.  All right reserved.

  This library is free software; you can redistribute it and/or
  modify it under the terms of the GNU Lesser General Public
  License as published by the Free Software Foundation; either
  version 2.1 of the License, or (at your option) any later version.

  This library is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
  See the GNU Lesser General Public License for more details.

  You should have received a copy of the GNU Lesser General Public
  License along with this library; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/

#include "sam.h"
#include <string.h>
#include "sam_ba_monitor.h"
#include "sam_ba_serial.h"
#include "board_driver_serial.h"

#include <stdlib.h>

extern uint32_t __sketch_vectors_ptr; // Exported value from linker script

#define ADDRESS_SIZE		(sizeof(uint32_t))
#define SPM_PAGESIZE		64
#define PAGES_PER_BLOCK		2

#define SYSEX_ID0  				'y'
#define SYSEX_ID1               't'
#define SYSEX_ID2               'x'

#define REQUEST_RST                 0x12
#define REQUEST_BOOT_MODE           0x13
#define REQUEST_UPLOAD_SELF         0x14
#define REQUEST_UPLOAD_OTHER        0x15
#define REQUEST_FIRM_DATA_UPLOAD    0x16

#define STATUS_ACK					1
#define STATUS_NAK					2

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
	0xF0, SYSEX_ID0, SYSEX_ID1, SYSEX_ID2, STATUS_ACK,0xF7
};

uint8_t nak_msg[] =
{
	0xF0, SYSEX_ID0, SYSEX_ID1, SYSEX_ID2, STATUS_NAK,0xF7
};

uint32_t PAGE_SIZE, PAGES, MIN_FLASH, MAX_FLASH, ENCODED_BLOCK_SIZE;

//DATA + CHECKSUM + ADDRESS + SysExIDs + CMD
//#define BUFFER_SIZE       ((SPM_PAGESIZE*PAGES_PER_BLOCK*8)/7 + (SPM_PAGESIZE*PAGES_PER_BLOCK%7 + 1) + 1 + 4 + 3 + 1)

#define BUFFER_SIZE	256

uint8_t block_cnt = 0;
uint8_t sysex_data[SPM_PAGESIZE*PAGES_PER_BLOCK+ADDRESS_SIZE];

uint8_t *stream;
uint8_t recv_buffer[BUFFER_SIZE];

uint8_t sysex_cnt;
uint8_t in_sysex = 0;
uint8_t pagesCnt;

volatile uint8_t rstFlg = 0;

/* Provides one common interface to handle both USART and USB-CDC */
typedef struct
{
  /* send one byte of data */
  int (*put_c)(int value);
  /* Get one byte */
  int (*get_c)(void);
  /* Receive buffer not empty */
  bool (*is_rx_ready)(void);
  /* Send given data (polling) */
  uint32_t (*putdata)(void const* data, uint32_t length);
  /* Get data from comm. device */
  uint32_t (*getdata)(void* data, uint32_t length);
  /* Send given data (polling) using xmodem (if necessary) */
  uint32_t (*putdata_xmd)(void const* data, uint32_t length);
  /* Get data from comm. device using xmodem (if necessary) */
  uint32_t (*getdata_xmd)(void* data, uint32_t length);
} t_monitor_if;

/* Initialize structures with function pointers from supported interfaces */
const t_monitor_if uart_if =
{
  .put_c =       serial_putc,
  .get_c =       serial_getc,
  .is_rx_ready = serial_is_rx_ready,
  .putdata =     serial_putdata,
  .getdata =     serial_getdata,
  .putdata_xmd = serial_putdata_xmd,
  .getdata_xmd = serial_getdata_xmd
};


/* The pointer to the interface object use by the monitor */
t_monitor_if * ptr_monitor_if;

/* b_terminal_mode mode (ascii) or hex mode */
volatile bool b_terminal_mode = false;
volatile bool b_sam_ba_interface_usart = false;

/* Pulse generation counters to keep track of the time remaining for each pulse type */
#define TX_RX_LED_PULSE_PERIOD 100
volatile uint16_t txLEDPulse = 0; // time remaining for Tx LED pulse
volatile uint16_t rxLEDPulse = 0; // time remaining for Rx LED pulse

void sam_ba_monitor_init(uint8_t com_interface)
{
  //Selects the requested interface for future actions
  if (com_interface == SAM_BA_INTERFACE_USART)
  {
    ptr_monitor_if = (t_monitor_if*) &uart_if;
    b_sam_ba_interface_usart = true;
  }
}

/*
 * Central SAM-BA monitor putdata function using the board LEDs
 */
static uint32_t sam_ba_putdata(t_monitor_if* pInterface, void const* data, uint32_t length)
{
	uint32_t result ;

	result=pInterface->putdata(data, length);

	return result;
}

/*
 * Central SAM-BA monitor getdata function using the board LEDs
 */
static uint32_t sam_ba_getdata(t_monitor_if* pInterface, void* data, uint32_t length)
{
	uint32_t result ;

	result=pInterface->getdata(data, length);

	return result;
}



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

uint8_t write_block(void)
{
	uint8_t success = 0;
	
	stream = recv_buffer;
	
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

void handle_sysex(void)
{
	uint8_t res = 0;
	if (sysex_cnt >= HEADER_SIZE)
	{	
		if (recv_buffer[REQUEST_ID] == REQUEST_UPLOAD_SELF)
		{
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
			sam_ba_putdata(ptr_monitor_if,ack_msg,sizeof(ack_msg));
		}
		else if (recv_buffer[REQUEST_ID] == REQUEST_RST)
		{
			//reset device
			rstFlg = 1;
		}
		else if (recv_buffer[REQUEST_ID] == REQUEST_FIRM_DATA_UPLOAD)
		{
			res = write_block();
			
			if(res)
			{
				sam_ba_putdata(ptr_monitor_if,ack_msg,sizeof(ack_msg));
				
				//animation here//
			}
			else
			{
				sam_ba_putdata(ptr_monitor_if,nak_msg,sizeof(nak_msg));
			}
		}
	}
}

void handle_midi(uint8_t c)
{
	if (c == 0xF0)
	{
		if (in_sysex == 1)
		{
			handle_sysex();
		}
		sysex_cnt = 0;
		in_sysex = 1;
		return;
	}
	else if (c == 0xF7)
	{
		if (in_sysex == 1)
		{
			handle_sysex();
		}
		in_sysex = 0;
		return;
	}

	if (in_sysex)
	{
		recv_buffer[sysex_cnt++] = c;

		if (sysex_cnt == 3)
		{
			if (recv_buffer[ID0] != SYSEX_ID0 || recv_buffer[ID1] != SYSEX_ID1 || recv_buffer[ID2] != SYSEX_ID2)
			{
				in_sysex = 0;
			}
		}

		if (sysex_cnt > BUFFER_SIZE)
		{
			//discard too long message
			in_sysex = 0;
		}
	}
}

static void sam_ba_monitor_loop(void)
{
	uint8_t data[SIZEBUFMAX];
	
	if(sam_ba_getdata(ptr_monitor_if, data, SIZEBUFMAX))
	{
		handle_midi(data[0]);
	}
}

/**
 * \brief This function starts the SAM-BA monitor.
 */
void sam_ba_monitor_run(void)
{
  uint32_t pageSizes[] = { 8, 16, 32, 64, 128, 256, 512, 1024 };
  PAGE_SIZE = pageSizes[NVMCTRL->PARAM.bit.PSZ];
  PAGES = NVMCTRL->PARAM.bit.NVMP;
  MAX_FLASH = PAGE_SIZE * PAGES;
  MIN_FLASH = (uint32_t)&__sketch_vectors_ptr ; // starting address

  //calculate encoded data size
  ENCODED_BLOCK_SIZE = encodedSysExSize(sizeof(sysex_data));
  
  while (1)
  {
    sam_ba_monitor_loop();
  }
}
