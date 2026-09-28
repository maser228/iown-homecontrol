# Send/Receive Remote Transcript

This is an analysis of a "Send Remote" between a KLR200 remote paired to a number of blinds, and a second KLR200 which has been factory reset.

## Initial Discovery
The initial discovery is done with un-authenticated packets.
```
1. [  New  -> x00 ff fb]  Discovery Request  (0x28):
2. [Office ->    New ]  Discovery Answer   (0x29): FF C0 4D 84 A6 01 CC (00 00)
3. [  New  -> Office ]  Discovery Confirm. (0x2c):
4. [Office  ->   New ]  Discov. Conf. Answ (0x2d):
```
Packet 1 = new remote broadcasting recover
Packet 2 = office remote responding with metadata (including its address `0x4D 84 a6`)
Packet 3 = new remote confirms the discovery
Packet 4 = office remote confirms the confirmation


## Data Transfer Using `0x4A`/`0x4B`
After discovery, the "target" controller pulls data from the "source".  The process is:
1. Target asks requests a type of data via a `0x46` packet (exact format unknown, but appears to contain at least a 1-byte address).
2. Conjecture: The source copies the requested data into an IO buffer for the target to retrieve from.  The address of this buffer always increases by one for each subsequent `0x46` request, even when the requested data address jumps around.
3. The source replies with a `0x47` packet with payload `[buffer address (1 byte)] [flags? (1 byte)] [number of bytes to pull (2 bytes)]`.  If there is no data available, the source replies with `00 00 00 00` (i.e. no buffer and no bytes) and no transfer takes place.
4. The maximum number of bytes per exchange is 18.  For transfers larger than this, multiple packets are used.  The target calculates the number of packets that will be needed.
5. The target makes a `0x4a` request to the source using the address and the *highest* packet number (but the data still comes over in "forward" order).
6. The source replies with the same address and packet number, followed by the data (up to 18 bytes).
7. The target continues making requests with descending packet numbers, and the source replies with the corresponding data, until reaching packet `00 01`.
8. The target makes a last `0x4a` request with the address + packet number `00 00`.  This source always replies to this with `[address] 00 00`, essentially mirroring the request.

#### Example:
1. The target makes an `0x46` request with this data: `01 01 00 00 16 00 00 00 3c`.  (I believe the `01 00 00 16` is the important part.)
2. The source replies with `65 00 00 78`.  This means the data is to be pulled from address `0x65` and there are 120 bytes to be retrieved (0x78 = 120d).  This will require six 18-byte packets plus one 12-byte packet (6*18 + 12 = 120), for a total of 7 data packets.
3. The target requests the first packet with a `0x4a` command with data `65 00 07` (address and packet number).  The source responds with a `0x4b` packet with `65 00 07` followed by 18-bytes of payload.
4. This continues for packets `65 00 06`...`65 00 01`, where all but the last have 18 bytes in the return payload, and the last has 12.
5. The target sends a final `0x4a` packet with data `65 00 00` and the source replies with a `0x4b` packet, also with `65 00 00`.  The transfer is complete.

### What Are The Groups?
These are the requests made by the target controller.  The first two had no data available, so none was transferred.
There are fragments of ASCII strings appearing in some of the groups, which may help identify them.

- `00 00 00 02` group -- no data and no transfer took place
- `00 00 00 03` group -- no data and no transfer took place
- `00 00 00 01` group: ?
- `01 00 00 13` group: addresses of all paired devices (blinds) appear in this group, each followed by `02 80 9d 01 00 00 00` (type of device?)
- `01 00 00 1c` group: ?
- `01 00 00 14` group: ?
- `01 00 00 15` group: contains product names (e.g. "Orange Room", "Kitchen"), probably in unicode (two bytes per letter) with length and other bytes interspersed
- `01 00 00 16` group: contains group names (e.g. "Kitchen", "All Blinds")
- `01 00 00 19` group: ? -- has `01` for byte 2 in 0x47
- `01 00 00 17` group: contains "My home"
- `01 00 00 1a` group: ? -- has `01` for byte 2 in 0x47
- `01 00 00 18` group: contains "All vertical blinds", which I'm not sure appears in the UI
- `01 00 00 1b` group: ? -- has `01` for byte 2 in 0x47

There is some KLR200 functionality I'm not using:

