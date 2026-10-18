# MTG Personal Collection & Deck Manager
## Game / Product Design Document

**Working title:** MTGTool  
**Document type:** Game Design / Product Design Document  
**Primary platform:** Linux Mint  
**Primary implementation language:** C++20  
**Primary data source:** MTGJSON  
**Primary interface:** Command-line interface  
**Secondary interface:** Local web application  
**Database:** SQLite  
**Build system:** GNU Make  
**Status:** Initial design specification

---

# 1. Executive Summary

MTGTool is a personal Magic: The Gathering collection and deck-management application 
intended to be built primarily as a learning and hobby project.

The application will allow a user to:

- Maintain a complete local catalog of Magic cards.
- Track exact physical printings owned by the user.
- Organize cards into one or more collections.
- Create and manage decklists that reference exact printings.
- Search the card catalog by name, set, collector number, color, card type, and other properties.
- Import card metadata from MTGJSON.
- Work offline after card data has been imported.
- Export decklists to common human-readable formats.
- Later expose the same functionality through a local web interface.
- Potentially evolve into a multi-user league application.

The design intentionally emphasizes implementing substantial portions of the system manually 
rather than relying on an existing collection-management application or large application framework.

External libraries are limited to infrastructure-oriented components such as 
SQLite, libcurl, liblzma, and a JSON parser. 

Application behavior, data modeling, search logic, collection logic, deck logic, 
migrations, command handling, and most other application systems 
will be written specifically for this project.

---

# 2. Project Goals

## 2.1 Primary Goals

The primary goals of MTGTool are:

1. Build a useful personal Magic collection manager.
2. Track exact printings rather than only card names.
3. Maintain a fully local database.
4. Work well from the Linux command line.
5. Provide a clean C++ architecture that can later support multiple frontends.
6. Learn by implementing most application logic directly.
7. Make the program easy to extend over time.
8. Avoid dependence on a permanently available remote API for normal use.

---

## 2.2 Learning Goals

The project should provide opportunities to practice:

- Modern C++ design.
- RAII.
- SQLite and SQL.
- Database migrations.
- HTTP networking.
- JSON parsing.
- Streaming large datasets.
- XZ decompression.
- CLI design.
- Search query parsing.
- Domain modeling.
- File I/O.
- Error handling.
- Unit and integration testing.
- Makefile-based builds.
- Web API design.
- HTML, CSS, and JavaScript.
- Docker and containerization.
- Software architecture and modularity.

The project should favor implementing meaningful application logic 
over assembling a collection of pre-existing frameworks.

---

# 3. Design Philosophy

The project follows several design principles.

## 3.1 Offline First

Once MTGJSON data has been downloaded and imported, 
all normal functionality should operate without an internet connection.

Internet access should primarily be used for:

- Updating card data.
- Downloading new sets.
- Refreshing metadata.
- Potentially downloading card images.

---

## 3.2 Exact Printing Identity

A card in MTGTool is not merely a card name.

For example:

```text
Lightning Bolt
```

is a card identity, while:

```text
Lightning Bolt
Magic 2011
Collector Number 149
English
Foil
```

represents a physical printing.

MTGTool should preserve this distinction throughout its architecture.

Decks and collection entries should normally reference exact printings.

---

## 3.3 Thin Frontends, Shared Core

The CLI and future local web interface should not duplicate business logic.

Instead:

```text
CLI
  |
  v
Application/Core Layer
  |
  v
Storage + Search + Import
  |
  v
SQLite
```

Later:

```text
CLI --------\
             \
              -> Core Library -> SQLite
             /
Local Web --/
```

This architecture allows both interfaces to behave consistently.

---

## 3.4 Small Infrastructure Dependencies

MTGTool should use external libraries where 
reimplementing the functionality would not meaningfully improve the project.

Reasonable dependencies include:

- SQLite
- libcurl
- liblzma
- simdjson or RapidJSON

The project should implement its own:

- Database wrapper.
- Repository layer.
- Migration runner.
- Domain models.
- Search query language.
- Deck rules.
- Collection logic.
- CLI command routing.
- Import pipeline.
- Web API handlers.

---

# 4. Target User

Initially, the primary user is the developer.

The expected user profile is:

- Uses Linux Mint.
- Is comfortable with terminal applications.
- Wants to track a physical Magic collection.
- Wants to distinguish exact printings.
- Enjoys programming and wants the application itself to be a hobby project.
- May later want to share collection and deck information with friends.

A secondary future audience is a private Magic playgroup or league.

---

# 5. Project Scope

## 5.1 Version 1 Scope

Version 1 should include:

- Local SQLite database.
- MTGJSON set import.
- Card search.
- Exact printing identification.
- Collection management.
- Deck creation.
- Deck editing.
- Decklist display.
- Deck export.
- CLI interface.
- Database migrations.
- Automated tests for core functionality.

---

## 5.2 Version 2 Scope

A later version may add:

- Local web UI.
- Card images.
- Advanced search syntax.
- Collection statistics.
- Deck statistics.
- Commander-specific features.
- Deck legality checks.
- Price information.
- Wishlist functionality.
- Multiple named collections.
- Deck tags.
- Collection tags.

---

## 5.3 Long-Term Scope

Potential long-term features include:

- Multiple users.
- Private league support.
- Friend collections.
- Trade lists.
- Want lists.
- Deck sharing.
- Deck comments.
- League standings.
- Match history.
- Deck history.
- Tournament results.
- Web authentication.
- Docker deployment.
- Mobile-friendly local interface.

---

# 6. Non-Goals

The early project should not attempt to:

- Reimplement SQLite.
- Reimplement TLS.
- Write a custom HTTP stack.
- Scrape arbitrary card websites.
- Build a full competitive tournament platform.
- Support public internet deployment.
- Duplicate every MTGJSON field immediately.
- Implement every Magic rules interaction.
- Store every card image locally by default.
- Recreate existing deck-building websites feature-for-feature.

Keeping the early scope controlled is important for finishing a usable version.

---

# 7. Data Provider

## 7.1 MTGJSON

MTGJSON is the primary card-data provider.

The system should consume MTGJSON files rather than depend on continuous API requests.

The preferred import strategy begins with individual set files.

Example conceptual URL:

```text
https://mtgjson.com/api/v5/<SETCODE>.json.xz
```

