#include "core/repository.hpp"

#include <algorithm>
#include <stdexcept>

namespace mtg {

std::vector<SearchResult> Repository::search_printings(std::string_view query,
                                                        const std::optional<std::string>& set_code,
                                                        int limit) {
    if (limit <= 0) limit = 20;
    if (limit > 500) limit = 500;

    const std::string sql = set_code
        ? "SELECT uuid,name,set_code,collector_number,language,rarity,finishes_json "
          "FROM printings WHERE name LIKE ? COLLATE NOCASE AND set_code=? COLLATE NOCASE "
          "ORDER BY name COLLATE NOCASE,set_code,collector_number LIMIT ?;"
        : "SELECT uuid,name,set_code,collector_number,language,rarity,finishes_json "
          "FROM printings WHERE name LIKE ? COLLATE NOCASE "
          "ORDER BY name COLLATE NOCASE,set_code,collector_number LIMIT ?;";

    SqliteStmt stmt(db_, sql);
    stmt.bind(1, "%" + std::string(query) + "%");
    int index = 2;
    if (set_code) stmt.bind(index++, *set_code);
    stmt.bind(index, limit);

    std::vector<SearchResult> results;
    while (stmt.step()) {
        results.push_back(SearchResult{
            stmt.col_text(0), stmt.col_text(1), stmt.col_text(2), stmt.col_text(3),
            stmt.col_text(4), stmt.col_text(5), stmt.col_text(6)});
    }
    return results;
}

std::optional<Printing> Repository::printing_by_uuid(std::string_view uuid) {
    SqliteStmt stmt(db_,
        "SELECT uuid,name,set_code,collector_number,rarity,language,mana_cost,mana_value,type_line,rules_text,"
        "power,toughness,loyalty,color_identity_json,colors_json,finishes_json,sku_ids_json,identifiers_json,is_full_art,is_promo,"
        "border_color,frame_version,security_stamp FROM printings WHERE uuid=?;");
    stmt.bind(1, uuid);
    if (!stmt.step()) return std::nullopt;
    Printing p;
    p.uuid = stmt.col_text(0);
    p.name = stmt.col_text(1);
    p.set_code = stmt.col_text(2);
    p.collector_number = stmt.col_text(3);
    p.rarity = stmt.col_text(4);
    p.language = stmt.col_text(5);
    p.mana_cost = stmt.col_text(6);
    p.mana_value = stmt.col_double(7);
    p.type_line = stmt.col_text(8);
    p.rules_text = stmt.col_text(9);
    p.power = stmt.col_text(10);
    p.toughness = stmt.col_text(11);
    p.loyalty = stmt.col_text(12);
    p.color_identity_json = stmt.col_text(13);
    p.colors_json = stmt.col_text(14);
    p.finishes_json = stmt.col_text(15);
    p.sku_ids_json = stmt.col_text(16);
    p.identifiers_json = stmt.col_text(17);
    p.is_full_art = stmt.col_int(18) != 0;
    p.is_promo = stmt.col_int(19) != 0;
    p.border_color = stmt.col_text(20);
    p.frame_version = stmt.col_text(21);
    p.security_stamp = stmt.col_text(22);
    return p;
}

std::int64_t Repository::collection_id(std::string_view name) {
    SqliteStmt stmt(db_, "SELECT id FROM collections WHERE name=? COLLATE NOCASE;");
    stmt.bind(1, name);
    if (!stmt.step()) throw std::runtime_error("collection not found: " + std::string(name));
    return stmt.col_i64(0);
}

void Repository::create_collection(std::string_view name) {
    SqliteStmt stmt(db_, "INSERT INTO collections(name) VALUES(?);");
    stmt.bind(1, name);
    stmt.step();
}

std::vector<NamedEntity> Repository::list_collections() {
    SqliteStmt stmt(db_, "SELECT id,name,'' FROM collections ORDER BY name COLLATE NOCASE;");
    std::vector<NamedEntity> out;
    while (stmt.step()) out.push_back({stmt.col_i64(0), stmt.col_text(1), stmt.col_text(2)});
    return out;
}

void Repository::add_collection_item(std::string_view collection,
                                     std::string_view uuid,
                                     int quantity,
                                     std::string_view finish,
                                     std::string_view condition,
                                     std::string_view notes) {
    if (quantity <= 0) throw std::runtime_error("quantity must be greater than zero");
    const auto id = collection_id(collection);
    SqliteStmt stmt(db_,
        "INSERT INTO collection_items(collection_id,printing_uuid,finish,condition,quantity,notes) "
        "VALUES(?,?,?,?,?,?) "
        "ON CONFLICT(collection_id,printing_uuid,finish,condition) DO UPDATE SET "
        "quantity=collection_items.quantity+excluded.quantity, "
        "notes=CASE WHEN excluded.notes='' THEN collection_items.notes ELSE excluded.notes END;");
    stmt.bind(1, id);
    stmt.bind(2, uuid);
    stmt.bind(3, finish);
    stmt.bind(4, condition);
    stmt.bind(5, quantity);
    stmt.bind(6, notes);
    stmt.step();
}

void Repository::remove_collection_item(std::string_view collection,
                                        std::string_view uuid,
                                        int quantity,
                                        std::string_view finish,
                                        std::string_view condition) {
    if (quantity <= 0) throw std::runtime_error("quantity must be greater than zero");
    const auto id = collection_id(collection);
    SqliteTransaction tx(db_);
    SqliteStmt update(db_,
        "UPDATE collection_items SET quantity=quantity-? "
        "WHERE collection_id=? AND printing_uuid=? AND finish=? AND condition=?;");
    update.bind(1, quantity);
    update.bind(2, id);
    update.bind(3, uuid);
    update.bind(4, finish);
    update.bind(5, condition);
    update.step();
    SqliteStmt cleanup(db_,
        "DELETE FROM collection_items WHERE collection_id=? AND printing_uuid=? AND finish=? AND condition=? AND quantity<=0;");
    cleanup.bind(1, id);
    cleanup.bind(2, uuid);
    cleanup.bind(3, finish);
    cleanup.bind(4, condition);
    cleanup.step();
    tx.commit();
}

std::vector<CollectionItem> Repository::collection_items(std::string_view collection) {
    const auto id = collection_id(collection);
    SqliteStmt stmt(db_,
        "SELECT p.uuid,p.name,p.set_code,p.collector_number,ci.finish,ci.condition,ci.quantity,ci.notes "
        "FROM collection_items ci JOIN printings p ON p.uuid=ci.printing_uuid "
        "WHERE ci.collection_id=? ORDER BY p.name COLLATE NOCASE,p.set_code,p.collector_number;");
    stmt.bind(1, id);
    std::vector<CollectionItem> out;
    while (stmt.step()) {
        out.push_back({stmt.col_text(0), stmt.col_text(1), stmt.col_text(2), stmt.col_text(3),
                       stmt.col_text(4), stmt.col_text(5), stmt.col_int(6), stmt.col_text(7)});
    }
    return out;
}

std::int64_t Repository::deck_id(std::string_view name) {
    SqliteStmt stmt(db_, "SELECT id FROM decks WHERE name=? COLLATE NOCASE;");
    stmt.bind(1, name);
    if (!stmt.step()) throw std::runtime_error("deck not found: " + std::string(name));
    return stmt.col_i64(0);
}

void Repository::create_deck(std::string_view name, std::string_view format, std::string_view notes) {
    SqliteStmt stmt(db_, "INSERT INTO decks(name,format,notes) VALUES(?,?,?);");
    stmt.bind(1, name);
    stmt.bind(2, format);
    stmt.bind(3, notes);
    stmt.step();
}

std::vector<NamedEntity> Repository::list_decks() {
    SqliteStmt stmt(db_, "SELECT id,name,format FROM decks ORDER BY name COLLATE NOCASE;");
    std::vector<NamedEntity> out;
    while (stmt.step()) out.push_back({stmt.col_i64(0), stmt.col_text(1), stmt.col_text(2)});
    return out;
}

void Repository::add_deck_card(std::string_view deck,
                               std::string_view uuid,
                               int quantity,
                               std::string_view finish,
                               std::string_view board) {
    if (quantity <= 0) throw std::runtime_error("quantity must be greater than zero");
    const auto id = deck_id(deck);
    SqliteStmt stmt(db_,
        "INSERT INTO deck_cards(deck_id,printing_uuid,finish,board,quantity) VALUES(?,?,?,?,?) "
        "ON CONFLICT(deck_id,printing_uuid,finish,board) DO UPDATE SET quantity=deck_cards.quantity+excluded.quantity;");
    stmt.bind(1, id);
    stmt.bind(2, uuid);
    stmt.bind(3, finish);
    stmt.bind(4, board);
    stmt.bind(5, quantity);
    stmt.step();
    SqliteStmt touch(db_, "UPDATE decks SET updated_at=datetime('now') WHERE id=?;");
    touch.bind(1, id);
    touch.step();
}

void Repository::remove_deck_card(std::string_view deck,
                                  std::string_view uuid,
                                  int quantity,
                                  std::string_view finish,
                                  std::string_view board) {
    if (quantity <= 0) throw std::runtime_error("quantity must be greater than zero");
    const auto id = deck_id(deck);
    SqliteTransaction tx(db_);
    SqliteStmt update(db_,
        "UPDATE deck_cards SET quantity=quantity-? WHERE deck_id=? AND printing_uuid=? AND finish=? AND board=?;");
    update.bind(1, quantity);
    update.bind(2, id);
    update.bind(3, uuid);
    update.bind(4, finish);
    update.bind(5, board);
    update.step();
    SqliteStmt cleanup(db_,
        "DELETE FROM deck_cards WHERE deck_id=? AND printing_uuid=? AND finish=? AND board=? AND quantity<=0;");
    cleanup.bind(1, id);
    cleanup.bind(2, uuid);
    cleanup.bind(3, finish);
    cleanup.bind(4, board);
    cleanup.step();
    SqliteStmt touch(db_, "UPDATE decks SET updated_at=datetime('now') WHERE id=?;");
    touch.bind(1, id);
    touch.step();
    tx.commit();
}

std::vector<DeckCard> Repository::deck_cards(std::string_view deck) {
    const auto id = deck_id(deck);
    SqliteStmt stmt(db_,
        "SELECT p.uuid,p.name,p.set_code,p.collector_number,dc.finish,dc.board,dc.quantity "
        "FROM deck_cards dc JOIN printings p ON p.uuid=dc.printing_uuid "
        "WHERE dc.deck_id=? "
        "ORDER BY CASE dc.board WHEN 'commander' THEN 0 WHEN 'main' THEN 1 WHEN 'side' THEN 2 ELSE 3 END,"
        "p.name COLLATE NOCASE,p.set_code,p.collector_number;");
    stmt.bind(1, id);
    std::vector<DeckCard> out;
    while (stmt.step()) {
        out.push_back({stmt.col_text(0), stmt.col_text(1), stmt.col_text(2), stmt.col_text(3),
                       stmt.col_text(4), stmt.col_text(5), stmt.col_int(6)});
    }
    return out;
}

std::optional<NamedEntity> Repository::deck_info(std::string_view deck) {
    SqliteStmt stmt(db_, "SELECT id,name,format FROM decks WHERE name=? COLLATE NOCASE;");
    stmt.bind(1, deck);
    if (!stmt.step()) return std::nullopt;
    return NamedEntity{stmt.col_i64(0), stmt.col_text(1), stmt.col_text(2)};
}

} // namespace mtg
