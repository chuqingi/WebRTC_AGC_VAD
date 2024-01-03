# WebRTC_AGC_VAD
C implementation of WebRTC_AGC_VAD

## Table of contents
1. [Development environments](#dev_env)
2. [Usages](#usage)

## Development environments <a name="dev_env"></a>
* Coding language: C
* IDE: VSCode
* Extensions: C/C++

## Usages <a name="usage"></a>
* Input: input.wav (Sampling rate: only 8k or 16k; Bit Depth: only 16bit)
* Output: vad_result.wav (vad result)
```c
cd ~/WebRTC_AGC_VAD
./ gcc ./*.c -o main_test
./ main_test.exe input.wav 
```