A later full-database import may use:

```text
AllPrintings.json.xz
```

---

## 7.2 Why MTGJSON

MTGJSON fits the project because it provides:

- Rich card metadata.
- Exact printing records.
- Stable UUID identifiers.
- Set-specific information.
- Collector numbers.
- Language information.
- Finish information.
- External identifiers.
- Offline downloadable datasets.

The MTGJSON UUID should be treated as the authoritative printing identifier inside imported data.

---

# 8. Domain Model

The application separates several concepts.

## 8.1 Card Concept

A card concept represents the general game object.

Examples:

- Lightning Bolt
- Sol Ring
- Counterspell

It may eventually be useful to create a separate oracle-level identity table.

For the initial version, MTGJSON printing records may contain enough 
card-level information to avoid prematurely introducing another layer.

---

## 8.2 Printing

A printing represents a specific released version of a card.

A printing includes information such as:

- MTGJSON UUID.
- Card name.
- Set.
- Collector number.
- Language.
- Rarity.
- Frame.
- Border.
- Available finishes.
- Mana cost.
- Type.
- Rules text.
- Power.
- Toughness.
- Loyalty.
- External identifiers.

The printing UUID is the primary imported card identifier.

---

## 8.3 Physical Collection Item

A collection item represents physical copies owned by the user.

A collection entry references a printing and adds user-specific information such as:

- Quantity.
- Finish.
- Condition.
- Notes.
- Acquisition date.
- Acquisition price.
- Storage location.

The first version only needs a subset of these fields.

---

## 8.4 Deck

A deck contains:

- ID.
- Name.
- Format.
- Description or notes.
- Creation date.
- Last modification date.

A deck contains one or more deck entries.

---

## 8.5 Deck Entry

A deck entry references:

- Deck.
- Exact printing.
- Quantity.
- Zone.

Possible zones include:

```text
main
side
commander
maybeboard
companion
```

The initial implementation may support only:

```text
main
side
commander
maybeboard
```

---

# 9. Database Design

SQLite is the primary database.

Benefits include:

- No server process.
- Single portable database file.
- Reliable transactions.
- Good C API.
- Good indexing.
- Optional full-text search.
- Easy backups.
- Easy Docker volume handling later.

---

# 10. Initial Database Schema

A possible initial migration is shown below.

```sql
PRAGMA foreign_keys = ON;

CREATE TABLE IF NOT EXISTS schema_migrations
(
    version INTEGER PRIMARY KEY,
    applied_at TEXT NOT NULL DEFAULT (datetime('now'))
);

CREATE TABLE IF NOT EXISTS sets
(
    code TEXT PRIMARY KEY,
    name TEXT NOT NULL,
    release_date TEXT,
    type TEXT,
    total_set_size INTEGER,
    base_set_size INTEGER,
    is_online_only INTEGER,
    is_foil_only INTEGER
);

CREATE TABLE IF NOT EXISTS printings
(
    uuid TEXT PRIMARY KEY,
    name TEXT NOT NULL,

    set_code TEXT NOT NULL
        REFERENCES sets(code)
        ON UPDATE CASCADE,

    collector_number TEXT NOT NULL,
    rarity TEXT,
    language TEXT,

    mana_cost TEXT,
    mana_value REAL,

    type_line TEXT,
    text TEXT,

    power TEXT,
    toughness TEXT,
    loyalty TEXT,

    color_identity TEXT,
    colors TEXT,
    finishes TEXT,
    identifiers TEXT,

    is_full_art INTEGER,
    is_promo INTEGER,

    border_color TEXT,
    frame_version TEXT,
    security_stamp TEXT
);

CREATE INDEX IF NOT EXISTS idx_printings_name
ON printings(name);

CREATE INDEX IF NOT EXISTS idx_printings_set_num
ON printings(set_code, collector_number);

CREATE TABLE IF NOT EXISTS collections
(
    id INTEGER PRIMARY KEY,
    name TEXT NOT NULL UNIQUE
);

CREATE TABLE IF NOT EXISTS collection_items
(
    collection_id INTEGER NOT NULL
        REFERENCES collections(id)
        ON DELETE CASCADE,

    printing_uuid TEXT NOT NULL
        REFERENCES printings(uuid)
        ON DELETE RESTRICT,

    quantity INTEGER NOT NULL
        CHECK(quantity >= 0),

    finish TEXT,
    condition TEXT,
    notes TEXT,

    PRIMARY KEY (
        collection_id,
        printing_uuid,
        finish,
        condition
    )
);

CREATE TABLE IF NOT EXISTS decks
(
    id INTEGER PRIMARY KEY,
    name TEXT NOT NULL UNIQUE,
    format TEXT,
    notes TEXT,
    created_at TEXT NOT NULL DEFAULT (datetime('now')),
    updated_at TEXT NOT NULL DEFAULT (datetime('now'))
);

CREATE TABLE IF NOT EXISTS deck_cards
(
    deck_id INTEGER NOT NULL
        REFERENCES decks(id)
        ON DELETE CASCADE,

    printing_uuid TEXT NOT NULL
        REFERENCES printings(uuid)
        ON DELETE RESTRICT,

    quantity INTEGER NOT NULL
        CHECK(quantity > 0),

    board TEXT NOT NULL DEFAULT 'main'
        CHECK(
            board IN (
                'main',
                'side',
                'commander',
                'maybeboard'
            )
        ),

    PRIMARY KEY
    (
        deck_id,
        printing_uuid,
        board
    )
);
```

---

# 11. Future Database Extensions

Later migrations may add:

```text
card_faces
oracle_cards
tags
printing_tags
deck_tags
collection_tags
wishlists
wishlist_items
prices
price_history
locations
acquisitions
trades
users
league_members
matches
league_seasons
```

Schema evolution should always happen through migration files.

---

# 12. Database Migration System

Migration files should be stored in:

```text
migrations/
```

Example:

```text
migrations/
├── 001_init.sql
├── 002_indexes.sql
├── 003_fts.sql
├── 004_collection_locations.sql
└── 005_wishlist.sql
```

The migration runner should:

1. Open the database.
2. Create `schema_migrations` if necessary.
3. Read migration files.
4. Determine the current database version.
5. Apply missing migrations in numerical order.
6. Record each migration after successful completion.
7. Abort and roll back on failure.

