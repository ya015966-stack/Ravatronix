#include "modem_at_parser.h"

#include <string.h>

void modem_at_parser_init(modem_at_parser_t *parser)
{
    if (parser == NULL) {
        return;
    }
    memset(parser, 0, sizeof(*parser));
}

static void reset_line(modem_at_parser_t *parser)
{
    parser->length = 0U;
    parser->overflow = false;
}

modem_at_event_t modem_at_parser_feed(
    modem_at_parser_t *parser,
    uint8_t byte,
    char *out_line,
    size_t out_capacity)
{
    if (parser == NULL || out_line == NULL || out_capacity == 0U) {
        return MODEM_AT_EVENT_OVERFLOW;
    }

    if (parser->swallow_lf) {
        parser->swallow_lf = false;
        if (byte == (uint8_t)'\n') {
            return MODEM_AT_EVENT_NONE;
        }
    }

    if (parser->prompt_space_pending) {
        parser->prompt_space_pending = false;
        if (byte == (uint8_t)' ') {
            return MODEM_AT_EVENT_NONE;
        }
    }

    if (byte == (uint8_t)'>' && parser->length == 0U && !parser->overflow) {
        parser->prompt_space_pending = true;
        return MODEM_AT_EVENT_PROMPT;
    }

    if (byte == (uint8_t)'\r' || byte == (uint8_t)'\n') {
        if (byte == (uint8_t)'\r') {
            parser->swallow_lf = true;
        }
        if (parser->overflow) {
            reset_line(parser);
            return MODEM_AT_EVENT_OVERFLOW;
        }
        if (parser->length == 0U) {
            return MODEM_AT_EVENT_NONE;
        }
        if (parser->length + 1U > out_capacity) {
            reset_line(parser);
            return MODEM_AT_EVENT_OVERFLOW;
        }
        memcpy(out_line, parser->line, parser->length);
        out_line[parser->length] = '\0';
        reset_line(parser);
        return MODEM_AT_EVENT_LINE;
    }

    if (parser->overflow) {
        return MODEM_AT_EVENT_NONE;
    }

    if (parser->length >= sizeof(parser->line) - 1U) {
        parser->overflow = true;
        return MODEM_AT_EVENT_NONE;
    }

    parser->line[parser->length++] = (char)byte;
    return MODEM_AT_EVENT_NONE;
}
