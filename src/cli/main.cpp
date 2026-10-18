#include "core/migrations.hpp"
#include "core/mtgjson.hpp"
#include "core/repository.hpp"
#include "core/sqlite.hpp"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace
{

struct Config
{
    std::string db = "data/mtg.sqlite";
    std::string migrations = "migrations";
};

bool has_flag(const std::vector<std::string>& args, std::string_view flag)
{
    return (std::find(args.begin(), args.end(), flag) != args.end());
}

std::optional<std::string> option(const std::vector<std::string>& args, std::string_view key)
{
    for (std::size_t i = 0; i + 1 < args.size(); ++i)
    {
        if (args[i] == key)
            return args[i + 1];
    }
    return (std::nullopt);
}

int int_option(const std::vector<std::string>& args, std::string_view key, int fallback)
{
    const auto value = option(args, key);
    if (!value)
        return fallback;
    try
    {
        return (std::stoi(*value));
    }
    catch (...)
    {
        throw std::runtime_error("invalid integer for " + std::string(key) + ": " + *value);
    }
}

std::vector<std::string> strip_global_options(int argc, char** argv, Config& config)
{
    std::vector<std::string> out;
    for (int i = 1; i < argc; ++i)
    {
        const std::string arg = argv[i];
        if (arg == "--db")
        {
            if (i + 1 >= argc)
                throw std::runtime_error("--db requires a path");
            config.db = argv[++i];
        }
        else if (arg == "--migrations")
        {
            if (i + 1 >= argc)
                throw std::runtime_error("--migrations requires a path");
            config.migrations = argv[++i];
        }
        else
        {
            out.push_back(arg);
        }
    }
    return (out);
}

void require_schema(mtg::SqliteDb& db)
{
    if (!db.table_exists("printings"))
    {
        throw std::runtime_error("database is not initialized; run `mtg init` first");
    }
}

void print_usage(void)
{
    std::cout << R"(MTGTool - local MTG collection and deck manager

Usage:
  mtg [--db PATH] [--migrations DIR] <command> ...

Database:
  mtg init

MTGJSON:
  mtg import-set SETCODE [--force] [--no-cache]
  mtg import-file PATH.json[.xz]

Cards:
  mtg search QUERY [--set SETCODE] [--limit N]
  mtg printing show UUID

Collections:
  mtg collection create NAME
  mtg collection list
  mtg collection show NAME
  mtg collection add NAME --uuid UUID [--qty N] [--finish F] [--condition C] [--notes TEXT]
  mtg collection remove NAME --uuid UUID [--qty N] [--finish F] [--condition C]

Decks:
  mtg deck create NAME [--format FORMAT] [--notes TEXT]
  mtg deck list
  mtg deck show NAME
  mtg deck add NAME --uuid UUID [--qty N] [--finish F] [--board main|side|commander|maybeboard]
  mtg deck remove NAME --uuid UUID [--qty N] [--finish F] [--board ...]
  mtg deck export NAME [--format text|csv|json] [--out PATH]

Examples:
  mtg init
  mtg import-set CMM
  mtg search "Sol Ring" --set CMM
  mtg collection create "Main"
  mtg deck create "Atraxa" --format commander
)";
}

std::string csv_escape(std::string_view value)
{
    bool quote = false;
    for (char c : value)
        if (c == ',' || c == '"' || c == '\n' || c == '\r')
            quote = true;
    if (!quote)
        return (std::string(value));

    std::string out = "\"";
    for (char c : value)
    {
        if (c == '"')
            out += "\"\"";
        else
            out += c;
    }
    out += '"';
    return (out);
}

std::string json_escape(std::string_view value)
{
    std::string out;
    for (char raw : value)
    {
        const unsigned char c = static_cast<unsigned char>(raw);
        switch (c) {
        case '"': out += "\\\""; break;
        case '\\': out += "\\\\"; break;
        case '\n': out += "\\n"; break;
        case '\r': out += "\\r"; break;
        case '\t': out += "\\t"; break;
        default:
            if (c < 0x20)
            {
                std::ostringstream ss;
                ss << "\\u" << std::hex << std::setw(4) << std::setfill('0') << static_cast<int>(c);
                out += ss.str();
            }
            else
                out += static_cast<char>(c);
        }
    }
    return (out);
}