The migration system is a useful learning project and avoids hardcoding schema creation inside C++.

---

# 13. Core Software Architecture

Recommended layout:

```text
mtgtool/
├── Makefile
├── README.md
├── LICENSE
│
├── migrations/
│   ├── 001_init.sql
│   └── ...
│
├── src/
│   ├── cli/
│   │   ├── main.cpp
│   │   ├── command_router.cpp
│   │   └── commands/
│   │       ├── init.cpp
│   │       ├── import.cpp
│   │       ├── search.cpp
│   │       ├── collection.cpp
│   │       └── deck.cpp
│   │
│   ├── core/
│   │   ├── model/
│   │   │   ├── card.hpp
│   │   │   ├── printing.hpp
│   │   │   ├── collection.hpp
│   │   │   └── deck.hpp
│   │   │
│   │   ├── storage/
│   │   │   ├── sqlite.hpp
│   │   │   ├── sqlite.cpp
│   │   │   ├── migrations.hpp
│   │   │   ├── migrations.cpp
│   │   │   └── repositories/
│   │   │
│   │   ├── ingest/
│   │   │   ├── mtgjson.hpp
│   │   │   ├── mtgjson.cpp
│   │   │   ├── xz.hpp
│   │   │   └── xz.cpp
│   │   │
│   │   ├── search/
│   │   │   ├── query.hpp
│   │   │   ├── query.cpp
│   │   │   ├── parser.hpp
│   │   │   └── parser.cpp
│   │   │
│   │   └── util/
│   │       ├── http.hpp
│   │       ├── http.cpp
│   │       ├── filesystem.hpp
│   │       └── strings.hpp
│   │
│   └── web/
│       ├── main.cpp
│       ├── routes/
│       └── static/
│           ├── index.html
│           ├── app.js
│           └── style.css
│
├── tests/
│   ├── storage/
│   ├── ingest/
│   ├── search/
│   └── deck/
│
├── data/
│   ├── mtg.sqlite
│   ├── cache/
│   └── downloads/
│
└── docs/
    └── design.md
```

---

# 14. Core Modules

## 14.1 Model Layer

Contains data structures representing domain objects.

Example:

```cpp
struct Printing
{
    std::string uuid;
    std::string name;
    std::string set_code;
    std::string collector_number;
    std::string rarity;
    std::string language;
};
```

Models should avoid database-specific logic whenever practical.

---

## 14.2 Storage Layer

Responsible for:

- Opening SQLite databases.
- Executing SQL.
- Prepared statements.
- Transactions.
- Migrations.
- Repository methods.
- Error translation.

The rest of the application should not call raw SQLite functions everywhere.

---

## 14.3 Ingest Layer

Responsible for:

- Downloading MTGJSON files.
- Decompressing `.xz`.
- Parsing JSON.
- Converting MTGJSON records into application models.
- Inserting/updating database records.
- Recording import metadata.

---

## 14.4 Search Layer

Responsible for:

- Basic search.
- Advanced search syntax.
- Translating search expressions into safe SQL.
- Filtering results.
- Ranking results.

---

## 14.5 Application Services

As the project grows, application operations should move into service classes.

Examples:

```text
CollectionService
DeckService
ImportService
SearchService
WishlistService
```

Frontends should call these services rather than directly manipulate repositories.

---

# 15. SQLite Wrapper

A small RAII wrapper should be implemented.

Primary classes:

```text
SqliteDb
SqliteStmt
SqliteTransaction
SqliteError
```

Responsibilities include:

- Database lifetime.
- Statement lifetime.
- Binding values.
- Fetching columns.
- Automatic cleanup.
- Throwing descriptive exceptions.

Example conceptual API:

```cpp
SqliteDb db("data/mtg.sqlite");

SqliteStmt stmt(
    db,
    "SELECT name FROM printings WHERE uuid = ?"
);

stmt.bind(1, uuid);

if (stmt.step())
{
    std::cout << stmt.col_text(0);
}
```

---

# 16. Transactions

Bulk imports must use transactions.

Instead of:

```text
INSERT
COMMIT
INSERT
COMMIT
INSERT
COMMIT
```

the importer should use:

```text
BEGIN
INSERT
INSERT
INSERT
...
COMMIT
```

Potential batch size:

```text
5,000 to 20,000 records
```

The optimal value can be tuned later.

---

# 17. Import-Time SQLite Settings

During large imports:

```sql
PRAGMA foreign_keys=ON;
PRAGMA journal_mode=WAL;
PRAGMA synchronous=NORMAL;
PRAGMA temp_store=MEMORY;
```

Cache size may also be increased temporarily.

Example:

```sql
PRAGMA cache_size=-200000;
```

This is approximately 200 MB when using SQLite's negative-KB form.

---

# 18. HTTP Layer

libcurl should handle HTTP downloads.

A small `HttpClient` wrapper should expose operations such as:

```cpp
HttpResponse get_string(
    const std::string& url
);

void download_file(
    const std::string& url,
    const std::string& path
);
```

The wrapper should configure:

- Redirect following.
- User agent.
- Compression support.
- Timeouts.
- Error handling.

File downloads should use:

```text
destination.tmp
```

followed by a rename after successful completion.

This avoids leaving corrupt final files after an interrupted download.

---

# 19. XZ Decompression

MTGJSON provides compressed `.xz` files.

liblzma should be used for decompression.

Initial set imports may decompress into memory because individual set files are manageable.

Large datasets such as `AllPrintings.json.xz` should eventually use streaming decompression.

Future pipeline:

```text
HTTP download
    |
    v
XZ stream
    |
    v
JSON parser
    |
    v
Database batch
```

This avoids loading the entire MTG database into memory.

---

# 20. JSON Parsing

Preferred parser:

```text
simdjson
```

Alternative:

```text
RapidJSON
```

The parser should support efficient handling of large files.

For small set imports, convenience can be favored.

For `AllPrintings`, parsing should eventually be streaming or incremental.

---

# 21. MTGJSON Import Strategy

The import process should initially focus on individual sets.

Example command:

```bash
mtg import-set MH3
```

Processing:

```text
User enters set code
        |
        v
Construct MTGJSON URL
        |
        v
Download <SET>.json.xz
        |
        v
Decompress
        |
        v
Parse Set object
        |
        v
Upsert set metadata
        |
        v
Loop through cards
        |
        v
Upsert printing records
        |
        v
Commit transaction
```

