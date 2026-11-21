#include "wav_codec.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
int main(void) {
    uint8_t header[RAVA_WAV_HEADER_BYTES];
    assert(!rava_wav_write_pcm16_mono_16k_header(header, 0));
    assert(!rava_wav_write_pcm16_mono_16k_header(header, 3));
    assert(rava_wav_write_pcm16_mono_16k_header(header, 4));
    uint8_t wav[48] = {0}; memcpy(wav, header, 44);
    wav[44] = 0x01; wav[45] = 0x00; wav[46] = 0xff; wav[47] = 0x7f;
    size_t offset = 0; uint32_t bytes = 0;
    assert(rava_wav_parse_pcm16_mono_16k(wav, sizeof(wav), &offset, &bytes));
    assert(offset == 44 && bytes == 4);
    wav[22] = 2;
    assert(!rava_wav_parse_pcm16_mono_16k(wav, sizeof(wav), &offset, &bytes));
    assert(rava_pcm32_to_pcm16(0x7fffffff, 14) == INT16_MAX);
    assert(rava_pcm32_to_pcm16((int32_t)0x80000000u, 14) == INT16_MIN);
    assert(rava_pcm32_to_pcm16(16384, 14) == 1);
    puts("wav_codec: all assertions passed"); return 0;
}
