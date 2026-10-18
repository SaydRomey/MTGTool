#include "core/mtgjson.hpp"

#include "core/json.hpp"
#include "core/xz.hpp"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <iostream>
#include <stdexcept>

namespace mtg {
namespace {

std::string uppercase(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
        return static_cast<char>(std::toupper(c));
    });
    return value;
}

const json::Value& required_object_member(const json::Value& object, std::string_view key) {
    const json::Value* value = object.find(key);
    if (!value) throw std::runtime_error("MTGJSON field missing: " + std::string(key));
    return *value;
}

const json::Value& locate_set(const json::Value& root) {
    if (!root.is_object()) throw std::runtime_error("MTGJSON root must be an object");
    const auto& data = required_object_member(root, "data");
    if (!data.is_object()) throw std::runtime_error("MTGJSON data must be an object");
    if (data.find("cards")) return data;

    // Supports an AllPrintings-like wrapper when it contains exactly one selected set.
    const auto& object = data.as_object();
    if (object.size() == 1 && object.begin()->second.is_object() && object.begin()->second.find("cards")) {
        return object.begin()->second;
    }
    throw std::runtime_error("MTGJSON file does not contain a single set object");
}

std::string text_field(const json::Value& card, std::string_view primary, std::string_view fallback = {}) {
    std::string result = json::string_or(card, primary);
    if (result.empty() && !fallback.empty()) result = json::string_or(card, fallback);
    return result;
}

} // namespace

ImportResult MtgJsonImporter::import_set_download(const std::string& raw_set_code,
                                                   const std::filesystem::path& cache_dir,
                                                   bool force_download,
                                                   bool keep_download) {
    const std::string set_code = uppercase(raw_set_code);
    if (set_code.empty()) throw std::runtime_error("set code cannot be empty");

    std::filesystem::create_directories(cache_dir);
    const auto archive = cache_dir / (set_code + ".json.xz");
    const std::string url = "https://mtgjson.com/api/v5/" + set_code + ".json.xz";

    if (force_download || !std::filesystem::exists(archive)) {
        std::cout << "Downloading " << url << "\n";
        HttpClient{}.download_file(url, archive);
    } else {
        std::cout << "Using cached " << archive.string() << "\n";
    }

    std::cout << "Decompressing " << archive.string() << "\n";
    const std::string json_text = decompress_xz_file(archive);
    auto result = import_json(json_text, "MTGJSON", archive.string());

    if (!keep_download) {
        std::error_code ec;
        std::filesystem::remove(archive, ec);
    }
    return result;
}

ImportResult MtgJsonImporter::import_file(const std::filesystem::path& path) {
    std::string text;
    if (path.extension() == ".xz") text = decompress_xz_file(path);
    else text = read_text_file(path);
    return import_json(text, "MTGJSON", path.string());
}

