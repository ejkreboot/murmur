#include "AudioPlayer.h"

// ── Constructor ───────────────────────────────────────────────────────────────
MurmurPlayer::MurmurPlayer(uint8_t sdCS, uint8_t spiSck, uint8_t spiMiso, uint8_t spiMosi,
                         uint8_t i2sBclk, uint8_t i2sFsync, uint8_t i2sDout)
  : _sdCS(sdCS), _spiSck(spiSck), _spiMiso(spiMiso), _spiMosi(spiMosi),
    _bclk(i2sBclk), _fsync(i2sFsync), _dout(i2sDout),
    _decoder(new MP3DecoderHelix())
{}

// ── begin ─────────────────────────────────────────────────────────────────────
bool MurmurPlayer::begin() {
  // Mount SD — initialise SPI bus with explicit pins first
  SPI.begin(_spiSck, _spiMiso, _spiMosi, _sdCS);
  if (!SD.begin(_sdCS, SPI)) {
    return false;
  }
  _collectFolders();  _collectTracks();

  if (_tracks.empty()) {
    return false;
  }

  _initPipeline();
  playTrack(0);
  return true;
}

// ── update ────────────────────────────────────────────────────────────────────
void MurmurPlayer::update() {
  if (!_playing || !_file) return;

  // Periodic SD health check (~every 2 s) to detect card removal without
  // adding SPI overhead on every audio copy cycle.
  static uint32_t lastSdCheck = 0;
  uint32_t now = millis();
  if (now - lastSdCheck >= 2000) {
    lastSdCheck = now;
    if (!SD.exists("/")) {
      _closeTrack();
      _sdError = true;
      return;
    }
  }

  // Copy a chunk from the open file through the decoder pipeline.
  // copy() returns the number of bytes written; 0 means end-of-file.
  if (_copier.copy() == 0) {
    // Track finished — handle according to repeat mode
    switch (_repeatMode) {
      case RepeatMode::ONE:
        playTrack(_currentIndex);
        break;
      case RepeatMode::ALL:
        next();
        break;
      case RepeatMode::OFF:
        if (_currentIndex < (int)_tracks.size() - 1) {
          playTrack(_currentIndex + 1);
        } else {
          _closeTrack();      // stop at end of playlist
          _stoppedAtEnd = true;
        }
        break;
    }
  }
}

// ── currentName ───────────────────────────────────────────────────────────────
// Returns just the filename component (no leading path, with extension).
// _tracks stores full relative paths like "Folder/song.mp3" or "song.mp3";
// the display strips the extension itself via _stripExtension.
String MurmurPlayer::currentName() const {
  if (_tracks.empty()) return "";
  const String& full = _tracks[_currentIndex];
  int slash = full.lastIndexOf('/');
  return (slash >= 0) ? full.substring(slash + 1) : full;
}

// ── setPlaylist ─────────────────────────────────────────────────────────────
void MurmurPlayer::setPlaylist(const String& folder) {
  _activePlaylist = folder;
  _collectTracks(folder);
  if (_tracks.empty()) return;
  _stoppedAtEnd = false;
  playTrack(0);
}

// ── playTrack ─────────────────────────────────────────────────────────────────
void MurmurPlayer::playTrack(int index) {
  if (_tracks.empty()) return;

  // Clamp index
  if (index < 0) index = (int)_tracks.size() - 1;
  if (index >= (int)_tracks.size()) index = 0;

  _closeTrack();
  _currentIndex = index;
  _openTrack(_currentIndex);
}

// ── next / prev ───────────────────────────────────────────────────────────────
void MurmurPlayer::next() {
  playTrack((_currentIndex + 1) % (int)_tracks.size());
}

void MurmurPlayer::prev() {
  int idx = _currentIndex - 1;
  if (idx < 0) idx = (int)_tracks.size() - 1;
  playTrack(idx);
}

// ── shutdown ──────────────────────────────────────────────────────────────────
void MurmurPlayer::shutdown() {
  _closeTrack();  // close open file, clear _playing
  _i2s.end();     // stop I2S DMA, release DMA buffers
  SD.end();       // flush and unmount FAT filesystem
  SPI.end();      // release SPI bus GPIO pins
  delete _eq;
  _eq = nullptr;
}

// ── togglePause ───────────────────────────────────────────────────────────────
void MurmurPlayer::togglePause() {
  _playing = !_playing;
  // When pausing, stop feeding I2S so it goes quiet.
  // The file position is preserved; update() simply won't copy while !_playing.
}

