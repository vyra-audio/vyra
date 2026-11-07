# MIDI controllers

Vyra receives USB MIDI input through JUCE. MIDI callbacks only enqueue fixed-size
events; the UI/message thread applies the associated transport or mixer action.
The action set includes play/pause, cue, three hot cues, loop, beat jump, sync,
headphone cue, relative jog, pitch, three EQ bands, filter, and echo amount per
deck, plus crossfader, channel level, and master gain. Assignments and the
chosen device identifier are stored in the user's Vyra settings file.

Open **Settings → MIDI Controller**, select an input device, then select an
action and choose **Learn**. Press the intended button or move the fader/knob.
The MIDI Learn operation replaces an existing duplicate assignment to prevent
one physical control from triggering multiple actions. Select **Generic MIDI
(learn)** to clear a preset and build a custom mapping. Buttons can send note
messages, standard knobs and faders can send 7-bit CC, and any continuous
action can learn a 14-bit pitch-bend message. Crossfader, channel level, filter,
echo amount, and master gain use the incoming continuous position. EQ maps
CC 0–127 to -12 dB through +12 dB. Pitch maps the MIDI center to 0%, with
the endpoints at -16% and +16%; 14-bit pitch bend preserves the exact center
and endpoints.
Relative jog learns MIDI CC controls using the common signed-wrap encoding:
values 1–63 move forward, 65–127 move backward (127 is one reverse tick), and
0/64 are neutral. Each tick seeks by 20 ms; this is a relative nudge, not
scratch emulation. Controllers using binary-offset or absolute jog messages
need a mapping mode not yet implemented.

Use **Export Map** to save assignments as a versioned Vyra JSON file, or
**Import Map** to restore one on another installation. Imports are validated
before replacing the current map; malformed entries, duplicate actions, and
multiple actions assigned to the same physical MIDI message are rejected.

Button actions include play/pause, cue, three hot cues, loop toggle, four-beat
forward/backward jump, one-shot tempo sync, and deck headphone cue for each
deck. Continuous actions include crossfader, channel level, pitch, each EQ band,
filter, echo amount, and master gain. Loop toggle uses the beat length selected
in that deck's loop control (four beats by default). Beat jumps require an
analyzed BPM. Sync requires usable BPM estimates on both decks. Headphone cue
only responds while separate headphone monitoring is enabled. Transport
commands are momentary and learned from note buttons; relative-jog actions use
MIDI CC; other continuous actions accept either CC or pitch bend.

Compatibility follows the standard MIDI input protocol rather than a vendor
driver or proprietary HID protocol. Any deck/controller that the operating
system exposes as a MIDI input can be mapped, but this does not imply that
every control is pre-mapped or tested on hardware. Devices that expose only
HID, require vendor scripts, or use timecode/DVS are not supported by this
MIDI workflow.

## Included profiles

These are deliberately small starter mappings for controls whose MIDI data was
checked against the referenced Mixxx controller files. They are not exhaustive
Mixxx mappings, and were not tested on physical hardware with Vyra:

| Profile | Included starter controls | Source |
|---|---|---|
| Numark Party Mix (original generation) | Play/cue on both decks; crossfader | [Mixxx Numark Party Mix mapping](https://github.com/mixxxdj/mixxx/blob/main/res/controllers/Numark-Party-Mix.midi.xml) |
| Pioneer DDJ-400 | Play/cue on both decks | [Mixxx DDJ-400 mapping](https://github.com/mixxxdj/mixxx/blob/main/res/controllers/Pioneer-DDJ-400.midi.xml) |
| Pioneer DDJ-FLX4 | Play/cue on both decks | [Mixxx DDJ-FLX4 mapping](https://github.com/mixxxdj/mixxx/blob/main/res/controllers/Pioneer-DDJ-FLX4.midi.xml) |
| Hercules DJControl Inpulse 500 | Play/cue on both decks | [Mixxx Inpulse 500 mapping](https://github.com/mixxxdj/mixxx/blob/main/res/controllers/Hercules_DJControl_Inpulse_500.midi.xml) |

The **Numark Party Mix II** has a dedicated MIDI Learn entry, but no preset is
claimed: the checked Mixxx mapping catalog provided a mapping for the original
Party Mix, not a verified Party Mix II mapping. Connect the Party Mix II and
learn each control. Device enumeration and successful MIDI input still depend
on the operating system exposing it as a MIDI input.

The Traktor Kontrol S2 MK3 is not included as a MIDI profile. Its Mixxx mapping
is HID-based, so these MIDI-only controls cannot use that profile.

## Current boundaries

Profiles only cover the starter controls in their table; additional actions
can be assigned with MIDI Learn. Scratch-grade jog behavior, LEDs, MIDI
output/feedback, HID devices, timecode/DVS, and vendor-specific scripts are not
implemented. Device enumeration, profile tables, MIDI matching, and the learn
workflow have automated coverage; actual controller behavior must be verified
with hardware before compatibility is advertised.
