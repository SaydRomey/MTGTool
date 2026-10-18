PRAGMA foreign_keys = ON;

CREATE TABLE IF NOT EXISTS sets (
    code TEXT PRIMARY KEY,
    name TEXT NOT NULL,
    release_date TEXT,
    type TEXT,
    total_set_size INTEGER,
    base_set_size INTEGER,
    is_online_only INTEGER NOT NULL DEFAULT 0,
    is_foil_only INTEGER NOT NULL DEFAULT 0,
    updated_at TEXT NOT NULL DEFAULT (datetime('now'))
);

CREATE TABLE IF NOT EXISTS printings (
    uuid TEXT PRIMARY KEY,
    name TEXT NOT NULL,
    set_code TEXT NOT NULL REFERENCES sets(code) ON UPDATE CASCADE ON DELETE RESTRICT,
    collector_number TEXT NOT NULL,
    rarity TEXT,
    language TEXT,
    mana_cost TEXT,
    mana_value REAL,
    type_line TEXT,
    rules_text TEXT,
    power TEXT,
    toughness TEXT,
    loyalty TEXT,
    color_identity_json TEXT NOT NULL DEFAULT '[]',
    colors_json TEXT NOT NULL DEFAULT '[]',
    finishes_json TEXT NOT NULL DEFAULT '[]',
    sku_ids_json TEXT NOT NULL DEFAULT '{}',
    identifiers_json TEXT NOT NULL DEFAULT '{}',
    is_full_art INTEGER NOT NULL DEFAULT 0,
    is_promo INTEGER NOT NULL DEFAULT 0,
    border_color TEXT,
    frame_version TEXT,
    security_stamp TEXT,
    updated_at TEXT NOT NULL DEFAULT (datetime('now'))
);

CREATE INDEX IF NOT EXISTS idx_printings_name ON printings(name COLLATE NOCASE);
CREATE INDEX IF NOT EXISTS idx_printings_set_num ON printings(set_code, collector_number);
CREATE INDEX IF NOT EXISTS idx_printings_language ON printings(language);

CREATE TABLE IF NOT EXISTS collections (
    id INTEGER PRIMARY KEY,
    name TEXT NOT NULL UNIQUE COLLATE NOCASE,
    created_at TEXT NOT NULL DEFAULT (datetime('now'))
);

CREATE TABLE IF NOT EXISTS collection_items (
    collection_id INTEGER NOT NULL REFERENCES collections(id) ON DELETE CASCADE,
    printing_uuid TEXT NOT NULL REFERENCES printings(uuid) ON DELETE RESTRICT,
    finish TEXT NOT NULL DEFAULT 'nonfoil',
    condition TEXT NOT NULL DEFAULT 'NM',
    quantity INTEGER NOT NULL CHECK(quantity >= 0),
    notes TEXT NOT NULL DEFAULT '',
    PRIMARY KEY (collection_id, printing_uuid, finish, condition)
);

CREATE TABLE IF NOT EXISTS decks (
    id INTEGER PRIMARY KEY,
    name TEXT NOT NULL UNIQUE COLLATE NOCASE,
    format TEXT NOT NULL DEFAULT '',
    notes TEXT NOT NULL DEFAULT '',
    created_at TEXT NOT NULL DEFAULT (datetime('now')),
    updated_at TEXT NOT NULL DEFAULT (datetime('now'))
);

CREATE TABLE IF NOT EXISTS deck_cards (
    deck_id INTEGER NOT NULL REFERENCES decks(id) ON DELETE CASCADE,
    printing_uuid TEXT NOT NULL REFERENCES printings(uuid) ON DELETE RESTRICT,
    finish TEXT NOT NULL DEFAULT 'nonfoil',
    board TEXT NOT NULL DEFAULT 'main' CHECK(board IN ('main','side','commander','maybeboard')),
    quantity INTEGER NOT NULL CHECK(quantity > 0),
    PRIMARY KEY (deck_id, printing_uuid, finish, board)
);

CREATE TABLE IF NOT EXISTS imports (
    id INTEGER PRIMARY KEY,
    source TEXT NOT NULL,
    set_code TEXT,
    source_version TEXT,
    imported_at TEXT NOT NULL DEFAULT (datetime('now')),
    card_count INTEGER NOT NULL DEFAULT 0,
    source_path TEXT NOT NULL DEFAULT ''
);
