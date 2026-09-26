// Radio sniffer for io-HomeControl packets, written for the Adafruit
// nRF82540 Feather.  Scans 802.15.4 channels 15, 20, and 25 looking for packets,
// and prints them to the serial port.  The preamble, SFD and length byte
// are not included in the output -- the CRC is.

#include <Adafruit_TinyUSB.h> // required for Serial to resolve

// Array of target channels
const uint8_t scanChannels[] = {15, 20, 25};


int currentChannelIndex = 1;
uint8_t currentChannel = scanChannels[currentChannelIndex];
const int numChannels = sizeof(scanChannels);

// Timing -- too short and you spend all your time switching channels, too long and you miss channel changes
const unsigned long SCAN_WINDOW_MS = 1;  // time (in ms) to listen on a channel before hopping
const unsigned long LOCK_DURATION_MS = 2; // time (in ms) to stay on a channel after a packet arrives on that channel

unsigned long lastChannelSwitchTime = 0;
unsigned long lastPacketReceivedTime = 0;
bool isLocked = false;

// Create a buffer for Direct Memory Access (DMA) storage
// 802.15.4 maximum packet size is 127 bytes + 1 length byte
uint8_t rx_packet_buffer[128] __attribute__((aligned(4)));

void setup() {
  Serial.begin(921600);
  while (!Serial) delay(10); // Wait for Serial Monitor to connect
  Serial.println("--- Initializing ---");

  // Reset/power up the radio peripheral
  NRF_RADIO->POWER = 0;
  delay(10);
  NRF_RADIO->POWER = 1;

  // Set Radio Mode to IEEE 802.15.4 (250kbps, O-QPSK modulation)
  NRF_RADIO->MODE = (RADIO_MODE_MODE_Ieee802154_250Kbit << RADIO_MODE_MODE_Pos);

  // Set radio to fast ramp-up for faster channel changes
  NRF_RADIO->MODECNF0 = (RADIO_MODECNF0_RU_Fast << RADIO_MODECNF0_RU_Pos) | (RADIO_MODECNF0_DTX_Center << RADIO_MODECNF0_DTX_Pos);

  // Customize Packet Engine Configuration 0 (PCNF0)
  // LFLEN = 8 bits (1 byte length field).
  // S0LEN = 0, S1LEN = 0 (Deactivates standard 802.15.4 frame parsing overhead)
  // PLEN = 32-bit zero (Catches your standard 4x 0x00 preambles)
  NRF_RADIO->PCNF0 = (8UL << RADIO_PCNF0_LFLEN_Pos) |
                     (0UL << RADIO_PCNF0_S0LEN_Pos) |
                     (0UL << RADIO_PCNF0_S1LEN_Pos);

  // Customize Packet Engine Configuration 1 (PCNF1)
  // MAXLEN = 127 bytes max payload depth
  // STATLEN = 0 (Payload size is dynamic i.e. controlled by length byte)
  // BALEN = 0 (No proprietary base network address tracking needed)
  NRF_RADIO->PCNF1 = (127UL << RADIO_PCNF1_MAXLEN_Pos) |
                     (0UL << RADIO_PCNF1_STATLEN_Pos) |
                     (0UL << RADIO_PCNF1_BALEN_Pos) |
                     (RADIO_PCNF1_ENDIAN_Little << RADIO_PCNF1_ENDIAN_Pos) |
                     (0UL << RADIO_PCNF1_WHITEEN_Pos); // Disable whitening

  // Change standard SFD (Sync Word/Byte) from 0xA7 to 0x56
  NRF_RADIO->SFD = 0x56;

  // Disable CRC hardware filtering (Read CRC but do not discard failures)
  // IEEE 802.15.4 uses a 2-byte ITU-T CRC
  NRF_RADIO->CRCCNF = (RADIO_CRCCNF_LEN_Disabled << RADIO_CRCCNF_LEN_Pos) |
                      (RADIO_CRCCNF_SKIPADDR_Ieee802154 << RADIO_CRCCNF_SKIPADDR_Pos);
  NRF_RADIO->CRCPOLY = 0x8408; // Standard 802.15.4 CRC-16 polynomial
  NRF_RADIO->CRCINIT = 0;

  // Point the Radio DMA to our memory buffer array
  NRF_RADIO->PACKETPTR = (uint32_t)rx_packet_buffer;

  // Turn off shortcuts that might restrict data stream
  NRF_RADIO->SHORTS = 0;

  // Start the receiver
  setRadioChannel(currentChannel);
  startListening();
}

