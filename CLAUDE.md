# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Git Conventions

- Do not add Co-Authored-By lines or any Claude/AI attribution in commit messages.
- Do not reference Claude or AI assistance in commits.

## Project Overview

This is a native Pebble C API watchface app (projectType: "native"). It targets the Emery and Gabbro platforms using SDK v3.

The watchface displays time, date, configurable complication slots, Bluetooth connection status, weather from Open-Meteo, and the animated MATLAB logo. User configuration is handled through Clay on the phone side and received directly by the C app through AppMessage.

## Build & Deploy

```bash
npm install
pebble build
pebble install --emulator gabbro
pebble install --phone
```

The repo also includes `build.sh`, which must be run from WSL. It wipes, builds, installs to an emulator, and leaves log streaming attached:

```bash
./build.sh gabbro
```

## Architecture

- `src/c/main.c` - Native watchface implementation: UI layers, APNG logo animation, settings persistence, tick/battery/bluetooth/touch/accelerometer services, and AppMessage handling.
- `src/pkjs/index.js` - Phone-side JavaScript: initializes Clay and fetches Open-Meteo weather when the watch requests it.
- `src/pkjs/config.js` - Clay settings schema.
- `src/pkjs/custom-clay.js` - Clay custom behavior that prevents duplicate complication slot selections.
- `package.json` - Pebble metadata, message keys, target platforms, dependencies, and C resources.

## Message Keys

Clay settings use these keys:

`BackgroundColor`, `TextColor`, `TemperatureUnit`, `DateFormat`, `HourFormat`, `ComplicationLeft`, `ComplicationMiddle`, `ComplicationRight`, `VibeOnDisconnect`, `VibeOnConnect`, `LogoRotationTrigger`.

Weather messaging uses:

`RequestWeather`, `WeatherTemperatureC`, `WeatherUV`.

## Reference Material

- Rebble developer documentation: https://developer.repebble.com/
- `reference/cApiDefaultTemplate/` - Native C API template used as the structural baseline for the current migration.
- `reference/originalWatchface/` - Original MATLAB Time C watchface for comparison.
- `reference/newApproach/` - Previous Moddable/Alloy experiment.
