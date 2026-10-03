# Device compatibility

Which audio interfaces work, how well, and with what measured latency. The data
lives in [`data/compatibility/devices.toml`](../data/compatibility/devices.toml);
this page explains what it means and how to contribute to it.

## Status: nothing measured yet

**No device has been tested, and nothing here is `supported`.** The engine does
not exist yet (the repository is pre-M0), so the list holds only the limits the
roadmap already knows about — two unsupported Thunderbolt families from 4.8, and
one family prioritised for the rig. It is not yet useful for deciding what to
buy.

For the current breakdown by status, ask the data rather than this page:

```sh
python3 tools/validate_compatibility.py
# devices.toml: 3 device(s) valid (unsupported: 2, untested: 1), 0 warning(s)
```

That count is deliberately not duplicated in prose here. A hand-copied tally on
the page people use to choose hardware is exactly the kind of number that goes
quietly stale, and the validator already computes it on every run.

The Phase 1 acceptance criteria require at least 10 USB class-compliant
interfaces from different vendors, plus one Thunderbolt or PCIe interface, to
pass the suite (roadmap 4.13). That is the bar this list has to clear.

## What the statuses mean

| Status | Meaning |
|---|---|
| **Supported** | Measured on the rig. Figures published. Requires a measurement and a tested block — the validator rejects the claim otherwise. |
| **Partial** | Works with stated limitations: missing interface controls, a restricted rate set, higher latency than the class would suggest. The limitation is named. |
| **Unsupported** | Does not work, with the cause recorded and whether an application could fix it. |
| **Untested** | Nobody has measured it. Says nothing about whether it works. |

## Why some devices cannot be supported

Support depends on the kernel driver. Where Linux exposes no ALSA device, there
is nothing for this project to drive, and no amount of application work changes
that — the roadmap is explicit about this limit (4.8) and so is this list.

Those devices are marked `unsupported` with a stated reason, and with a pointer
to a class-compliant mode where the hardware has one. If a kernel driver later
appears, the entry gets re-measured and the status changes.

The reverse also holds: a device being absent from this list means nobody has
tested it, not that it fails.

## What gets published per device

Latency is published per device, not as one headline number, because the
practical floor is set by the interface, its driver and the bus (roadmap 4.6).
An entry can carry, per backend and latency setting:

- Smallest stable period size and measured round-trip latency (test QS-02)
- Whether the engine added zero extra periods, as QS-02 requires
- Xruns over a 24 h soak (QS-08)
- THD+N where measured (QS-03), which is a property of the converters
- Known quirks: unusual rate sets, implicit-feedback endpoints, fixed formats
- Which interface controls are exposed: phantom power, pad, instrument, gain

Every figure must come from the procedure in
[`docs/TEST-PLAN.md`](TEST-PLAN.md). Vendor specification sheets are not
measurements, and the validator will not stop you entering one — only review
will, so please don't.

## Contributing a device

1. Copy [`data/compatibility/TEMPLATE.toml`](../data/compatibility/TEMPLATE.toml)
   and fill it in. [`SCHEMA.md`](../data/compatibility/SCHEMA.md) documents every
   field.
2. Append it to `devices.toml`.
3. Run the validator:

   ```sh
   python3 tools/validate_compatibility.py
   ```

4. Open a pull request. CI runs the same validator.

The validator enforces that a device cannot be called `supported` without a
measurement, that `unsupported` carries a reason, and that measurements record
the kernel and sound-server version they were taken on. These rules exist
because people will buy hardware based on this list.

Once the device report tool exists (roadmap 4.8), it will generate most of an
entry for you.