std::string deck_export(const std::vector<mtg::DeckCard>& cards, std::string_view format)
{
    std::ostringstream out;
    if (format == "csv")
    {
        out << "board,quantity,name,set,collector_number,finish,uuid\n";
        for (const auto& c : cards)
        {
            out << csv_escape(c.board) << ',' << c.quantity << ',' << csv_escape(c.name) << ','
                << csv_escape(c.set_code) << ',' << csv_escape(c.collector_number) << ','
                << csv_escape(c.finish) << ',' << csv_escape(c.uuid) << '\n';
        }
    }
    else if (format == "json")
    {
        out << "[\n";
        for (std::size_t i = 0; i < cards.size(); ++i)
        {
            const auto& c = cards[i];
            out << "  {\"board\":\"" << json_escape(c.board) << "\",\"quantity\":" << c.quantity
                << ",\"name\":\"" << json_escape(c.name) << "\",\"set\":\"" << json_escape(c.set_code)
                << "\",\"collectorNumber\":\"" << json_escape(c.collector_number)
                << "\",\"finish\":\"" << json_escape(c.finish) << "\",\"uuid\":\"" << json_escape(c.uuid) << "\"}";
            if (i + 1 != cards.size())
                out << ',';
            out << '\n';
        }
        out << "]\n";
    }
    else if (format == "text")
    {
        std::string current;
        for (const auto& c : cards)
        {
            if (c.board != current)
            {
                if (!current.empty())
                    out << '\n';
                current = c.board;
                out << "# " << current << "\n";
            }
            out << c.quantity << ' ' << c.name << " [" << c.set_code << " #" << c.collector_number << "]";
            if (!c.finish.empty())
                out << " (" << c.finish << ')';
            out << '\n';
        }
    }
    else
    {
        throw std::runtime_error("unknown export format: " + std::string(format));
    }
    return (out.str());
}

void command_init(const Config& config)
{
    mtg::SqliteDb db(config.db);
    mtg::MigrationRunner::apply(db, config.migrations);
    std::cout << "Initialized " << config.db << "\n";
}

void command_import_set(const Config& config, const std::vector<std::string>& args)
{
    if (args.size() < 2)
        throw std::runtime_error("usage: mtg import-set SETCODE [--force] [--no-cache]");

    mtg::SqliteDb db(config.db);
    require_schema(db);
    const std::filesystem::path cache = std::filesystem::path(config.db).parent_path() / "cache";
    mtg::MtgJsonImporter importer(db);
    const auto result = importer.import_set_download(args[1], cache, has_flag(args, "--force"), !has_flag(args, "--no-cache"));
    std::cout << "Imported " << result.card_count << " cards from " << result.set_code << " - " << result.set_name << "\n";
}

void command_import_file(const Config& config, const std::vector<std::string>& args)
{
    if (args.size() < 2)
        throw std::runtime_error("usage: mtg import-file PATH.json[.xz]");

    mtg::SqliteDb db(config.db);
    require_schema(db);
    mtg::MtgJsonImporter importer(db);
    const auto result = importer.import_file(args[1]);
    std::cout << "Imported " << result.card_count << " cards from " << result.set_code << " - " << result.set_name << "\n";
}

