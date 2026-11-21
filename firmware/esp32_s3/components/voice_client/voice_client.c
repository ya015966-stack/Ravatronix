#include "voice_client.h"
#include "audio_service.h"
#include "wav_codec.h"
#include "network_transport.h"
#include "esp_heap_caps.h"
#include "esp_timer.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <stdio.h>
#include <stdbool.h>
#include <string.h>

#define TAG "rava_voice"
#define MAX_RECORD_MS 8000U
#define MIN_RECORD_MS 250U
#define MAX_REQUEST_TIMEOUT_MS 90000U
#define MAX_RESPONSE_WAV_BYTES (1024U * 1024U)
#define MIC_BLOCK_SAMPLES 256U
#define HTTP_READ_CHUNK 1024U
#define MAX_AUTH_HEADER 384U

static esp_err_t capture_pcm16(uint8_t *wav,size_t capacity,const voice_client_config_t *config,uint32_t *captured)
{
    uint32_t target=(RAVA_WAV_SAMPLE_RATE*config->record_ms)/1000U;
    size_t pcm_bytes=(size_t)target*sizeof(int16_t);
    if(capacity<RAVA_WAV_HEADER_BYTES+pcm_bytes) return ESP_ERR_INVALID_SIZE;
    int32_t raw[MIC_BLOCK_SAMPLES];
    int16_t converted[MIC_BLOCK_SAMPLES];
    uint32_t total=0;
    int64_t deadline=esp_timer_get_time()+((int64_t)config->record_ms+2500LL)*1000LL;
    while(total<target) {
        if(esp_timer_get_time()>deadline) return ESP_ERR_TIMEOUT;
        size_t got=0,wanted=target-total;
        if(wanted>MIC_BLOCK_SAMPLES) wanted=MIC_BLOCK_SAMPLES;
        esp_err_t err=audio_service_read_pcm(raw,wanted,&got,500U);
        if(err==ESP_ERR_TIMEOUT) continue;
        if(err!=ESP_OK) return err;
        if(got==0 || got>wanted) continue;
        for(size_t i=0;i<got;i++) converted[i]=rava_pcm32_to_pcm16(raw[i],config->mic_right_shift);
        memcpy(wav+RAVA_WAV_HEADER_BYTES+(size_t)total*sizeof(int16_t),converted,got*sizeof(int16_t));
        total+=(uint32_t)got;
    }
    if(!rava_wav_write_pcm16_mono_16k_header(wav,(uint32_t)pcm_bytes)) return ESP_ERR_INVALID_SIZE;
    *captured=total;
    return ESP_OK;
}

static esp_err_t play_response(const uint8_t *wav,size_t wav_bytes,uint32_t *audio_bytes)
{
    size_t data_offset=0;
    uint32_t data_bytes=0;
    if(!rava_wav_parse_pcm16_mono_16k(wav,wav_bytes,&data_offset,&data_bytes)) return ESP_ERR_INVALID_RESPONSE;
    const int16_t *pcm=(const int16_t *)(const void *)(wav+data_offset);
    size_t total=data_bytes/sizeof(int16_t),offset=0;
    while(offset<total) {
        size_t chunk=total-offset;
        if(chunk>MIC_BLOCK_SAMPLES) chunk=MIC_BLOCK_SAMPLES;
        size_t written=0;
        esp_err_t err=audio_service_write_pcm(pcm+offset,chunk,&written,1500U);
        if(err!=ESP_OK) return err;
        if(written==0 || written>chunk) return ESP_FAIL;
        offset+=written;
    }
    *audio_bytes=data_bytes;
    return ESP_OK;
}

