#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace mtg {

struct Printing {
    std::string uuid;
    std::string name;
    std::string set_code;
    std::string collector_number;
    std::string rarity;
    std::string language;
    std::string mana_cost;
    double mana_value = 0.0;
    std::string type_line;
    std::string rules_text;
    std::string power;
    std::string toughness;
    std::string loyalty;
    std::string color_identity_json = "[]";
    std::string colors_json = "[]";
    std::string finishes_json = "[]";
    std::string sku_ids_json = "{}";
    std::string identifiers_json = "{}";
    bool is_full_art = false;
    bool is_promo = false;
    std::string border_color;
    std::string frame_version;
    std::string security_stamp;
};

struct SearchResult {
    std::string uuid;
    std::string name;
    std::string set_code;
    std::string collector_number;
    std::string language;
    std::string rarity;
    std::string finishes_json;
};

struct CollectionItem {
    std::string uuid;
    std::string name;
    std::string set_code;
    std::string collector_number;
    std::string finish;
    std::string condition;
    int quantity = 0;
    std::string notes;
};

struct DeckCard {
    std::string uuid;
    std::string name;
    std::string set_code;
    std::string collector_number;
    std::string finish;
    std::string board;
    int quantity = 0;
};

struct NamedEntity {
    std::int64_t id = 0;
    std::string name;
    std::string extra;
};

} // namespace mtg
