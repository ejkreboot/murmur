# Murmur

An audio "casette" player for the 21st century.

---

## Philosophy

Most audio hardware today is optimized for connectivity. Bluetooth, WiFi, cloud sync, firmware updates, pairing modes, app dependencies. These features solve real problems, but they also introduce friction — dead batteries from background radios, pairing failures at the worst moments, apps that require accounts, devices that stop working when servers go away, constant distractions and potentials for invasion of privacy or other undesirable intrusions.

Murmur is a deliberate step in the other direction. It plays audio files from a microSD card through a 3.5mm jack. That's the whole product. No wireless radios in use, no companion app, no account required. You put files on a card and press play.

Future revisions may (or may not) add internet and bluetooth features if they "just work" and maintain the Murmur ethos. 

The name comes from the idea that good audio gear should be unobtrusive. It should murmur in the background of your life, not demand attention.

---

## Hardware

**Microcontroller:** ESP32-S3-WROOM-1U (8MB flash)  
**DAC / Headphone driver:** Texas Instruments TAD5242  
**Battery management:** Microchip MCP73871  
**Storage:** MicroSD (via USB-C when connected to a computer)  
**Motion sensor:** ST LIS3DH (3-axis accelerometer)  
**Charging / Data:** USB-C  
**Output:** 3.5mm stereo headphone jack (PJ-327C-4A)

The ESP32-S3 was chosen for its capable USB peripheral support and audio I2S interface, not its wireless features. The TAD5242 handles D/A conversion and drives headphones directly. The MCP73871 manages single-cell Li-Ion/LiPo charging with thermistor-based temperature protection.

---

## User's Manual

For features, usage instructions, button controls, and audiobook support, see the **[Murmur User's Manual](https://ejkreboot.github.io/murmur)**.

---

## Repository Contents

```
BOM.csv          — Full bill of materials with JLCPCB part numbers
docs/            — User's manual and supporting images
gerbers/         — Fabrication files (Gerber + drill) ready for PCB manufacture
firmware/murmur/ — Arduino sketch and source for the ESP32-S3
firmware/tools/  — Utilities (logo bitmap converter)
images/          — PCB renders, schematics, logos, and 3D model
Models/          — 3D-printable enclosure and button cap files (.3mf, .step)
```

---

## Firmware

The firmware lives in `firmware/murmur/` and is an Arduino sketch targeting the **ESP32-S3-WROOM-1U**. It uses the [Arduino AudioTools](https://github.com/pschatzmann/arduino-audio-tools) library with the Helix MP3 decoder.

### Building

1. Install the [Arduino IDE](https://www.arduino.cc/en/software) (or PlatformIO) with ESP32-S3 board support.
2. Install the required libraries: **Arduino AudioTools**, **Adafruit SSD1306**, **Adafruit LIS3DH**.
3. Open `firmware/murmur/murmur.ino`.
4. Select the **ESP32S3 Dev Module** board and configure: 8 MB flash, USB CDC on boot enabled.
5. Upload.

### Architecture

The firmware is modular and class-based:

| Module | Responsibility |
|--------|---------------|
| `murmur.ino` | App state machine, NVS persistence, sleep/wake, main loop |
| `AudioPlayer` | MP3 decode, I2S output, EQ, volume, track/folder navigation |
| `ButtonManager` | 5-button debounce FSM with tap-defer and long-press detection |
| `DisplayManager` | OLED screens: track, settings menu, playlist, chapters, battery |
| `AccelManager` | LIS3DH flat-detect for display sleep/wake |
| `ID3Parser` | ID3v2 tag parsing: genre, chapters, MP3 frame header |
| `config.h` | Pin definitions, mode enums, tuning constants |

---

## Building One

The gerbers are in `gerbers/` and the BOM references JLCPCB part numbers, so ordering a board through JLCPCB (or any compatible fab) should be straightforward. The BOM includes all SMD components; assembly is possible by hand but a stencil is recommended given the QFN and LGA packages.

3D-printable enclosure and button cap models are in `Models/`.

---

## Contributing

This project is open source under the CERN-OHL-S-2.0 license. Improvements and modifications encouraged. Issues and pull requests are welcome.

---

## License

CERN-OHL-S-2.0
