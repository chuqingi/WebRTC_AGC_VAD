#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>


/*The WAVE file format*/
typedef struct tagWAVHEADER {
	uint8_t   ChunkID[4];     
	uint32_t  ChunkSize;      
	uint8_t   Format[4];      
	uint8_t   FmtChunkID[4];  
	uint32_t  FmtChunkSize;   
	uint16_t  AudioFormat;    
	uint16_t  NumChannels;    
	uint32_t  SampleRate;     
	uint32_t  ByteRate;       
	uint16_t  BlockAlign;     
	uint16_t  BitsPerSample;
	uint8_t   DataChunkID[4];
	uint32_t  DataChunkSize;
} WAVHEADER;

/*Init*/
typedef struct {
  int32_t downState[8];
  int16_t HPstate;
  int16_t counter;
  int16_t logRatio;           // log( P(active) / P(inactive) ) (Q10)
  int16_t meanLongTerm;       // Q10
  int32_t varianceLongTerm;   // Q8
  int16_t stdLongTerm;        // Q10
  int16_t vadThreshold;
  int16_t vad_flage;
  int16_t vad_result[160];
} AgcVad;


static const int16_t kAvgDecayTime = 250;  // frames; < 3000
static const int16_t kNormalVadThreshold = 400;
// allpass filter coefficients.
static const uint16_t kResampleAllpass1[3] = {3284, 24441, 49528};
static const uint16_t kResampleAllpass2[3] = {12199, 37471, 60255};

// C + the 32 most significant bits of A * B
#define WEBRTC_SPL_SCALEDIFF32(A, B, C) (C + (B >> 16) * A + (((uint32_t)(B & 0x0000FFFF) * A) >> 16))
#define WEBRTC_SPL_MUL_16_U16(a, b) ((int32_t)(int16_t)(a) * (uint16_t)(b))
// Multiply a 32-bit value with a 16-bit value and accumulate to another input:
#define MUL_ACCUM_1(a, b, c) WEBRTC_SPL_SCALEDIFF32(a, b, c)
#define MUL_ACCUM_2(a, b, c) WEBRTC_SPL_SCALEDIFF32(a, b, c)
#define WEBRTC_SPL_SQRT_ITER(N)                 \
  try1 = root + (1 << (N));                     \
  if (value >= try1 << (N)) {                   \
    value -= try1 << (N);                       \
    root |= 2 << (N);                           \
  }


int32_t WebRtcSpl_SqrtFloor(int32_t value);
int16_t WebRtcSpl_SatW32ToW16(int32_t value32);
int16_t WebRtcSpl_AddSatW16(int16_t a, int16_t b);
int16_t WebRtcSpl_DivW32W16ResW16(int32_t num, int16_t den);
int32_t WebRtcSpl_DivW32W16(int32_t num, int16_t den);
void WebRtcSpl_DownsampleBy2(const int16_t* in, size_t len, int16_t* out, int32_t* filtState);
void WebRtcAgc_InitVad(AgcVad* vadInst);
void WebRtcAgc_ProcessVad(AgcVad* vadInst, const int16_t* in, size_t nrSamples);
void WebRtcAgc_SpeakerInactiveCtrl(AgcVad* state);
void Agc_Vad(AgcVad* state, const int16_t* readbuf, size_t nrSamples);
void De_Init(AgcVad* state);