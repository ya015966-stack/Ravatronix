#pragma once

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    NETWORK_ROUTE_NONE = 0,
    NETWORK_ROUTE_WIFI,
    NETWORK_ROUTE_MODEM
} network_route_t;

typedef enum {
    NETWORK_ROUTE_DISABLED = 0,
    NETWORK_ROUTE_DOWN,
    NETWORK_ROUTE_LINK_UP,
    NETWORK_ROUTE_READY,
    NETWORK_ROUTE_DEGRADED
} network_route_state_t;

typedef struct {
    network_route_state_t state;
    uint32_t latency_ms;
} network_route_metrics_t;

/*
 * Select a route that has passed the gateway health probe.
 * When both routes are READY, retain the current READY route unless
 * the alternative is faster by more than hysteresis_margin_ms and
 * the caller confirms the minimum dwell interval has elapsed.
 * With no current READY route, select the lower-latency READY route;
 * ties prefer Wi-Fi.
 */
network_route_t network_policy_select(
    network_route_t current,
    const network_route_metrics_t *wifi,
    const network_route_metrics_t *modem,
    uint32_t hysteresis_margin_ms,
    bool dwell_elapsed
);

#ifdef __cplusplus
}
#endif
