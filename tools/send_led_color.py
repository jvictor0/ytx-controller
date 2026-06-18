#!/usr/bin/env python3
"""
Send YTX LED color SysEx messages from the command line.

Sets LED color for any component (digital button, encoder switch, encoder ring)
that has CC feedback configured on the specified channel/CC.

Usage:
    python send_led_color.py                    # List available MIDI ports
    python send_led_color.py <port> <channel> <cc> <r> <g> <b>

Example:
    python send_led_color.py "Kilowhat" 4 0 0 127 0    # CC0 on channel 4 = green
    python send_led_color.py "Kilowhat" 0 1 127 0 0 2 0 127 0 3 0 0 127  # Multiple

Supported components (matched by feedback channel/CC):
    - Digital button LEDs
    - Encoder switch (pushbutton) LEDs
    - Encoder ring LEDs

Install dependency:
    pip install mido python-rtmidi
"""

import sys
import mido

def list_ports():
    print("Available MIDI output ports:")
    for i, port in enumerate(mido.get_output_names()):
        print(f"  {i}: {port}")

def send_led_color(port_name, channel, cc_colors):
    """
    Send LED color SysEx.
    
    cc_colors: list of (cc, r, g, b) tuples
    """
    # YTX header: 'y' 't' 'x' + status(0) + wish(1=SET) + msgType(0=specialRequests) + reqId(0x20)
    #
    header = [0x79, 0x74, 0x78, 0x00, 0x01, 0x00, 0x20]
    
    # Channel (0-15)
    #
    data = [channel & 0x0F]
    
    # Add CC/RGB tuples
    #
    for cc, r, g, b in cc_colors:
        data.extend([cc & 0x7F, r & 0x7F, g & 0x7F, b & 0x7F])
    
    sysex_data = header + data
    
    print(f"Sending SysEx to '{port_name}':")
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
        print("Sent!")
    
    return True

def main():
    if len(sys.argv) == 1:
        list_ports()
        print("\nUsage: send_led_color.py <port> <channel> <cc> <r> <g> <b> [<cc2> <r2> <g2> <b2> ...]")
        print("\nExample:")
        print("  send_led_color.py Kilowhat 4 0 0 127 0      # CC0 channel 4 = green")
        print("  send_led_color.py Kilowhat 2 10 127 0 0 11 0 127 0 12 0 0 127")
        return
    
    if len(sys.argv) < 7:
        print("Error: Need at least port, channel, cc, r, g, b")
        return
    
    port_name = sys.argv[1]
    channel = int(sys.argv[2])
    
    # Parse CC/RGB tuples (groups of 4 after channel)
    #
    args = [int(x) for x in sys.argv[3:]]
    if len(args) % 4 != 0:
        print("Error: CC/RGB values must come in groups of 4 (cc, r, g, b)")
        return
    
    cc_colors = []
    for i in range(0, len(args), 4):
        cc_colors.append((args[i], args[i+1], args[i+2], args[i+3]))
    
    send_led_color(port_name, channel, cc_colors)

if __name__ == "__main__":
    main()

