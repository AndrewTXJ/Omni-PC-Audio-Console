# Omni-PC-audio-console Roadmap

*Version 3: simple on the surface, professional underneath, PC routing first.*

A Voicemeeter-style mixer and router for the PC, with a professional engine. You pick your devices, tick A/B buttons to route, and your apps just work. When you need more, the same mixer opens up into EQ, dynamics, plugins, sends and a full console view. Linux first, then processing, networking and control, then Windows.

**How to read this document**
- **Priorities:** **P0** must ship in that phase, **P1** should ship, **P2** is a stretch goal.
- **Layers:** **S** = in the Simple view, **E** = in the Expert panel (one click from any strip or bus), **C** = in the optional Console view.
- Latency and quality numbers are *targets to validate on the reference test rig*, not promises.

---

## At a glance

| Phase | Theme | Headline deliverables |
|---|---|---|
| 1. Linux core | A Voicemeeter-style mixer that just works, on a transparent, lowest-latency engine | Simple view, virtual devices, any-to-any patching, per-app routing, default-device handling, profiles, USB and Thunderbolt interface support |
| 2. Processing and connectivity | Simple knobs on a pro processing chain, plus PC automation | Gate, comp and denoise knobs, EQ, plugins and FX, push-to-talk, ducking, PC-to-PC audio, system volume hook, MIDI, hotkeys, OSC |
| 3. Windows | Same app, same profiles | WASAPI and ASIO, signed virtual devices, Voicemeeter import, installer |

---

## 1. Principles and the jobs it must do

### Principles
1. **Simple by default, deep on demand.** The Simple view stays small and predictable. Pro features live one click away and never clutter it.
2. **PC use is the use case.** Apps, headsets, speakers, monitors that go to sleep, USB devices that come and go, shortcuts and a tray icon are first-class, not afterthoughts.
3. **Working in two minutes.** A first-run wizard takes a new user from install to routed sound without reading docs.
4. **Never leave the PC silent or noisy.** Outputs start muted, routing loops are blocked, a safety limiter is always last, and if the engine dies, normal system audio is restored.
5. **Honest and transparent.** An untouched path is bit-exact, resampling and latency are visible, and nothing colours the sound unless the user turns it on.
6. **Lowest latency where it matters.** Monitoring paths get the lowest latency the hardware allows, without giving up stability.
7. **Everything is controllable.** One control API sits behind the UI, hotkeys, MIDI, OSC, a CLI and a web remote.
8. **One engine, two operating systems.** Profiles move between Linux and Windows.

### The jobs it must do

| Job | How the console does it |
|---|---|
| Play the game on speakers and music in headphones | Per-app routing, or each app plays into its own virtual device, and the A buttons pick the output |
| Send game, music and mic to the stream, but not the call | Tick B1 on those strips and leave B1 off on the chat strip |
| Be on a call without hearing yourself or getting echo | The call app records from a B bus that excludes the call's own audio (mix-minus) |
| Fix a quiet or noisy mic | Gain knob now; Gate, Comp and Denoise knobs in Phase 2 |
| Switch headphones and speakers with one key | A profile or an output hotkey |
| Control all volume from one place | The console as default device, with the OS volume bound to a fader (Phase 2) |
| Move audio between two PCs | A send and receive wizard over the network (Phase 2) |
| Monitor with minimal latency on a pro interface | Lowest latency mode and direct-monitoring controls |

**Where it fits.** Existing Linux tools each cover one slice: pavucontrol for volumes, qpwgraph and Helvum for patching, EasyEffects for effects, Carla for plugin hosting. Omni-PC-audio-console puts them behind one Voicemeeter-style mixer, with a professional engine underneath.

---

## 2. The experience: three layers

### 2.1 Simple view (default) [S]
A conceptual layout (the real design comes from the usability tests in M0):

```
  HARDWARE INPUT 1        VIRTUAL INPUT 1         BUS A1                  BUS B1
  [ USB Mic      v ]      [ Omni Main      ]      [ Headphones   v ]      [ Omni Stream    ]
  Gain (o)   Pan (o)      Gain (o)   Pan (o)      Gain (o)                Gain (o)
  fader + meter           fader + meter           fader + meter           fader + meter
  A1[x] A2[ ] B1[x]       A1[x] A2[x] B1[ ]       MUTE  MONO  EQ          MUTE  MONO  EQ
  MONO   SOLO   MUTE      MONO   SOLO   MUTE
  Gate   Comp   Denoise   (Phase 2 knobs on mic strips)
```

- **Strip:** device picker (hardware strips), Gain knob, Pan knob, fader with meter, A and B buttons, MONO, SOLO, MUTE.
- **Bus:** device picker (hardware buses), Gain, fader with meter, MUTE, MONO and an EQ button (Phase 2).
- **Top bar:** profile picker, latency setting (Lowest, Balanced, Safe), a panic mute, and tabs for Mixer, Apps, Devices and Patch.
- **SOLO** listens to one strip on the monitor output (A1 by default) without touching any B bus, so a stream is never disturbed.
- **Admission rule:** a control enters the Simple view only if a typical PC user would use it weekly. Everything else lives in the Expert panel.

### 2.2 Expert panel [E]
One click on a strip or bus name opens a panel: trim range, polarity, input and output delay, filters and EQ, dynamics, inserts, pan law, send level with pre/post tap, solo mode and metering style. The **Patch** window sits beside it: any channel to any channel, for people who want a patchbay. Specs are in section 7.

### 2.3 Console view [C]
An optional alternate layout for studio and live use: many strips, sends, groups, a monitor section (dim, mono, talkback) and large meters. It is another view of the same session, arriving in Phase 2 (basic monitor controls in Phase 1 as P1).

### 2.4 Layout sizes
Layouts are views of the same session, and switching never touches the audio.

| Layout | Size | Like |
|---|---|---|
| Compact | 3 hardware and 2 virtual inputs; A1 to A3 and B1 to B2 | Voicemeeter Banana |
| Standard | 5 hardware and 3 virtual inputs; A1 to A5 and B1 to B3 | Voicemeeter Potato |
| Expanded | Any number of strips and buses, scrolling | Beyond Voicemeeter |

### 2.5 First run [S]
1. **Pick where you listen and your mic.** Each device shows a test tone button and a live meter.
2. **Pick a goal:** Everyday, Gaming and chat, Streaming, Podcast, or Music and studio. A template sets up the strips, buses and buttons.
3. **Make the console the system default** with one click. A visible *Restore normal audio* button undoes it.
4. **Test.** Tone to each output, speak into the mic and watch the meter, play a sound from any app and watch its strip move.