---

# 22. Import Idempotency

Importing the same set repeatedly must be safe.

The importer should use SQLite upserts.

Concept:

```sql
INSERT INTO printings (
    uuid,
    name,
    set_code,
    collector_number
)
VALUES (?, ?, ?, ?)

ON CONFLICT(uuid)
DO UPDATE SET
    name = excluded.name,
    set_code = excluded.set_code,
    collector_number = excluded.collector_number;
```

This allows card metadata to be updated without deleting personal collection or deck data.

---

# 23. Imported Data vs Personal Data

Imported data and personal data must remain conceptually separate.

Imported:

```text
sets
printings
oracle metadata
external identifiers
```

Personal:

```text
collections
collection_items
decks
deck_cards
wishlists
notes
tags
```

Updating MTGJSON must never destroy personal records.

This is one of the most important database design rules for the application.

---

# 24. Import Metadata

A future table should track imports.

Example:

```sql
CREATE TABLE imports (
    id INTEGER PRIMARY KEY,
    source TEXT NOT NULL,
    source_version TEXT,
    set_code TEXT,
    imported_at TEXT NOT NULL,
    card_count INTEGER,
    file_hash TEXT
);
```

Benefits:

- Debugging.
- Repeatability.
- Detecting stale sets.
- Showing database freshness.

---

# 25. Command-Line Interface

The CLI is the initial primary interface.

Primary executable:

```bash
mtg
```

General structure:

```bash
mtg <command> [subcommand] [options]
```

---

# 26. Initial CLI Commands

## 26.1 Initialize

```bash
mtg init
```

Creates:

```text
data/
data/mtg.sqlite
data/cache/
```

Then applies database migrations.

---

## 26.2 Import a Set

```bash
mtg import-set MH3
```

Possible options:

```bash
mtg import-set MH3 --force
mtg import-set MH3 --keep-json
mtg import-set MH3 --no-cache
```

---

## 26.3 Search

Basic:

```bash
mtg search "Lightning Bolt"
```

Possible output:

```text
UUID                                  SET  NUMBER  LANG  RARITY    NAME
--------------------------------------------------------------------------------
xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx  M11  149     en    uncommon  Lightning Bolt
...
```

---

## 26.4 Collection Creation

```bash
mtg collection create "Main Collection"
```

List:

```bash
mtg collection list
```

---

## 26.5 Add Card to Collection

Exact UUID:

```bash
mtg collection add \
    --collection "Main Collection" \
    --uuid <uuid> \
    --qty 2
```

A friendlier form may be added later:

```bash
mtg collection add \
    --set M11 \
    --number 149 \
    --finish foil \
    --qty 2
```

---

## 26.6 Remove Cards

```bash
mtg collection remove \
    --collection "Main Collection" \
    --uuid <uuid> \
    --qty 1
```

---

## 26.7 Display Collection

```bash
mtg collection show "Main Collection"
```

Optional filters:

```bash
mtg collection show "Main Collection" --set MH3
mtg collection show "Main Collection" --foil
```

---

# 27. Deck Commands

## 27.1 Create Deck

```bash
mtg deck create "Jund Midrange"
```

With format:

```bash
mtg deck create "Jund Midrange" --format modern
```

Commander example:

```bash
mtg deck create "Atraxa" --format commander
```

---

## 27.2 Add Card

```bash
mtg deck add \
    "Jund Midrange" \
    --uuid <uuid> \
    --qty 4 \
    --board main
```

---

## 27.3 Add Commander

```bash
mtg deck add \
    "Atraxa" \
    --uuid <uuid> \
    --qty 1 \
    --board commander
```

---

## 27.4 Display Deck

```bash
mtg deck show "Atraxa"
```

Possible output:

```text
Atraxa
Format: Commander

Commander
---------
1 Atraxa, Praetors' Voice [2X2 #188]

Mainboard
---------
1 Sol Ring [CMM #396]
1 Arcane Signet [ELD #331]
...
```

---

## 27.5 Deck Export

```bash
mtg deck export "Atraxa"
```

Potential formats:

```bash
mtg deck export "Atraxa" --format text
mtg deck export "Atraxa" --format csv
mtg deck export "Atraxa" --format json
```

Later formats may include:

```text
Moxfield-compatible
Archidekt-compatible
MTGO-like
Arena-like
Cockatrice
```

---

# 28. Search System

Search should initially support straightforward database queries.

Examples:

```bash
mtg search "Sol Ring"
mtg search --set CMM "Sol Ring"
mtg search --rarity rare
```

Later, MTGTool can implement its own compact search language.

Example:

```text
name:"lightning bolt"
set:m11
rarity:uncommon
color:r
type:instant
mv<=2
foil:true
```

Combined:

```bash
mtg search 'type:creature color:g mv<=3'
```

---

# 29. Search Parser Architecture

The search system should be implemented as:

```text
Raw query
    |
    v
Tokenizer
    |
    v
Parser
    |
    v
Search AST
    |
    v
SQL Builder
    |
    v
Prepared Statement
```

Example AST:

```cpp
struct SearchFilter
{
    Field field;
    Operator op;
    std::string value;
};
```

The query parser is an excellent future learning component.

---

# 30. Full-Text Search

SQLite FTS5 may eventually improve text searching.

Potential indexed fields:

```text
name
type_line
rules text
```

Example virtual table:

```sql
CREATE VIRTUAL TABLE printing_fts USING fts5(
    uuid UNINDEXED,
    name,
    type_line,
    text
);
```

FTS should be introduced only after the regular schema is stable.

---

# 31. Collection Features

Initial collection fields:

- Printing.
- Quantity.
- Finish.
- Condition.
- Notes.

Possible conditions:

```text
NM
LP
MP
HP
DMG
```

The application should avoid hardcoding assumptions where possible.

---

# 32. Collection Locations

A future feature can track storage location.

Examples:

```text
Binder 1
Binder 2
Rare Binder
Commander Box
Bulk Box A
Trade Binder
Deck: Atraxa
```

Potential table:

```sql
locations (
    id,
    name,
    notes
)
```

Collection items can reference a location.

---

# 33. Wishlist

A wishlist is a natural extension.

Example commands:

```bash
mtg wishlist add --uuid <uuid>
mtg wishlist list
mtg wishlist remove --uuid <uuid>
```

Possible fields:

```text
desired quantity
preferred set
preferred finish
maximum price
priority
notes
```

This later becomes useful for a private league application.

---

# 34. Deck Validation

The early version should not attempt to implement all Magic rules.

Later, deck validation may include:

- Deck size.
- Sideboard size.
- Commander count.
- Commander color identity.
- Singleton rules.
- Basic land exceptions.
- Format legality.
- Banned/restricted cards.

Validation should be a separate subsystem.

Example:

```cpp
ValidationResult
DeckValidator::validate(
    const Deck& deck,
    const FormatRules& rules
);
```

---

# 35. Card Images

Card images should initially be represented by URLs rather than stored in SQLite.

Possible strategy:

```text
printing
  |
  +-- external Scryfall identifier
             |
             v
         image URL
```

Later the local web UI may cache images under:

```text
data/images/
```

Example:

```text
data/images/<uuid>.jpg
```

Image caching should remain optional.

---

# 36. Local Web Application

The second major frontend will be a local web application.

The web application should reuse all core functionality.

Concept:

```text
Browser
   |
   | HTTP
   v
mtg-web
   |
   v
Core Services
   |
   v
SQLite
```

---

# 37. Local Web Backend

Potential lightweight C++ frameworks include:

- Crow
- Drogon
- Restinio
- Pistache

A small framework is preferred because the goal remains to implement application logic manually.

Crow is a reasonable future option for a simple local application.

---

# 38. Web API

Possible endpoints:

```text
GET  /api/search
GET  /api/printings/:uuid

GET  /api/collections
POST /api/collections

GET  /api/collections/:id
POST /api/collections/:id/cards
DELETE /api/collections/:id/cards/:uuid

GET  /api/decks
POST /api/decks

GET  /api/decks/:id
POST /api/decks/:id/cards
DELETE /api/decks/:id/cards/:uuid
```

---

# 39. Web Pages

Potential local pages:

```text
/
    Dashboard

/cards
    Search cards

/collection
    Browse owned cards

/decks
    Deck list

/decks/:id
    Deck editor

/import
    Card database management

/wishlist
    Wanted cards
```

---

# 40. Frontend Technology

The first local web version should remain simple.

Recommended stack:

```text
HTML
CSS
Vanilla JavaScript
```

A large SPA framework is unnecessary at first.

The browser can call the local C++ REST API using `fetch()`.

---

# 41. Example Deck Editor Layout

Desktop concept:

```text
+--------------------------------------------------------------+
| Atraxa                                   Commander | 100 cards |
+-----------------------------+--------------------------------+
| Search Cards                | Deck                           |
|                             |                                |
| [ Sol Ring             ]    | Commander                      |
|                             | 1 Atraxa                       |
| Results                     |                                |
| -------------------------   | Mainboard                      |
| Sol Ring CMM #396           | 1 Sol Ring                     |
| Sol Ring C21 #263           | 1 Arcane Signet                |
| Sol Ring 2XM #282           | ...                            |
|                             |                                |
+-----------------------------+--------------------------------+
```

The exact printing should always be visible when adding a card.

---

# 42. Makefile Build System

GNU Make is the project build system.

The Makefile should support:

```bash
make
make debug
make release
make run
make clean
```

Later:

```bash
make test
make web
make format
```

---

# 43. Compiler Settings

Recommended baseline:

```text
-std=c++20
-Wall
-Wextra
-Wpedantic
-Wshadow
-Wconversion
-Wsign-conversion
```

Debug builds:

```text
-O0
-g3
-fsanitize=address,undefined
-fno-omit-frame-pointer
```

Release builds:

```text
-O2
-DNDEBUG
```

---

# 44. Automatic Dependency Files

Use:

```text
-MMD -MP
```

for generated `.d` files.

This ensures source files are rebuilt when included headers change.

---

# 45. System Dependencies

For Linux Mint, expected packages include:

```bash
sudo apt install \
    build-essential \
    pkg-config \
    libsqlite3-dev \
    libcurl4-openssl-dev \
    liblzma-dev
```

If simdjson is installed system-wide, its development package may also be used.

Alternatively it can be vendored.

---

# 46. Dependency Philosophy

System packages are preferred for:

```text
SQLite
libcurl
liblzma
```

The JSON library may either be:

- System-installed.
- Vendored into the repository.
- Included as a Git submodule.

Vendoring simdjson makes builds more reproducible while still keeping the dependency visible.

---

# 47. Error Handling

The initial application may use exceptions for fatal operational errors.

Examples:

```text
SqliteError
HttpError
ImportError
ParseError
MigrationError
```

The CLI should catch application-level exceptions in `main()` and print concise messages.

Example:

```text
error: unable to import MH3: database is locked
```

Debug builds may optionally print more context.

---

# 48. Logging

The first version may use a small custom logger.

Levels:

```text
ERROR
WARN
INFO
DEBUG
TRACE
```

Example:

```text
[INFO] importing MH3
[INFO] downloaded 12.4 MB
[INFO] parsed 521 cards
[INFO] import complete
```

A third-party logging framework is not necessary initially.

---

# 49. Configuration

Potential configuration file:

```text
~/.config/mtgtool/config.ini
```

or:

```text
~/.config/mtgtool/config.toml
```

Fields might include:

```text
database_path
download_cache_path
image_cache_path
default_collection
default_export_format
web_port
```

The first release can simply use command-line arguments and sensible defaults.

---

# 50. Default Paths

Possible Linux defaults:

```text
~/.local/share/mtgtool/mtg.sqlite
~/.cache/mtgtool/
~/.config/mtgtool/
```

During development, a simpler project-local layout may be used:

```text
./data/mtg.sqlite
./data/cache/
```

---

# 51. Testing Strategy

Tests should focus on the application logic the project implements.

## Unit Tests

Examples:

```text
search tokenizer
search parser
deck calculations
format rules
collection quantity changes
MTGJSON field conversions
```

## Integration Tests

Examples:

```text
migration creation
repository insert/select
set import
duplicate import
deck persistence
collection persistence
```

## Regression Tests

When a bug is fixed, a test reproducing it should be added.

---

# 52. Test Database

Tests should use temporary SQLite databases.

Example:

