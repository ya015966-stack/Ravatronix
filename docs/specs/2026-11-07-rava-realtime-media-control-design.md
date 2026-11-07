# Ravatronix realtime media and control architecture

**Date:** 2026-11-07  
**Status:** Design approved by user; awaiting written-spec review  
**Scope:** Server-side communication design only. No firmware, app, deployment, or hardware changes are included.

## 1. Goals

Prepare RAVA for an authenticated, continuous, two-way audio/video session between the robot and its Android app:

- Live OV3660 camera video to the app.
- Robot microphone audio to the app.
- App microphone audio to a future robot speaker.
- Authenticated status and control messaging.
- Wi-Fi first; A7670E 4G data connectivity later.
- Preserve the currently stable camera firmware until a separately approved, tested change is necessary.

The microphone and speaker are planned future hardware and are not currently available. Their exact interfaces and codecs remain subject to hardware selection and compatibility tests.

## 2. Selected architecture

Use a backend with three logically separate responsibilities:

1. **HTTPS API and identity:** device/app authentication, session creation, authorization, and configuration.
2. **WebSocket signaling and control:** device presence, session coordination, status, and bounded control messages.
3. **Media Gateway:** WebRTC media termination/relay for low-latency video and bidirectional audio. The gateway may use a suitable SFU/media server; the exact product is an implementation decision after compatibility and hosting constraints are evaluated.

The ESP32-S3 should not be assumed capable of acting as a full general-purpose WebRTC endpoint without measurement. First validate whether its camera output and future audio peripherals can feed the selected media path reliably. If direct WebRTC on the board proves impractical, use a small device-side transport adapter and let the gateway handle session/media interoperability. Do not claim this path works until a bench test demonstrates it.

## 3. Component responsibilities

- **ESP32-S3-N16R8:** camera capture, device health, local hardware coordination, and authenticated outbound connection. Keep time-critical camera work isolated from network/control tasks.
- **OV3660:** live image source, using the already verified board pin map and current stable firmware as the baseline.
- **Future microphone and speaker:** audio capture/playback. Select interfaces only after checking available pins, power, sample rates, and driver support.
- **A7670E:** later provides cellular data transport. It has no GPS; location features require a separate GPS-capable module or another approved location source.
- **Backend/API:** device and user authentication, session authorization, signaling, control authorization, and service health.
- **Media Gateway:** forwards WebRTC audio/video between the robot and authorized app session.
- **Android app:** sign-in, connect/disconnect, live media view, microphone mute, speaker/audio controls, robot status, and later permitted movement controls.

## 4. Protocol boundaries

| Purpose | Protocol |
|---|---|
| Sign-in, session creation, configuration | HTTPS |
| Device/app signaling, status, control | WSS (WebSocket over TLS) |
| Live video and audio | WebRTC, subject to device-side feasibility testing |
| Media connectivity fallback | ICE with STUN/TURN as required by deployment |

Do not send media frames as repeated JSON messages over the control WebSocket. Keep media and control channels separate.

## 5. Session and data flow

1. Device boots and establishes outbound internet connectivity over Wi-Fi during the first phase.
2. Device authenticates to the backend using a per-device credential; credentials are never embedded in public source code or sent in URL query parameters.
3. App authenticates as the owner and requests a session.
4. Backend verifies ownership and grants one active control lease.
5. Backend coordinates WebRTC signaling; media flows through the selected gateway.
6. App and device exchange status/control messages over WSS.
7. When the session ends or connectivity is lost, the backend expires the session and the device returns to a safe idle state.
8. Later, Wi-Fi transport can be replaced or supplemented by A7670E cellular data without changing the app-level session model.

## 6. Security and safety requirements

- Only the authenticated owner account may initiate a session.
- Device and app identities must be independently authenticated.
- Use TLS for HTTPS and WSS; use secure WebRTC media transport.
- Store secrets in backend environment/secret storage, not Git, app bundles, or firmware source.
- Use short-lived session credentials and rotate/revoke device credentials when compromised.
- Permit only one active movement-control lease at a time.
- Reject movement commands when the lease is absent, expired, or unauthorized.
- Device-side watchdog/dead-man behavior must stop movement on disconnect, stale commands, or backend loss.
- Provide an explicit stop command and a local safety path that does not depend on cloud availability.
- Camera/audio access must be visibly indicated on the app/device where practical; no covert recording feature is in scope.

## 7. Failure handling

- Authentication failure: reject connection; do not fall back to anonymous access.
- Lost WebSocket: invalidate the control lease and stop motion.
- Lost media path: show disconnected/reconnecting state; do not imply that video/audio is live.
- Gateway unavailable: keep control disabled and report service unavailable.
- Oversized or malformed payload: reject with a bounded error; do not allocate unbounded memory.
- Cellular/Wi-Fi handover: re-authenticate or resume only through a fresh, authorized session; never preserve a stale movement lease.

## 8. Delivery phases

1. Document and test the existing authenticated device-to-backend transport over Wi-Fi.
2. Define versioned signaling, status, and session messages.
3. Evaluate media gateway candidates and test the OV3660-to-gateway video path without replacing stable firmware prematurely.
4. Add WebRTC video to an Android test client.
5. After microphone/speaker hardware is selected, validate audio capture/playback and add two-way audio.
6. Add owner authentication and one-user control lease in the Android app.
7. Add A7670E data connectivity and test reconnection/handover.
8. Add movement control only after local stop behavior and disconnect safety are verified.

## 9. Existing repository baseline and non-goals

The current backend branch has a Node service with a health endpoint, authenticated device WebSocket handshake, placeholder chat acknowledgement, and authenticated JPEG upload validation. The upload endpoint currently acknowledges a JPEG but does not persist, stream, or forward it. This is not live video and is not an AI integration.

This design does not implement or claim:
- deployed backend or media gateway;
- working live video or audio;
- a completed Android app;
- AI conversation;
- GPS/location;
- motor-control firmware;
- verified 4G connectivity.

## 10. Acceptance criteria for the first implementation plan

- Existing stable camera firmware remains unchanged during initial server protocol work.
- Device authenticates outbound over Wi-Fi without exposing secrets in URLs or repository files.
- App and device signaling schemas are versioned and documented.
- Owner-only session authorization and single control lease are enforced server-side.
- Disconnect, expired lease, and stale command paths all result in a safe stop.
- A media-gateway proof of concept demonstrates continuous OV3660 video before the feature is called live.
- Two-way audio is marked pending until actual microphone and speaker hardware is selected and tested.
