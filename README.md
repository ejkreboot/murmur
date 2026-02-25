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

## Repository Contents

```
BOM.csv          — Full bill of materials with JLCPCB part numbers
gerbers/         — Fabrication files (Gerber + drill) ready for PCB manufacture
images/          — Schematics, renderings, logo
```


---

## Status

PCBs in production, not yet tested.
Enclosure design (for 3D printers) under development.
Firmware under development.

---

## Building One

The gerbers are in `gerbers/` and the BOM references JLCPCB part numbers, so ordering a board through JLCPCB (or any compatible fab) should be straightforward. The BOM includes all SMD components; assembly is possible by hand but a stencil is recommended given the QFN and LGA packages.

Firmware is not yet in this repository.

---

## Contributing

This project is open source under the CERN-OHL-S-2.0 license. Improvements and modifications encouraged. Issues and pull requests are welcome.

---

## License

CERN-OHL-S-2.0
