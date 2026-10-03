# Test plan

The quality specifications in [roadmap 7.7](ROADMAP.md) state *targets*. This
document states how each one is **measured**: the signal, the procedure, and the
criterion that distinguishes a pass from a fail. A target without a procedure
cannot gate a release. The Phase 1 and Phase 2 acceptance lists (4.13, 5.10)
each end with "the quality table rows pass"; Phase 3 (6.6) does not cite the
table, spelling the same criteria out inline instead — published WASAPI and ASIO
latency, one profile loading on both platforms, and a 24 h soak including sleep
and resume — which is exactly 7.7's single Phase 3 row, covered here by QS-19.

Figures are reproduced from the roadmap. They are targets to validate on the
reference rig, not promises, and where a figure is set by converters rather than
by the engine this plan says so.

## How the tests are grouped

Every specification is assigned to one of two harnesses. Which one decides when
a test can run and what it can prove.

| Harness | Runs where | Proves |
|---|---|---|
| **Offline** | CI, no sound card, faster than real time (4.1) | Arithmetic: nulls, gain accuracy, filter responses, curves, alignment |
| **Rig** | Reference hardware with loopback cables (section 8) | Everything involving a converter, a clock, a bus, a driver or an operating system |

The split matters for scheduling, and it is uneven twice over — by phase, and
by milestone. Ten of the nineteen specifications run entirely offline; QS-03 and
QS-06 each add an offline Part A, so **twelve** can be exercised with no sound
card at all.

| Phase | Specifications | Fully offline | Part-offline |
|---|---|---|---|
| 1 | 10 | 4 — QS-01, QS-04, QS-05, QS-07 | QS-03A, QS-06A |
| 2 | 8 | 6 — QS-11 to QS-15, QS-18 | — |
| 3 | 1 | 0 | — |

Phase 1 is mostly rig work, because most of its targets are about converters,
clocks, buses and drivers, which no renderer can simulate.

**A harness is not a schedule.** Offline does not mean runnable at M1: a test can
need no sound card and still need engine features that arrive later. Of Phase 1's
four fully-offline tests, only QS-01 and QS-05 are reachable at M1, which is
graph, strips, buses, sends, smoothing and meters. QS-04 needs patching (M3), and
QS-07 needs a patch change, a profile switch and a layout switch, so it is not
complete until profiles land at M5. Each test below names what it needs; do not
read the Offline column as "ready first".

Phase 2 inverts: six of eight are offline, because DSP correctness is
arithmetic. That is where building the offline renderer first (4.1, M1) pays
off, and it is worth knowing in advance that the early payoff is smaller than
the eventual one.

A test is listed **Offline** only where the whole result is independent of
hardware, and **Both** where it genuinely splits. THD+N (QS-03) splits: the
engine's own arithmetic contribution is bounded offline and that part gates CI,
while the absolute figure is converter-limited and can only be measured on the
rig. QS-06 splits the same way. Neither is a rig-only test, and neither is
fully offline — hence Both, and hence twelve specifications that touch the
offline harness rather than ten.

## Test index

