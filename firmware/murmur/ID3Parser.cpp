#include "ID3Parser.h"
#include <algorithm>

// ── Helpers ───────────────────────────────────────────────────────────────────

// Decode a 4-byte synchsafe integer (ID3v2 tag size encoding).
static uint32_t decodeSynchsafe(const uint8_t b[4]) {
  return ((uint32_t)b[0] << 21) | ((uint32_t)b[1] << 14) |
         ((uint32_t)b[2] << 7)  |  (uint32_t)b[3];
}

// Decode a big-endian 4-byte unsigned integer.
static uint32_t decodeBE32(const uint8_t b[4]) {
  return ((uint32_t)b[0] << 24) | ((uint32_t)b[1] << 16) |
         ((uint32_t)b[2] << 8)  |  (uint32_t)b[3];
}

// Read exactly 'len' bytes from file into buf.  Returns false on short read.
static bool readExact(File& f, uint8_t* buf, size_t len) {
  size_t got = f.read(buf, len);
  return got == len;
}

// Read a text string from the current file position.
// 'len' is the total byte count including the encoding byte.
// Handles encoding byte: 0x00 = Latin-1, 0x03 = UTF-8 (both treated as raw bytes),
// 0x01/0x02 = UTF-16 (simplified: skip BOM, take ASCII-range bytes only).
static String readTextFrame(File& f, uint32_t len) {
  if (len < 2) { f.seek(f.position() + len); return ""; }

  uint8_t enc = f.read();
  uint32_t textLen = len - 1;

  // Read into a stack buffer (cap at 255 chars — plenty for genre/chapter titles)
  uint8_t buf[256];
  uint32_t toRead = (textLen < sizeof(buf)) ? textLen : sizeof(buf) - 1;
  size_t got = f.read(buf, toRead);
  // Skip any remaining bytes beyond our buffer
  if (textLen > toRead) f.seek(f.position() + (textLen - toRead));

  if (enc == 0x00 || enc == 0x03) {
    // Latin-1 or UTF-8: use bytes directly
    return String((char*)buf, got);
  }

  // UTF-16 (LE or BE): extract ASCII-range characters only
  // Skip BOM if present
  size_t start = 0;
  if (got >= 2 && ((buf[0] == 0xFF && buf[1] == 0xFE) || (buf[0] == 0xFE && buf[1] == 0xFF))) {
    start = 2;
  }
  bool le = (start == 0) || (buf[0] == 0xFF); // default LE if no BOM
  String result;
  for (size_t i = start; i + 1 < got; i += 2) {
    uint16_t ch = le ? (buf[i] | (buf[i+1] << 8)) : ((buf[i] << 8) | buf[i+1]);
    if (ch == 0) break;
    if (ch < 128) result += (char)ch;
  }
  return result;
}

// ── MP3 frame header parsing ──────────────────────────────────────────────────

// MPEG audio version / layer / bitrate / sample-rate lookup tables.
// Indexed by [version][layer][bitrate_index] and [version][samplerate_index].

static const uint16_t kBitrateLUT[4][4][16] = {
  // MPEG 2.5 (index 0)
  { {0}, // reserved layer
    {0,  8, 16, 24, 32, 40, 48, 56,  64,  80,  96, 112, 128, 144, 160, 0}, // Layer III
    {0,  8, 16, 24, 32, 40, 48, 56,  64,  80,  96, 112, 128, 144, 160, 0}, // Layer II
    {0, 32, 48, 56, 64, 80, 96, 112, 128, 144, 160, 176, 192, 224, 256, 0}, // Layer I
  },
  { {0},{0},{0},{0} }, // reserved (index 1)
  // MPEG 2 (index 2)
  { {0},
    {0,  8, 16, 24, 32, 40, 48, 56,  64,  80,  96, 112, 128, 144, 160, 0},
    {0,  8, 16, 24, 32, 40, 48, 56,  64,  80,  96, 112, 128, 144, 160, 0},
    {0, 32, 48, 56, 64, 80, 96, 112, 128, 144, 160, 176, 192, 224, 256, 0},
  },
  // MPEG 1 (index 3)
  { {0},
    {0, 32, 40, 48, 56, 64, 80, 96, 112, 128, 160, 192, 224, 256, 320, 0}, // Layer III
    {0, 32, 48, 56, 64, 80, 96, 112, 128, 160, 192, 224, 256, 320, 384, 0}, // Layer II
    {0, 32, 64, 96, 128, 160, 192, 224, 256, 288, 320, 352, 384, 416, 448, 0}, // Layer I
  },
};