```text
/tmp/mtgtool-test-XXXX.sqlite
```

or SQLite in-memory databases:

```text
:memory:
```

File-based temporary databases should be used when testing migrations or WAL behavior.

---

# 53. Import Test Fixtures

Do not test the importer against the entire MTGJSON dataset every time.

Instead create small fixture files:

```text
tests/fixtures/
├── minimal_set.json
├── dual_faced_card.json
├── foil_card.json
├── malformed_set.json
└── empty_set.json
```

This produces deterministic and fast tests.

---

# 54. Special Card Cases

The importer must eventually account for:

- Double-faced cards.
- Split cards.
- Adventure cards.
- Meld cards.
- Tokens.
- Basic lands.
- Promotional cards.
- Non-English cards.
- Cards without power/toughness.
- Cards with variable power/toughness.
- Collector numbers containing letters.
- Special finishes.

For this reason, collector numbers should remain strings.

Never assume:

```text
collector_number = integer
```

---

# 55. Exact Finish Tracking

A printing may support multiple finishes.

Examples:

```text
nonfoil
foil
etched
```

The printing record describes available finishes.

The physical collection item describes the finish actually owned.

Therefore:

```text
printing.finishes
```

and:

```text
collection_item.finish
```

represent different concepts.

---

# 56. Data Integrity

Important constraints include:

- Deck entries must reference existing printings.
- Collection entries must reference existing printings.
- Quantity cannot be negative.
- Deck quantity should be greater than zero.
- Deleting imported printing data should not silently delete owned cards.
- Updates should preserve UUID identity.
- Foreign keys must always be enabled.

Use:

```sql
PRAGMA foreign_keys=ON;
```

whenever a connection opens.

---

# 57. Backups

Because SQLite is a single file, backups can be straightforward.

Future command:

```bash
mtg backup
```

Possible output:

```text
backups/mtg-2026-09-20.sqlite
```

Later:

```bash
mtg restore backups/mtg-2026-09-20.sqlite
```

Backups should include only personal/database data, not regenerable caches.

---

# 58. Data Export

The program should provide a way to export personal information independently of SQLite.

Possible future command:

```bash
mtg export-data backup.json
```

The JSON could include:

```text
collections
collection items
decks
deck cards
wishlists
tags
```

Imported MTGJSON card data can be excluded because it is regenerable.

---

# 59. Docker Strategy

Docker is optional for the early CLI application.

Native Linux development is simpler and gives better access to debugging tools.

Docker becomes more useful once the local web application exists.

Potential future structure:

```text
Dockerfile
docker-compose.yml
```

Example concept:

```yaml
services:
  mtg:
    build: .
    ports:
      - "8080:8080"
    volumes:
      - ./data:/app/data
```

SQLite would live in the mounted data directory.

---

# 60. Why Docker Is Not Required Initially

For Linux Mint:

- GCC is available natively.
- SQLite is available natively.
- libcurl is available natively.
- liblzma is available natively.
- Debugging is easier natively.
- File permissions are simpler.
- Build iteration is faster.

Docker should solve a real deployment problem rather than be introduced only for novelty.

---

# 61. Git Repository Practices

Recommended branches:

```text
main
feature/importer
feature/decks
feature/search
```

For a solo project, even a simple:

```text
main
```

with short-lived feature branches is enough.

---

# 62. Suggested `.gitignore`

```gitignore
build/
data/
*.o
*.d
*.sqlite
*.db

.cache/
.vscode/
.idea/

*.tmp
*.log
```

Test fixtures and migration files should remain committed.

---

# 63. Development Milestones

## Milestone 0 — Project Skeleton

Goals:

- Repository exists.
- Makefile compiles.
- CLI executable runs.
- SQLite wrapper works.
- libcurl wrapper works.

Success:

```bash
./build/bin/mtg hello
```

---

## Milestone 1 — Database Foundation

Implement:

- Database open.
- Migration runner.
- Initial schema.
- Prepared statement wrapper.
- Transaction wrapper.

Success:

```bash
mtg init
```

creates a usable database.

---

## Milestone 2 — MTGJSON Set Import

Implement:

- Download set archive.
- XZ decompression.
- JSON parsing.
- Set upsert.
- Printing upsert.

Success:

```bash
mtg import-set MH3
```

imports the set into SQLite.

---

## Milestone 3 — Card Search

Implement:

```bash
mtg search "Sol Ring"
```

Result displays:

```text
name
set
collector number
language
rarity
UUID
```

---

## Milestone 4 — Collection Management

Implement:

```bash
mtg collection create
mtg collection add
mtg collection remove
mtg collection show
```

At this point the application becomes practically useful.

---

## Milestone 5 — Deck Management

Implement:

```bash
mtg deck create
mtg deck add
mtg deck remove
mtg deck show
mtg deck export
```

---

## Milestone 6 — Advanced Search

Implement query parsing such as:

```text
set:mh3
type:creature
color:g
mv<=3
foil:true
```

---

## Milestone 7 — Full MTG Dataset

Add support for:

```text
AllPrintings.json.xz
```

Use streaming decompression and parsing.

---

## Milestone 8 — Local Web API

Build:

```text
mtg-web
```

Expose card, deck, collection, and search endpoints.

---

## Milestone 9 — Local Web UI

Implement:

- Dashboard.
- Card browser.
- Collection browser.
- Deck editor.
- Import page.

---

## Milestone 10 — League Features

Expand toward a private friend-group application.

Potential functionality:

- Users.
- Shared decklists.
- Shared want lists.
- Trade lists.
- League standings.
- Match tracking.

---

# 64. Suggested Implementation Order

A practical development order is:

```text
1. Makefile
2. CLI entry point
3. SQLite wrapper
4. Migration runner
5. Schema
6. HTTP downloader
7. XZ decompressor
8. MTGJSON parser
9. Import command
10. Search command
11. Collection repository
12. Collection commands
13. Deck repository
14. Deck commands
15. Tests
16. Advanced search
17. Full dataset import
18. Local web backend
19. Local web frontend
```

This order minimizes wasted work because every later feature builds on already-tested layers.

---

# 65. Suggested Initial Classes

Core:

```text
SqliteDb
SqliteStmt
SqliteTransaction

MigrationRunner

HttpClient
XzReader

MtgJsonImporter

SetRepository
PrintingRepository
CollectionRepository
DeckRepository

SearchParser
SearchQuery
SearchService

CollectionService
DeckService
```