## "Pull" Key Transfer
This works the same as has been previously documented by others.
```
[  New  -> Office ]  Launch Key Transfer(0x38): 31 6E 71 BB 01 49 (26 17)
[Office  ->   New ]  Key Transfer       (0x32): FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF (FF FF)
[  New  -> Office ]  Challenge Request  (0x3c): A6 3D 20 CD C8 0F (1A 86)
[Office  ->   New ]  Challenge Response (0x3d): 7F 62 28 CE 32 14 (B2 A9)
```


## Data Group Examples
These are the captured packets from a specific transfer.  Note that the addresses are arbitrary per transfer as noted above.

#### 02 and 03 Groups -- No Data
```
[  New  -> Office ]   -- Unknown --     (0x46): 01 00 00 00 02 00 00 00 00
[Office  ->   New ]   -- Unknown --     (0x47): 00 00 00 00

[  New  -> Office ]   -- Unknown --     (0x46): 01 00 00 00 03 00 00 00 00
[Office ->  New   ]   -- Unknown --     (0x47): 00 00 00 00
```
#### `00 00 00 01` group
```
[  New  -> Office ]   -- Unknown --     (0x46): 01 00 00 00 01 00 00 00 00
                                                (missing packet)

[  New  -> Office ]  Lrg. Data Request  (0x4a): 0c 00 03
[Office  ->   New ]  Lrg. Data Answer   (0x4b): 0c 00 03 11 5d d1 02 80 9d 01 00 00 00 51 6b 10 02 80 9d 01 00   <-- 11 5d d1 = Master BR blind addr, 51 6b 10 = Office blind addr
[  New  -> Office ]  Lrg. Data Request  (0x4a): 0c 00 02
[Office  ->   New ]  Lrg. Data Answer   (0x4b): 0c 00 02 00 00 c6 7e f3 02 80 9d 01 00 00 00 d8 6c 56 02 80 9d  <-- c6 7e f3 = Org Room blind address, d8 6c 56 = Kitchen Blind 1 address
[  New  -> Office ]  Lrg. Data Request  (0x4a): 0c 00 01
[Office  ->   New ]  Lrg. Data Answer   (0x4b): 0c 00 01 01 00 00 00 05 65 e7 02 80 9d 01 00 00 00  <-- 05 65 e7 = Kitchen 2 address,
[  New  -> Office ]  Lrg. Data Request  (0x4a): 0c 00 00
[Office  ->   New ]  Lrg. Data Answer   (0x4b): 0c 00 00
```

#### `01 00 00 14` Data
```
[  New  -> Office ]   -- Unknown --     (0x46): 01 01 00 00 14 00 00 00 50
[Office  ->   New ]   -- Unknown --     (0x47): 0f 00 00 50

[  New  -> Office ]  Lrg. Data Request  (0x4a): 0f 00 05
[Office  ->   New ]  Lrg. Data Answer   (0x4b): 0f 00 05 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
[  New  -> Office ]  Lrg. Data Request  (0x4a): 0f 00 04
[Office  ->   New ]  Lrg. Data Answer   (0x4b): 0f 00 04 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
[  New  -> Office ]  Lrg. Data Request  (0x4a): 0f 00 03
[Office  ->   New ]  Lrg. Data Answer   (0x4b): 0f 00 03 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
[  New  -> Office ]  Lrg. Data Request  (0x4a): 0f 00 02
[Office  ->   New ]  Lrg. Data Answer   (0x4b): 0f 00 02 00 00 00 00 00 00 04 04 00 01 01 00 80 00 00 1e 00 01
[  New  -> Office ]  Lrg. Data Request  (0x4a): 0f 00 01
[Office  ->   New ]  Lrg. Data Answer   (0x4b): 0f 00 01 02 00 64 3c 32 1e 1e 3c
[  New  -> Office ]  Lrg. Data Request  (0x4a): 0f 00 00
[Office  ->   New ]  Lrg. Data Answer   (0x4b): 0f 00 00
```

