#include "ExpenseTracker.hpp"
#include <chrono>
#include <fstream>
#include <iostream>
#include <limits>
#include <stdexcept>

namespace {
int checks = 0;
void check(bool ok, const std::string& message) {
    ++checks;
    if (!ok) throw std::runtime_error(message);
}
template<class Function> void rejects(Function action, const std::string& message) {
    bool threw = false;
    try { action(); } catch (const std::exception&) { threw = true; }
    check(threw, message);
}
void write(const std::filesystem::path& file, const std::string& bytes) {
    std::ofstream out(file, std::ios::binary);
    out << bytes;
    if (!out) throw std::runtime_error("Test fixture write failed");
}
std::string read(const std::filesystem::path& file) {
    std::ifstream in(file, std::ios::binary);
    return {std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
}
struct TempDirectory {
    std::filesystem::path path;
    TempDirectory() {
        const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
        path = std::filesystem::temp_directory_path() / ("expense-tests-" + std::to_string(stamp));
        if (!std::filesystem::create_directory(path)) throw std::runtime_error("Test directory collision");
    }
    ~TempDirectory() { std::error_code ec; std::filesystem::remove_all(path, ec); }
};
} // namespace

int main() {
    using namespace expense;
    try {
        Money amount = 123;
        for (const auto& pair : {std::pair<const char*, Money>{"0.01", 1}, {"12", 1200}, {"12.3", 1230},
                                {" 0012.30 ", 1230}, {"92233720368547758.07", std::numeric_limits<Money>::max()}}) {
            check(parseAmount(pair.first, amount) && amount == pair.second, "Valid amount");
        }
        for (const auto* text : {"", " ", "0", "0.00", "-1", "+1", ".50", "1.", "1.001", "1e3", "nan", "inf",
                                "1,000", "1 0", "1.2.3", "92233720368547758.08", "92233720368547759", "18446744073709551616"}) {
            amount = 42;
            check(!parseAmount(text, amount) && amount == 42, std::string("Reject amount: ") + text);
        }
        for (Money i = 1; i < 10000; ++i)
            check(parseAmount(formatMoney(i), amount) && amount == i, "Money format/parse round-trip");
        check(formatMoney(0) == "0.00", "Zero total formatting");
        for (const auto* date : {"2000-02-29", "2024-02-29", "0001-01-01", "9999-12-31", "2026-04-30"})
            check(validDate(date), "Valid date");
        for (const auto* date : {"1900-02-29", "2026-02-29", "2026-04-31", "0000-01-01", "2026-13-01", "2026-00-01",
                                "2026-01-00", "2026-1-01", "2026-01-32", "abcd-ef-gh", "", "2026/01/01"})
            check(!validDate(date), "Invalid date");
        Id id = 0;
        check(parseId("18446744073709551615", id) && id == std::numeric_limits<Id>::max(), "Maximum ID");
        for (const auto* text : {"0", "-1", "+1", "1.0", "", "18446744073709551616", "1x"})
            check(!parseId(text, id), "Invalid ID");
        check(!validText(" \t", 80) && !validText("x\ny", 80) && !validText("x\x1b", 80), "Empty/control text");
        check(!validText(std::string(81, 'x'), 80), "Long category");

        ExpenseTracker tracker;
        check(tracker.total() == 0 && tracker.categoryTotal("none") == 0, "Empty totals");
        const auto first = tracker.add("2026-10-31", " Food ", "Lunch, \"special\"", 12550);
        const auto second = tracker.add("2026-10-31", "Food", "Tea", 25);
        tracker.add("2026-10-30", "Travel", "Bus", 4000);
        check(first == 1 && second == 2, "Sequential IDs");
        check(tracker.total() == 16575 && tracker.categoryTotal(" Food ") == 12575, "Exact totals");
        check(tracker.byCategory("Food").size() == 2 && tracker.byCategory("food").empty(), "Case-sensitive categories");
        rejects([&] { tracker.add("2026-02-30", "Food", "Bad", 1); }, "Invalid date add");
        rejects([&] { tracker.add("2026-10-31", "", "Bad", 1); }, "Empty category add");
        rejects([&] { tracker.add("2026-10-31", "Food", "", 1); }, "Empty description add");
        rejects([&] { tracker.add("2026-10-31", "Food", "Bad", 0); }, "Zero add");
        rejects([&] { tracker.add("2026-10-31", "Food", "Bad", -1); }, "Negative add");
        check(!tracker.remove(999) && tracker.remove(second) && !tracker.remove(second), "Deletion / missing ID");
        check(tracker.total() == 16550 && tracker.categoryTotal("Food") == 12550, "Totals after delete");
        ExpenseTracker full;
        full.add("2000-02-29", "Limit", "Maximum", std::numeric_limits<Money>::max());
        rejects([&] { full.add("2000-02-29", "Limit", "Overflow", 1); }, "Aggregate overflow");
        check(full.all().size() == 1 && full.total() == std::numeric_limits<Money>::max(), "Overflow leaves state unchanged");
        full.remove(1);
        full.add("2000-02-29", "Limit", "Recovered", 1);
        check(full.total() == 1, "Overflow recovery after deletion");

        TempDirectory temp;
        const auto file = temp.path / "test.csv";
        tracker.save(file);
        const auto original = read(file);
        check(original.find("\"Lunch, \"\"special\"\"\"") != std::string::npos, "CSV escaping");
        ExpenseTracker loaded;
        loaded.load(file);
        check(loaded.total() == tracker.total() && loaded.all().size() == 2 &&
              loaded.all()[0].description == "Lunch, \"special\"", "CSV round-trip");
        check(loaded.add("2026-10-31", "Other", "Next", 1) == 4, "Loaded ID continuation");
        loaded.save(file);
        check(!std::filesystem::exists(file.string() + ".bak") && !std::filesystem::exists(file.string() + ".lock"), "Save cleanup");
        loaded.load(temp.path / "missing.csv");
        check(loaded.all().empty() && loaded.total() == 0, "Missing file clears state");

        const std::string h = "id,date,category,description,amount_paise\n";
        const std::string good = "1,2026-10-31,Food,Lunch,100\n";
        const std::vector<std::string> corrupt = {
            "", "wrong\n", h + "\n", h + "1,2026-10-31,Food,Lunch\n", h + good + good,
            h + "0,2026-10-31,Food,Lunch,100\n", h + "1,2026-02-30,Food,Lunch,100\n",
            h + "1,2026-10-31,Food,Lunch,0\n", h + "1,2026-10-31,Food,Lunch,-1\n",
            h + "1,2026-10-31,Food,Lunch,9223372036854775808\n",
            h + "18446744073709551616,2026-10-31,Food,Lunch,1\n",
            h + "1,2026-10-31,\"Food,Lunch,100\n", h + "1,2026-10-31,F\"ood,Lunch,100\n",
            h + "1,2026-10-31,\"Food\"x,Lunch,100\n", h + "1,2026-10-31,Food,Lunch,100,\n",
            h + "1,2026-10-31,Food,\"Lunch\nDinner\",100\n",
            h + "1,2026-10-31,,Lunch,100\n", h + "1,2026-10-31,Food,,100\n",
            h + "1,2026-10-31,Food,Lunch,9223372036854775807\n2,2026-10-31,Food,Tea,1\n",
            h + "1,2026-10-31," + std::string(81, 'x') + ",Lunch,100\n",
            h + "1,2026-10-31,Food," + std::string(501, 'x') + ",100\n"
        };
        for (const auto& bytes : corrupt) {
            write(file, bytes);
            const auto before = tracker.total();
            rejects([&] { tracker.load(file); }, "Reject corrupted CSV");
            check(read(file) == bytes && tracker.total() == before && tracker.all().size() == 2, "Transactional load / untouched file");
        }
        write(file, std::string(16 * 1024 * 1024 + 1, 'x'));
        rejects([&] { tracker.load(file); }, "Oversized file");
        write(file, "id,date,category,description,amount_paise\r\n5,2024-02-29,Food,Tea,10\r\n");
        loaded.load(file);
        check(loaded.total() == 10 && loaded.all()[0].id == 5, "CRLF support");
        write(file, h + "18446744073709551615,2026-10-31,Food,Last,1");
        loaded.load(file);
        rejects([&] { loaded.add("2026-10-31", "Food", "No ID", 1); }, "Exhausted IDs");

        write(file, original);
        std::filesystem::create_directory(file.string() + ".lock");
        rejects([&] { tracker.save(file); }, "Save lock conflict");
        check(read(file) == original, "Lock failure preserves original");
        std::filesystem::remove(file.string() + ".lock");
        write(file.string() + ".bak", "preserve backup");
        rejects([&] { tracker.save(file); }, "Backup conflict");
        check(read(file) == original && read(file.string() + ".bak") == "preserve backup", "Backup preserved");
        std::filesystem::remove(file.string() + ".bak");
        std::filesystem::rename(file, file.string() + ".bak");
        loaded.load(file);
        check(loaded.total() == tracker.total() && std::filesystem::exists(file) &&
              !std::filesystem::exists(file.string() + ".bak"), "Interrupted rename recovery");
        std::filesystem::remove(file);
        write(file.string() + ".bak", "bad backup");
        rejects([&] { loaded.load(file); }, "Bad recovery backup");
        check(!std::filesystem::exists(file) && read(file.string() + ".bak") == "bad backup", "Bad backup preserved");
        rejects([&] { tracker.save(temp.path / "missing" / "test.csv"); }, "Missing parent save");
        const auto directory = temp.path / "directory.csv";
        std::filesystem::create_directory(directory);
        rejects([&] { tracker.save(directory); }, "Directory destination save");
        rejects([&] { loaded.load(directory); }, "Directory source load");
        check(!std::filesystem::exists(directory.string() + ".lock"), "Failure lock cleanup");

        ExpenseTracker many;
        for (int i = 0; i < 100000; ++i) many.add("2026-10-31", "Bulk", std::string(500, 'x'), 1);
        rejects([&] { many.add("2026-10-31", "Bulk", "Too many", 1); }, "Row count limit");
        const auto bounded = temp.path / "bounded.csv";
        write(bounded, original);
        rejects([&] { many.save(bounded); }, "Save file size limit");
        check(read(bounded) == original && !std::filesystem::exists(bounded.string() + ".lock"), "Oversized save preserves original and cleans staging");

        std::cout << "PASS: " << checks << " core assertions.\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "FAIL after " << checks << " assertions: " << e.what() << '\n';
        return 1;
    }
}
