/****************************************************************************
Developer: Jiaxin Li
E-mail: 1319376761@qq.com
Github: https://github.com/chuqingi/WebRTC_AGC_VAD
****************************************************************************/
#include "vad.h"
#define VADRESULT "vad_result.wav"


void Vad_Main(AgcVad* state, int32_t fs, FILE* f1, FILE* f2) {
  size_t readbuf_len = (int16_t)(0.01 * fs);  // it must be 10 ms
  int16_t* readbuf = (int16_t*)malloc(readbuf_len * sizeof(int16_t));
  int16_t i;
  int read;
  while (1) {
    read = fread(readbuf, sizeof(int16_t), readbuf_len, f1);
    if (read != readbuf_len) {
      break;
    }
    Agc_Vad(state, readbuf, readbuf_len);
    for (i = 0; i < readbuf_len; i++) {
      state->vad_result[i] = state->vad_flage * 32767;
    }
    fwrite(state->vad_result, sizeof(int16_t), readbuf_len, f2);
  }
  free(readbuf);
}

int main(int argc, char** argv) {
  WAVHEADER* WAVE = malloc(sizeof(WAVHEADER));
  AgcVad* state = malloc(sizeof(AgcVad));
	if (argc != 2) {
		printf("usage: %s input.wav\n", argv[0]);
    exit(1);
	}
	FILE* f1 = fopen(argv[1], "rb");
	if (!f1) {
		printf("input open is failed!\n");
    exit(1);
	}
	fread(WAVE, sizeof(WAVHEADER), 1, f1);
	int32_t fs = WAVE->SampleRate;
  printf("fs = %d\n", fs);
  if (fs != 16000 && fs != 8000) {
		printf("unsupported samplerate!\n");
    exit(1);
  }
	FILE* f2 = fopen(VADRESULT, "wb");
  fwrite(WAVE, sizeof(WAVHEADER), 1, f2);

  WebRtcAgc_InitVad(state);
  Vad_Main(state, fs, f1, f2);
  De_Init(state);

  free(WAVE);
	fclose(f1);
	fclose(f2);
	return 0;
}