void command_search(const Config& config, const std::vector<std::string>& args)
{
    if (args.size() < 2)
        throw std::runtime_error("usage: mtg search QUERY [--set SET] [--limit N]");

    mtg::SqliteDb db(config.db);
    require_schema(db);
    mtg::Repository repo(db);
    const auto set = option(args, "--set");
    const int limit = int_option(args, "--limit", 20);
    const auto rows = repo.search_printings(args[1], set, limit);
    std::cout << std::left << std::setw(5) << "SET" << std::setw(10) << "NUMBER" << std::setw(12) << "LANG"
              << std::setw(12) << "RARITY" << std::setw(32) << "NAME" << "UUID\n";
    std::cout << std::string(110, '-') << '\n';
    for (const auto& r : rows)
    {
        std::cout << std::left << std::setw(5) << r.set_code << std::setw(10) << r.collector_number
                  << std::setw(12) << r.language << std::setw(12) << r.rarity
                  << std::setw(32) << r.name.substr(0, 31) << r.uuid << '\n';
    }
    std::cout << rows.size() << " result(s)\n";
}

void command_printing(const Config& config, const std::vector<std::string>& args)
{
    if (args.size() < 3 || args[1] != "show")
        throw std::runtime_error("usage: mtg printing show UUID");

    mtg::SqliteDb db(config.db);
    require_schema(db);
    mtg::Repository repo(db);
    const auto p = repo.printing_by_uuid(args[2]);
    if (!p)
        throw std::runtime_error("printing not found: " + args[2]);
    std::cout << p->name << "\n"
              << "UUID: " << p->uuid << "\n"
              << "Set: " << p->set_code << " #" << p->collector_number << "\n"
              << "Language: " << p->language << "\n"
              << "Rarity: " << p->rarity << "\n"
              << "Mana cost: " << p->mana_cost << "\n"
              << "Mana value: " << p->mana_value << "\n"
              << "Type: " << p->type_line << "\n"
              << "Text: " << p->rules_text << "\n"
              << "P/T/L: " << p->power << '/' << p->toughness << '/' << p->loyalty << "\n"
              << "Colors: " << p->colors_json << "\n"
              << "Color identity: " << p->color_identity_json << "\n"
              << "Finishes: " << p->finishes_json << "\n"
              << "Finish SKU IDs: " << p->sku_ids_json << "\n"
              << "Identifiers: " << p->identifiers_json << "\n";
}

void command_collection(const Config& config, const std::vector<std::string>& args)
{
    if (args.size() < 2)
        throw std::runtime_error("usage: mtg collection <create|list|show|add|remove> ...");
    mtg::SqliteDb db(config.db);
    require_schema(db);
    mtg::Repository repo(db);
    const std::string& sub = args[1];

    if (sub == "create")
    {
        if (args.size() < 3)
            throw std::runtime_error("usage: mtg collection create NAME");
        repo.create_collection(args[2]);
        std::cout << "Created collection " << args[2] << "\n";
    }
    else if (sub == "list")
    {
        for (const auto& c : repo.list_collections())
            std::cout << c.id << '\t' << c.name << '\n';
    }
    else if (sub == "show")
    {
        if (args.size() < 3)
            throw std::runtime_error("usage: mtg collection show NAME");
        int total = 0;
        for (const auto& c : repo.collection_items(args[2]))
        {
            total += c.quantity;
            std::cout << c.quantity << "x " << c.name << " [" << c.set_code << " #" << c.collector_number << "] "
                      << c.finish << ' ' << c.condition;
            if (!c.notes.empty())
                std::cout << " - " << c.notes;
            std::cout << "\n  " << c.uuid << '\n';
        }
        std::cout << "Total physical cards: " << total << '\n';
    }
    else if (sub == "add" || sub == "remove")
    {
        if (args.size() < 3) throw std::runtime_error("collection name is required");
        const auto uuid = option(args, "--uuid");
        if (!uuid)
            throw std::runtime_error("--uuid is required");
        const int qty = int_option(args, "--qty", 1);
        const std::string finish = option(args, "--finish").value_or("nonfoil");
        const std::string condition = option(args, "--condition").value_or("NM");
        if (sub == "add")
        {
            repo.add_collection_item(args[2], *uuid, qty, finish, condition, option(args, "--notes").value_or(""));
            std::cout << "Added " << qty << " card(s)\n";
        }
        else
        {
            repo.remove_collection_item(args[2], *uuid, qty, finish, condition);
            std::cout << "Removed up to " << qty << " card(s)\n";
        }
    }
    else
    {
        throw std::runtime_error("unknown collection command: " + sub);
    }
}