| ID | Specification | Harness | Phase |
|---|---|---|---|
| [QS-01](#qs-01-unity-path-transparency) | Unity-path transparency | Offline | 1 |
| [QS-02](#qs-02-engine-added-latency) | Engine-added latency | Rig | 1 |
| [QS-03](#qs-03-noise-and-distortion) | Noise and distortion | Both | 1 |
| [QS-04](#qs-04-channel-isolation) | Channel isolation | Offline | 1 |
| [QS-05](#qs-05-gain-and-pan-accuracy) | Gain and pan accuracy | Offline | 1 |
| [QS-06](#qs-06-resampler-quality) | Resampler quality | Both | 1 |
| [QS-07](#qs-07-click-free-changes) | Click-free changes | Offline | 1 |
| [QS-08](#qs-08-stability) | Stability (24 h soak) | Rig | 1 |
| [QS-09](#qs-09-cpu) | CPU budget | Rig | 1 |
| [QS-10](#qs-10-recovery) | Recovery | Rig | 1 |
| [QS-11](#qs-11-eq-and-filters) | EQ and filters | Offline | 2 |
| [QS-12](#qs-12-compressor-and-gate) | Compressor and gate | Offline | 2 |
| [QS-13](#qs-13-latency-compensation) | Latency compensation | Offline | 2 |
| [QS-14](#qs-14-limiter) | Limiter | Offline | 2 |
| [QS-15](#qs-15-loudness-meter) | Loudness meter | Offline | 2 |
| [QS-16](#qs-16-network) | Network (AES67) | Rig | 2 |
| [QS-17](#qs-17-system-volume) | System volume binding | Rig | 2 |
| [QS-18](#qs-18-plugin-safety) | Plugin safety | Offline | 2 |
| [QS-19](#qs-19-windows-parity) | Windows parity | Rig | 3 |

---

## Phase 1

### QS-01 Unity-path transparency

**Target.** Bit-exact for 16- and 24-bit audio — a null result of silence — on a
stereo strip with balance centred, Gain and fader at 0 dB.

**Why it is first.** This is the roadmap's honesty principle (5) made
falsifiable. If it fails, every other transparency claim is void.

- **Signal.** Full-scale-adjacent and low-level test vectors, plus dither-level
  signals, at 16- and 24-bit: a 1 kHz sine at −1 dBFS, a −60 dBFS sine, a
  pseudo-random sequence with a fixed seed, and a single-sample impulse.
- **Procedure.** Render source through one stereo strip to one bus and out,
  with balance centred, Gain 0 dB, fader 0 dB, no inserts, no pan law applied,
  no resampling, device format equal to project format. Subtract the output
  from the input sample for sample.
- **Pass.** The difference is exactly zero for every sample — not "below a
  threshold". Any non-zero sample is a failure and must be explained.
- **Also assert.** The bit-transparent indicator (7.6) reads true for exactly
  the configurations that pass this test, and false otherwise. An indicator
  that lies is worse than no indicator.

### QS-02 Engine-added latency

**Target.** 0 extra periods, measured against a raw ALSA loopback baseline.

- **Baseline.** Measure round-trip latency of a plain ALSA loopback at the same
  device, rate and period size, with the console not running.
- **Procedure.** Repeat through the console, one strip to one bus, no inserts.
  Subtract. Candidate tool: `jack_iodelay` or an equivalent impulse round-trip
  measurement — confirm the choice in M0.
- **Pass.** The difference is 0 periods. A fractional-sample offset from
  measurement noise is acceptable; a whole period is not.
- **Record.** Input, processing, output and round-trip figures separately, since
  4.6 requires all four to be displayed in the UI. Cross-check the displayed
  figures against the measured ones — this test validates the display too.
- **Repeat for** every backend 4.7 defines — ALSA direct, PipeWire, PulseAudio
  and JACK — and each latency setting, publishing per device rather than as one
  headline number (4.6). 4.13's acceptance criterion is unqualified ("latency
  numbers are published for each backend"), so PulseAudio and JACK are in scope
  even though they map to Safe and to PipeWire's JACK layer respectively; a
  figure that is merely poor is still a figure, and omitting it is what makes
  the Safe label unverifiable.

### QS-03 Noise and distortion

**Target.** THD+N of −120 dB or better, limited by converters and not the engine.

**Note on what this proves.** On real hardware the converters dominate, so a rig
measurement cannot isolate the engine. The test is therefore in two parts, and
only the first can fail the engine.

- **Part A, offline.** Render a 997 Hz sine at −1 dBFS through the unity path in
  64-bit float and measure THD+N of the result. The engine's own contribution
  must be at or below the arithmetic noise floor. This is a stricter test than
  the target and is the one that gates CI.
- **Part B, rig.** Measure analogue loopback THD+N with the console in the path
  and with it absent. The two figures must agree within measurement error,
  demonstrating the engine adds nothing. Use AES17-style method (section 7.7),
  with a notch at the fundamental and a defined measurement bandwidth.
- **Pass.** Part A at the float noise floor; Part B showing no degradation
  attributable to the console.
- **Publish** the absolute figure per interface in the compatibility database,
  since it is a property of the hardware.

### QS-04 Channel isolation

**Target.** Bit-exact silence on unpatched channels.

- **Procedure.** Drive one channel of a multichannel device at −1 dBFS. Render
  all other channels. Repeat for each channel in turn, and repeat with a
  many-to-one and a one-to-many patch active elsewhere in the graph (4.2).
- **Pass.** Every unpatched channel is exactly zero for every sample.
- **Why both patch shapes.** A one-to-many patch is where an accumulation bug
  would leak into a channel nobody routed, and that is the failure this test
  exists to catch.

### QS-05 Gain and pan accuracy

**Target.** Within 0.01 dB for gain, 0.05 dB for the selected pan law.

- **Gain.** For Gain and fader independently, sweep the full range (Gain ±24 dB
  in 0.1 dB steps; fader −inf to +12 dB) and measure the RMS ratio of output to
  a steady input at each step. Include the documented behaviours of 7.2: 0 dB is
  exactly unity, and double-click resets to exactly 0 dB.
- **Pan.** For each selectable law (0, −3, −4.5, −6 dB) sweep the pan control
  and check both legs against the law's formula. Centre must match the law's
  stated centre attenuation, and a centred *balance* on a stereo strip must be
  exactly unity — which is what QS-01 depends on.
- **Pass.** Within 0.01 dB and 0.05 dB respectively, with 0 dB and centred
  balance exact rather than merely within tolerance.

### QS-06 Resampler quality

**Target.** THD+N of −120 dB or better, with no audible modulation over a 24 h
drift test with two interfaces.

- **Part A, offline.** Resample a swept sine and a multitone between each
  supported rate pair (44.1, 48, 88.2, 96, 176.4, 192 kHz). Measure THD+N, and measure
  spurious images separately — an image at −115 dB is a different defect from
  broadband noise and the single THD+N figure can hide it.
- **Part B, rig.** Two interfaces on independent clocks, 24 hours, with a steady
  tone through the asynchronous path. Log the PI-controlled ratio (7.6) and the
  reported drift in ppm throughout.
- **Pass.** Part A meets the target at every rate pair. Part B shows no
  dropouts, no ratio oscillation, and sideband modulation products below
  −90 dBFS. "No audible modulation" is operationalised as that sideband figure,
  because an audibility claim is not testable in CI.
- **Also assert.** Clock badges (7.6) correctly report master, shared clock or
  ASRC throughout, and the displayed drift tracks the measured drift.
- **Note.** Run this against libsoxr's variable-rate mode before any in-house
  converter is written (7.6), so the build-or-adopt decision rests on numbers.
  This interacts with ADR-0002, since libsoxr is LGPL.

### QS-07 Click-free changes

**Target.** Transients below −90 dBFS when toggling mute, solo, patch or
profile, with a 1 kHz tone at −20 dBFS.

- **Procedure.** With the tone running, perform each operation and capture the
  output: mute on and off, solo on and off, a patch change, a profile switch, a
  fader jump, a layout switch (2.4 promises this never touches audio), and a
  device format change. Measure the peak deviation from the expected envelope.
- **Pass.** Every transient below −90 dBFS. Verify the documented ramp times of
  7.2 are actually applied: 5 to 10 ms for mutes, 10 to 20 ms for level
  changes, about 10 ms crossfade for patches.
- **Edge cases that must be included.** An operation landing mid-ramp, two
  operations within one period, and a profile switch that changes many
  parameters at once. These are where a ramp implementation typically breaks,
  and a test that only toggles one mute at rest will pass a broken engine.

### QS-08 Stability

**Target.** 24 h soak with 16+ strips, 8+ buses and 2 interfaces: zero xruns at
the default setting, no memory growth.

- **Procedure.** Build the graph, drive every input, and run for 24 hours at the
  default latency setting. Sample RSS and the engine's own allocation counters
  at a fixed interval. Log every xrun with a timestamp.
- **Pass.** Zero xruns. Resident memory flat after warm-up — define the
  criterion as no upward trend over the final 20 hours, rather than an absolute
  ceiling, so slow leaks cannot hide behind a generous bound.
- **Instrument, do not just observe.** Assert that no allocation occurs on the
  audio thread at all (4.6), by counting allocations on that thread rather than
  inferring from RSS. A leak off the audio thread and an allocation on it are
  different bugs with different severities.
- **Include** a parallel run with periodic profile switches and patch changes,
  since a 24 h run at rest exercises much less than normal use.

### QS-09 CPU

**Target.** 64 channels at 48 kHz and 64 frames under 25% of one core on a
mid-range CPU, without heavy FX.

- **Procedure.** 64 channels through the configuration above. Measure per-period
  processing time, not average CPU percentage.
- **Pass.** Mean under 25% of one core, and report the 99.9th percentile and
  maximum. A mean that passes while the worst case exceeds the period deadline
  is a failing engine, and the mean alone will not show it.
- **"Mid-range CPU" is not yet defined anywhere**, and this figure is
  meaningless until it is. Naming it is an M0 task: pick one part, record it in
  the rig specification below, publish every figure against it, and re-measure
  when it changes. Until a part is named, treat QS-09 as unrunnable rather than
  as passing.
- **Also record.** DSP load as the UI reports it (4.11), checked against
  measurement, since users will make decisions based on that number.

### QS-10 Recovery

**Target.** Unplug and replug restores the patch within 2 s without glitching
other paths; an engine crash restores system audio within 5 s.

- **Hotplug procedure.** With audio running on two interfaces, unplug one.
  Measure time from replug to restored patch. Capture the *other* interface's
  output throughout.
- **Pass.** Patch restored within 2 s, ghost device shown as offline in between
  (4.2), and the surviving path shows no transient above −90 dBFS — reusing
  QS-07's criterion, because "without glitching other paths" means exactly that.
- **Crash procedure.** Kill the engine process ungracefully (SIGKILL, so no
  cleanup path runs). Measure time until normal system audio works.
- **Pass.** Restored within 5 s by the guardian (4.1), previous defaults
  restored, and no device left in an exclusively-acquired state. Repeat with the
  engine killed while holding a card in Lowest mode, which is the case where a
  leaked `ReserveDevice1` acquisition would silence the machine — the failure
  principle 4 exists to prevent.
- **Also test.** Sleep and resume, monitor sleep, USB power events, and a
  PipeWire or session-manager restart (4.4), each with the same criteria.

---

## Phase 2

### QS-11 EQ and filters

**Target.** Response within 0.1 dB of design, bit-exact null when bypassed,
sweep artifacts below −90 dBFS.

- **Response.** Measure the transfer function of each filter type (HPF at 12, 18
  and 24 dB/octave; bell, shelf and LPF bands) against its analytic design, at
  several frequencies, Q values and gains, and at every supported sample rate.
- **Near Nyquist.** Include bands placed close to Nyquist, where a bilinear
  transform deviates most. 7.5 marks this correction P1, so record the deviation
  before and after it lands rather than treating it as pass or fail.
- **Bypass.** Assert an exact null, as in QS-01 — per band and for the whole EQ.
- **Sweeps.** Automate a continuous parameter sweep of frequency, gain and Q
  while a tone runs; measure artifacts. This is what coefficient interpolation
  (7.5) exists for, and a static response test cannot detect zipper noise.
- **Pass.** 0.1 dB, exact null, artifacts below −90 dBFS.

### QS-12 Compressor and gate

**Target.** Static curves within 0.2 dB of the displayed curve.

- **Static.** Step through input levels and measure output, building the
  measured curve for a range of thresholds, ratios and knees. Compare against
  the curve the UI draws — the target is agreement with what the user is *shown*,
  not with an internal formula.
- **Dynamic.** Measure attack and release against their settings with tone
  bursts, and verify the gain-reduction meter against measured reduction.
- **Gate.** Verify threshold, range, hold, and that hysteresis prevents
  chattering at a level sitting on the threshold.
- **Pass.** Static curves within 0.2 dB; timing within a tolerance to be fixed
  in M6; no chatter at the threshold.

### QS-13 Latency compensation

**Target.** Parallel paths with different insert latency stay aligned to
1 sample.

- **Procedure.** Two strips to one bus, an insert reporting non-zero latency on
  one only. Send an impulse to both and measure arrival offset at the bus. Repeat
  with several latencies, including one larger than the period size, and with a
  chain of inserts whose latencies sum.
- **Pass.** Offset within 1 sample under the *Align* policy (7.5).
- **Also assert.** Under the *Live* policy, no compensation is applied and
  high-latency inserts are bypassed or flagged — the toggle must do what it says
  in both positions, and a test that only checks Align leaves half the feature
  unverified.
- **And.** A plugin that misreports its latency is detected or bounded, since
  that is the common real-world failure and 5.2 already anticipates misbehaving
  plugins.

### QS-14 Limiter

**Target.** True-peak ceiling respected within 0.1 dB.

- **Procedure.** Drive with material engineered to produce inter-sample peaks —
  a sine near Nyquist, square waves, and hard-clipped programme material — and
  measure true peak at the output with a 4x oversampled meter (7.4).
- **Pass.** True peak never exceeds the ceiling by more than 0.1 dB, at every
  supported sample rate. Include 44.1 kHz, where inter-sample peaks are worst.
- **Also assert.** The safety limiter is last in every bus (4.3, 7.1) and cannot
  be bypassed, including when the Phase 2 limiter replaces it (7.5); and an
  optional headphone-bus ceiling (section 8) is honoured. This is a hearing-safety
  control, so its test is non-negotiable.

### QS-15 Loudness meter

**Target.** Passes the EBU loudness conformance test signals (Tech 3341 and
3342).

- **Procedure.** Run the published conformance signals and compare against their
  stated expected values, for momentary, short-term and integrated loudness and
  loudness range.
- **Pass.** Every case within its tolerance as the specifications define it.
- **Also check.** True-peak per ITU-R BS.1770 with 4x oversampling (7.4), and
  the target presets of 5.6 — verifying the *numbers* against current platform
  specifications when shipping, since 5.6 notes they change.

### QS-16 Network

**Target.** AES67 over a wired LAN adds under 10 ms — aiming for 3 to 5 ms at
1 ms packet time — with no dropouts over 24 h.

- **Procedure.** Two machines, wired, PTPv2 via linuxptp, at the roadmap's
  default 1 ms packet time (5.7) — state the packet time with every figure,
  since latency scales with it and the 3-to-5 ms aim is quoted at 1 ms.
  Measure end-to-end latency by impulse with both machines' clocks referenced to
  the same PTP domain. Run 24 hours logging dropouts, jitter and buffer
  behaviour.
- **Baseline.** The target is *added* latency, so measure what is subtracted:
  the same impulse through both machines' local paths with no network hop, as
  QS-02 does for the engine. Added latency is the difference. Without this the
  criterion cannot be evaluated at all.
- **Pass.** Under 10 ms added over that baseline; no dropouts in 24 h.
- **Also test.** Behaviour when PTP is lost mid-stream, when the switch does not
  honour DSCP marking, and under induced packet loss — a clean-LAN-only result
  will not predict a user's network.
- **Note.** Unless the interface is PTP-locked, streams are still resampled to
  the local device clock (5.7), so QS-06 applies to this path as well.
- **VBAN** has no clock sync (5.7); test its jitter buffer separately, and do not
  hold it to the AES67 figure.

### QS-17 System volume

**Target.** OS and fader changes mirror each other within 100 ms.

- **Procedure.** Change the OS volume via media keys, the desktop slider, and
  `wpctl`/`pactl`; measure time until the bound fader reflects it. Reverse the
  direction and measure again.
- **Pass.** Within 100 ms in both directions, with no oscillation — a
  bidirectional binding (5.8) can feed back on itself, and a single-direction
  test will not reveal it.
- **Also assert.** 100% equals 0 dB per the defined curve, and the "no double
  attenuation" option (5.8) genuinely leaves the OS volume at 100%.
- **Repeat** on each desktop environment in the matrix, since this is where
  behaviour diverges most. Note that the roadmap certifies *distributions*
  (open decision 6, ADR-0007), not desktop environments, while 4.4 and section 10
  make the desktop the thing that actually varies: KDE via StatusNotifier versus
  GNOME needing an extension, and X11 grabs versus Wayland portals. Defining
  that desktop matrix — at minimum KDE and GNOME, on X11 and on Wayland — is an
  open item for ADR-0007; this test cannot be scoped until it exists.

### QS-18 Plugin safety

**Target.** A crashing or NaN-producing plugin never takes down the engine; it is
bypassed with a fade.

- **Procedure.** Build deliberately hostile test plugins: one that returns NaN,
  one that returns denormals, one that segfaults, one that blocks for 100 ms,
  one that allocates on the audio thread, one that misreports latency.
- **Pass.** For each: audio continues on all other paths, the offending plugin
  is bypassed with a fade, the user is told, and no NaN or Inf reaches an output
  — the guards of 4.1 hold at every processor boundary.
- **Also assert.** Float-to-integer conversion saturates rather than wraps (4.1).
  A wrap turns a mild overshoot into full-scale noise, which is a
  hearing-safety matter, not just a correctness one.
- **Run both** sandboxed and in-process (5.2), since the guarantees differ and
  in-process is the default on monitor paths.

---

## Phase 3

### QS-19 Windows parity

**Target.** The same profile loads; WASAPI and ASIO latency published; 24 h soak
including sleep and resume.

- **Profile portability.** Load a profile authored on Linux, and confirm
  endpoints rebind by role and name (6.5). The import report must list anything
  unmapped rather than failing silently — and per ADR-0005, a profile needing
  more virtual endpoints than the driver provides is the expected hard case.
- **Latency.** Repeat QS-02 for WASAPI shared, WASAPI exclusive and ASIO, and
  publish per interface.
- **Soak.** 24 h including sleep and resume, hotplug, sample-rate changes and
  default-device changes (6.6).
- **Driver lifecycle.** Install, upgrade, rollback and clean uninstall on
  Windows 10 and 11 with Secure Boot and HVCI on (6.2, 6.6).
- **Coexistence.** Alongside Voicemeeter and VB-Cable with no name or ID
  collisions (6.4), including the 31-character MME name limit.
- **Re-run the whole offline suite** on Windows. Every offline test above is
  platform-independent arithmetic and should pass identically; any divergence is
  a real finding.

---

## Reference rig

Section 8 calls for reference interfaces, loopback cables and nightly automated
runs. The rig is what the Phase 1 acceptance criteria depend on — "latency
numbers are published for each backend and a reference interface", and "at least
10 USB class-compliant interfaces from different vendors, plus one Thunderbolt or
PCIe interface, pass the suite" (4.13).

**Interfaces.** To satisfy 4.13, at minimum: 10 USB class-compliant interfaces
from different vendors spanning USB 2 and USB 3, budget and professional; one
Thunderbolt or PCIe interface; one Bluetooth headset for the profile-switch case
in 4.4; and one device with a high channel count for QS-09. Record every device
in `data/compatibility/`.

QS-04 is listed as Offline and needs no hardware: its channel count is a
property of the graph, not of a card. A high-channel device is still worth
having to confirm the offline result holds against a real multichannel driver,
but that confirmation is a rig extra, not what QS-04 is gated on.

**Fixtures.** Analogue loopback cables, a digital loopback where available,
a measurement-grade interface for QS-03 Part B, a switchable USB hub for
QS-10's power-event cases, and two machines on a managed switch with DSCP and
PTP support for QS-16.

QS-10 also covers monitor sleep, and 4.2's ghost-device cases are an HDMI
monitor going to sleep, a USB hub and a dock. The hub alone does not cover
them, so the rig additionally needs a display with HDMI or DisplayPort audio
that can be put to sleep, and a dock or docking station. Without both, two of
the three cases 4.2 names go untested.

**Hosts.** A defined mid-range CPU for QS-09, named explicitly so the figure
means something. One host per certified distribution (ADR-0007), plus a
PulseAudio-only configuration and a Windows 10 and a Windows 11 host for Phase 3.

**Automation.** The offline suite runs on every commit. The rig suite runs
nightly, with the 24 h tests (QS-06B, QS-08, QS-16, and QS-19's soak once
Phase 3 starts) on a weekly schedule. Results
are published per device rather than as a single headline figure (4.6).

## Candidate tooling

Measurements follow AES17-style methods where applicable, using open tools
(7.7). These are candidates to confirm in M0, not settled choices:

| Need | Candidates |
|---|---|
| Round-trip latency | `jack_iodelay`, or an impulse method in the harness |
| Signal generation and capture | `alsabat` (alsa-utils), `sox`, the project's own tone generator (4.11) |
| FFT, THD+N, sweep analysis | numpy/scipy, or the engine's own analysis code. Note the posture change: `tools/` is standard-library-only so it prejudges nothing about ADR-0003, but numerical analysis is not, and a measurement harness may reasonably take dependencies the repository tooling does not. Decide deliberately rather than by drift, and keep the two sets of rules stated separately |
| Loudness conformance | The published EBU Tech 3341 and 3342 signals |
| Null and bit-exactness | Sample-level comparison in the offline harness; no external tool needed |

The offline harness should emit machine-readable results so CI can gate on them
and so figures reach the compatibility database without being retyped.

## Open questions

These are not yet answerable and should be resolved as the named milestones land:

1. **Timing tolerances for QS-12** (attack and release) are not specified in
   7.7. Fix them in M6 when the compressor's design settles.
2. **"Mid-range CPU" for QS-09** needs a named part before the figure is
   meaningful.
3. **Whether the rig suite can gate releases** depends on how flaky hardware
   tests prove to be. A flaky gate gets ignored, which is worse than an advisory
   one; decide after the first month of nightly runs.
4. **The engine language (ADR-0003)** decides the offline harness's host
   language. The Python tooling here is deliberately independent of it, but the
   harness that drives the renderer will not be.
5. **The desktop matrix for QS-17** does not exist. ADR-0007 certifies
   distributions; 4.4 and section 10 make the *desktop* the thing that varies
   (KDE versus GNOME, X11 versus Wayland). QS-17 and the tray and hotkey work
   cannot be scoped until that matrix is named — it belongs in ADR-0007.
6. **Whether the measurement harness may take dependencies.** `tools/` is
   standard-library-only so it prejudges nothing about ADR-0003, but numerical
   analysis realistically wants numpy and scipy. Decide the two postures
   separately and in the open, rather than letting the stricter one erode.
