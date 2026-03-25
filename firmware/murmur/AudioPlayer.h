#pragma once
#include <Arduino.h>
#include <SPI.h>
#include <SD.h>
#include <vector>
#include <algorithm>

#include "AudioTools.h"
#include "AudioTools/AudioCodecs/CodecMP3Helix.h"
#include "config.h"
#include "ID3Parser.h"

// ── Forward declarations of pipeline types ─────────────────────────────────
using I2SStream_t      = I2SStream;
using VolumeStream_t   = VolumeStream;
using EncodedAudioStream_t = EncodedAudioStream;
using NumberFormatConverterStream_t = NumberFormatConverterStream;
using AudioStreamPipeline_t = StreamCopy;  // used internally

class MurmurPlayer : public AudioInfoSupport {
public:
  MurmurPlayer(uint8_t sdCS, uint8_t spiSck, uint8_t spiMiso, uint8_t spiMosi,
              uint8_t i2sBclk, uint8_t i2sFsync, uint8_t i2sDout);

  // Returns false if SD mount or audio init fails.
  bool begin();

  // AudioInfoSupport — invoked by the decoder when stream properties are known.
  void      setAudioInfo(AudioInfo info) override;
  AudioInfo audioInfo()                 override { return _streamInfo; }

  // Call every loop() — feeds the audio pipeline.
  void update();

  // Playback control
  void playTrack(int index);   // 0-based
  void next();
  void prev();
  void togglePause();

  // Graceful shutdown: stop playback, de-init I2S DMA, unmount SD, release SPI.
  // After this call the object is unusable until the device restarts.
  void shutdown();

  // Volume  0.0 – 1.0
  void     setVolume(float v);
  float    getVolume() const { return _volumeLevel; }

  // EQ preset
  void setEQ(EqMode mode);

  // Repeat mode
  void       setRepeatMode(RepeatMode mode) { _repeatMode = mode; }
  RepeatMode getRepeatMode()          const { return _repeatMode; }

  // Returns true (once) when playback stopped at end-of-playlist in OFF repeat mode.
  bool stoppedAtEnd();

  // Track info
  int           trackCount()    const { return (int)_tracks.size(); }
  int           currentIndex()  const { return _currentIndex; }
  String        currentName()   const;  // basename only (no folder path, with extension)
  bool          isPlaying()     const { return _playing; }

  // Audiobook support
  bool                            isAudiobook()          const;
  const std::vector<ChapterInfo>& getChapters()          const { return _id3.chapters; }
  int                             getCurrentChapterIndex() const;
  String                          getChapterName()       const;
  size_t                          getFilePosition()      const;
  void                            seekToChapter(int chapterIndex);
  void                            seekToPosition(size_t byteOffset);

  // Playlist (folder) support
  // Calling setPlaylist("") restores "All Songs" mode.
  void                       setPlaylist(const String& folder);
  const String&              getPlaylistName()  const { return _activePlaylist; }
  const std::vector<String>& getFolders()       const { return _folders; }

private:
  // Hardware pins
  uint8_t _sdCS;
  uint8_t _spiSck, _spiMiso, _spiMosi;
  uint8_t _bclk, _fsync, _dout;

  // Audio pipeline objects
  I2SStream                   _i2s;
  VolumeStream                _volume;
  NumberFormatConverterStream _bits32;
  Equalizer3Bands*            _eq      = nullptr;  // heap-alloc'd after _bits32 is ready
  ConfigEqualizer3Bands       _eqConfig;
  EncodedAudioStream          _decoder;   // MP3 → PCM
  StreamCopy                  _copier;    // decoder → pipeline

  // Playback state
  std::vector<String> _tracks;
  std::vector<String> _folders;        // top-level SD directories (populated once in begin())
  String              _activePlaylist; // "" = All Songs; folder name otherwise
  int        _currentIndex = 0;
  float      _volumeLevel  = 0.5f;
  bool       _playing      = false;
  bool       _stoppedAtEnd = false;
  RepeatMode _repeatMode   = RepeatMode::ALL;
  File       _file;
  AudioInfo  _streamInfo;     // last detected stream properties (sample_rate, channels)
  ID3Metadata _id3;           // parsed ID3 metadata for current track

  void _initPipeline();
  void _openTrack(int index, size_t resumeOffset = 0);
  void _applyStreamRate(uint32_t sampleRate, uint8_t channels);
  void _closeTrack();
  void _collectTracks(const String& folder = "");  // "" = all songs (root + subfolders)
  void _collectFolders();                           // populate _folders once
};