void command_deck(const Config& config, const std::vector<std::string>& args)
{
    if (args.size() < 2)
        throw std::runtime_error("usage: mtg deck <create|list|show|add|remove|export> ...");
    mtg::SqliteDb db(config.db);
    require_schema(db);
    mtg::Repository repo(db);
    const std::string& sub = args[1];

    if (sub == "create")
    {
        if (args.size() < 3)
            throw std::runtime_error("usage: mtg deck create NAME [--format FORMAT]");
        repo.create_deck(args[2], option(args, "--format").value_or(""), option(args, "--notes").value_or(""));
        std::cout << "Created deck " << args[2] << "\n";
    }
    else if (sub == "list")
    {
        for (const auto& d : repo.list_decks())
        {
            std::cout << d.id << '\t' << d.name;
            if (!d.extra.empty()) std::cout << "\t" << d.extra;
            std::cout << '\n';
        }
    }
    else if (sub == "show")
    {
        if (args.size() < 3)
            throw std::runtime_error("usage: mtg deck show NAME");
        const auto info = repo.deck_info(args[2]);
        if (!info)
            throw std::runtime_error("deck not found: " + args[2]);
        std::cout << info->name;
        if (!info->extra.empty())
            std::cout << " (" << info->extra << ')';
        std::cout << "\n\n" << deck_export(repo.deck_cards(args[2]), "text");
    }
    else if (sub == "add" || sub == "remove")
    {
        if (args.size() < 3)
            throw std::runtime_error("deck name is required");
        const auto uuid = option(args, "--uuid");
        if (!uuid)
            throw std::runtime_error("--uuid is required");
        const int qty = int_option(args, "--qty", 1);
        const std::string finish = option(args, "--finish").value_or("nonfoil");
        const std::string board = option(args, "--board").value_or("main");
        if (sub == "add")
        {
            repo.add_deck_card(args[2], *uuid, qty, finish, board);
            std::cout << "Added " << qty << " card(s) to " << args[2] << '\n';
        }
        else
        {
            repo.remove_deck_card(args[2], *uuid, qty, finish, board);
            std::cout << "Removed up to " << qty << " card(s) from " << args[2] << '\n';
        }
    }
    else if (sub == "export")
    {
        if (args.size() < 3)
            throw std::runtime_error("usage: mtg deck export NAME [--format text|csv|json] [--out PATH]");
        const std::string format = option(args, "--format").value_or("text");
        const std::string output = deck_export(repo.deck_cards(args[2]), format);
        const auto path = option(args, "--out");
        if (path)
        {
            std::ofstream file(*path, std::ios::binary);
            if (!file)
                throw std::runtime_error("cannot open export path: " + *path);
            file << output;
            std::cout << "Exported to " << *path << '\n';
        }
        else
        {
            std::cout << output;
        }
    }
    else
    {
        throw std::runtime_error("unknown deck command: " + sub);
    }
}

} // namespace

int main(int argc, char** argv)
{
    try
    {
        Config config;
        const auto args = strip_global_options(argc, argv, config);
        if (args.empty() || args[0] == "help" || args[0] == "--help" || args[0] == "-h")
        {
            print_usage();
            return (0);
        }

        const std::string& command = args[0];
        if (command == "init")
            command_init(config);
        else if (command == "import-set")
            command_import_set(config, args);
        else if (command == "import-file")
            command_import_file(config, args);
        else if (command == "search")
            command_search(config, args);
        else if (command == "printing")
            command_printing(config, args);
        else if (command == "collection")
            command_collection(config, args);
        else if (command == "deck")
            command_deck(config, args);
        else
            throw std::runtime_error("unknown command: " + command);
        return (0);
    } catch (const std::exception& error)
    {
        std::cerr << "error: " << error.what() << '\n';
        return (1);
    }
}
