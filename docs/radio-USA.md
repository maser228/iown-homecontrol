# io-homecontrol radio protocol description (US version)

## Physical specifications

The US version of io-homecontrol uses modified (i.e. non-standard) 2.4 GHz 801.15.4 frames.

- IEEE 801.15.4 channels used:
  - 15: 2425 MHz
  - 20: 2450 MHz
  - 25: 2475 MHz
- Data/Baud Rate: 250 kbps
- Modulation: O-QPSK
- Standards: Based onIEEE 802.15.4, but with non-standard sync byte and frame format (see below) 
- Checks: CRC

## Raw data sending
The physical level (PHY) is based on 802.15.4, but with a non-standard sync byte (0x56 instead of the standard 0xA7).

In accordance with 802.15.4, data is transmitted as 8-bit octets (no start bits or stop bits). Bytes are transmitted in order, LSB first.  

## PHY Frame Format
|   Synchronization Header    |    Frame Length     | Payload (including CRC) |
|:---------------------------:|:-------------------:|:-----------------------:|
|  0x00 0x00 0x00 0x00 0x56   |       1 byte        |       <=127 bytes       | 

Per 802.14.5, messages start with a synchronization header consisting of four 0x00 bytes followed by a single-byte start-of-frame delimiter (SFD), also known as the sync byte.  The SFD used by io-HC is a non-standard value of 0x56 -- this means that 802.15.4 radios which strictly respect the standard will not read io-HC frames, see below. 

Following the synchronization header is a single message-length byte.  The length calculation doesn't include the header or length byte itself, but does include the two CRC bytes (see below).  The highest bit of this byte is reserved 0, resulting in a maximum payload length (including CRC) of 127 bytes.

The payload (MAC-layer) is transmitted next.  The 802.14.5 MAC header is not used but the two-byte CRC is.  See LinkLayer-US for more information.

## Wake-Up Packets (Low-Power)

When addressing low-power devices (e.g. solar blinds), before sending the "real" packets, the remote sends up to 512 five-byte "wake-up" packets, at a rate of one per ms, with payloads in the form `0x00 [0x00 | 0x01] 0xnn` (+ 2x CRC bytes), where the second and third bytes count down from 0x01 0xFF by (approximately) 1 until reaching 0x00 0x00, at which point the "real" packet is sent.  This may be because the target devices' radios only receive intermittently (to save power) -- if a device sees any one of these messages, it will either keep its radio on long enough to receive the "real" packet, or schedule a radio-on time to receive it.

It's not known what the first two bytes are used for, but a possible reason is to produce a five-byte payload, which is the length of an 802.14.5 "acknowledgement" frame.  Lengths 0-4 and 6-8 are reserved in the 802.15.4 standard, and attempting to send such packets may cause issues with either sending or receiving radios which enforce this.

## Radio Hardware

The radio transceiver used in the US versions of the KLR200 touch-screen remote and the KLF200 gateway is the Atmel AT86RF233.  While this device supports compliant 802.15.4 frames, io-HC generates non-compliant (proprietary) frames by implementing the following deviations from the standard:
- custom start-of-frame delimiter
- custom payload i.e. without 802.14.5 MAC header

Since io-HC doesn't use the standard's MAC header, the chip's frame filtering and address filtering can't be used, and the TRX_END interrupt is used to detect incoming frames instead of the usual AMI interrupt.  However, the automatic CRC generation and checking of the chip are used.

While some hardware allows ignoring the MAC header via "promiscuous mode", because the SFD is used for byte sync, radios which are hard-coded to the 802.15.4 standard's SFD value of 0xA7 (such as the popular Sonoff Zigbee 3.0 USB Dongle-E) will _not_ sync on io-HC messages and either produce gibberish or (more often) not return any data at all.