#### `01 00 00 15` Data
```
[  New  -> Office ]   -- Unknown --     (0x46): 01 01 00 00 15 00 00 00 78
[Office  ->   New ]   -- Unknown --     (0x47): 10 00 02 58

[  New  -> Office ]  Lrg. Data Request  (0x4a): 10 00 22
[Office  ->   New ]  Lrg. Data Answer   (0x4b): 10 00 22 00 00 01 00 00 01 00 00 0e 00 4d 00 61 00 73 00 74 00
[  New  -> Office ]  Lrg. Data Request  (0x4a): 10 00 21
[Office  ->   New ]  Lrg. Data Answer   (0x4b): 10 00 21 65 00 72 00 20 00 42 00 65 00 64 00 72 00 6f 00 6f 00
[  New  -> Office ]  Lrg. Data Request  (0x4a): 10 00 20
[Office  ->   New ]  Lrg. Data Answer   (0x4b): 10 00 20 6d 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
[  New  -> Office ]  Lrg. Data Request  (0x4a): 10 00 1f
[Office  ->   New ]  Lrg. Data Answer   (0x4b): 10 00 1f 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
[  New  -> Office ]  Lrg. Data Request  (0x4a): 10 00 1e
[Office  ->   New ]  Lrg. Data Answer   (0x4b): 10 00 1e 00 00 00 00 d1 5d 11 00 00 00 00 00 00 00 00 00 00 00
[  New  -> Office ]  Lrg. Data Request  (0x4a): 10 00 1d
[Office  ->   New ]  Lrg. Data Answer   (0x4b): 10 00 1d 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
[  New  -> Office ]  Lrg. Data Request  (0x4a): 10 00 1c
[Office  ->   New ]  Lrg. Data Answer   (0x4b): 10 00 1c 00 00 00 00 00 00 00 00 00 00 00 00 01 01 01 00 00 01
[  New  -> Office ]  Lrg. Data Request  (0x4a): 10 00 1b
[Office  ->   New ]  Lrg. Data Answer   (0x4b): 10 00 1b 00 00 06 00 4f 00 66 00 66 00 69 00 63 00 65 00 00 00
[  New  -> Office ]  Lrg. Data Request  (0x4a): 10 00 1a
[Office  ->   New ]  Lrg. Data Answer   (0x4b): 10 00 1a 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
[  New  -> Office ]  Lrg. Data Request  (0x4a): 10 00 19
[Office  ->   New ]  Lrg. Data Answer   (0x4b): 10 00 19 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
[  New  -> Office ]  Lrg. Data Request  (0x4a): 10 00 18
[Office  ->   New ]  Lrg. Data Answer   (0x4b): 10 00 18 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 10 6b
[  New  -> Office ]  Lrg. Data Request  (0x4a): 10 00 17
[Office  ->   New ]  Lrg. Data Answer   (0x4b): 10 00 17 51 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
[  New  -> Office ]  Lrg. Data Request  (0x4a): 10 00 16
[Office  ->   New ]  Lrg. Data Answer   (0x4b): 10 00 16 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
[  New  -> Office ]  Lrg. Data Request  (0x4a): 10 00 15
[Office  ->   New ]  Lrg. Data Answer   (0x4b): 10 00 15 00 00 00 00 00 00 02 02 01 00 00 01 00 00 0b 00 4f 00
[  New  -> Office ]  Lrg. Data Request  (0x4a): 10 00 14
[Office  ->   New ]  Lrg. Data Answer   (0x4b): 10 00 14 72 00 61 00 6e 00 67 00 65 00 20 00 52 00 6f 00 6f 00
[  New  -> Office ]  Lrg. Data Request  (0x4a): 10 00 13
[Office  ->   New ]  Lrg. Data Answer   (0x4b): 10 00 13 6d 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
[  New  -> Office ]  Lrg. Data Request  (0x4a): 10 00 12
[Office  ->   New ]  Lrg. Data Answer   (0x4b): 10 00 12 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
[  New  -> Office ]  Lrg. Data Request  (0x4a): 10 00 11
[Office  ->   New ]  Lrg. Data Answer   (0x4b): 10 00 11 00 00 00 00 00 00 00 00 00 00 f3 7e c6 00 00 00 00 00
[  New  -> Office ]  Lrg. Data Request  (0x4a): 10 00 10
[Office  ->   New ]  Lrg. Data Answer   (0x4b): 10 00 10 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
[  New  -> Office ]  Lrg. Data Request  (0x4a): 10 00 0f
[Office  ->   New ]  Lrg. Data Answer   (0x4b): 10 00 0f 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
[  New  -> Office ]  Lrg. Data Request  (0x4a): 10 00 0e
[Office  ->   New ]  Lrg. Data Answer   (0x4b): 10 00 0e 03 03 00 00 00 01 00 00 0d 00 4b 00 69 00 74 00 63 00
[  New  -> Office ]  Lrg. Data Request  (0x4a): 10 00 0d
[Office  ->   New ]  Lrg. Data Answer   (0x4b): 10 00 0d 68 00 65 00 6e 00 20 00 52 00 69 00 67 00 68 00 74 00
[  New  -> Office ]  Lrg. Data Request  (0x4a): 10 00 0c
[Office  ->   New ]  Lrg. Data Answer   (0x4b): 10 00 0c 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
[  New  -> Office ]  Lrg. Data Request  (0x4a): 10 00 0b
[Office  ->   New ]  Lrg. Data Answer   (0x4b): 10 00 0b 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
[  New  -> Office ]  Lrg. Data Request  (0x4a): 10 00 0a
[Office  ->   New ]  Lrg. Data Answer   (0x4b): 10 00 0a 00 00 00 00 56 6c d8 00 00 00 00 00 00 00 00 00 00 00
[  New  -> Office ]  Lrg. Data Request  (0x4a): 10 00 09
[Office  ->   New ]  Lrg. Data Answer   (0x4b): 10 00 09 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
[  New  -> Office ]  Lrg. Data Request  (0x4a): 10 00 08
[Office  ->   New ]  Lrg. Data Answer   (0x4b): 10 00 08 00 00 00 00 00 00 00 00 00 00 00 00 04 04 00 00 00 01
[  New  -> Office ]  Lrg. Data Request  (0x4a): 10 00 07
[Office  ->   New ]  Lrg. Data Answer   (0x4b): 10 00 07 00 00 0c 00 4b 00 69 00 74 00 63 00 68 00 65 00 6e 00
[  New  -> Office ]  Lrg. Data Request  (0x4a): 10 00 06
[Office  ->   New ]  Lrg. Data Answer   (0x4b): 10 00 06 20 00 4c 00 65 00 66 00 74 00 00 00 00 00 00 00 00 00
[  New  -> Office ]  Lrg. Data Request  (0x4a): 10 00 05
[Office  ->   New ]  Lrg. Data Answer   (0x4b): 10 00 05 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
[  New  -> Office ]  Lrg. Data Request  (0x4a): 10 00 04
[Office  ->   New ]  Lrg. Data Answer   (0x4b): 10 00 04 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 e7 65
[  New  -> Office ]  Lrg. Data Request  (0x4a): 10 00 03
[Office  ->   New ]  Lrg. Data Answer   (0x4b): 10 00 03 05 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
[  New  -> Office ]  Lrg. Data Request  (0x4a): 10 00 02
[Office  ->   New ]  Lrg. Data Answer   (0x4b): 10 00 02 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
[  New  -> Office ]  Lrg. Data Request  (0x4a): 10 00 01
[Office  ->   New ]  Lrg. Data Answer   (0x4b): 10 00 01 00 00 00 00 00 00
[  New  -> Office ]  Lrg. Data Request  (0x4a): 10 00 00
[Office  ->   New ]  Lrg. Data Answer   (0x4b): 10 00 00
```