Everything the wizard sets up stays editable, and it can be rerun at any time.

### 2.6 Voicemeeter concept map

| Voicemeeter | Omni-PC-audio-console | Phase |
|---|---|---|
| Hardware input strips | Input strips with a device picker | 1 |
| Virtual inputs (VAIO, AUX) | Virtual playback devices that apps play into, as many as you like on Linux | 1 |
| A buses | Output buses bound to hardware | 1 |
| B buses | Virtual capture devices that apps record from | 1 |
| A/B buttons, Mono, Solo, Mute | Same, plus Gain and Pan knobs | 1 |
| Patch Composer and Matrix | The Patch window (Expert) | 1 |
| Comp and Gate knobs | Simple knobs over a full processing chain | 2 |
| Bus EQ | Bus EQ, up to full parametric | 2 |
| VBAN | VBAN, plus AES67 and NDI | 2 |
| Macro Buttons and Remote API | Shortcuts, macros, OSC, WebSocket and CLI | 2 |
| Recorder | One-click record on any bus | 2 |
| VST hosting through external hosts | Built-in LV2, CLAP and VST3 hosting | 2 |
| Virtual ASIO | JACK and PipeWire on Linux; an ASIO bridge on Windows | 3 (P2) |

This map follows Voicemeeter's public documentation. Verify the details before using the comparisons publicly.

---

## 3. How routing works

| Stage | What it is | Example |
|---|---|---|
| Source | Anything that produces audio | USB mic, a game, a browser, another PC, a file |
| Strip | One input channel with Gain, Pan, fader and A/B buttons | "Mic", "Game", "Music" |
| Bus | A mix you can send anywhere. A buses go to hardware; B buses become virtual microphones that apps can record from | A1 headphones, A2 speakers, B1 stream, B2 chat |
| Destination | Where a bus ends up | Headphones, speakers, OBS, Discord, another PC |

- **Apps connect through virtual devices.** Playback devices ("Omni Main", "Omni Aux" and more) are where apps play into a strip. Capture devices (the B buses) are what apps record from.
- **An A/B button is a send** at 0 dB, post-fader, to that bus. In the Expert panel, right-click it to change the send level, tap point or pan.
- **Any channel can be anything.** The "hardware" and "virtual" labels are only defaults. A strip can take any source (physical input, virtual device, network stream, app capture, file or tone), and a bus can go to any destination. The Patch window wires individual channels.
- **Mix-minus is an unticked button.** A call app that records from B2, with the call's own strip unticked on B2, never hears itself.
- The full signal flow, with tap points, is in section 7.1.

---

## 4. Phase 1: Core mixer on Linux

**Goal:** a Voicemeeter-style mixer that just works on Linux, on a transparent, glitch-free, lowest-latency engine, with any USB or Thunderbolt interface that Linux supports.

### 4.1 Engine and architecture [P0]
- **Daemon and clients.** The engine runs as a headless daemon (systemd user service). The UI, tray, hotkeys, CLI and web remote are clients of one command and state API. The window can close without stopping the audio.
- **Compiled schedule.** The graph runs as one flat pass inside the device callback. Rebuilds happen off the audio thread and swap in atomically.
- **Offline renderer.** The same engine renders faster than real time from files, so routing and DSP behaviour are testable in CI without a sound card.
- **Sessions and profiles** are versioned JSON with autosave. After a crash the last state is restored with a short fade-in.
- **Guards.** NaN/Inf and denormal protection on every processor output, and float-to-integer conversion that saturates and never wraps.
- **Guardian.** A tiny supervisor restores normal system audio if the engine is down for more than a few seconds, so the PC is never left silent.

