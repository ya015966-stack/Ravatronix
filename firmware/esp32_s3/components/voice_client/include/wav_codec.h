#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#define RAVA_WAV_HEADER_BYTES 44U
#define RAVA_WAV_SAMPLE_RATE 16000U
#define RAVA_WAV_CHANNELS 1U
#define RAVA_WAV_BITS_PER_SAMPLE 16U
bool rava_wav_write_pcm16_mono_16k_header(uint8_t header[RAVA_WAV_HEADER_BYTES], uint32_t data_bytes);
bool rava_wav_parse_pcm16_mono_16k(const uint8_t *wav, size_t wav_bytes, size_t *data_offset, uint32_t *data_bytes);
int16_t rava_pcm32_to_pcm16(int32_t sample, unsigned right_shift);
