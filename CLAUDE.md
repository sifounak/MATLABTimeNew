# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Git Conventions

- Do not add Co-Authored-By lines or any Claude/AI attribution in commit messages.
- Do not reference Claude or AI assistance in commits.

## Project Overview

This is a Pebble smartwatch watchface app built with the **Moddable SDK** (projectType: "moddable"). It targets Pebble Time Round (emery) and Pebble 2 (gabbro) platforms using SDK v3.

The watchface displays time, date, battery level, Bluetooth connection status, and weather (fetched from Open-Meteo API). It supports user configuration via Clay settings page.

## Build & Deploy

```bash
npm install                  # Install dependencies (@moddable/pebbleproxy, @rebble/clay)
pebble build                 # Build the watchface
pebble install --emulator emery   # Install to emulator
pebble install --phone       # Install to connected phone
```

## Architecture

The app uses three layers that communicate via Pebble's AppMessage system:

- **`src/c/mdbl.c`** — Minimal C bootstrap that creates the window and initializes the Moddable JS engine
- **`src/embeddedjs/main.js`** — Core watchface logic running on-watch via Moddable runtime (Poco renderer, BMF fonts, sensors, fetch API). This is where all display rendering and business logic lives.
- **`src/pkjs/index.js`** — Phone-side JS that initializes Clay config and proxies messages between phone and Moddable runtime via `@moddable/pebbleproxy`

### Configuration

- **`src/pkjs/config.js`** — Clay configuration schema defining settings UI (colors, temperature unit, date visibility, hour format)
- **`src/embeddedjs/manifest.json`** — Moddable resource manifest for font compilation (Jersey10-Regular at sizes 56 and 24)
- **`package.json`** — Pebble project metadata including message keys, capabilities, and target platforms

### Message Keys

Settings flow from Clay config page through AppMessage using these keys: `BackgroundColor`, `TextColor`, `TemperatureUnit`, `ShowDate`, `HourFormat`.

## Reference Material

- Rebble developer documentation: https://developer.repebble.com/

The `reference/` directory contains reference implementations:
- `reference/newApproach/` — A more complete watchface example with image resources and wscript build (older Pebble SDK style)
- `reference/originalWatchface/` — Original watchface for comparison

## Key Technical Details

- Rendering uses Commodetto/Poco (Moddable's 2D graphics engine)
- Custom fonts are loaded as BMF resources with RLE-compressed alpha bitmaps
- Weather is fetched via `fetch()` from Open-Meteo API using device GPS coordinates
- Settings persist via `localStorage` on the watch
- Weather data is cached for 1 hour in localStorage
