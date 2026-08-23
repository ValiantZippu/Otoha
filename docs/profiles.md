# Device Profiles (Otoha Sound)

A profile binds a full `ProcessingState` to a context so the right sound is
recalled automatically:

```
AudioProfile { id, name, outputDeviceId, dspState, enabled }
```

Example intent:

| Output device    | Profile   |
|------------------|-----------|
| Speakers         | Natural   |
| Headphones       | Music     |
| Bluetooth buds   | Bass      |

## Resolution order

1. Profile explicitly bound to the active output-device id.
2. The **Default** profile (`outputDeviceId == ""`, id `"default"`).
3. Nothing — caller uses a neutral/bypassed state.

Implemented in `platform::ProfileManager` (`Source/Platform/DeviceProfiles.h`),
which is pure data + logic: no OS calls, fully unit-testable.

## M7 status

Architecture only. `resolveForDevice()` exists so backends with reliable
device identification can opt in later; automatic profile switching on device
change is deliberately NOT activated until a backend can identify devices
stably across reconnects.

## Presets vs profiles

Presets (`otoha::DspPreset`) define parameter *values*; profiles bind those
values to devices. Otoha Sound reuses the exact same preset engine as Studio
— there is no second preset system.