#### `01 01 00 00 16` Group
```
[  New  -> Office ]   -- Unknown --     (0x46): 01 01 00 00 16 00 00 00 3c
[Office  ->   New ]   -- Unknown --     (0x47): 11 00 00 78

[  New  -> Office ]  Lrg. Data Request  (0x4a): 11 00 07
[Office  ->   New ]  Lrg. Data Answer   (0x4b): 11 00 07 00 00 01 00 80 02 07 00 4b 00 69 00 74 00 63 00 68 00
[  New  -> Office ]  Lrg. Data Request  (0x4a): 11 00 06
[Office  ->   New ]  Lrg. Data Answer   (0x4b): 11 00 06 65 00 6e 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
[  New  -> Office ]  Lrg. Data Request  (0x4a): 11 00 05
[Office  ->   New ]  Lrg. Data Answer   (0x4b): 11 00 05 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
[  New  -> Office ]  Lrg. Data Request  (0x4a): 11 00 04
[Office  ->   New ]  Lrg. Data Answer   (0x4b): 11 00 04 00 00 00 00 02 00 01 01 01 00 80 02 0a 00 41 00 6c 00
[  New  -> Office ]  Lrg. Data Request  (0x4a): 11 00 03
[Office  ->   New ]  Lrg. Data Answer   (0x4b): 11 00 03 6c 00 20 00 42 00 6c 00 69 00 6e 00 64 00 73 00 00 00
[  New  -> Office ]  Lrg. Data Request  (0x4a): 11 00 02
[Office  ->   New ]  Lrg. Data Answer   (0x4b): 11 00 02 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
[  New  -> Office ]  Lrg. Data Request  (0x4a): 11 00 01
[Office  ->   New ]  Lrg. Data Answer   (0x4b): 11 00 01 00 00 00 00 00 00 00 00 00 00 05 00
[  New  -> Office ]  Lrg. Data Request  (0x4a): 11 00 00
[Office  ->   New ]  Lrg. Data Answer   (0x4b): 11 00 00
```

