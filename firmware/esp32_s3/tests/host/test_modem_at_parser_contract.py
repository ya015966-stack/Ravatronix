from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parents[2]


def test_production_at_parser_handles_fragmented_lines_prompt_and_urc(tmp_path):
    source = ROOT / "components/modem_at_service/modem_at_parser.c"
    header = ROOT / "components/modem_at_service/include/modem_at_parser.h"
    harness = tmp_path / "at_parser_test.c"
    binary = tmp_path / "at_parser_test"
    harness.write_text(r"""
#include <assert.h>
#include <string.h>
#include "modem_at_parser.h"

int main(void) {
    modem_at_parser_t parser;
    char line[MODEM_AT_LINE_CAPACITY];
    modem_at_parser_init(&parser);

    assert(modem_at_parser_feed(&parser, 'O', line, sizeof(line)) == MODEM_AT_EVENT_NONE);
    assert(modem_at_parser_feed(&parser, 'K', line, sizeof(line)) == MODEM_AT_EVENT_NONE);
    assert(modem_at_parser_feed(&parser, '\r', line, sizeof(line)) == MODEM_AT_EVENT_LINE);
    assert(strcmp(line, "OK") == 0);
    assert(modem_at_parser_feed(&parser, '\n', line, sizeof(line)) == MODEM_AT_EVENT_NONE);

    assert(modem_at_parser_feed(&parser, '>', line, sizeof(line)) == MODEM_AT_EVENT_PROMPT);
    assert(modem_at_parser_feed(&parser, ' ', line, sizeof(line)) == MODEM_AT_EVENT_NONE);

    const char *urc = "+HTTPACTION: 1,200,44\r\n";
    modem_at_event_t event = MODEM_AT_EVENT_NONE;
    for (size_t i = 0; urc[i] != '\0'; ++i) {
        event = modem_at_parser_feed(&parser, (unsigned char)urc[i], line, sizeof(line));
    }
    assert(event == MODEM_AT_EVENT_LINE);
    assert(strcmp(line, "+HTTPACTION: 1,200,44") == 0);
    return 0;
}
""", encoding="utf-8")
    subprocess.run(
        ["cc", "-std=c11", "-Wall", "-Wextra", "-Werror",
         "-I", str(header.parent), str(source), str(harness), "-o", str(binary)],
        check=True,
        capture_output=True,
        text=True,
    )
    subprocess.run([str(binary)], check=True)
