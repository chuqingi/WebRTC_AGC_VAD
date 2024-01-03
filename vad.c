#include "vad.h"


int32_t WebRtcSpl_SqrtFloor(int32_t value) {
  int32_t root = 0, try1;

  WEBRTC_SPL_SQRT_ITER (15);
  WEBRTC_SPL_SQRT_ITER (14);
  WEBRTC_SPL_SQRT_ITER (13);
  WEBRTC_SPL_SQRT_ITER (12);
  WEBRTC_SPL_SQRT_ITER (11);
  WEBRTC_SPL_SQRT_ITER (10);
  WEBRTC_SPL_SQRT_ITER ( 9);
  WEBRTC_SPL_SQRT_ITER ( 8);
  WEBRTC_SPL_SQRT_ITER ( 7);
  WEBRTC_SPL_SQRT_ITER ( 6);
  WEBRTC_SPL_SQRT_ITER ( 5);
  WEBRTC_SPL_SQRT_ITER ( 4);
  WEBRTC_SPL_SQRT_ITER ( 3);
  WEBRTC_SPL_SQRT_ITER ( 2);
  WEBRTC_SPL_SQRT_ITER ( 1);
  WEBRTC_SPL_SQRT_ITER ( 0);

  return root >> 1;
}

void WebRtcSpl_DownsampleBy2(const int16_t* in, size_t len, int16_t* out, int32_t* filtState) {
  int32_t tmp1, tmp2, diff, in32, out32;
  size_t i;

  register int32_t state0 = filtState[0];
  register int32_t state1 = filtState[1];
  register int32_t state2 = filtState[2];
  register int32_t state3 = filtState[3];
  register int32_t state4 = filtState[4];
  register int32_t state5 = filtState[5];
  register int32_t state6 = filtState[6];
  register int32_t state7 = filtState[7];

  for (i = (len >> 1); i > 0; i--) {
    // lower allpass filter
    in32 = (int32_t)(*in++) * (1 << 10);
    diff = in32 - state1;
    tmp1 = MUL_ACCUM_1(kResampleAllpass2[0], diff, state0);
    state0 = in32;
    diff = tmp1 - state2;
    tmp2 = MUL_ACCUM_2(kResampleAllpass2[1], diff, state1);
    state1 = tmp1;
    diff = tmp2 - state3;
    state3 = MUL_ACCUM_2(kResampleAllpass2[2], diff, state2);
    state2 = tmp2;
    // upper allpass filter
    in32 = (int32_t)(*in++) * (1 << 10);
    diff = in32 - state5;
    tmp1 = MUL_ACCUM_1(kResampleAllpass1[0], diff, state4);
    state4 = in32;
    diff = tmp1 - state6;
    tmp2 = MUL_ACCUM_1(kResampleAllpass1[1], diff, state5);
    state5 = tmp1;
    diff = tmp2 - state7;
    state7 = MUL_ACCUM_2(kResampleAllpass1[2], diff, state6);
    state6 = tmp2;
    // add two allpass outputs, divide by two and round
    out32 = (state3 + state7 + 1024) >> 11;
    // limit amplitude to prevent wrap-around, and write to output array
    *out++ = WebRtcSpl_SatW32ToW16(out32);
  }

  filtState[0] = state0;
  filtState[1] = state1;
  filtState[2] = state2;
  filtState[3] = state3;
  filtState[4] = state4;
  filtState[5] = state5;
  filtState[6] = state6;
  filtState[7] = state7;
}

int16_t WebRtcSpl_SatW32ToW16(int32_t value32) {
  int16_t out16 = (int16_t)value32;
  if (value32 > 32767) {
    out16 = 32767;
  } else if (value32 < -32768) {
    out16 = -32768;
  }
  return out16;
}

int16_t WebRtcSpl_AddSatW16(int16_t a, int16_t b) {
  return WebRtcSpl_SatW32ToW16((int32_t)a + (int32_t)b);
}

int16_t WebRtcSpl_DivW32W16ResW16(int32_t num, int16_t den) {
  // Guard against division with 0
  if (den != 0) {
    return (int16_t)(num / den);
  } else {
    return (int16_t)0x7FFF;
  }
}

int32_t WebRtcSpl_DivW32W16(int32_t num, int16_t den) {
  // Guard against division with 0
  if (den != 0) {
    return (int32_t)(num / den);
  } else {
    return (int32_t)0x7FFFFFFF;
  }
}