// ── setVolume ─────────────────────────────────────────────────────────────────
void MurmurPlayer::setVolume(float v) {
  _volumeLevel = constrain(v, 0.0f, 1.0f);
  _volume.setVolume(_volumeLevel);
}

// ── setEQ ─────────────────────────────────────────────────────────────────────
void MurmurPlayer::setEQ(EqMode mode) {
  switch (mode) {
    case EqMode::FLAT:
      _eqConfig.gain_low    = 1.0f;
      _eqConfig.gain_medium = 1.0f;
      _eqConfig.gain_high   = 1.0f;
      break;
    case EqMode::BASS_BOOST:
      _eqConfig.gain_low    = 1.8f;
      _eqConfig.gain_medium = 1.0f;
      _eqConfig.gain_high   = 0.9f;
      break;
    case EqMode::LOUD:
      _eqConfig.gain_low    = 1.7f;
      _eqConfig.gain_medium = 0.8f;
      _eqConfig.gain_high   = 1.5f;
      break;
  }
  // Gain values in _eqConfig are read live by the EQ filter each sample — no begin() needed.
}

// ── setAudioInfo ─────────────────────────────────────────────────────────────
// Called by the MP3 decoder whenever it parses stream headers.  I2S is already
// pre-configured from the MP3 frame header in _openTrack(), so this only acts
// as a safety net for mid-stream rate changes (rare — concatenated MP3s).
void MurmurPlayer::setAudioInfo(AudioInfo info) {
  bool rateChanged = (info.sample_rate != _streamInfo.sample_rate ||
                      info.channels    != _streamInfo.channels);

  if (rateChanged) {
    _applyStreamRate(info.sample_rate, info.channels);
  }
}

// ── stoppedAtEnd ──────────────────────────────────────────────────────────────
bool MurmurPlayer::stoppedAtEnd() {
  if (_stoppedAtEnd) { _stoppedAtEnd = false; return true; }
  return false;
}

// ── sdError ───────────────────────────────────────────────────────────────────
bool MurmurPlayer::sdError() {
  if (_sdError) { _sdError = false; return true; }
  return false;
}

// ── _initPipeline ─────────────────────────────────────────────────────────────
void MurmurPlayer::_initPipeline() {
  // I2S output — 32-bit word length to match TAD5242 hardware config
  // buffer_count × buffer_size = DMA headroom in bytes.
  // At 44100 Hz / 32-bit / stereo = 352800 B/s;
  // 12 × 1024 = 12288 B ≈ 35 ms — enough to absorb an OLED refresh at 160 MHz.
  auto cfg         = _i2s.defaultConfig(TX_MODE);
  cfg.pin_bck      = _bclk;
  cfg.pin_ws       = _fsync;
  cfg.pin_data     = _dout;
  cfg.sample_rate  = 44100;
  cfg.channels     = 2;
  cfg.bits_per_sample = 32;
  cfg.use_apll     = true;
  cfg.buffer_count = 12;
  cfg.buffer_size  = 1024;
  _i2s.begin(cfg);

  // Volume stream — set output before begin() so format propagates
  _volume.setOutput(_i2s);
  auto vcfg = _volume.defaultConfig();
  vcfg.copyFrom(cfg);          // 32-bit, matches data arriving from _bits32
  _volume.begin(vcfg);
  _volume.setVolume(_volumeLevel);

  // Bit-width converter: EQ outputs 16-bit, DAC wants 32-bit
  _bits32.setOutput(_volume);
  _bits32.begin(16, 32);  // begin(fromBits, toBits)

  // EQ: operates on 16-bit PCM before bit-width conversion
  // Gains update live via _eqConfig pointer — no restart needed for gain changes.
  _eqConfig.sample_rate    = 44100;
  _eqConfig.channels       = 2;
  _eqConfig.bits_per_sample = 16;
  _eqConfig.gain_low    = 1.0f;
  _eqConfig.gain_medium = 1.0f;
  _eqConfig.gain_high   = 1.0f;
  _eq = new Equalizer3Bands(_bits32);
  _eq->begin(_eqConfig);

  // MP3 decoder feeds into EQ
  _decoder.setOutput(*_eq);
  _decoder.addNotifyAudioChange(*this);  // receive AudioInfo callbacks
  _decoder.begin();

  // StreamCopy will be configured per-track in _openTrack()
}

