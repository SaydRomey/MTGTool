# MTGTool

A local-first Magic: The Gathering collection and deck manager written in C++20 for Linux.  

It uses MTGJSON for exact printing data, SQLite for storage, 
GNU Make for builds, libcurl for downloads, and liblzma for `.xz` decompression.

This repository deliberately implements its own small JSON parser, 
SQLite RAII layer, migration runner, repositories, importer, CLI routing, 
collection logic, and deck logic rather than relying on a large application framework.

## Implemented

- GNU Make debug/release builds
- AddressSanitizer + UBSan in debug mode
- SQLite migrations
- MTGJSON individual-set download/import
- Local `.json` and `.json.xz` import
- Exact printing UUID storage
- Card-name search with optional set filter
- Printing detail view
- Named collections
- Quantity, finish, condition, and notes on owned cards
- Deck creation and exact-printing deck entries
- Main/side/commander/maybeboard zones
- Text, CSV, and JSON deck export
- Small custom JSON parser
- Automated test target

## Dependencies on Linux Mint

```bash
sudo apt update
sudo apt install build-essential pkg-config libsqlite3-dev libcurl4-openssl-dev liblzma-dev
```

Optional formatting target:

```bash
sudo apt install clang-format
```

## Build

Debug build (default, with ASan/UBSan):

```bash
make
```

Release build:

```bash
make release
```

Run tests:

```bash
make test
```

The executable is:

```text
build/bin/mtg
```

## First use

Initialize the database from the project root:

```bash
./build/bin/mtg init
```

Download and import an MTGJSON set:

```bash
./build/bin/mtg import-set CMM
```

The current MTGJSON v5 file server publishes per-set files such as `CMM.json.xz`, and MTGTool downloads from:

```text
https://mtgjson.com/api/v5/<SETCODE>.json.xz
```

Downloaded files are cached under `data/cache/`.

Force a refresh:

```bash
./build/bin/mtg import-set CMM --force
```

Import an already downloaded file:

```bash
./build/bin/mtg import-file path/to/CMM.json.xz
./build/bin/mtg import-file path/to/CMM.json
```

## Search

```bash
./build/bin/mtg search "Sol Ring"
./build/bin/mtg search "Sol Ring" --set CMM
./build/bin/mtg search "Lightning" --limit 50
```

Inspect one exact printing:

```bash
./build/bin/mtg printing show <uuid>
```

## Collections

```bash
./build/bin/mtg collection create "Main"
./build/bin/mtg collection list
```

Add exact physical copies:

```bash
./build/bin/mtg collection add "Main" \
  --uuid <uuid> \
  --qty 2 \
  --finish foil \
  --condition NM \
  --notes "binder 1"
```

Display:

```bash
./build/bin/mtg collection show "Main"
```

Remove copies:

```bash
./build/bin/mtg collection remove "Main" \
  --uuid <uuid> \
  --qty 1 \
  --finish foil \
  --condition NM
```

## Decks

```bash
./build/bin/mtg deck create "Atraxa" --format commander
./build/bin/mtg deck list
```

Add a commander:

```bash
./build/bin/mtg deck add "Atraxa" \
  --uuid <uuid> \
  --qty 1 \
  --finish nonfoil \
  --board commander
```

Add a main-deck card:

```bash
./build/bin/mtg deck add "Atraxa" \
  --uuid <uuid> \
  --qty 1 \
  --board main
```

Display:

```bash
./build/bin/mtg deck show "Atraxa"
```

Export:

```bash
./build/bin/mtg deck export "Atraxa" --format text
./build/bin/mtg deck export "Atraxa" --format csv --out atraxa.csv
./build/bin/mtg deck export "Atraxa" --format json --out atraxa.json
```

## Optional Docker build

Docker is not required on Linux Mint, but a Dockerfile is included for reproducible builds:

```bash
docker build -t mtgtool .
mkdir -p data
docker run --rm -v "$PWD/data:/app/data" mtgtool init
docker run --rm -v "$PWD/data:/app/data" mtgtool import-set CMM
docker run --rm -v "$PWD/data:/app/data" mtgtool search "Sol Ring"
```

Native development is still recommended because debugger and sanitizer workflows are simpler.

## Custom database path

Global options can appear anywhere in the command line:

```bash
./build/bin/mtg --db ~/cards/mtg.sqlite init
./build/bin/mtg search "Counterspell" --db ~/cards/mtg.sqlite
```

If you run the executable outside the repository root, point it to migrations explicitly:

```bash
./build/bin/mtg init --migrations /path/to/mtgtool/migrations
```

## Project layout

```text
.
├── Makefile
├── migrations/
│   └── 001_init.sql
├── src/
│   ├── cli/
│   │   └── main.cpp
│   └── core/
│       ├── http.*
│       ├── json.*
│       ├── migrations.*
│       ├── models.hpp
│       ├── mtgjson.*
│       ├── repository.*
│       ├── sqlite.*
│       └── xz.*
└── tests/
    ├── fixtures/minimal_set.json
    └── test_main.cpp
```

## Important design decisions

### Exact printings

MTGJSON `uuid` is the card-printing key stored in `printings.uuid`.  
Collection and deck records reference this UUID. 
Finish is stored separately because one MTGJSON printing can support multiple physical finishes.  
The importer also preserves MTGJSON `skuIds`, 
which provide finish-specific identifiers in current MTGJSON data.

### Imported vs personal data

`sets` and `printings` are regenerable MTGJSON data.  
`collections`, `collection_items`, `decks`, and `deck_cards` are personal data.  
Re-importing a set performs UUID-based UPSERTs and does not delete your personal records.

### Collector numbers are strings

Magic collector numbers are not always integers (`2a`, special prefixes, etc.), 
so the schema intentionally stores them as text.

### Why a custom JSON parser?

Per-set MTGJSON files are manageable in memory 
and implementing JSON parsing is useful for this learning-oriented project.  
The parser supports objects, arrays, numbers, booleans, null, escaped strings,
 Unicode escapes, and surrogate pairs.

For a future `AllPrintings.json.xz` importer, 
I might replace the in-memory JSON stage with a streaming/SAX-style parser 
to avoid holding the full dataset in RAM.

## Next features worth implementing

- Search query language (`set:`, `type:`, `color:`, `mv<=`, etc.)
- SQLite FTS5 for oracle text/type/name search
- Interactive printing picker in the CLI
- Wishlist/trade list
- Collection storage locations
- Ownership checks for decks (exact printing vs any printing)
- Commander legality and color-identity validation
- Deck statistics
- Local C++ HTTP server and web UI
- Card-image cache using Scryfall identifiers stored by MTGJSON
- Incremental `mtg update` command
- Streaming `AllPrintings` importer

## Notes on MTGJSON

MTGJSON is external data. Its format can evolve.  
The importer is deliberately defensive about optional card fields, 
but if a future MTGJSON version renames core fields, 
the mapping in `src/core/mtgjson.cpp` is the place to update.