void WebRtcAgc_ProcessVad(AgcVad* state, const int16_t* in, size_t nrSamples) {
  uint32_t nrg;
  int32_t out, tmp32, tmp32b;
  uint16_t tmpU16;
  int16_t k, subfr, tmp16;
  int16_t buf1[8];
  int16_t buf2[4];
  int16_t HPstate;
  int16_t zeros, dB;
  int64_t tmp64;

  // process in 10 sub frames of 1 ms (to save on memory)
  nrg = 0;
  HPstate = state->HPstate;
  for (subfr = 0; subfr < 10; subfr++) {
    // downsample to 4 kHz
    if (nrSamples == 160) {
      // 16k -> 4k
      for (k = 0; k < 8; k++) {
        tmp32 = (int32_t)in[2 * k] + (int32_t)in[2 * k + 1];
        tmp32 >>= 1;
        buf1[k] = (int16_t)tmp32;
      }
      in += 16;
      WebRtcSpl_DownsampleBy2(buf1, 8, buf2, state->downState);
    } else {
      // 8k -> 4k
      WebRtcSpl_DownsampleBy2(in, 8, buf2, state->downState);
      in += 8;
    }
    // high pass filter and compute energy
    for (k = 0; k < 4; k++) {
      out = buf2[k] + HPstate;
      // 600? debug?
      tmp32 = 600 * out;
      HPstate = (int16_t)((tmp32 >> 10) - buf2[k]);
      // Add 'out * out / 2**6' to 'nrg' in a non-overflowing
      // way. Guaranteed to work as long as 'out * out / 2**6' fits in
      // an int32_t.
      nrg += out * (out / (1 << 6));
      nrg += out * (out % (1 << 6)) / (1 << 6);
    }
  }
  state->HPstate = HPstate;

  // find number of leading zeros
  if (!(0xFFFF0000 & nrg)) {
    zeros = 16;
  } else {
    zeros = 0;
  }
  if (!(0xFF000000 & (nrg << zeros))) {
    zeros += 8;
  }
  if (!(0xF0000000 & (nrg << zeros))) {
    zeros += 4;
  }
  if (!(0xC0000000 & (nrg << zeros))) {
    zeros += 2;
  }
  if (!(0x80000000 & (nrg << zeros))) {
    zeros += 1;
  }

  // energy level (range {-32..30}) (Q10) 
  dB = (15 - zeros) * (1 << 11);   //  (15 - zeros) * 2 *  (1 << 10)  zeros(range {0..31})

  // Update statistics
  if (state->counter < kAvgDecayTime) {
    // decay time = AvgDecTime * 10 ms
    state->counter++;
  }

  // update long-term estimate of mean energy level (Q10)
  // state->meanLongTerm = state->counter/(state->counter+1)*state->meanLongTerm + 1/(state->counter+1)*dB
  tmp32 = state->meanLongTerm * state->counter + dB;
  state->meanLongTerm = WebRtcSpl_DivW32W16ResW16(tmp32, WebRtcSpl_AddSatW16(state->counter, 1));

  // update long-term estimate of variance in energy level (Q8)
  // state->varianceLongTerm = state->counter/(state->counter+1)*state->varianceLongTerm + 1/(state->counter+1)*((dB*dB) >> 12)
  tmp32 = (dB * dB) >> 12;
  tmp32 += state->varianceLongTerm * state->counter;
  state->varianceLongTerm = WebRtcSpl_DivW32W16(tmp32, WebRtcSpl_AddSatW16(state->counter, 1));

  // update long-term estimate of standard deviation in energy level (Q10)
  // state->stdLongTerm = sqrt((state->varianceLongTerm << 12) - state->meanLongTerm*state->meanLongTerm)
  tmp32 = state->meanLongTerm * state->meanLongTerm;
  tmp32 = (state->varianceLongTerm << 12) - tmp32;
  state->stdLongTerm = (int16_t)WebRtcSpl_SqrtFloor(tmp32);

  // update voice activity measure (Q10)
  // 16 => 1 << 4
  // state->logRatio = ((((13/16*state->logRatio) << 12) >> 10 + 3/16*(dB-state->meanLongTerm)/state->stdLongTerm << 12) >> 6) << 4
  tmp16 = 3 << 12;
  tmp32 = ((int32_t)dB - state->meanLongTerm) * tmp16;
  tmp32 = WebRtcSpl_DivW32W16(tmp32, state->stdLongTerm);
  tmpU16 = (13 << 12);
  tmp32b = WEBRTC_SPL_MUL_16_U16(state->logRatio, tmpU16);
  tmp64 = tmp32;
  tmp64 += tmp32b >> 10;
  tmp64 >>= 6;

  // limit
  if (tmp64 > 2048) {
    tmp64 = 2048;
  } else if (tmp64 < -2048) {
    tmp64 = -2048;
  }
  state->logRatio = (int16_t)tmp64;  // Q10
}

void WebRtcAgc_SpeakerInactiveCtrl(AgcVad* state) {
  /* Check if the near end speaker is inactive.
   * If that is the case the VAD threshold is
   * increased since the VAD speech model gets
   * more sensitive to any sound after a long
   * silence.
  */
  int32_t tmp32;
  int16_t vadThresh;

  if (state->stdLongTerm < 2500) {
    state->vadThreshold = 1500;
  } else {
    vadThresh = kNormalVadThreshold;
    if (state->stdLongTerm < 4500) {
      /* Scale between min and max threshold */
      vadThresh += (4500 - state->stdLongTerm) / 2;
    }
    /* state->vadThreshold = (31 * state->vadThreshold + vadThresh) / 32; */
    tmp32 = vadThresh + 31 * state->vadThreshold;
    state->vadThreshold = (int16_t)(tmp32 >> 5);
  }
}

void WebRtcAgc_InitVad(AgcVad* state) {
  int16_t k;
  state->HPstate = 0;   // state of high pass filter
  state->logRatio = 0;  // log( P(active) / P(inactive) )
  // average input level (Q10)
  state->meanLongTerm = 15 << 10;
  // variance of input level (Q8)
  state->varianceLongTerm = 500 << 8;
  state->stdLongTerm = 0;  // standard deviation of input level in dB
  state->counter = 3;  // counts updates
  for (k = 0; k < 8; k++) {
    // downsampling filter
    state->downState[k] = 0;
  }
  state->vad_flage = 0;
  state->vadThreshold = kNormalVadThreshold;
}

void De_Init(AgcVad* state) {
  free(state);
}

void Agc_Vad(AgcVad* state, const int16_t* readbuf, size_t nrSamples) {
  WebRtcAgc_ProcessVad(state, readbuf, nrSamples);
  WebRtcAgc_SpeakerInactiveCtrl(state);
  if (state->logRatio >= state->vadThreshold) {
    state->vad_flage = 1;
  } else {
    state->vad_flage = 0;
  }
}