# Architecture

## Current slice

The desktop application is a JUCE GUI application. `AudioEngine` owns the
`AudioDeviceManager`, decoder registry, read-ahead thread, two transports, and
the audio callback source. The UI owns deck and mixer controls, but only sends
commands and reads published meter/transport values; it does not decode audio
inside paint or timer callbacks. Track readers are created on a small worker
pool and use JUCE's read-ahead thread during playback.

The callback mixes two stereo deck buffers, applies per-channel and
equal-power crossfade gains, then applies master gain and a bounded peak
limiter. Scratch buffers are allocated in `prepareToPlay`; unexpectedly large
device blocks are handled as bounded chunks rather than resizing audio buffers
in the callback. Meter values are published through atomics.

## Boundaries for future subsystems

- **Deck/transport:** own playback position, cue/loop state, pitch and jog
  behavior; do not own library metadata. Current pitch uses a resampling stage
  and therefore shifts pitch as well as tempo; no key lock is claimed.
- **Mixer/DSP:** channel strips, routing, EQ, filters, crossfader, master and
  headphone buses, effects, and meters. Headphone PFL uses an optional stereo
  output pair after the master pair; the bus is generated from the same
  per-deck processed samples, before channel gain and crossfade.
- **Library:** SQLite persistence, search, playlists, history, relinking, and
  metadata. Database and file I/O remain off the audio callback.
- **Analysis/waveforms:** background workers produce versioned analysis/cache
  records consumed by the UI and deck state. The current waveform cache stores
  512 mono peak bins as 8-bit samples in SQLite; analysis runs off the audio
  callback and cached values are reused when loading a library track. Library
  indexing also estimates BPM from a downsampled onset envelope when embedded
  tempo metadata is absent; it does not produce a beatgrid.
- **MIDI:** device discovery and mapping persistence are handled separately
  from deck commands. MIDI callbacks enqueue fixed-size events for the message
  thread; device output/LED feedback and HID integration are not implemented.
- **Discord Rich Presence:** the pinned Discord RPC client communicates with
  the local desktop client. Track metadata and playback timestamps are shared
  only when enabled; the feature is on by default and user-configurable.
- **Recording:** capture a selected engine bus and encode on a worker path;
  never encode or access the filesystem in the real-time callback. Current
  recording pushes the limited master output to JUCE's threaded WAV writer;
  the callback only copies into its bounded FIFO and reports overflow.
- **UI/configuration:** layouts, themes, workspace state, and device preferences
  remain independent of engine implementation.

## Real-time and correctness constraints

The callback performs no project-level file, database, UI, or analysis work and
does not resize buffers. Track replacement and transport control use JUCE
transport synchronization; this first slice has not been proven lock-free or
benchmarked for worst-case callback latency. It is therefore a functional audio
foundation, not a latency certification. The next audio-hardening step must
measure callback timing and contention on supported devices, and replace any
blocking path before making professional low-latency claims.

## Portability and dependencies

CMake fetches JUCE 7.0.12 unless `VYRA_USE_SYSTEM_JUCE` is selected. JUCE selects
the platform's available audio backend; device/backend support must be verified
on each OS. Format support depends on configured JUCE modules. ASIO, named
controller compatibility, and codec/export claims require platform-specific
build and hardware tests before being advertised.
