#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define MODEM_AT_LINE_CAPACITY 256U

typedef enum {
    MODEM_AT_EVENT_NONE = 0,
    MODEM_AT_EVENT_LINE,
    MODEM_AT_EVENT_PROMPT,
    MODEM_AT_EVENT_OVERFLOW
} modem_at_event_t;

typedef struct {
    char line[MODEM_AT_LINE_CAPACITY];
    size_t length;
    bool overflow;
    bool swallow_lf;
    bool prompt_space_pending;
} modem_at_parser_t;

void modem_at_parser_init(modem_at_parser_t *parser);

/*
 * Feed one byte from the UART stream.
 * LINE returns a NUL-terminated response line in out_line.
 * PROMPT reports the modem's raw-data prompt ('>').
 * OVERFLOW reports a line longer than the bounded parser buffer.
 * Binary payload bytes must be consumed by the transport's explicit
 * length-delimited raw-data state, not passed to this line parser.
 */
modem_at_event_t modem_at_parser_feed(
    modem_at_parser_t *parser,
    uint8_t byte,
    char *out_line,
    size_t out_capacity
);

#ifdef __cplusplus
}
#endif