// ── _applyStreamRate ──────────────────────────────────────────────────────────
// Reclock I2S and EQ to match the given sample rate / channel count.
void MurmurPlayer::_applyStreamRate(uint32_t sampleRate, uint8_t channels) {
  _streamInfo.sample_rate = sampleRate;
  _streamInfo.channels    = channels;

  AudioInfo i2sInfo;
  i2sInfo.sample_rate    = sampleRate;
  i2sInfo.channels       = channels;
  i2sInfo.bits_per_sample = 32;
  _i2s.setAudioInfo(i2sInfo);

  if (_eq) {
    _eqConfig.sample_rate = sampleRate;
    _eqConfig.channels    = channels;
    _eq->begin(_eqConfig);
  }
}

// ── _openTrack ────────────────────────────────────────────────────────────────
void MurmurPlayer::_openTrack(int index, size_t resumeOffset) {
  String path = "/" + _tracks[index];
  _file = SD.open(path.c_str());

  if (!_file) {
    _playing = false;
    return;
  }

  // Mute during setup to prevent any noise from rate reconfiguration.
  _volume.setVolume(0.0f);

  // Parse ID3 metadata: genre, chapters, sample rate, bitrate, channels.
  ID3Parser::parse(_file, _id3);
  // File is seeked back to 0 by the parser.

  // Pre-configure I2S and EQ to the file's actual sample rate BEFORE starting
  // the decoder.  This eliminates the race where decoded frames hit I2S at the
  // wrong clock rate.
  if (_id3.sampleRate != 0 &&
      (_id3.sampleRate != _streamInfo.sample_rate ||
       _id3.channels   != _streamInfo.channels)) {
    _applyStreamRate(_id3.sampleRate, _id3.channels);
  }

  // Reset decoder state for new file
  _decoder.begin();

  // If resuming from a bookmark, seek past the ID3 tag to the saved position.
  if (resumeOffset > 0) {
    _file.seek(resumeOffset);
  }

  // Wire the copier: SD file → decoder
  _copier.begin(_decoder, _file);
  _copier.resize(2048);
  _playing = true;

  // Unmute — I2S is already at the correct rate.
  _volume.setVolume(_volumeLevel);
}

// ── isAudiobook ───────────────────────────────────────────────────────────────
bool MurmurPlayer::isAudiobook() const {
  String g = _id3.genre;
  g.toLowerCase();
  return g.indexOf("audiobook") >= 0;
}

// ── getCurrentChapterIndex ────────────────────────────────────────────────────
// Returns the index of the chapter that contains the current file position,
// or -1 if there are no chapters or position is outside all chapter ranges.
int MurmurPlayer::getCurrentChapterIndex() const {
  if (_id3.chapters.empty() || !_file || _id3.bitrate == 0) return -1;

  // Convert file byte position to approximate time in ms.
  size_t pos = _file.position();
  size_t audioStart = _id3.tagSize;
  if (pos < audioStart) return 0;
  uint32_t ms = (uint32_t)(((double)(pos - audioStart) * 8.0) / _id3.bitrate);

  for (int i = (int)_id3.chapters.size() - 1; i >= 0; i--) {
    if (ms >= _id3.chapters[i].startMs) return i;
  }
  return 0;
}

// ── getChapterName ────────────────────────────────────────────────────────────
String MurmurPlayer::getChapterName() const {
  int idx = getCurrentChapterIndex();
  if (idx < 0) return "";
  return _id3.chapters[idx].title;
}

// ── getFilePosition ───────────────────────────────────────────────────────────
size_t MurmurPlayer::getFilePosition() const {
  if (!_file) return 0;
  return _file.position();
}

// ── seekToChapter ─────────────────────────────────────────────────────────────
void MurmurPlayer::seekToChapter(int chapterIndex) {
  if (chapterIndex < 0 || chapterIndex >= (int)_id3.chapters.size()) return;
  if (!_file || _id3.bitrate == 0) return;

  const ChapterInfo& ch = _id3.chapters[chapterIndex];
  size_t byteOffset = _id3.tagSize +
                      (size_t)((double)ch.startMs / 1000.0 * _id3.bitrate * 1000.0 / 8.0);

  // Mute briefly during seek to avoid any decoder transient.
  _volume.setVolume(0.0f);

  _file.seek(byteOffset);
  _decoder.begin();
  _copier.begin(_decoder, _file);
  _copier.resize(2048);
  _playing = true;

  // Unmute — sample rate hasn't changed within the same file.
  _volume.setVolume(_volumeLevel);
}