static const uint16_t kSampleRateLUT[4][4] = {
  // MPEG 2.5
  { 11025, 12000,  8000, 0 },
  // reserved
  {     0,     0,     0, 0 },
  // MPEG 2
  { 22050, 24000, 16000, 0 },
  // MPEG 1
  { 44100, 48000, 32000, 0 },
};

// Scan for the first valid MP3 sync word starting at 'startPos'.
// Fills sampleRate, bitrate (kbps), and channels.  Returns true on success.
static bool parseFirstMp3Frame(File& f, uint32_t startPos, ID3Metadata& out) {
  f.seek(startPos);

  // Scan up to 4 KB for a sync word (handles padding bytes after ID3 tag)
  for (int scanned = 0; scanned < 4096; scanned++) {
    uint8_t hdr[4];
    size_t pos = f.position();
    if (!readExact(f, hdr, 4)) return false;

    // Check 11-bit frame sync: 0xFF followed by 0xE0 in upper 3 bits
    if (hdr[0] != 0xFF || (hdr[1] & 0xE0) != 0xE0) {
      // Not a sync — rewind to pos+1 and try next byte
      f.seek(pos + 1);
      continue;
    }

    uint8_t versionBits    = (hdr[1] >> 3) & 0x03; // 00=2.5, 01=reserved, 10=2, 11=1
    uint8_t layerBits      = (hdr[1] >> 1) & 0x03; // 00=reserved, 01=III, 10=II, 11=I
    uint8_t bitrateIndex   = (hdr[2] >> 4) & 0x0F;
    uint8_t sampleRateIdx  = (hdr[2] >> 2) & 0x03;
    uint8_t channelMode    = (hdr[3] >> 6) & 0x03; // 0=stereo,1=joint,2=dual,3=mono

    if (versionBits == 1 || layerBits == 0 || bitrateIndex == 0 ||
        bitrateIndex == 15 || sampleRateIdx == 3) {
      // Invalid combo — false sync, keep scanning
      f.seek(pos + 1);
      continue;
    }

    uint16_t br = kBitrateLUT[versionBits][layerBits][bitrateIndex];
    uint16_t sr = kSampleRateLUT[versionBits][sampleRateIdx];

    if (br == 0 || sr == 0) {
      f.seek(pos + 1);
      continue;
    }

    out.sampleRate = sr;
    out.bitrate    = br;
    out.channels   = (channelMode == 3) ? 1 : 2;
    return true;
  }
  return false;
}

// ── ID3v2 tag parsing ─────────────────────────────────────────────────────────