CLI:

```text
CommandRouter
InitCommand
ImportSetCommand
SearchCommand
CollectionCommand
DeckCommand
```

---

# 66. Example Core Relationships

```text
ImportSetCommand
       |
       v
ImportService
       |
       +---- HttpClient
       |
       +---- XzReader
       |
       +---- MtgJsonParser
       |
       +---- SetRepository
       |
       +---- PrintingRepository
```

Deck commands:

```text
DeckCommand
    |
    v
DeckService
    |
    +---- DeckRepository
    |
    +---- PrintingRepository
```

---

# 67. Repository Pattern

Repositories isolate SQL.

Example:

```cpp
class PrintingRepository
{
public:
    explicit PrintingRepository(SqliteDb& db);

    void upsert(const Printing& printing);

    std::optional<Printing>
    find_by_uuid(std::string_view uuid);

    std::vector<Printing>
    search_by_name(std::string_view name);
};
```

This makes business logic testable and prevents raw SQL from spreading throughout the codebase.

---

# 68. Transaction RAII

A transaction class can automatically roll back unless committed.

Concept:

```cpp
class SqliteTransaction
{
public:
    explicit SqliteTransaction(SqliteDb& db);

    void commit();

    ~SqliteTransaction();
};
```

Example:

```cpp
SqliteTransaction tx(db);

repo.upsert(set);

for (const auto& card : cards)
    repo.upsert(card);

tx.commit();
```

If an exception occurs before `commit()`, the destructor performs rollback.

---

# 69. Search Result Presentation

CLI output should be compact and scannable.

Example:

```text
SET  #     LANG  FINISH         RARITY     NAME
-------------------------------------------------------------
CMM  396   en    foil,nonfoil   uncommon   Sol Ring
2XM  282   en    foil,nonfoil   uncommon   Sol Ring
C21  263   en    nonfoil        uncommon   Sol Ring
```

A verbose flag may display UUID and identifiers:

```bash
mtg search "Sol Ring" --verbose
```

---

# 70. Interactive Printing Selection

A useful future CLI mode could allow:

```bash
mtg collection add "Sol Ring"
```

Then:

```text
1. CMM #396 — nonfoil/foil
2. 2XM #282 — nonfoil/foil
3. C21 #263 — nonfoil

Select printing: _
```

This keeps exact-printing tracking while making the CLI easier to use.

---

# 71. Collection-to-Deck Awareness

Later, MTGTool can indicate whether a deck is fully owned.

Example:

```bash
mtg deck ownership "Atraxa"
```

Output:

```text
Owned:     92 / 100
Missing:    8

Missing:
1 Mana Crypt
1 Fierce Guardianship
...
```

This is a strong feature because collection and deck data share exact printing identities.

---

# 72. Flexible Ownership Matching

Exact printings should remain the canonical deck references, but the application may offer alternate ownership modes.

Examples:

```text
exact-printing
same-card-any-printing
same-card-preferred-language
```

This lets the user answer different questions:

```text
Do I own this exact copy?
```

versus:

```text
Do I own any playable copy of this card?
```

---

# 73. Statistics

Potential future statistics:

Collection:

```text
total physical cards
unique printings
unique card names
sets represented
foil percentage
rarity distribution
color distribution
```

Deck:

```text
mana curve
color distribution
card type distribution
land count
average mana value
owned percentage
```

These statistics are natural additions to the future web interface.

---

# 74. Price Support

Pricing should not be part of the initial design.

If later added, keep price data separate from canonical card metadata.

Possible table:

```sql
prices (
    printing_uuid,
    source,
    currency,
    finish,
    value,
    recorded_at
)
```

Historical prices could later be stored in `price_history`.

---

# 75. Multi-User / League Evolution

The architecture should leave room for an eventual private league application.

Possible entities:

```text
users
league
league_members
seasons
matches
match_players
deck_versions
trades
want_lists
```

Users could expose selected information:

```text
public decks
trade binder
wishlist
collection statistics
```

Private data should remain private by default.

---

# 76. League Use Case

A future friend group could use the application to:

1. Add personal collections.
2. Share decklists.
3. Publish wanted cards.
4. Publish cards available for trade.
5. Record league matches.
6. View league standings.
7. Track deck performance.

This can be built later on top of the same core printing database.

---

# 77. Versioning Decks

A future feature can preserve deck history.

Example:

```text
Atraxa v1
Atraxa v2
Atraxa v3
```

Rather than overwriting entries, the application can record revisions.

Potential tables:

```text
deck_versions
deck_version_cards
```

This becomes especially useful for league match history.

---

# 78. Security Considerations

For a purely local CLI:

- Security requirements are minimal.
- Database file permissions should follow the user account.

For the local web app:

- Bind to `127.0.0.1` by default.
- Do not bind publicly unless explicitly configured.
- Validate all API inputs.
- Use prepared SQL statements.
- Avoid shelling out using untrusted strings.

For multi-user deployment:

- Authentication becomes mandatory.
- Authorization must be implemented.
- Passwords must be securely hashed.
- HTTPS should be provided by a reverse proxy.

These concerns belong to later phases.

---

# 79. Performance Targets

The initial application should feel immediate for local operations.

Targets:

```text
CLI startup:             < 250 ms when practical
Simple card search:      < 100 ms
Collection lookup:       < 100 ms
Deck display:            < 100 ms
Individual set import:   seconds, depending on set size
Full database import:    minutes are acceptable
```

Exact performance will depend on dataset size and hardware.

---

# 80. Memory Targets

Individual set import:

```text
tens to low hundreds of MB acceptable
```

Full `AllPrintings` import:

```text
should eventually be streaming
```

The complete uncompressed dataset should not need to be loaded into RAM simultaneously.

---

# 81. Data Update Strategy

Initial:

```bash
mtg import-set MH3
```

Later:

```bash
mtg update
```

`mtg update` could:

1. Fetch MTGJSON metadata.
2. Determine changed/new sets.
3. Download changed files.
4. Re-import them.
5. Record import version.
6. Preserve personal data.

---

# 82. Update Safety

Card-data updates should be atomic where practical.

A failed import should not leave half-updated set data.

Possible approach:

```text
BEGIN TRANSACTION
update set
update printings
record import
COMMIT
```

On failure:

```text
ROLLBACK
```

---

# 83. Handling Deleted or Changed Upstream Printings

MTGJSON records may theoretically change.

The application should initially:

- Upsert current records.
- Avoid automatically deleting missing UUIDs.

A later synchronization mode may mark records as inactive rather than deleting them.

This protects historical collection and deck references.

---

# 84. Documentation

Repository documentation should include:

```text
README.md
docs/design.md
docs/database.md
docs/cli.md
docs/import.md
```

The README should stay concise.

This design document can serve as the broader architectural reference.

---

# 85. README Outline

Recommended:

```text
# MTGTool

## Features
## Screenshots
## Requirements
## Building
## Usage
## Importing MTGJSON
## Project Structure
## Development
## Roadmap
## License
```

---

# 86. Coding Style

Recommended principles:

- Prefer RAII.
- Prefer value types.
- Avoid global mutable state.
- Keep ownership obvious.
- Prefer `std::string_view` for non-owning inputs.
- Use `std::optional` for absent values.
- Use `enum class`.
- Avoid unnecessary inheritance.
- Keep functions small enough to test.
- Keep SQL out of UI code.
- Keep CLI parsing out of domain code.

---

# 87. Example Enums

```cpp
enum class DeckBoard
{
    Main,
    Side,
    Commander,
    Maybeboard
};

enum class CardFinish
{
    Nonfoil,
    Foil,
    Etched
};

enum class CardCondition
{
    NearMint,
    LightlyPlayed,
    ModeratelyPlayed,
    HeavilyPlayed,
    Damaged
};
```

Database serialization can map these values to strings.

---

# 88. Potential Future TUI

Between CLI and web UI, another possible frontend is a terminal UI.

Possible libraries:

```text
FTXUI
ncurses
```

This is optional.

A TUI could provide:

- Card search.
- Split-pane deck editing.
- Collection browsing.
- Keyboard shortcuts.

Because the core is frontend-independent, adding a TUI would not require redesigning storage.

---

# 89. Definition of MVP

The application qualifies as a Minimum Viable Product when the following workflow works:

```bash
make

./build/bin/mtg init

./build/bin/mtg import-set CMM

./build/bin/mtg search "Sol Ring"

./build/bin/mtg collection create "Main"

./build/bin/mtg collection add \
    --collection Main \
    --uuid <sol-ring-uuid> \
    --qty 1

./build/bin/mtg deck create "Commander Deck" \
    --format commander

./build/bin/mtg deck add \
    "Commander Deck" \
    --uuid <sol-ring-uuid> \
    --qty 1

./build/bin/mtg deck show "Commander Deck"
```

If this workflow is reliable, the fundamental architecture is working.

---

# 90. Recommended First Development Sprint

The first sprint should focus only on foundation and importing.

## Deliverables

```text
Makefile
src/cli/main.cpp
src/core/storage/sqlite.*
src/core/storage/migrations.*
src/core/util/http.*
src/core/ingest/xz.*
src/core/ingest/mtgjson.*
migrations/001_init.sql
```

## Commands

```bash
mtg init
mtg import-set <SET>
mtg search <NAME>
```

Avoid collection and deck logic until importing and search are reliable.

---

# 91. Second Development Sprint

Focus:

```text
collections
```

Implement:

```bash
mtg collection create
mtg collection list
mtg collection add
mtg collection remove
mtg collection show
```

Add tests for:

- Quantity changes.
- Duplicate additions.
- Invalid UUIDs.
- Database constraints.

---

# 92. Third Development Sprint

Focus:

```text
decks
```

Implement:

```bash
mtg deck create
mtg deck list
mtg deck add
mtg deck remove
mtg deck show
mtg deck export
```

Add tests for:

- Board types.
- Quantities.
- Printing references.
- Deck deletion.
- Export formatting.

---

# 93. Fourth Development Sprint

Focus:

```text
quality of life
```

Possible work:

- Better search.
- Interactive selection.
- Config files.
- Better formatting.
- Import progress.
- Colorized CLI output.
- Backup command.
- More tests.

---

# 94. Fifth Development Sprint

Focus:

```text
local web app
```

Implement:

```text
mtg-web executable
REST API
HTML card search
collection page
deck page
```

Do not duplicate business logic.

---

# 95. Architectural Rule Summary

The project should maintain these rules:

1. MTGJSON UUID is the exact printing identity.
2. Personal data references printing UUIDs.
3. Imported data and personal data remain separate.
4. SQLite is the local source of truth after import.
5. Frontends do not contain business logic.
6. SQL is isolated behind storage/repository code.
7. Imports are transactional.
8. Updates must not destroy personal data.
9. CLI comes before web UI.
10. Docker is optional until deployment requires it.
11. Exact printing information is always preserved.
12. The project should favor learning and custom implementation over framework-heavy shortcuts.

---

# 96. Final Product Vision

The long-term goal is a local-first Magic: The Gathering ecosystem built around a reliable personal card database.

At its simplest, MTGTool is:

```text
a fast command-line collection and deck manager
```

At its next stage, it becomes:

```text
a local web-based collection and deck-building application
```

And in its most ambitious form, it can evolve into:

```text
a private Magic league platform for a friend group
```

while preserving the same core systems:

```text
MTGJSON
   |
   v
Import Pipeline
   |
   v
SQLite
   |
   v
Core C++ Services
   |
   +------ CLI
   |
   +------ Local Web UI
   |
   +------ Future League Server
```

That progression keeps the project useful at every milestone 
while leaving substantial room to experiment with C++, databases, 
web development, search systems, deck logic, and software architecture.

---

# 97. Immediate Next Step

The next implementation milestone should be the database foundation.

Recommended files:

```text
migrations/001_init.sql

src/core/storage/sqlite.hpp
src/core/storage/sqlite.cpp

src/core/storage/migrations.hpp
src/core/storage/migrations.cpp

src/cli/main.cpp
```

The first complete user-facing command should be:

```bash
mtg init
```

After that works reliably, the next target should be:

```bash
mtg import-set <SETCODE>
```

followed by:

```bash
mtg search "<CARD NAME>"
```

At that point, MTGTool will have a functioning foundation on which collection, 
deck, web, and league functionality can be built.
