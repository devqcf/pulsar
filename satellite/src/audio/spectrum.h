// Music features from the speaker reference channels of the capture (what the station itself
// plays, after the softvol): 8 bands, loudness, beats. One VQE chunk (256 or 512 samples at 16 kHz
// after decimation) per call; runs on the frontend's processing thread.
#pragma once

#include <array>
#include <complex>
#include <cstddef>
#include <vector>

#include "audio/music_frame.h"

namespace cs {

class SpectrumAnalyzer {
 public:
  SpectrumAnalyzer(unsigned rate, size_t chunk_frames);

  // False when the chunk is not 256 or 512 samples at 16 kHz (rate a multiple of 16 kHz).
  bool ok() const { return n_ != 0; }
  void reset();
  // spk: chunk_frames frames of nspk interleaved channels in [-1, 1]. True when out was filled.
  bool process(const float *spk, size_t nspk, MusicFrame &out);

 private:
  size_t n_ = 0;
  size_t decim_ = 0;
  std::vector<float> window_;
  std::vector<std::complex<float>> twiddle_;
  std::array<size_t, 8> lo_{}, hi_{};  // band b = bins lo_[b] .. hi_[b], inclusive
  std::vector<std::complex<float>> buf_;
  float ref_db_ = 0;  // running peak, dB; set by reset()
  float bass_mean_ = 0;
  int since_beat_ = 0;
};

}  // namespace cs