void loop() {
  unsigned long currentTime = millis();
  // Serial.println(currentTime);

  // Wait until the radio hardware signals it has successfully captured a full packet
  if (NRF_RADIO->EVENTS_END) {
    NRF_RADIO->EVENTS_END = 0; // Clear flag

    // Lock onto this channel and update the watchdog timer
    isLocked = true;
    lastPacketReceivedTime = currentTime;

    // Byte 0 in DMA configuration holds the payload length reported by the transmitter
    uint8_t packet_length = rx_packet_buffer[0];

    // Send any packets to the serial port (can add filters here if desired)
    // if (packet_length != 5) {  <-- this gets rid of the 512 wake-up packets that would clutter your logs otherwise
    if (true) {  // <-- use this for cluttered logs

      // Hex dump the payload out to the serial port
      Serial.print(currentChannel);  Serial.print(": ");  // channel number
      for (int i = 1; i <= packet_length; i++) {
        if (rx_packet_buffer[i] < 0x10) Serial.print("0"); // Pad single hex digits
        Serial.print(rx_packet_buffer[i], HEX);
        Serial.print(" ");
      }
      // Serial.print(NRF_RADIO->EVENTS_CRCOK ? "CRC VALID" : "CRC INVALID / UNFILTERED");
      Serial.println();
    }

    // Clean up and instantly reset the radio to catch the next incoming transmission
    NRF_RADIO->EVENTS_CRCOK = 0;
    memset((void*)rx_packet_buffer, 0, sizeof(rx_packet_buffer)); // Flush buffer

    NRF_RADIO->TASKS_START = 1;
  }

  // Handle Channel Locking Watchdog
  if (isLocked && (currentTime - lastPacketReceivedTime > LOCK_DURATION_MS)) {
    isLocked = false;
    // Serial.println("Lock broken. Resuming channel scan...");
    lastChannelSwitchTime = currentTime; // Smooth transition back to scanning
  }

  // Hop channels if it's time to hop.  This process should take 40.5 us using the chip's "fast ramp-up" mode
  if (!isLocked && (currentTime - lastChannelSwitchTime > SCAN_WINDOW_MS)) {
    // Stop the radio safely before changing frequency configurations
    stopRadio();

    // Advance to the next channel in the sequence
    currentChannelIndex = (currentChannelIndex + 1) % numChannels;
    setRadioChannel(scanChannels[currentChannelIndex]);

    // Update state tracking
    lastChannelSwitchTime = currentTime;

    // Restart the radio back on the new frequency
    startListening();
  }

}


void setRadioChannel(uint8_t channel) {
  currentChannel = channel;
  uint32_t frequencyMhzOffset = 5 + (5 * (channel - 11));
  NRF_RADIO->FREQUENCY = frequencyMhzOffset;  // <-- chip uses offset from 2400 MHz
  // Serial.print("Channel: "); Serial.print(channel); Serial.print(" Frequency: "); Serial.println(2400 + frequencyMhzOffset);
}


void stopRadio() {
  NRF_RADIO->TASKS_DISABLE = 1;
  while (NRF_RADIO->EVENTS_DISABLED == 0); // Chip sets this register until it's done disabling the radio
  NRF_RADIO->EVENTS_DISABLED = 0;
}


void startListening() {
  NRF_RADIO->TASKS_RXEN = 1;
  // Serial.println("Waiting for EVENTS_READY");
  while(NRF_RADIO->EVENTS_READY == 0); // Wait for the PLL synthesize lock
  // Serial.println("Starting tasks");
  NRF_RADIO->TASKS_START = 1;
}
