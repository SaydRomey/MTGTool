#pragma once

#include "core/models.hpp"
#include "core/sqlite.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace mtg {

class Repository {
public:
    explicit Repository(SqliteDb& db) : db_(db) {}

    std::vector<SearchResult> search_printings(std::string_view query,
                                                const std::optional<std::string>& set_code,
                                                int limit);
    std::optional<Printing> printing_by_uuid(std::string_view uuid);

    void create_collection(std::string_view name);
    std::vector<NamedEntity> list_collections();
    void add_collection_item(std::string_view collection,
                             std::string_view uuid,
                             int quantity,
                             std::string_view finish,
                             std::string_view condition,
                             std::string_view notes);
    void remove_collection_item(std::string_view collection,
                                std::string_view uuid,
                                int quantity,
                                std::string_view finish,
                                std::string_view condition);
    std::vector<CollectionItem> collection_items(std::string_view collection);

    void create_deck(std::string_view name, std::string_view format, std::string_view notes);
    std::vector<NamedEntity> list_decks();
    void add_deck_card(std::string_view deck,
                       std::string_view uuid,
                       int quantity,
                       std::string_view finish,
                       std::string_view board);
    void remove_deck_card(std::string_view deck,
                          std::string_view uuid,
                          int quantity,
                          std::string_view finish,
                          std::string_view board);
    std::vector<DeckCard> deck_cards(std::string_view deck);
    std::optional<NamedEntity> deck_info(std::string_view deck);

private:
    SqliteDb& db_;
    std::int64_t collection_id(std::string_view name);
    std::int64_t deck_id(std::string_view name);
};

} // namespace mtg
