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

## Features

- **MP3 playback** from microSD card via 3.5mm headphone jack
- **OLED display** (SSD1306 128×64) — track info with scrolling title, volume bar, play/pause icon
- **Audiobook support** — automatic detection via ID3v2 genre tag, chapter navigation (CHAP/CTOC frames), and persistent bookmarks across power cycles
- **Playlist / folder navigation** — browse and select folders as playlists; "All Songs" mode plays everything
- **3-band equalizer** — Loud, Bass Boost, and Flat presets
- **Sleep timer** — 30-minute and 60-minute auto-pause options
- **Repeat modes** — All, One, Off
- **Accelerometer sleep/wake** — display turns off when the device is laid flat for 2 seconds; wakes when picked up
- **Deep sleep** — hold Play for 3 seconds; wakes on Play button press with minimal power draw
- **Low battery protection** — monitors MCP73871 LBO signal, displays charging status, and auto-recovers
- **Settings persistence** — volume, EQ, repeat mode, and audiobook bookmarks saved to NVS flash

---

## Usage

1. **Load music** — Copy MP3 files to a microSD card. Organize into folders for playlist support.
2. **Insert card** — Place the microSD card into the Murmur slot.
3. **Play** — Press the Play button. Murmur begins playback from the first track (or the last audiobook bookmark).

### Button Controls

| Button | Short press | Long press |
|--------|------------|------------|
| **Play** | Play / Pause | Hold 3 s → deep sleep |
| **Play ×2** | Toggle settings menu | — |
| **Next** | Next track | Chapter menu (audiobooks) |
| **Prev** | Previous track | Playlist menu |
| **Vol Up** | Volume up | — |
| **Vol Down** | Volume down | — |

### Audiobooks

Files whose ID3v2 genre tag contains "Audiobook" are treated as audiobooks. Murmur saves your position automatically when you pause, change playlists, or power off, and resumes where you left off on the next boot. If the file contains CHAP frames, long-press Next to open a chapter navigator.

---

## Repository Contents

```
BOM.csv          — Full bill of materials with JLCPCB part numbers
gerbers/         — Fabrication files (Gerber + drill) ready for PCB manufacture
firmware/murmur/ — Arduino sketch and source for the ESP32-S3
firmware/tools/  — Utilities (logo bitmap converter)
Models/          — 3D-printable enclosure and button cap files (.3mf, .step)
images/          — PCB 3D model, logo source, enclosure STEP file
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