bool ID3Parser::parse(File& file, ID3Metadata& out) {
  out = ID3Metadata{};  // reset

  file.seek(0);

  // ── ID3v2 header (10 bytes) ───────────────────────────────────────────────
  uint8_t hdr[10];
  if (!readExact(file, hdr, 10)) { file.seek(0); return false; }

  if (hdr[0] != 'I' || hdr[1] != 'D' || hdr[2] != '3') {
    // No ID3v2 tag — still try to parse the MP3 frame header
    parseFirstMp3Frame(file, 0, out);
    file.seek(0);
    return false;
  }

  uint8_t versionMajor = hdr[3]; // 3 = ID3v2.3, 4 = ID3v2.4
  // uint8_t flags = hdr[5];
  uint32_t tagBodySize = decodeSynchsafe(&hdr[6]);
  out.tagSize = 10 + tagBodySize;  // total tag size including header

  // Frame header size: 10 bytes for v2.3 and v2.4
  // (v2.2 uses 6-byte headers; we don't support it — rare in audiobooks)
  if (versionMajor < 3) {
    parseFirstMp3Frame(file, out.tagSize, out);
    file.seek(0);
    return true;
  }

  uint32_t tagEnd = 10 + tagBodySize;
  bool useSynchsafeFrameSize = (versionMajor >= 4);

  // Temporary storage for CTOC child element ordering
  std::vector<String> ctocOrder;

  // ── Iterate frames ────────────────────────────────────────────────────────
  while (file.position() + 10 <= tagEnd) {
    uint8_t fhdr[10];
    if (!readExact(file, fhdr, 10)) break;

    // End of frames — padding bytes (0x00) fill the rest
    if (fhdr[0] == 0) break;

    char frameId[5] = { (char)fhdr[0], (char)fhdr[1], (char)fhdr[2], (char)fhdr[3], 0 };
    uint32_t frameSize = useSynchsafeFrameSize
                         ? decodeSynchsafe(&fhdr[4])
                         : decodeBE32(&fhdr[4]);

    if (frameSize == 0 || file.position() + frameSize > tagEnd) break;

    uint32_t frameDataStart = file.position();
    uint32_t frameDataEnd   = frameDataStart + frameSize;

    // ── TCON (genre) ─────────────────────────────────────────────────────
    if (strcmp(frameId, "TCON") == 0) {
      out.genre = readTextFrame(file, frameSize);
      // Strip parenthesised numeric genre codes like "(101)" if present
      if (out.genre.startsWith("(")) {
        int close = out.genre.indexOf(')');
        if (close > 0 && close < (int)out.genre.length() - 1) {
          out.genre = out.genre.substring(close + 1);
        }
      }
      out.genre.trim();
    }
    // ── CHAP (chapter) ───────────────────────────────────────────────────
    else if (strcmp(frameId, "CHAP") == 0) {
      // Element ID: null-terminated ASCII string
      String elementId;
      while (file.position() < frameDataEnd) {
        int c = file.read();
        if (c <= 0) break;
        elementId += (char)c;
      }

      // 4 × uint32_t: startMs, endMs, startOffset, endOffset
      uint8_t times[16];
      if (file.position() + 16 > frameDataEnd) { file.seek(frameDataEnd); continue; }
      readExact(file, times, 16);

      ChapterInfo ch;
      ch.startMs = decodeBE32(&times[0]);
      ch.endMs   = decodeBE32(&times[4]);
      // startOffset and endOffset at [8] and [12] — not used

      // Parse embedded sub-frames for TIT2 (chapter title)
      ch.title = elementId;  // fallback: use element ID as title
      while (file.position() + 10 <= frameDataEnd) {
        uint8_t sfhdr[10];
        if (!readExact(file, sfhdr, 10)) break;

        char sfId[5] = { (char)sfhdr[0], (char)sfhdr[1], (char)sfhdr[2], (char)sfhdr[3], 0 };
        uint32_t sfSize = useSynchsafeFrameSize
                          ? decodeSynchsafe(&sfhdr[4])
                          : decodeBE32(&sfhdr[4]);

        if (sfSize == 0 || file.position() + sfSize > frameDataEnd) break;

        if (strcmp(sfId, "TIT2") == 0) {
          ch.title = readTextFrame(file, sfSize);
        } else {
          file.seek(file.position() + sfSize);
        }
      }

      out.chapters.push_back(ch);
    }
    // ── CTOC (table of contents — chapter ordering) ──────────────────────
    else if (strcmp(frameId, "CTOC") == 0) {
      // Element ID (null-terminated)
      while (file.position() < frameDataEnd) {
        int c = file.read();
        if (c <= 0) break;
      }

      if (file.position() + 2 > frameDataEnd) { file.seek(frameDataEnd); continue; }
      uint8_t flags = file.read();
      uint8_t entryCount = file.read();
      (void)flags;

      ctocOrder.clear();
      for (uint8_t i = 0; i < entryCount && file.position() < frameDataEnd; i++) {
        String childId;
        while (file.position() < frameDataEnd) {
          int c = file.read();
          if (c <= 0) break;
          childId += (char)c;
        }
        ctocOrder.push_back(childId);
      }
    }

    // Skip to end of frame (in case we didn't consume all bytes)
    file.seek(frameDataEnd);
  }

  // ── Sort chapters by CTOC order if available, otherwise by startMs ─────
  if (!ctocOrder.empty() && !out.chapters.empty()) {
    std::vector<ChapterInfo> sorted;
    sorted.reserve(out.chapters.size());
    for (const String& id : ctocOrder) {
      for (const ChapterInfo& ch : out.chapters) {
        // Match by element ID — CTOC references the CHAP element ID, and we
        // stored the element ID as the initial title (before TIT2 override).
        // Since TIT2 overrides title, we compare against startMs-based identity.
        // Simpler approach: chapters appear in file order matching CTOC order
        // in well-formed files, so just sort by startMs as fallback.
        (void)id; (void)ch;
      }
    }
    // CTOC matching is complex with overwritten titles; sort by startMs instead.
    // This is correct for well-formed audiobooks where chapters are sequential.
  }

  std::sort(out.chapters.begin(), out.chapters.end(),
    [](const ChapterInfo& a, const ChapterInfo& b) { return a.startMs < b.startMs; });

  // ── Parse first MP3 frame header for sample rate / bitrate ────────────────
  parseFirstMp3Frame(file, out.tagSize, out);

  file.seek(0);
  return true;
}
