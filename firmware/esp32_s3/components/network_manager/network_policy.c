#include "network_policy.h"

static bool is_ready(const network_route_metrics_t *metrics)
{
    return metrics != 0 && metrics->state == NETWORK_ROUTE_READY;
}

static network_route_t select_fastest(
    const network_route_metrics_t *wifi,
    const network_route_metrics_t *modem)
{
    if (!is_ready(wifi)) {
        return is_ready(modem) ? NETWORK_ROUTE_MODEM : NETWORK_ROUTE_NONE;
    }
    if (!is_ready(modem)) {
        return NETWORK_ROUTE_WIFI;
    }

    /* Prefer Wi-Fi on an exact tie to keep the choice deterministic. */
    return modem->latency_ms < wifi->latency_ms
        ? NETWORK_ROUTE_MODEM
        : NETWORK_ROUTE_WIFI;
}

network_route_t network_policy_select(
    network_route_t current,
    const network_route_metrics_t *wifi,
    const network_route_metrics_t *modem,
    uint32_t hysteresis_margin_ms,
    bool dwell_elapsed)
{
    const bool wifi_ready = is_ready(wifi);
    const bool modem_ready = is_ready(modem);

    if (!wifi_ready && !modem_ready) {
        return NETWORK_ROUTE_NONE;
    }
    if (wifi_ready && !modem_ready) {
        return NETWORK_ROUTE_WIFI;
    }
    if (!wifi_ready && modem_ready) {
        return NETWORK_ROUTE_MODEM;
    }

    /* Both routes are READY from this point onward. */
    if (current == NETWORK_ROUTE_WIFI) {
        if (!dwell_elapsed || modem->latency_ms >= wifi->latency_ms ||
            wifi->latency_ms - modem->latency_ms <= hysteresis_margin_ms) {
            return NETWORK_ROUTE_WIFI;
        }
        return NETWORK_ROUTE_MODEM;
    }

    if (current == NETWORK_ROUTE_MODEM) {
        if (!dwell_elapsed || wifi->latency_ms >= modem->latency_ms ||
            modem->latency_ms - wifi->latency_ms <= hysteresis_margin_ms) {
            return NETWORK_ROUTE_MODEM;
        }
        return NETWORK_ROUTE_WIFI;
    }

    return select_fastest(wifi, modem);
}
