#include "wav_codec.h"
#include <limits.h>
#include <string.h>
static void put_u16le(uint8_t *p, uint16_t v) { p[0]=(uint8_t)v; p[1]=(uint8_t)(v>>8); }
static void put_u32le(uint8_t *p, uint32_t v) { p[0]=(uint8_t)v; p[1]=(uint8_t)(v>>8); p[2]=(uint8_t)(v>>16); p[3]=(uint8_t)(v>>24); }
static uint16_t get_u16le(const uint8_t *p) { return (uint16_t)p[0] | ((uint16_t)p[1]<<8); }
static uint32_t get_u32le(const uint8_t *p) { return (uint32_t)p[0] | ((uint32_t)p[1]<<8) | ((uint32_t)p[2]<<16) | ((uint32_t)p[3]<<24); }

bool rava_wav_write_pcm16_mono_16k_header(uint8_t h[RAVA_WAV_HEADER_BYTES], uint32_t data_bytes)
{
    if (!h || data_bytes == 0 || (data_bytes & 1U) != 0 || data_bytes > UINT32_MAX-36U) return false;
    memset(h,0,RAVA_WAV_HEADER_BYTES);
    memcpy(h,"RIFF",4); put_u32le(h+4,data_bytes+36U); memcpy(h+8,"WAVE",4);
    memcpy(h+12,"fmt ",4); put_u32le(h+16,16U); put_u16le(h+20,1U);
    put_u16le(h+22,RAVA_WAV_CHANNELS); put_u32le(h+24,RAVA_WAV_SAMPLE_RATE);
    put_u32le(h+28,RAVA_WAV_SAMPLE_RATE*2U); put_u16le(h+32,2U);
    put_u16le(h+34,RAVA_WAV_BITS_PER_SAMPLE); memcpy(h+36,"data",4);
    put_u32le(h+40,data_bytes);
    return true;
}

bool rava_wav_parse_pcm16_mono_16k(const uint8_t *wav,size_t wav_bytes,size_t *data_offset,uint32_t *data_bytes)
{
    if (!wav || !data_offset || !data_bytes || wav_bytes<RAVA_WAV_HEADER_BYTES) return false;
    if (memcmp(wav,"RIFF",4)!=0 || memcmp(wav+8,"WAVE",4)!=0) return false;
    if ((uint64_t)get_u32le(wav+4)+8U != wav_bytes) return false;
    size_t offset=12U, payload_offset=0;
    bool have_fmt=false,have_data=false;
    uint16_t encoding=0,channels=0,block_align=0,bits=0;
    uint32_t sample_rate=0,byte_rate=0,payload_size=0;
    while (offset+8U<=wav_bytes) {
        uint32_t chunk_size=get_u32le(wav+offset+4U);
        size_t chunk_start=offset+8U;
        uint64_t end64=(uint64_t)chunk_start+chunk_size;
        if (end64>wav_bytes) return false;
        size_t chunk_end=(size_t)end64;
        if (memcmp(wav+offset,"fmt ",4)==0) {
            if (chunk_size<16U) return false;
            encoding=get_u16le(wav+chunk_start); channels=get_u16le(wav+chunk_start+2U);
            sample_rate=get_u32le(wav+chunk_start+4U); byte_rate=get_u32le(wav+chunk_start+8U);
            block_align=get_u16le(wav+chunk_start+12U); bits=get_u16le(wav+chunk_start+14U);
            have_fmt=true;
        } else if (memcmp(wav+offset,"data",4)==0) {
            payload_size=chunk_size; payload_offset=chunk_start; have_data=true;
        }
        offset=chunk_end+(chunk_size&1U);
        if (offset>wav_bytes) return false;
    }
    if (!have_fmt || !have_data || encoding!=1U || channels!=1U ||
        sample_rate!=RAVA_WAV_SAMPLE_RATE || byte_rate!=32000U ||
        block_align!=2U || bits!=RAVA_WAV_BITS_PER_SAMPLE ||
        payload_size==0U || (payload_size&1U)!=0U ||
        payload_offset+(size_t)payload_size!=wav_bytes) return false;
    *data_offset=payload_offset; *data_bytes=payload_size;
    return true;
}

int16_t rava_pcm32_to_pcm16(int32_t sample,unsigned right_shift)
{
    if (right_shift>31U) right_shift=31U;
    int64_t scaled=(int64_t)sample/((int64_t)1<<right_shift);
    if (scaled>INT16_MAX) scaled=INT16_MAX;
    if (scaled<INT16_MIN) scaled=INT16_MIN;
    return (int16_t)scaled;
}