ImportResult MtgJsonImporter::import_json(std::string_view json_text,
                                          std::string_view source,
                                          std::string_view source_path) {
    const json::Value root = json::parse(json_text);
    const json::Value& set = locate_set(root);

    const std::string code = uppercase(json::string_or(set, "code"));
    const std::string name = json::string_or(set, "name");
    if (code.empty() || name.empty()) throw std::runtime_error("set is missing code or name");

    const json::Value* cards_value = set.find("cards");
    if (!cards_value || !cards_value->is_array()) throw std::runtime_error("set cards field is missing or not an array");

    SqliteTransaction tx(db_);
    SqliteStmt upsert_set(db_,
        "INSERT INTO sets(code,name,release_date,type,total_set_size,base_set_size,is_online_only,is_foil_only,updated_at) "
        "VALUES(?,?,?,?,?,?,?,?,datetime('now')) "
        "ON CONFLICT(code) DO UPDATE SET name=excluded.name,release_date=excluded.release_date,type=excluded.type,"
        "total_set_size=excluded.total_set_size,base_set_size=excluded.base_set_size,"
        "is_online_only=excluded.is_online_only,is_foil_only=excluded.is_foil_only,updated_at=datetime('now');");
    upsert_set.bind(1, code);
    upsert_set.bind(2, name);
    upsert_set.bind(3, json::string_or(set, "releaseDate"));
    upsert_set.bind(4, json::string_or(set, "type"));
    upsert_set.bind(5, json::int_or(set, "totalSetSize"));
    upsert_set.bind(6, json::int_or(set, "baseSetSize"));
    upsert_set.bind(7, json::bool_or(set, "isOnlineOnly") ? 1 : 0);
    upsert_set.bind(8, json::bool_or(set, "isFoilOnly") ? 1 : 0);
    upsert_set.step();

    SqliteStmt upsert_card(db_,
        "INSERT INTO printings(uuid,name,set_code,collector_number,rarity,language,mana_cost,mana_value,type_line,rules_text,"
        "power,toughness,loyalty,color_identity_json,colors_json,finishes_json,sku_ids_json,identifiers_json,is_full_art,is_promo,"
        "border_color,frame_version,security_stamp,updated_at) "
        "VALUES(?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,datetime('now')) "
        "ON CONFLICT(uuid) DO UPDATE SET name=excluded.name,set_code=excluded.set_code,collector_number=excluded.collector_number,"
        "rarity=excluded.rarity,language=excluded.language,mana_cost=excluded.mana_cost,mana_value=excluded.mana_value,"
        "type_line=excluded.type_line,rules_text=excluded.rules_text,power=excluded.power,toughness=excluded.toughness,"
        "loyalty=excluded.loyalty,color_identity_json=excluded.color_identity_json,colors_json=excluded.colors_json,"
        "finishes_json=excluded.finishes_json,sku_ids_json=excluded.sku_ids_json,identifiers_json=excluded.identifiers_json,is_full_art=excluded.is_full_art,"
        "is_promo=excluded.is_promo,border_color=excluded.border_color,frame_version=excluded.frame_version,"
        "security_stamp=excluded.security_stamp,updated_at=datetime('now');");

    int count = 0;
    for (const auto& card : cards_value->as_array()) {
        if (!card.is_object()) continue;
        const std::string uuid = json::string_or(card, "uuid");
        const std::string card_name = json::string_or(card, "name");
        const std::string number = json::string_or(card, "number");
        if (uuid.empty() || card_name.empty() || number.empty()) continue;

        upsert_card.reset();
        upsert_card.clear_bindings();
        int i = 1;
        upsert_card.bind(i++, uuid);
        upsert_card.bind(i++, card_name);
        upsert_card.bind(i++, code);
        upsert_card.bind(i++, number);
        upsert_card.bind(i++, json::string_or(card, "rarity"));
        upsert_card.bind(i++, json::string_or(card, "language"));
        upsert_card.bind(i++, json::string_or(card, "manaCost"));
        upsert_card.bind(i++, json::number_or(card, "manaValue", json::number_or(card, "convertedManaCost", 0.0)));
        upsert_card.bind(i++, text_field(card, "type", "typeLine"));
        upsert_card.bind(i++, text_field(card, "text", "oracleText"));
        upsert_card.bind(i++, json::string_or(card, "power"));
        upsert_card.bind(i++, json::string_or(card, "toughness"));
        upsert_card.bind(i++, json::string_or(card, "loyalty"));
        upsert_card.bind(i++, json::dump_or(card, "colorIdentity", "[]"));
        upsert_card.bind(i++, json::dump_or(card, "colors", "[]"));
        upsert_card.bind(i++, json::dump_or(card, "finishes", "[]"));
        upsert_card.bind(i++, json::dump_or(card, "skuIds", "{}"));
        upsert_card.bind(i++, json::dump_or(card, "identifiers", "{}"));
        upsert_card.bind(i++, json::bool_or(card, "isFullArt") ? 1 : 0);
        upsert_card.bind(i++, json::bool_or(card, "isPromo") ? 1 : 0);
        upsert_card.bind(i++, json::string_or(card, "borderColor"));
        upsert_card.bind(i++, json::string_or(card, "frameVersion"));
        upsert_card.bind(i++, json::string_or(card, "securityStamp"));
        upsert_card.step();
        ++count;
    }

    std::string source_version;
    if (const auto* meta = root.find("meta"); meta && meta->is_object()) {
        source_version = json::string_or(*meta, "version");
        if (source_version.empty()) source_version = json::string_or(*meta, "date");
    }

    SqliteStmt record(db_,
        "INSERT INTO imports(source,set_code,source_version,card_count,source_path) VALUES(?,?,?,?,?);");
    record.bind(1, source);
    record.bind(2, code);
    record.bind(3, source_version);
    record.bind(4, count);
    record.bind(5, source_path);
    record.step();

    tx.commit();
    return ImportResult{code, name, count};
}

} // namespace mtg