esp_err_t voice_client_turn(const voice_client_config_t *config,voice_client_result_t *result)
{
    if(!config || !result || !config->https_url || !config->bearer_token ||
       config->https_url[0]=='\0' || config->bearer_token[0]=='\0' ||
       strncmp(config->https_url,"https://",8)!=0 ||
       config->record_ms<MIN_RECORD_MS || config->record_ms>MAX_RECORD_MS ||
       config->request_timeout_ms<1000U || config->request_timeout_ms>MAX_REQUEST_TIMEOUT_MS ||
       config->mic_right_shift>31U) return ESP_ERR_INVALID_ARG;

    memset(result,0,sizeof(*result));
    esp_err_t err=audio_service_init();
    if(err!=ESP_OK) return err;

    uint32_t samples=(RAVA_WAV_SAMPLE_RATE*config->record_ms)/1000U;
    size_t request_bytes=RAVA_WAV_HEADER_BYTES+(size_t)samples*sizeof(int16_t);
    uint8_t *request_wav=heap_caps_malloc(request_bytes,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);
    uint8_t *response_wav=heap_caps_malloc(MAX_RESPONSE_WAV_BYTES,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);
    if(!request_wav || !response_wav) {
        if(request_wav) heap_caps_free(request_wav);
        if(response_wav) heap_caps_free(response_wav);
        return ESP_ERR_NO_MEM;
    }

    uint32_t captured=0;
    err=capture_pcm16(request_wav,request_bytes,config,&captured);
    if(err!=ESP_OK) goto cleanup;
    result->captured_samples=captured;

    char authorization[MAX_AUTH_HEADER];
    int auth_len=snprintf(authorization,sizeof(authorization),"Bearer %s",config->bearer_token);
    if(auth_len<0 || (size_t)auth_len>=sizeof(authorization)) { err=ESP_ERR_INVALID_SIZE; goto cleanup; }

    network_transport_handle_t transport=NULL;
    err=network_transport_open(config->https_url,config->request_timeout_ms,
                               (uint32_t)request_bytes,"audio/wav",authorization,&transport);
    if(err!=ESP_OK) goto cleanup;

    size_t request_offset=0;
    while(request_offset<request_bytes) {
        size_t written=0;
        err=network_transport_write(transport,request_wav+request_offset,
                                    request_bytes-request_offset,&written);
        if(err!=ESP_OK || written==0 || written>request_bytes-request_offset) {
            if(err==ESP_OK) err=ESP_FAIL;
            break;
        }
        request_offset+=written;
    }

    int64_t content_length=-1;
    if(err==ESP_OK) {
        err=network_transport_finish(transport,&result->http_status,&content_length);
        if(err==ESP_OK && result->http_status!=200) err=ESP_FAIL;
    }

    if(err==ESP_OK) {
        if(content_length>(int64_t)MAX_RESPONSE_WAV_BYTES || content_length==0) {
            err=ESP_ERR_INVALID_SIZE;
        } else {
            size_t received=0;
            while(received<MAX_RESPONSE_WAV_BYTES &&
                  (content_length<0 || received<(size_t)content_length)) {
                size_t room=MAX_RESPONSE_WAV_BYTES-received;
                size_t ask=room>HTTP_READ_CHUNK ? HTTP_READ_CHUNK : room;
                size_t got=0;
                err=network_transport_read(transport,response_wav+received,ask,&got);
                if(err!=ESP_OK) break;
                if(got==0) break;
                if(got>ask) { err=ESP_ERR_INVALID_SIZE; break; }
                received+=got;
            }
            if(err==ESP_OK && (received==0 ||
               (content_length>=0 && received!=(size_t)content_length))) err=ESP_ERR_INVALID_SIZE;
            if(err==ESP_OK) err=play_response(response_wav,received,&result->response_audio_bytes);
        }
    }
    network_transport_close(transport);

cleanup:
    heap_caps_free(request_wav);
    heap_caps_free(response_wav);
    if(err==ESP_OK) ESP_LOGI(TAG,"voice turn complete: captured=%u samples, response=%u PCM bytes",
                             (unsigned)result->captured_samples,(unsigned)result->response_audio_bytes);
    else ESP_LOGW(TAG,"voice turn failed: %s (HTTP %d)",esp_err_to_name(err),result->http_status);
    return err;
}