### 4.2 Devices, virtual devices and patching [P0 · S/E]
- **Virtual devices:** playback devices that apps play into, and capture devices (the B buses) that apps record from. Names are friendly and renamable, and there is no fixed limit on Linux.
- **Device picker and Patch window.** Multichannel devices are split into pairs for the picker, and the Patch window (E) wires individual channels, many-to-one and one-to-many.
- **Surround:** 5.1 and 7.1 virtual devices (games often output surround) with a standard downmix to stereo.
- **Ghost devices.** When a device disappears (a monitor's HDMI audio going to sleep, a USB hub, a dock), its patches stay and show as offline, then reconnect automatically with retries.
- **Click-free changes.** Patches crossfade over about 10 ms, and mutes and fader moves are ramped.
- **Loop protection.** A patch that would create a feedback loop is blocked, with an explanation.

### 4.3 Strips and buses: the A/B model [P0 · S]
- **Strip (S):** device picker, Gain knob (±24 dB), Pan, fader (-inf to +12 dB, 0 dB = unity), A and B buttons, MONO, SOLO, MUTE, and a meter with a latched clip LED.
- **Bus (S):** device picker, fader, MUTE, MONO, meter.
- **Expert (E):** send level and pre/post tap per A/B button, polarity, input delay (0 to 2000 ms) for lip-sync, selectable pan law, solo mode (PFL, AFL or solo in place), output delay for alignment, and the Patch window.
- **Safety limiter** last in every bus [P0].
- Full ranges are in section 7.2.

### 4.4 PC integration on Linux [P0 · S]
- **Default device.** One switch makes the console the system default output and input, and restores the previous defaults on quit. Device priority is raised so the desktop does not steal the default back. Works with PipeWire (WirePlumber) and PulseAudio.
- **Apps panel.** Lists apps that are playing or recording. A dropdown per app picks its strip (playback) or bus (recording), and per-app volume sits on the same row. Rules are saved by application name, so routing is applied automatically at the next launch. Streams are re-targeted through the sound server (PipeWire metadata, or `move-sink-input` on Pulse).
- **Startup.** Autostart at login, start minimized, restore the last profile, and keep outputs muted until the engine is stable.
- **Tray and mini mixer.** KDE works through StatusNotifier. GNOME needs the AppIndicator extension, so a compact always-on-top mini mixer is the fallback.
- **Resilience.** Recovery after sleep and resume, monitor sleep, USB power events, and PipeWire or session-manager restarts.
- **Keys.** Media and volume keys, plus in-app shortcuts (global hotkeys arrive in Phase 2).
- **Bluetooth headsets** [P1]. They switch to a lower-quality profile when their mic is used, which changes the format mid-session. Handle that without breaking the graph, and tell the user why quality changed.
- **Restore normal audio** button and the guardian (4.1).

### 4.5 Meters, solo and monitoring [P0 basic, P1 more · S/E]
- **Simple:** peak meters with latched clip LEDs on every strip and bus, and SOLO as described in 2.1.
- **Expert:** true-peak and RMS/VU meters, PFL/AFL/solo in place, and a monitor section with dim and mono check [P1]. Talkback and cue mixes come with the Console view in Phase 2.
- Metering standards are in section 7.4.

### 4.6 Lowest-latency engine [P0 · S]
In the Simple view this is one control: **Latency: Lowest, Balanced or Safe**, with an auto-tuner that finds the lowest stable setting and a display of the measured round trip.

| Setting | Path | Typical use |
|---|---|---|
| **Lowest** | Direct ALSA `hw:` exclusive access, bypassing the sound server | Live monitoring, vocalists, performance |
| **Balanced** | Native PipeWire client at quantum 32 to 64 | Everyday use alongside other apps |
| **Safe** | PipeWire or Pulse with larger buffers | Weak hardware, streaming, long sessions |

**Engine rules**
- **One callback, one pass.** No thread hops, queues or extra buffering between input, mix and output.
- **Real-time discipline.** No allocation, locks, syscalls or logging on the audio thread; parameters arrive over lock-free queues.
- **Zero-copy, in-place processing,** SIMD mixing loops, and no format conversion on the hot path.
- **Resample only when forced.** Prefer a shared master clock.
- **Precise wakeups.** ALSA mmap with hardware timestamps, waking just before the period deadline, with memory pre-faulted and locked.
- **Minimal buffering.** Two periods where the device tolerates it, automatic fallback to three on xruns.
- **Reported latency everywhere.** Input, processing, output and round trip are measured and shown.
- **Tuning helper (`doctor`).** Checks `rtprio` and `memlock` limits, rtkit, CPU governor, `threadirqs`, USB autosuspend and IRQ affinity, and recommends a low-latency or PREEMPT_RT kernel (RT support is in mainline since 6.12).

**Illustrative budget** at 48 kHz with 32-frame periods (about 0.67 ms each)

| Stage | Typical contribution |
|---|---|
| Converters and interface firmware | 1 to 2 ms combined on good interfaces (device-specific) |
| Bus transfer and driver safety offset | 0.5 to 2 ms (USB is higher than Thunderbolt or PCIe) |
| Input and output buffers | About 3 periods, roughly 2 ms |
| Mixer engine | **0 extra periods** |

**Targets (to validate on the reference rig):** Lowest setting at 5 ms round trip or less on a good USB class-compliant interface and about 3 ms on PCIe or Thunderbolt-class hardware; Balanced under 6 ms at 64 frames. The practical floor is set by the interface, its driver and the bus, so measured results are published per device, not as one headline number.

### 4.7 Backends and clocking [P0]

| Backend | Approach | Notes |
|---|---|---|
| **PipeWire** (primary, Balanced) | Native client. The card uses the `pro-audio` profile so all channels are exposed raw. The console is the graph driver when it owns the hardware clock. Idle suspend is disabled to avoid pops and start-up delay | Quantum 32 to 128 |
| **ALSA direct** (Lowest) | `hw:` exclusive, mmap, 32 to 64 frame periods, `SCHED_FIFO` via rtkit or limits. The card is acquired politely through `org.freedesktop.ReserveDevice1` so PipeWire or Pulse release it, and handed back on exit | Other apps still reach the console through its virtual devices. Prototype making the ALSA clock the PipeWire driver in M2, so virtual devices stay sample-synchronous |
| **PulseAudio** | Native async API with small `tlength` and `fragsize` | Realistically 10 to 20 ms, so it maps to Safe. Works unchanged on `pipewire-pulse` |
| **JACK** | Free through PipeWire's JACK layer | A native backend is a cheap later add |

- **Virtual devices per backend:** PipeWire null-audio-sink or custom nodes; Pulse `module-null-sink` and `module-remap-source`; ALSA through `snd-aloop`.
- **Clocking in plain terms.** One device is the master clock. Anything on a different clock is resampled with a high-quality, visible resampler, and the UI shows a badge and the drift. Two interfaces can be used together as one aggregate device. Details are in section 7.6.

### 4.8 USB, Thunderbolt and PCIe interface support [P0]
The aim is that every USB and Thunderbolt interface that Linux exposes as an audio device works. Support depends on the kernel driver, so the work is to cover everything the kernel offers and make failures obvious and fixable.

- **USB class-compliant (UAC1 and UAC2)** through `snd-usb-audio`. Many models from vendors such as Focusrite, Behringer, Audient, PreSonus, MOTU and Zoom work without vendor drivers, depending on model and firmware. Handle the quirks that appear in practice: unusual rate sets, asynchronous and implicit-feedback endpoints, fixed-format devices.
- **Thunderbolt and PCIe** work wherever the kernel exposes an ALSA device: class-compliant modes, and PCIe cards with in-kernel drivers such as RME HDSP, HDSPe and MADI. **Honest limit:** some Thunderbolt interfaces (for example Apogee Thunderbolt units and Universal Audio Apollo) have no Linux driver, and an application cannot fix that. They are marked clearly in the compatibility list, with a pointer to a class-compliant mode when the hardware has one.
- **High channel counts** (32, 64 and more) with channel names from ALSA UCM profiles where available, shown as pairs.
- **Formats:** S16, S24_3LE, S24_LE and S32_LE, 44.1 to 192 kHz, negotiated automatically.
- **Interface controls in the UI:** the ALSA mixer controls (direct monitoring, phantom power, pad, instrument, input gain), including `scarlett2` controls for supported Focusrite models. Switching +48 V asks for confirmation and briefly mutes outputs to avoid thumps.
- **Hotplug and recovery:** udev detection and automatic recovery from USB resets, xruns and power cycles, with patches restored through ghost devices.
- **Permissions:** shipped udev rules, `audio` group handling, and USB autosuspend off for audio devices.
- **Compatibility list and tooling:** a maintained list of tested devices with measured latency and known quirks, a test rig covering USB 2, USB 3 and Thunderbolt hardware, and a built-in device report that users can attach to bug reports. Legacy FireWire through `snd-firewire` and FFADO is a P2 stretch.

### 4.9 Profiles [P0 · S]
- A **profile** is a snapshot of the whole console: routing, levels and device choices. Examples: Gaming, Music, Movie night, Call, Stream.
- Switch from the tray, a hotkey (Phase 2) or the CLI. Changes ramp, so nothing clicks.
- **Keep my devices** option, so a profile made at home still loads at the office.
- **Recall safe** (E) protects chosen strips or categories from profile changes, so a switch never disturbs the live mic.
- Auto-switch when a device connects [P1]; auto-switch when an app launches arrives in Phase 2.
- Undo and redo, autosave, import and export.

### 4.10 Templates [P0]

| Template | Strips | Buses | Notes |
|---|---|---|---|
| Everyday | Browser and media, system sounds (Aux), mic | A1 headphones, A2 speakers | One-key output switch |
| Gaming and chat | Game, chat, music, mic | A1 headset, A2 speakers, B1 stream | Game to speakers, chat to headset, mic to chat and stream |
| Streaming | Mic, game, music, chat, browser and alerts | A1 headphones (full mix), B1 Stream (OBS records this), B2 Chat send | Chat strip off B1; music ducks under voice (Phase 2); output delay for lip-sync |
| Podcast with remote guest | Host mics, guest (call app), music | A1 headphones, B1 Record, B2 Guest mix-minus | Mix-minus; isolation recording (Phase 2) |
| Music and home studio | Interface inputs, DAW or player | A1 monitors, A2 headphone cue | Lowest latency; hardware direct monitoring |
| Live event and AV | Playback laptop, mics, remote feeds | A1 main, A2 to A4 zones | Output delay per zone, safety limiter, talkback (Console view), OSC or web remote |
| Conferencing | Mic, headset, app audio | A1 headset, B1 call send | Mic cleanup (Phase 2), call recording |

### 4.11 UI and diagnostics [P0 · S]
- One window with a fixed, predictable layout. Click a device name to change it. Strips can be renamed, recoloured and reordered.
- **"No sound?" tracer** [P1]: click a strip and the path from source to output lights up, showing exactly where audio stops (muted, no bus ticked, device offline, loop blocked).
- **Always-visible status:** measured latency, xrun count, DSP load, and badges for active resampling.
- **Line-up tools:** a tone generator and pink noise, plus a built-in loopback latency test.
- Dark theme, full keyboard operation, scalable UI, drag and drop for strips, and a panic mute that is always visible.

### 4.12 Packaging [P0]
- AppImage, `.deb`, `.rpm` and AUR as primary formats. Flatpak is a PipeWire-only edition, because ALSA exclusive access and real-time priority need extra permissions there.
- systemd user unit, udev rules and a limits drop-in installed by the package.
- Tested on current Fedora, Ubuntu LTS, Debian stable and Arch (PipeWire), plus a PulseAudio-only setup.

### 4.13 Phase 1 acceptance
- [ ] A new user routes game to speakers, music to headphones, and mic plus game to a stream in under 5 minutes without docs (at least 8 of 10 test users).
- [ ] Every Simple view control works as labelled and persists in profiles: device pickers, Gain, Pan, fader, A/B buttons, MONO, SOLO, MUTE.
- [ ] Any source patches to any strip and any bus to any destination, live and click-free; loops are blocked.
- [ ] On fresh Fedora, Ubuntu LTS, Debian and Arch installs, virtual devices appear in other apps, per-app routing works, and system defaults are restored on exit and after a crash.
- [ ] Latency numbers are published for each backend and a reference interface (Lowest at 32 frames stable on one USB and one Thunderbolt or PCIe interface).
- [ ] At least 10 USB class-compliant interfaces from different vendors, plus one Thunderbolt or PCIe interface, pass the suite; unsupported devices are flagged in the compatibility list.
- [ ] Two interfaces work together as one aggregate device with no audible drift over 24 h.
- [ ] The Phase 1 rows of the quality table (7.7) pass.

---

## 5. Phase 2: Processing, connectivity and control

**Goal:** Voicemeeter-style simple knobs on top of a professional processing chain, plus the PC automation and connectivity that make it a daily driver.

### 5.1 Simple knobs on a pro chain [P0 · S/E]
- **Strip knobs (S):** Gate, Comp and Denoise on mic strips, plus an Fx send (reverb and delay) on any strip.
- **Bus EQ (S):** an EQ button on every bus, from a simple 3-band view up to full parametric in the Expert panel.
- **One-click presets (S):** Voice boost, Night mode (evens out loud and quiet), Bass boost, Speech clarity.
- **Expert panel (E):** HPF, parametric EQ, compressor, gate or expander, de-esser and limiter on every input and output.
- The knobs and the Expert panel drive the same processors, so "Edit" just reveals the real parameters. Specs are in section 7.5.
- **Latency policy:** a single toggle, *Low-latency monitoring*, keeps heavy effects off monitor buses so monitoring stays fast, while stream and record buses are fully time-aligned.

### 5.2 Plugins and effects [P0/P1 · E]
- **Plugin hosting:** LV2 first (the Linux standard), then CLAP, then VST3, with insert slots on every strip and bus.
- **Native effects:** delay, chorus, a feedback-delay-network reverb, pitch shift (low-latency live mode and a higher-quality mode that reports its latency).
- **Autotune:** pitch detection (YIN or McLeod) with correction by key, scale and speed. Host an existing open plugin first (zita-at1 or x42 Fat1) and write a native one later if needed.
- **Crash isolation:** an optional sandboxed process per plugin. It costs latency and CPU, so it is off by default on monitor paths.

### 5.3 Voice cleanup [P1 · S]
- **Noise suppression** (the Denoise knob) using RNNoise or DeepFilterNet (both permissively licensed; check each). Expect roughly 10 to 30 ms of added latency, so keep it off live monitor paths.
- **Echo cancellation** for speaker users [P2], using the speaker bus as the reference. PipeWire's WebRTC-based echo-cancel module is a useful reference design.

### 5.4 PC automation [P1 · S]
- **Push-to-talk and push-to-mute** from a hotkey, MIDI or OSC, with a ramp so it never clicks.
- **Ducking:** voice ducks music and game audio, with attack, hold and release.
- **Auto-switch profiles** when an app launches or a device connects.
- **Output switch hotkey:** headphones and speakers with one key.

### 5.5 Recording [P1 · S]
- A record button on any bus, writing WAV or FLAC with a timestamped name. Multitrack recording from any tap point is a P2 stretch.

### 5.6 Loudness and analysis [P1 · E]
- LUFS meters (momentary, short-term, integrated) on any bus, with target presets such as EBU R128 at -23 LUFS, podcasts around -16 LUFS and music streaming around -14 LUFS. Platform specs change, so verify them when shipping.
- RTA, spectrogram, phase correlation and goniometer. Standards are in section 7.4.

### 5.7 Network audio and PC-to-PC [P0/P1 · S]
In the Simple view this is a wizard: **Send this bus to another PC** and **Receive from another PC**, with discovery on the local network, a pairing step, and a latency setting (Low, Balanced, Safe) that shows packet loss and jitter. A received stream appears as an ordinary strip. A network endpoint is just an endpoint with a clock domain, so it gets the same visible resampling as any device.

| Protocol | Role | Notes |
|---|---|---|
| **VBAN** | Voicemeeter compatibility and simple PC-to-PC | Audio over UDP. The protocol specification is published by VB-Audio, so check the license of any implementation you reuse. No clock sync, so it needs a jitter buffer and resampling. The first end-to-end proof |
| **AES67 / RTP with PTP** | Professional LAN | L24 at 48 kHz, 1 ms packet time by default. PTPv2 through linuxptp, SAP and mDNS discovery, QoS (DSCP) marking per the AES67 recommendations. Streams are still resampled to the local device clock unless the interface is PTP-locked. Dante devices interoperate when switched to AES67 mode in Dante Controller |
| **NDI** | Optional, for A/V workflows (OBS, vMix) | The SDK is free but proprietary, so load it at runtime as an optional plugin and check redistribution terms. Its clocking and latency are tuned for video, so do not promise sample-accurate or lowest-latency audio over it |
| **Opus over UDP or SRT** | WAN and internet | Low-delay Opus frames, jitter buffer, forward error correction |

Order of work: VBAN first, then AES67 with PTP, then NDI. Security: bind to chosen interfaces, scope multicast, and authenticate any control traffic.

### 5.8 System volume and per-app control [P0 · S]
- A virtual "system" output so the OS default lands on a console channel.
- **Bidirectional binding** of OS volume and mute (media keys, desktop slider, `wpctl` and `pactl`) to that channel's fader, with a defined curve so 100% equals 0 dB.
- **No double attenuation:** an option locks the OS volume at 100% and controls level digitally.
- Per-app volume through stream properties, and binding of several channels to several devices.

### 5.9 Shortcuts, MIDI and remote control [P0 · S]
- **Assign anything:** right-click any button or fader, press a key or move a MIDI control, and it is bound (learn mode). Bindings are saved with the profile or globally.
- **Keyboard:** global hotkeys through X11 grabs, the xdg-desktop-portal GlobalShortcuts interface on Wayland (support varies by compositor), and an evdev fallback.
- **MIDI:** saved profiles, bidirectional feedback for motorized faders and LED rings, 14-bit CC for smooth fader resolution, and soft takeover (pickup) for non-motorized controllers. Mackie Control and HUI support with banks and layers.
- **OSC, WebSocket and CLI** covering every parameter, with meter subscriptions, rate limiting and authentication. This enables a phone or tablet remote, Stream Deck integration, and Voicemeeter-Macro-Buttons-style macros [P1], such as a profile switch plus mutes.

### 5.10 Phase 2 acceptance
- [ ] A new user improves a noisy mic in under 2 minutes using only the knobs.
- [ ] EQ, dynamics and a limiter are available on every input and output, with automatic latency compensation and the Low-latency monitoring toggle.
- [ ] Reverb, delay, chorus, pitch shift and autotune are usable live, with latency documented.
- [ ] Push-to-talk and ducking work without clicks; profiles auto-switch on app launch and device connect.
- [ ] Audio streams between two PCs on the same network through the wizard, without typing an IP address.
- [ ] OS volume keys and the volume slider control the chosen channel, and the fader controls the OS volume.
- [ ] MIDI learn works with at least two controllers including a Mackie-compatible surface; global hotkeys work on X11 and Wayland.
- [ ] The Phase 2 rows of the quality table (7.7) pass.

---

## 6. Phase 3: Windows port

**Goal:** the same app, profiles and behaviour on Windows. Design for this from Phase 1: the Linux backends already sit behind the interface the Windows backends implement.

### 6.1 Audio backends [P0]

| Backend | Notes |
|---|---|
| **WASAPI shared (IAudioClient3)** | Event-driven low-latency shared mode, roughly 3 to 10 ms depending on the driver |
| **WASAPI exclusive** | Lowest latency for direct device access, single client |
| **ASIO** | Needed for pro interfaces. ASIO drivers are usually single-client, so the console becomes the ASIO host and other apps reach the device through its virtual devices. Check current ASIO SDK licensing |
| **WDM-KS** | Optional |

### 6.2 Virtual devices: the hard part [P0]
Linux lets an app create virtual devices in user space. **Windows does not.**
1. **Virtual audio driver.** Evaluate the Microsoft SYSVAD/PortCls sample against the newer ACX framework (verify current guidance). It needs **EV code-signing and Microsoft attestation signing**, with WHQL optional. Test on Secure Boot and HVCI systems, and plan install, upgrade, rollback and clean uninstall.
2. **Device count.** A driver normally exposes a fixed set of endpoints. Evaluate creating endpoints at runtime so Windows matches Linux's unlimited devices; otherwise ship a fixed set (for example 4 in and 4 out) with an installer option for more.
3. **Names and coexistence.** Keep device names to 31 characters, because MME truncates longer ones. Use unique names and IDs so the driver can coexist with Voicemeeter and VB-Cable.
4. **Per-app capture** through the process loopback capture API (Windows 10 2004 or later; verify the exact minimum build), which covers many uses without a driver.
5. **Interop with existing virtual cables** as a stopgap: detect and use them, without bundling them.

Start the research and the signing certificate during Phase 2. This is the largest schedule risk in the project.

### 6.3 PC integration on Windows [P0 · S]
- **Default devices.** Windows has separate default roles for general use and for communications, so offer both (the Voicemeeter AUX pattern). There is no supported public API to set the default device programmatically. Many utilities use an undocumented system interface, so decide deliberately, document the risk, and always offer a guided shortcut to the Windows sound settings.
- **Per-app routing.** The Apps panel starts as a guided shortcut to Windows' own per-app device settings, plus capture through process loopback. Evaluate deeper integration.
- **Exclusive-mode apps.** Games and DAWs that take exclusive control of a device lock it. Detect this and warn clearly.
- **Format and enhancements.** In shared mode the device's *Default Format* sets the sample rate, so the engine matches it or converts visibly. Detect audio enhancements and spatial sound that can alter or break the signal.
- **Startup and resilience.** Start at login, tray icon, sleep and resume, hotplug and default-device changes (`IMMNotificationClient`), mapped onto ghost devices. Run the engine as an auto-restarting service, so an outage is brief. Restoring defaults after a failure depends on the same undocumented interface as above.
- **Scheduling and power.** MMCSS "Pro Audio" task class, a fine timer resolution, and an opt-out from process power throttling for the engine process.
- **Hotkeys.** Use `RegisterHotKey`, and avoid low-level keyboard hooks that games and anti-cheat software may dislike.
- **System integration.** `IAudioEndpointVolume` and session volume for the OS volume binding; WinMM or WinRT MIDI now, and evaluate the newer Windows MIDI Services; named pipes carrying the same control protocol; VST3 and CLAP as the plugin priority (LV2 is rare on Windows).

### 6.4 Voicemeeter migration and coexistence [P1]
- Best-effort import of Voicemeeter's saved settings (verify the file format against current releases), mapping strips, bus buttons, gains and device names, with an import report that lists anything not mapped.
- Runs alongside Voicemeeter and VB-Cable without name or ID collisions, and a short migration guide.
- An ASIO bridge so DAWs can use the console as an ASIO device (the Virtual ASIO equivalent) [P2].

### 6.5 Delivery [P0]
- MSI or MSIX installer with a driver install flow and elevation, a code-signed app, auto-update, and a clean uninstall.
- The session and profile format is shared between Linux and Windows. Patches bind to logical endpoints (role and name), with per-OS device mapping.

### 6.6 Phase 3 acceptance
- [ ] Feature parity with Phases 1 and 2, with platform exceptions documented.
- [ ] Virtual devices appear in Windows sound settings and work in a browser, call apps, OBS and DAWs, via the signed driver.
- [ ] Runs alongside Voicemeeter and VB-Cable with no collisions.
- [ ] A typical Voicemeeter Banana or Potato setup imports, with an import report.
- [ ] Latency and stability numbers are published for WASAPI shared, WASAPI exclusive and ASIO.
- [ ] The same profile loads on Linux and Windows, with endpoints rebinding by role and name.
- [ ] The driver installs, upgrades and uninstalls cleanly on Windows 10 and 11 with Secure Boot and HVCI on.
- [ ] A 24 h soak including sleep and resume, hotplug, sample-rate changes and default-device changes.

---

## 7. Pro layer reference

The professional functions that stay underneath the simple surface. Phases 1 to 3 say when each ships; this section says what it must do.

### 7.1 Signal flow and tap points
```
 CHANNEL STRIP (one per input)                                TAP POINTS
 ----------------------------------------------------------   -----------------------------------------
 Source endpoint (hardware, virtual, network, app capture)
    |  input patch
    v
 Polarity > Trim (Gain knob) > Input delay                    1  INPUT: input meter and clip LED
    |
    v
 HPF > Inserts: gate, EQ, compressor, FX  (Phase 2)           2  PRE-INSERT: direct out, "pre-EQ" sends
    |
    v
 Fader (-inf to +12 dB, 0 dB = unity)                         3  PRE-FADER: cue and monitor sends, PFL
    |
    v
 Pan or balance > Mute (ramped)                               4  POST-FADER: A/B sends, AFL, strip meter
    |
    v
 Send matrix (the A/B buttons) > buses

 BUS (one per output mix)
 ----------------------------------------------------------
 Sum of sends (32-bit float, optional 64-bit accumulation)
    |
    v
 Inserts: EQ, compressor, limiter (Phase 2) > Fader > Mute > Output delay (alignment)
    |
    v
 Safety limiter (always last) > Output patch > destination endpoint
 Bus meters: peak, true-peak, RMS/VU, LUFS (Phase 2)
```
The Simple view shows only the Gain knob, Pan, fader, mute and the A/B buttons. Everything else is reached through the Expert panel.

### 7.2 Strip and bus controls

| Control | Layer | Range and behaviour |
|---|---|---|
| Gain (trim) | S | +/-24 dB in 0.1 dB steps. Software gain cannot improve analog noise, so the UI nudges users to set interface hardware gain first |
| Fader | S | -inf to +12 dB, 0 dB = unity, 0.1 dB resolution near unity, fine-adjust modifier, double-click resets to 0 dB |
| Pan and balance | S | Pan on mono strips, balance on stereo strips. Pan law selectable (E): 0, -3 (default), -4.5 or -6 dB |
| Mute | S | Ramped over 5 to 10 ms, applied to every send of the strip. An option exempts pre-fader sends and the direct out |
| Solo | S, E | Simple solo as in 2.1. Expert: PFL (tap 3), AFL (tap 4) or solo in place, with solo-safe |
| Polarity, input delay | E | Polarity invert; delay of 0 to 2000 ms in ms or samples, allocated only when used, off the audio thread |
| Sends | E | Level, pre/post tap per bus, and pan on stereo sends |
| Direct out | E | A strip sent as is to an output from tap 2 or tap 4 |
| Output delay | E | Per bus, for lip-sync or speaker alignment |
| Stereo link, mute groups | E, C | Stereo link ties two strips; mute groups mute several together |
| VCA groups, subgroups, matrix buses | C | A VCA scales its members' faders without carrying audio; a subgroup sums strips before the main mix; a matrix mixes buses (for example zone feeds) |

All level changes use 10 to 20 ms smoothing, mutes ramp over 5 to 10 ms and patches crossfade over about 10 ms.

### 7.3 Monitoring and control room [C · P1 in Phase 1, rest in Phase 2]
- **Monitor section:** source select (main, any bus or the solo/cue bus), speaker A/B, dim (-20 dB by default), cut, mono check and monitor level, independent of the program mixes.
- **Talkback** to chosen buses or the monitor bus, momentary or latching, optionally dimming the program.
- **Headphone cue mixes:** one aux bus per performer, with sidetone (hearing yourself).
- **Zero-latency monitoring** through the interface's hardware mixer where it is exposed (4.8). Otherwise the software monitor path is the lowest-latency path in the engine.

### 7.4 Metering
- **Types:** digital peak with hold, true-peak (ITU-R BS.1770, 4x oversampled), RMS/VU with 300 ms integration, and later LUFS (momentary, short-term, integrated, loudness range per BS.1770 and EBU R128), PPM (IEC 60268-10 Type I and II) [P2], RTA, correlation and goniometer.
- **Clip indicators** are latched, per stage (input, post-fader, output), with a numeric peak-hold readout.
- **Reference level** is selectable for the 0 VU point (-18 or -20 dBFS).
- **Architecture:** meters are computed in the engine and published through lock-free shared memory at 30 to 60 Hz, so a busy UI can never disturb the audio. The tap point (pre or post fader) is selectable per strip.

### 7.5 Processing and latency compensation
- **Filters and EQ:** HPF (12, 18 or 24 dB per octave), 4 to 6 band parametric EQ with shelves, bell and LPF, and per-band bypass. Filter state is kept in 64-bit and coefficients are interpolated so sweeps never zipper. Response near Nyquist is corrected [P1]. Minimum-phase by default, with linear-phase on buses [P2].
- **Compressor:** peak or RMS detection, threshold, ratio, knee, attack, release, makeup, sidechain filter, external sidechain, parallel dry/wet mix and a gain-reduction meter.
- **Gate or expander:** threshold, range, attack, hold, release, hysteresis, sidechain filter and key listen.
- **Limiter:** look-ahead and true-peak, oversampled, on every bus. It replaces the Phase 1 safety limiter.
- **Oversampling** (2x to 4x) on non-linear processors, with the latency shown.
- **Insert order** defaults to HPF, gate, EQ, compressor, and is reorderable.
- **Latency compensation:** every processor reports its latency and the engine delays parallel paths so buses sum phase-aligned. The per-bus policy is *Align* (full compensation, for stream and record buses) or *Live* (no compensation, high-latency inserts bypassed or flagged, for monitor buses). This is the *Low-latency monitoring* toggle in the Simple view.

### 7.6 Clocking, resampling and bit depth
- **Project sample rate** is fixed per session (44.1, 48, 88.2, 96 or 192 kHz). The master clock is the primary hardware interface or the PipeWire graph clock.
- **Clock badges:** every endpoint shows *master*, *shared clock* or *ASRC*, plus its measured drift in ppm.
- **Asynchronous resampling:** a high-quality polyphase or sinc design with a slow PI-controlled ratio, so there is no audible pitch wobble. Evaluate libsoxr's variable-rate mode before writing an in-house converter. Target THD+N of -120 dB or better, with a low-latency (short filter) option whose cost is shown.
- **Apps that want a different rate** are converted at the virtual-device boundary with no engine restart. A deliberate project rate change mutes, switches and fades back in.
- **Bit depth:** the device's native format is negotiated and converted once at the boundary. Dither is off for 24- and 32-bit outputs by default, and TPDF dither (optionally noise-shaped) is used for 16-bit outputs and files.
- **Bit-transparent indicator:** the UI shows when a path is bit-exact (unity gain, same rate, no processing).

### 7.7 Quality specifications

| Specification | Target | Phase |
|---|---|---|
| Unity-path transparency | Bit-exact for 16- and 24-bit audio (null result of silence), on a stereo strip with balance centred, Gain and fader at 0 dB | 1 |
| Engine-added latency | 0 extra periods, against a raw ALSA loopback baseline | 1 |
| Noise and distortion | THD+N of -120 dB or better, limited by converters and not the engine | 1 |
| Channel isolation | Bit-exact silence on unpatched channels | 1 |
| Gain and pan accuracy | Within 0.01 dB for gain and 0.05 dB for the selected pan law | 1 |
| Resampler quality | THD+N of -120 dB or better, no audible modulation over a 24 h drift test with two interfaces | 1 |
| Click-free changes | Transients below -90 dBFS when toggling mute, solo, patch or profile (1 kHz tone at -20 dBFS) | 1 |
| Stability | 24 h soak with 16+ strips, 8+ buses and 2 interfaces: zero xruns at the default setting, no memory growth | 1 |
| CPU | 64 channels at 48 kHz and 64 frames under 25% of one core on a mid-range CPU, without heavy FX | 1 |
| Recovery | Unplug and replug restores the patch within 2 s without glitching other paths; an engine crash restores system audio within 5 s | 1 |
| EQ and filters | Response within 0.1 dB of design, bit-exact null when bypassed, sweep artifacts below -90 dBFS | 2 |
| Compressor and gate | Static curves within 0.2 dB of the displayed curve | 2 |
| Latency compensation | Parallel paths with different insert latency stay aligned to 1 sample | 2 |
| Limiter | True-peak ceiling respected within 0.1 dB | 2 |
| Loudness meter | Passes the EBU loudness conformance test signals (Tech 3341 and 3342) | 2 |
| Network | AES67 over a wired LAN adds under 10 ms (aim for 3 to 5 ms at 1 ms packet time), with no dropouts over 24 h | 2 |
| System volume | OS and fader changes mirror each other within 100 ms | 2 |
| Plugin safety | A crashing or NaN-producing plugin never takes down the engine; it is bypassed with a fade | 2 |
| Windows parity | Same profile loads; WASAPI and ASIO latency published; 24 h soak including sleep and resume | 3 |

Measurements follow AES17-style methods where applicable, using open tools.

### 7.8 Pro backlog [P2]
Kept on the list but deliberately off the main path, so the product stays simple: VCA groups and matrix buses in the Console view, linear-phase EQ, an auto-mixer for several open mics, multitrack recording with virtual soundcheck, NMOS IS-04/IS-05 discovery, redundant networks, FireWire support, convolution for room correction and HRTF virtual surround on headphones, an acoustic feedback detector, PPM meters, and a gain-staging assistant that listens and suggests hardware gain.

---

## 8. Cross-cutting concerns

### Technology choices
| Layer | Recommendation | Why |
|---|---|---|
| Engine | Rust (or C++20) | RT-safe, strong concurrency guarantees, good PipeWire bindings (`pipewire-rs`) |
| Control API | Versioned command and state protocol over a Unix socket (named pipe on Windows), with WebSocket and OSC gateways | One API for every client, and the same protocol on both platforms |
| UI | Qt 6/QML, or a web UI (Tauri) | Cross-platform; faders and meters draw easily; a web UI doubles as a phone or tablet remote |
| DSP | In-house channel strip, plus LV2, CLAP and VST3 hosting | Own the latency-critical core, host the rest |
| Test | Offline renderer, loopback rig, CI with hardware runners | Deterministic regression tests without a sound card |

- **Usability testing from day one:** a clickable Simple-view prototype is tested with real users in M0, and again at each release.
- **Measurement rig:** reference interfaces (USB 2, USB 3, Thunderbolt, PCIe, budget and professional), loopback cables, and nightly automated runs.
- **Hearing and equipment safety:** outputs start muted, a safety limiter is last in every bus, and an optional ceiling can be set on headphone buses.
- **Security:** the control API and network endpoints bind to localhost by default, with authentication and per-client permissions.
- **Accessibility:** scalable UI, high-contrast theme, full keyboard operation.
- **Licensing (decide early):** GPL or permissive affects what can be linked: libsoxr (LGPL), JUCE (GPL or commercial), RNNoise (BSD), the NDI SDK (proprietary), and the ASIO and VST3 SDKs (check current terms).
- **Documentation:** a five-minute quick start, the template cookbook, a "No sound?" troubleshooting guide, a Voicemeeter migration guide, a latency guide per backend, and a gain-staging guide (hardware gain first, peaks around -12 to -6 dBFS, Gain for balance, faders near unity).
- **Community:** a public compatibility database fed by the device report tool.

---

## 9. Milestones

Sizes are relative, for a small team of 1 to 3 engineers: S is weeks, M is 1 to 2 months, L is 2 to 4 months, XL is 4 months or more.

| # | Milestone | Phase | Size | Done when |
|---|---|---|---|---|
| M0 | Specs, test rig, key decisions and a clickable Simple-view prototype | 1 | S | Quality specs agreed; rig and reference interfaces in hand; license, language and UI stack decided; prototype tested with real users |
| M1 | Engine core and offline renderer | 1 | L | Graph, strips, buses, sends, smoothing and meters work; null tests run in CI without hardware |
| M2 | ALSA (Lowest) and PipeWire backends | 1 | L | First sound; device reservation; loopback harness; engine adds 0 extra periods |
| M3 | Virtual devices, patching, loop protection and Simple view v1 | 1 | L | Apps play into strips, A/B routing works, any source to any strip; **Alpha** to early testers |
| M4 | Clocking: resampling, aggregate devices, Pulse backend, compatibility list v1 | 1 | L | Two-interface soak passes; 10 USB devices tested |
| M5 | PC integration: default device, Apps panel, tray, autostart, guardian, profiles, templates, wizard, "No sound?" tracer, packaging | 1 | XL | Phase 1 acceptance passes; **Phase 1 release** |
| M6 | Simple knobs, EQ, Expert panels and latency policy | 2 | XL | Gate and Comp knobs, bus EQ, full dynamics and limiter in the Expert panel, Low-latency monitoring toggle |
| M7 | Plugin hosting, effects and voice cleanup | 2 | XL | LV2 and CLAP (then VST3), native FX, pitch shift, autotune, noise suppression |
| M8 | PC automation, recording and loudness | 2 | M | Push-to-talk, ducking, auto-profiles, record button, LUFS and analysis |
| M9 | Network audio and PC-to-PC wizard | 2 | L | VBAN, then AES67 with PTP, then NDI as an optional plugin |
| M10 | System volume hook, global hotkeys, MIDI, OSC, WebSocket and CLI | 2 | L | Phase 2 acceptance passes; **Phase 2 release** |
| M11 | Windows backends and PC integration | 3 | XL | WASAPI, ASIO, default-device handling, tray, hotkeys, MIDI, service |
| M12 | Windows virtual driver, installer, Voicemeeter import and parity release | 3 | XL | Signed driver, installer and parity tests pass; **Phase 3 release** |

---

## 10. Top risks

| Risk | Impact | Mitigation |
|---|---|---|
| The Simple view grows cluttered as features arrive | Loses the Voicemeeter-style simplicity | The admission rule in 2.1, usability tests at every release, Expert panel for everything else |
| Fights over the default device with the desktop or OS | Sound goes to the wrong place or nowhere | Raised device priority, guardian, Restore normal audio, clear UI state |
| Clock drift and resampler quality | Crackles or pitch wobble | Prototype early, mismatched-clock soak tests, evaluate libsoxr |
| Routing feedback loops | Loud howl | Cycle detection at patch time, safety limiter, outputs start muted |
| PulseAudio and ALSA latency limits | User disappointment | Be explicit in the UI; Lowest, Balanced and Safe labels; recommend PipeWire |
| Linux configuration diversity (PipeWire and WirePlumber versions, WirePlumber 0.4 versus 0.5 config formats, distro defaults) | Support burden | `doctor` tool, a four-distro test matrix, minimal reliance on config files |
| GNOME tray icons and Wayland global shortcuts | Hotkeys or tray missing | Mini mixer fallback, portal first with an evdev fallback, compositor test matrix |
| USB and Thunderbolt driver gaps | "My interface doesn't work" | Compatibility list, device report, clear messaging |
| Windows virtual driver, signing and device count | Schedule and cost | Research and certificate during Phase 2; process loopback as a fallback; evaluate runtime endpoint creation |
| Windows per-app routing and default-device control lack public APIs | Weaker Windows experience | Guided shortcuts first, deliberate decision on undocumented interfaces |
| NDI licensing and determinism | Legal and latency surprises | Optional runtime plugin; AES67 is the core network path |
| Plugin crashes and latency | Glitches | Sandbox option, NaN guards, the Low-latency monitoring toggle |
| Scope creep | Late delivery | P0/P1/P2 tags, templates before features, host plugins rather than writing them |

---

## 11. Open decisions

1. **License:** GPL or permissive. It decides how NDI, ASIO and VST3 are handled.
2. **Engine language and UI stack:** Rust or C++20; Qt 6 or a web UI.
3. **Default layout:** Compact (Banana-like) or Standard (Potato-like) for new users.
4. **Windows virtual device count:** a fixed set (for example 4 in and 4 out), or runtime creation if the driver can support it.
5. **Lowest setting:** the default for pro interfaces, or an advanced option.
6. **Support baseline:** minimum PipeWire and kernel versions, and the distributions to certify.
