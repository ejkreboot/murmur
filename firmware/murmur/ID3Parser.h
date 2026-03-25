#pragma once
#include <Arduino.h>
#include <SD.h>
#include <vector>

struct ChapterInfo {
  uint32_t startMs;
  uint32_t endMs;
  String   title;
};

struct ID3Metadata {
  String                  genre;
  std::vector<ChapterInfo> chapters;
  uint32_t                tagSize    = 0;   // total ID3v2 tag size (header + body)
  uint32_t                sampleRate = 0;   // from first MP3 frame header
  uint16_t                bitrate    = 0;   // kbps, from first MP3 frame header
  uint8_t                 channels   = 2;   // 1=mono, 2=stereo (from MP3 frame)
};

// Lightweight, read-only ID3v2 parser.
// Extracts only TCON (genre), CHAP/CTOC (chapters), and the first MP3 frame
// header (sample rate, bitrate, channels).  Skips all other frames efficiently.
// Leaves the file seeked back to position 0 when done.
namespace ID3Parser {
  // Parse an open SD file.  Populates 'out' and seeks the file back to 0.
  // Returns true if a valid ID3v2 tag was found (metadata may still be partial).
  bool parse(File& file, ID3Metadata& out);
}
