// Copyright 2012 Emilie Gillet.
//
// Author: Emilie Gillet (emilie.o.gillet@gmail.com)
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.

#ifndef TDM_ENVELOPE_H_
#define TDM_ENVELOPE_H_

#include "audio/dsp.h"
#include "audio/resources.h"
#include "pico/stdlib.h"
namespace braids {

using namespace stmlib;

enum ADSREnvelopeSegment {
  ADSR_ENV_SEGMENT_ATTACK = 0,
  ADSR_ENV_SEGMENT_DECAY = 1,
  ADSR_ENV_SEGMENT_DEAD = 2,
  ADSR_ENV_NUM_SEGMENTS,
};

class ADSREnvelope {
 public:
  ADSREnvelope() { }
  ~ADSREnvelope() { }

  void Init() {
    target_[ADSR_ENV_SEGMENT_ATTACK] = 65535;
    target_[ADSR_ENV_SEGMENT_DECAY] = 0;
    target_[ADSR_ENV_SEGMENT_DEAD] = 0;
    increment_[ADSR_ENV_SEGMENT_DEAD] = 0;
    priorValue_ = 0;
  }

  inline ADSREnvelopeSegment segment() const {
    return static_cast<ADSREnvelopeSegment>(segment_);
  }

  inline void Update(int32_t a, int32_t d) {
    increment_[ADSR_ENV_SEGMENT_ATTACK] = lut_env_portamento_increments[a];
    increment_[ADSR_ENV_SEGMENT_DECAY] = lut_env_portamento_increments[d];
  }
  
  inline void Trigger(ADSREnvelopeSegment segment) {
    if (segment == ADSR_ENV_SEGMENT_DEAD) {
      value_ = 0;
    }
    a_ = value_;
    b_ = target_[segment];
    priorValue_ = value_;
    segment_ = segment;
    phase_ = 0;
  }

  inline uint16_t Render() {
    uint32_t increment = increment_[segment_]>>7;
    phase_ += increment;
    if (phase_ < increment) {
      value_ = Mix(a_, b_, 65535);
      Trigger(static_cast<ADSREnvelopeSegment>(segment_ + 1));
    }
    if (increment_[segment_]) {
      value_ = Mix(a_, b_, Interpolate824(lut_env_expo, phase_));
    }
    return value_;
  }
  // returns the last exponential value
  inline uint16_t value() const { return value_; }
  inline uint16_t valueLin() const {
    return Mix(a_, b_, phase_>>16);
  }

 private:
  // Phase increments for each segment.
  uint32_t increment_[ADSR_ENV_NUM_SEGMENTS];
  
  // Value that needs to be reached at the end of each segment.
  uint16_t target_[ADSR_ENV_NUM_SEGMENTS];
  
  // Current segment.
  size_t segment_;
  
  // Start and end value of the current segment.
  uint16_t a_;
  uint16_t b_;
  uint16_t value_;
  uint16_t priorValue_;
  uint32_t phase_;

  DISALLOW_COPY_AND_ASSIGN(ADSREnvelope);
};

}  // namespace braids

#endif  // BRAIDS_ENVELOPE_H_