// ── seekToPosition ────────────────────────────────────────────────────────────
void MurmurPlayer::seekToPosition(size_t byteOffset) {
  if (!_file) return;

  _volume.setVolume(0.0f);

  _file.seek(byteOffset);
  _decoder.begin();
  _copier.begin(_decoder, _file);
  _copier.resize(2048);
  _playing = true;

  _volume.setVolume(_volumeLevel);
}

// ── _closeTrack ───────────────────────────────────────────────────────────────
void MurmurPlayer::_closeTrack() {
  _playing = false;
  if (_file) {
    _file.close();
  }
}

// ── _collectFolders ────────────────────────────────────────────────────────────
void MurmurPlayer::_collectFolders() {
  _folders.clear();

  File root = SD.open("/");
  if (!root) return;

  File entry;
  while ((entry = root.openNextFile())) {
    if (entry.isDirectory()) {
      String name = entry.name();
      if (name.startsWith("/")) name = name.substring(1);
      // Skip hidden dirs (.Spotlight, .Trashes, System Volume Information, etc.)
      if (!name.startsWith(".") && !name.startsWith("System")) {
        _folders.push_back(name);
      }
    }
    entry.close();
  }
  root.close();

  std::sort(_folders.begin(), _folders.end(),
    [](const String& a, const String& b) { return a < b; });
}

// ── _collectTracks ────────────────────────────────────────────────────────────
void MurmurPlayer::_collectTracks(const String& folder) {
  _tracks.clear();

  // Helper lambda: add an MP3 entry.name() to _tracks, normalising the path so
  // it is always relative to SD root (e.g. "Road Trip/song.mp3" or "song.mp3").
  auto addEntry = [&](File& e, const String& parentFolder) {
    if (e.isDirectory()) return;
    String name = e.name();
    if (name.startsWith("/")) name = name.substring(1);
    if (name.startsWith(".")) return;  // skip macOS resource forks etc.
    String upper = name;
    upper.toUpperCase();
    if (!upper.endsWith(".MP3")) return;
    // For subdirectory entries on ESP32 core 2.x, name is just the filename;
    // on core 3.x it is the full path. Normalise to "folder/file.mp3".
    if (!parentFolder.isEmpty() && !name.startsWith(parentFolder + "/")) {
      name = parentFolder + "/" + name;
    }
    _tracks.push_back(name);
  };

  if (folder.isEmpty()) {
    // All Songs: root-level MP3s first, then each subfolder in sorted order.
    File root = SD.open("/");
    if (root) {
      File entry;
      while ((entry = root.openNextFile())) {
        addEntry(entry, "");
        entry.close();
      }
      root.close();
    }
    for (const String& f : _folders) {
      File dir = SD.open("/" + f);
      if (!dir) continue;
      File entry;
      while ((entry = dir.openNextFile())) {
        addEntry(entry, f);
        entry.close();
      }
      dir.close();
    }
  } else {
    // Single-folder playlist.
    File dir = SD.open("/" + folder);
    if (dir) {
      File entry;
      while ((entry = dir.openNextFile())) {
        addEntry(entry, folder);
        entry.close();
      }
      dir.close();
    }
  }

  // Sort alphabetically within each group (root entries are already grouped before subfolders).
  // For a single folder the full list is sorted; for All Songs we preserve the root-first order
  // by sorting within each segment.
  if (folder.isEmpty()) {
    // Already appended root then each folder in order; just sort root entries among themselves
    // and each folder's entries among themselves by doing a stable sort on the full list
    // by their folder prefix then filename.
    std::sort(_tracks.begin(), _tracks.end(),
      [](const String& a, const String& b) {
        // Compare folder prefix first, then filename
        int sa = a.lastIndexOf('/');
        int sb = b.lastIndexOf('/');
        String fa = (sa >= 0) ? a.substring(0, sa) : "";
        String fb = (sb >= 0) ? b.substring(0, sb) : "";
        if (fa != fb) return fa < fb;  // root ("") sorts before any named folder
        return a < b;
      });
  } else {
    std::sort(_tracks.begin(), _tracks.end(),
      [](const String& a, const String& b) { return a < b; });
  }
}
