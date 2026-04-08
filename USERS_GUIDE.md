# Murmur User's Guide

Murmur is a simple, distraction-free MP3 player. No apps, no accounts, no wireless — just your music on a microSD card and a 3.5mm headphone jack.

---

## Getting Started

### 1. Prepare Your SD Card

Format a microSD card as **FAT32**. Copy your MP3 files to the root of the card, or organize them into folders.

```
SD Card
├── song1.mp3
├── song2.mp3
├── Road Trip/
│   ├── track1.mp3
│   └── track2.mp3
└── Chill/
    ├── intro.mp3
    └── sunset.mp3
```

- **Root-level files** are part of the "All Songs" playlist.
- **Folders** become selectable playlists. Each folder's MP3 files are sorted alphabetically.
- Files and folders are sorted alphabetically (case-insensitive).

### 2. Insert the Card & Power On

Slide the microSD card into the Murmur's card slot. Press the **Play** button. The OLED display will show a startup logo, then begin playback from the first track.

### 3. Plug In Headphones

Connect headphones or a speaker to the **3.5mm headphone jack**.

---

## Button Controls

Murmur has five buttons: **Play**, **Next**, **Prev** (Back), **Vol Up**, and **Vol Down**.

| Button | Short Press | Long Press (hold) |
|--------|------------|---------------------|
| **Play** | Play / Pause | Hold 3 seconds → Power off (deep sleep) |
| **Play × 2** (double-tap) | Open / close Settings menu | — |
| **Next** | Skip to next track | Open Chapter menu (audiobooks only) |
| **Prev** | Go to previous track | Open Playlist menu |
| **Vol Up** | Increase volume | Auto-repeats while held |
| **Vol Down** | Decrease volume | Auto-repeats while held |

---

## The Display

The OLED screen shows different information depending on what you're doing:

### Now Playing Screen

When music is playing, the display shows:
- **Play/pause icon** — indicates current playback state
- **Track counter** — e.g. "3/12" (track 3 of 12)
- **Volume bar** — visual indicator of the current volume level
- **Track title** — scrolls automatically if it's too long to fit on screen
- **Playlist or chapter name** — shown at the bottom (the active folder name, or the current chapter name for audiobooks)

### Volume Overlay

When you adjust the volume, a volume bar briefly appears on screen showing the new level.

---

## Playlists (Folders)

Murmur treats each folder on the SD card as a playlist.

### Browsing Playlists

1. **Long-press Prev** to open the Playlist menu.
2. Use **Vol Up / Vol Down** to scroll through the list.
3. The first option is always **"All Songs"**, which plays every MP3 on the card.
4. Press **Play** to select a playlist and start playback.
5. Press **Prev** to go back without changing the playlist.

---

## Settings Menu

**Double-tap Play** to open the Settings menu. Double-tap again (or select "Exit") to return to the Now Playing screen.

Use **Vol Up / Vol Down** to move between settings, and **Play** to cycle through options:

### Equalizer (EQ)

Three presets that shape the sound:
- **Loud** — boosted presence and punch
- **Bass Boost** — enhanced low-end
- **Flat** — no EQ applied (default)

### Sleep Timer

Automatically pauses playback after a set time — great for falling asleep to music or audiobooks:
- **Off** (default)
- **30 min**
- **60 min**

The timer resets each time you power on.

### Repeat Mode

Controls what happens when the current track or playlist ends:
- **All** — loop the entire playlist (default)
- **One** — repeat the current track
- **Off** — stop at the end of the playlist

---

## Audiobooks

Murmur has built-in audiobook support with bookmarking and chapter navigation.

### How Murmur Detects Audiobooks

An MP3 file is treated as an audiobook if its **ID3v2 genre tag** contains the word "Audiobook". You can set this tag using any music tagging tool (e.g. Mp3tag, MusicBrainz Picard, or Kid3).

### Bookmarks

Murmur **automatically saves your position** in an audiobook when you:
- Pause playback
- Switch to a different playlist
- Power off or the device goes to sleep

When you power on again, playback **resumes exactly where you left off**.

### Chapter Navigation

If your audiobook file contains **ID3v2 CHAP frames** (embedded chapter markers):

1. **Long-press Next** to open the Chapter menu.
2. Use **Vol Up / Vol Down** to scroll through chapters. The current chapter is highlighted.
3. Press **Play** to jump to the selected chapter.
4. Press **Next** or **Prev** to close the menu without jumping.

The current chapter name is displayed at the bottom of the Now Playing screen and updates automatically as playback progresses.

---

## Power Management

### Deep Sleep (Power Off)

**Hold the Play button for 3 seconds.** Murmur will display a power-off message, save your settings and audiobook bookmark, then enter deep sleep with minimal power draw.

To wake up, **press the Play button**. The device fully restarts and restores your last settings.

### Display Auto-Sleep

Murmur uses a built-in motion sensor. When you **set the device down flat for 2 seconds**, the display automatically turns off to save power. **Pick it up** and the display turns back on immediately. Audio playback continues uninterrupted in either case.

### Low Battery

When the battery is low, Murmur will:
1. Save your audiobook bookmark and settings.
2. Stop playback.
3. Show a **battery icon** on the display.

Plug in a USB-C cable to charge. The display will show a **lightning bolt** when charging. Once the battery recovers (or USB power is connected), the device automatically restarts.

---

## Charging

Connect a **USB-C cable** to charge the battery. You can also use the USB-C connection to **load files onto the microSD card** from a computer without removing the card.

---

## SD Card Tips

- **Supported format:** MP3 files only.
- **Card format:** FAT32.
- **File organization:** Files in the root and in one level of subfolders are recognized. Deeply nested folders are not scanned.
- **Naming:** Track order is alphabetical. Prefix filenames with numbers (e.g. `01 - Intro.mp3`, `02 - Chapter One.mp3`) if you want a specific play order.
- **No SD card?** Murmur will display "No SD Card" and wait. Insert a card and it will automatically restart.
- **Card removed during playback?** Murmur detects this, displays "SD Removed", and waits for you to reinsert the card.

---

## Settings That Persist

The following settings are **saved across power cycles** — you don't need to reconfigure them each time:

- Volume level
- EQ preset
- Repeat mode
- Audiobook bookmark (track + position)

The sleep timer always resets to **Off** on each boot.

---

## Quick Reference

| Action | How |
|--------|-----|
| Play / Pause | Press **Play** |
| Next track | Press **Next** |
| Previous track | Press **Prev** |
| Volume up/down | Press **Vol Up** / **Vol Down** |
| Open Settings | Double-tap **Play** |
| Open Playlists | Long-press **Prev** |
| Open Chapters | Long-press **Next** (audiobooks) |
| Power off | Hold **Play** for 3 seconds |
| Power on | Press **Play** |
