
#include "core/json.hpp"
#include "core/migrations.hpp"
#include "core/mtgjson.hpp"
#include "core/repository.hpp"
#include "core/sqlite.hpp"

#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>

namespace
{

void check(bool condition, const std::string& message)
{
    if (!condition)
        throw std::runtime_error("test failed: " + message);
}

void test_json(void)
{
    const auto v = mtg::json::parse(R"({"name":"Jace","n":4,"ok":true,"arr":["U",null]})");
    check(mtg::json::string_or(v, "name") == "Jace", "json string");
    check(mtg::json::int_or(v, "n") == 4, "json number");
    check(mtg::json::bool_or(v, "ok"), "json bool");
    check(v.find("arr") && v.find("arr")->is_array(), "json array");
}

void test_import_and_repository(void)
{
    const auto db_path = std::filesystem::temp_directory_path() / "mtgtool-tests.sqlite";
    std::filesystem::remove(db_path);
    std::filesystem::remove(db_path.string() + "-wal");
    std::filesystem::remove(db_path.string() + "-shm");

    mtg::SqliteDb db(db_path.string());
    mtg::MigrationRunner::apply(db, "migrations");
    mtg::MtgJsonImporter importer(db);
    const auto result = importer.import_file("tests/fixtures/minimal_set.json");
    check(result.card_count == 2, "import count");
    check(result.set_code == "TST", "set code");

    mtg::Repository repo(db);
    const auto found = repo.search_printings("Test", std::nullopt, 10);
    check(found.size() == 2, "search results");

    repo.create_collection("Main");
    repo.add_collection_item("Main", "00000000-0000-0000-0000-000000000001", 2, "foil", "NM", "test");
    const auto collection = repo.collection_items("Main");
    check(collection.size() == 1 && collection[0].quantity == 2, "collection add");

    repo.create_deck("Burn", "modern", "");
    repo.add_deck_card("Burn", "00000000-0000-0000-0000-000000000001", 4, "nonfoil", "main");
    const auto deck = repo.deck_cards("Burn");
    check(deck.size() == 1 && deck[0].quantity == 4, "deck add");

    std::filesystem::remove(db_path);
    std::filesystem::remove(db_path.string() + "-wal");
    std::filesystem::remove(db_path.string() + "-shm");
}

} // namespace

int main(void)
{
    try
    {
        test_json();
        test_import_and_repository();
        std::cout << "all tests passed\n";
        return 0;
    }
    catch (const std::exception& e)
    {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
