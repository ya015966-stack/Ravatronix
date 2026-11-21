from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parents[2]


def test_production_network_policy_selects_only_ready_route(tmp_path):
    source = ROOT / "components/network_manager/network_policy.c"
    header = ROOT / "components/network_manager/include/network_policy.h"
    harness = tmp_path / "policy_test.c"
    binary = tmp_path / "policy_test"
    harness.write_text(r"""
#include <assert.h>
#include <stdint.h>
#include "network_policy.h"

int main(void) {
    network_route_metrics_t wifi = { NETWORK_ROUTE_READY, 80 };
    network_route_metrics_t modem = { NETWORK_ROUTE_DOWN, 0 };
    assert(network_policy_select(NETWORK_ROUTE_NONE, &wifi, &modem, 10, true) == NETWORK_ROUTE_WIFI);
    modem.state = NETWORK_ROUTE_READY;
    modem.latency_ms = 35;
    assert(network_policy_select(NETWORK_ROUTE_WIFI, &wifi, &modem, 10, true) == NETWORK_ROUTE_MODEM);
    modem.latency_ms = 76;
    assert(network_policy_select(NETWORK_ROUTE_WIFI, &wifi, &modem, 10, true) == NETWORK_ROUTE_WIFI);
    modem.state = NETWORK_ROUTE_DEGRADED;
    assert(network_policy_select(NETWORK_ROUTE_MODEM, &wifi, &modem, 10, true) == NETWORK_ROUTE_WIFI);
    wifi.state = NETWORK_ROUTE_DOWN;
    assert(network_policy_select(NETWORK_ROUTE_WIFI, &wifi, &modem, 10, true) == NETWORK_ROUTE_NONE);
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