#### `01 00 00 19` Group
```
[  New  -> Office ]   -- Unknown --     (0x46): 01 01 00 00 19 00 00 4e 22
[Office  ->   New ]   -- Unknown --     (0x47): 12 01 00 10

[  New  -> Office ]  Lrg. Data Request  (0x4a): 12 00 01
[Office  ->   New ]  Lrg. Data Answer   (0x4b): 12 00 01 07 00 00 03 00 04 00 00 00 01 00 02 00 03 00 04
[  New  -> Office ]  Lrg. Data Request  (0x4a): 12 00 00
[Office  ->   New ]  Lrg. Data Answer   (0x4b): 12 00 00
```

#### `01 00 00 17` Group
```
[  New  -> Office ]   -- Unknown --     (0x46): 01 01 00 00 17 00 00 00 36
[Office  ->   New ]   -- Unknown --     (0x47): 13 00 00 36

[  New  -> Office ]  Lrg. Data Request  (0x4a): 13 00 03
[Office  ->   New ]  Lrg. Data Answer   (0x4b): 13 00 03 00 00 01 07 4d 00 79 00 20 00 68 00 6f 00 6d 00 65 00
[  New  -> Office ]  Lrg. Data Request  (0x4a): 13 00 02
[Office  ->   New ]  Lrg. Data Answer   (0x4b): 13 00 02 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
[  New  -> Office ]  Lrg. Data Request  (0x4a): 13 00 01
[Office  ->   New ]  Lrg. Data Answer   (0x4b): 13 00 01 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
[  New  -> Office ]  Lrg. Data Request  (0x4a): 13 00 00
[Office  ->   New ]  Lrg. Data Answer   (0x4b): 13 00 00
```

#### `01 00 00 1a` group
```
[  New  -> Office ]   -- Unknown --     (0x46): 01 01 00 00 1a 00 00 02 d2
[Office  ->   New ]   -- Unknown --     (0x47): 14 01 00 02

[  New  -> Office ]  Lrg. Data Request  (0x4a): 14 00 01
[Office  ->   New ]  Lrg. Data Answer   (0x4b): 14 00 01 00 00
[  New  -> Office ]  Lrg. Data Request  (0x4a): 14 00 00
[Office  ->   New ]  Lrg. Data Answer   (0x4b): 14 00 00
```

#### `01 00 00 18` Group
```
[  New  -> Office ]   -- Unknown --     (0x46): 01 01 00 00 18 00 00 00 3c
[Office  ->   New ]   -- Unknown --     (0x47): 15 00 00 3c

[  New  -> Office ]  Lrg. Data Request  (0x4a): 15 00 04
[Office  ->   New ]  Lrg. Data Answer   (0x4b): 15 00 04 00 00 00 00 80 02 13 00 41 00 6c 00 6c 00 20 00 76 00
[  New  -> Office ]  Lrg. Data Request  (0x4a): 15 00 03
[Office  ->   New ]  Lrg. Data Answer   (0x4b): 15 00 03 65 00 72 00 74 00 69 00 63 00 61 00 6c 00 20 00 62 00
[  New  -> Office ]  Lrg. Data Request  (0x4a): 15 00 02
[Office  ->   New ]  Lrg. Data Answer   (0x4b): 15 00 02 6c 00 69 00 6e 00 64 00 73 00 00 00 00 00 00 00 00 00
[  New  -> Office ]  Lrg. Data Request  (0x4a): 15 00 01
[Office  ->   New ]  Lrg. Data Answer   (0x4b): 15 00 01 00 00 00 00 05 00
[  New  -> Office ]  Lrg. Data Request  (0x4a): 15 00 00
[Office  ->   New ]  Lrg. Data Answer   (0x4b): 15 00 00
```

#### `01 00 00 1b` Group
```
[  New  -> Office ]   -- Unknown --     (0x46): 01 01 00 00 1b 00 00 4e 22
[Office  ->   New ]   -- Unknown --     (0x47): 16 01 00 0c

[  New  -> Office ]  Lrg. Data Request  (0x4a): 16 00 01
[Office  ->   New ]  Lrg. Data Answer   (0x4b): 16 00 01 05 00 00 00 00 01 00 02 00 03 00 04
[  New  -> Office ]  Lrg. Data Request  (0x4a): 16 00 00
[Office  ->   New ]  Lrg. Data Answer   (0x4b): 16 00 00
```



