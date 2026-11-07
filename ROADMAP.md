# Roadmap

Vyra is being developed incrementally. A listed item is a target, not a claim
that it already exists.

## Implemented in the initial foundation

- C++20/CMake/JUCE application structure and GPL-3.0-or-later licensing.
- Two stereo playback decks with asynchronous file opening, play/pause, cue,
  and seek.
- Per-deck gain, three-band EQ, filter, echo, equal-power crossfader, master
  gain, basic peak limiter, and output meters.
- Audio device settings.
- SQLite track library with asynchronous folder scanning, metadata extraction,
  text search, and cached waveform overview rendering.
- Basic automatic BPM estimation during library indexing when embedded tempo
  metadata is absent; the estimate is not yet a beatgrid or a verified tempo.
- Basic chroma-profile musical-key estimates for indexed tracks and tracks
  loaded directly into decks when embedded key metadata is absent.
- BPM display on loaded decks and four-beat forward/backward seeking based on
  the estimate, with track-boundary clamping.
- 1/2/4/8-beat region loops, pitch/speed adjustment up to ±16%, one-shot
  estimated-tempo sync, and master mix recording to 24-bit stereo WAV with
  pause/resume. These controls have not been verified on physical DJ hardware.
- Separate pre-fader headphone cue bus with selectable stereo output pair on
  devices exposing four or more outputs, deck PFL toggles, and cue/master mix.
- Persistent named playlists with track membership, playlist search, and
  duplicate-safe add/remove operations.
- Persistent searchable playback history with UTC timestamps and deck
  attribution, including tracks loaded directly from disk.
- Discord Rich Presence for the active deck's title and playback progress,
  with an opt-out setting enabled by default.
- Three per-deck hot cues and direct waveform seeking.
- Per-track hot-cue and loop-length persistence in the SQLite library.
- MIDI device selection and MIDI Learn for deck play/cue/hot cues, crossfader,
  channel level, loop toggle, beat jump, one-shot sync, headphone cue, pitch,
  per-band EQ, and relative-CC jog nudging, with persisted assignments, JSON
  mapping import/export, and starter profiles for four controller families.
  Physical device compatibility remains unverified.
- Focused tests for mixing math, DSP, and SQLite metadata persistence.

## Next milestones

1. Audio callback stress/latency tests, safe source replacement, device-change
   recovery, and supported-format verification.
2. Improve BPM/key analysis and add editable beat grids, continuous beat-aware
   sync, quantize, key-locked time-stretch, and scratch-grade jog/vinyl control.
3. Validate headphone routing across audio drivers; improve EQ/filter/effects,
   and add extensible effect interfaces.
4. Recording export formats/metadata, MIDI output/LED feedback, and hardware
   verification.
5. Four-deck mode, workspace persistence/layouts, themes,
   recovery, release packages, and cross-platform/hardware verification.

Platform backends, file formats, codecs, hardware profiles, and performance
will be marked supported only after build and runtime testing on the relevant
platform/device.
