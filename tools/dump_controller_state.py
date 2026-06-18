#!/usr/bin/env python3
"""
Trigger YTX controller state dump via SysEx.

This sends a SysEx message that causes the controller to send out
the current state of all encoders, digitals, and analogs as MIDI messages.

Usage:
    python dump_controller_state.py                 # List available MIDI ports
    python dump_controller_state.py <port>          # Send dump request

Example:
    python dump_controller_state.py "Kilowhat"

Install dependency:
    pip install mido python-rtmidi
"""

import sys
import mido

def list_ports():
    print("Available MIDI output ports:")
    for i, port in enumerate(mido.get_output_names()):
        print(f"  {i}: {port}")

def send_dump_request(port_name):
    """
    Send controller state dump request SysEx.
    
    Format: F0 79 74 78 00 01 00 21 F7
    """
    # YTX header: 'y' 't' 'x' + status(0) + wish(1=SET) + msgType(0=specialRequests) + reqId(0x21=dumpControllerState)
    #
    sysex_data = [0x79, 0x74, 0x78, 0x00, 0x01, 0x00, 0x21]
    
    print(f"Sending dump request to '{port_name}':")
    print(f"  F0 {' '.join(f'{b:02X}' for b in sysex_data)} F7")
    
    # Find matching port
    #
    matching = [p for p in mido.get_output_names() if port_name.lower() in p.lower()]
    if not matching:
        print(f"Error: No port matching '{port_name}' found")
        list_ports()
        return False
    
    with mido.open_output(matching[0]) as port:
        msg = mido.Message('sysex', data=sysex_data)
        port.send(msg)
        print("Sent! Controller should now send out all encoder/digital/analog states.")
    
    return True

def main():
    if len(sys.argv) == 1:
        list_ports()
        print("\nUsage: dump_controller_state.py <port>")
        print("\nExample:")
        print("  dump_controller_state.py Kilowhat")
        return
    
    port_name = sys.argv[1]
    send_dump_request(port_name)

if __name__ == "__main__":
    main()

