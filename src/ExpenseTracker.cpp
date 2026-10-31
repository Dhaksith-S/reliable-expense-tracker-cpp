#include "ExpenseTracker.hpp"
#include <algorithm>
#include <fstream>
#include <limits>
#include <set>
#include <stdexcept>
#include <utility>

namespace expense {
namespace {
constexpr std::size_t maxRows = 100000;
constexpr std::size_t maxFileBytes = 16 * 1024 * 1024;
const std::string header = "id,date,category,description,amount_paise";

std::string quote(const std::string& text) {
    std::string out = "\"";
    for (char c : text) { out += c; if (c == '"') out += '"'; }
    return out + '"';
}

std::vector<std::string> parseRow(const std::string& line) {
    std::vector<std::string> fields;
    std::size_t i = 0;
    while (true) {
        std::string field;
        if (i < line.size() && line[i] == '"') {
            ++i;
            bool closed = false;
            while (i < line.size()) {
                const char c = line[i++];
                if (c != '"') field += c;
                else if (i < line.size() && line[i] == '"') { field += '"'; ++i; }
                else { closed = true; break; }
            }
            if (!closed) throw std::runtime_error("Unclosed CSV quote");
            if (i < line.size() && line[i] != ',') throw std::runtime_error("Text after CSV quote");
        } else {
            while (i < line.size() && line[i] != ',') {
                if (line[i] == '"') throw std::runtime_error("Unexpected CSV quote");
                field += line[i++];
            }
        }
        fields.push_back(std::move(field));
        if (fields.size() > 5) throw std::runtime_error("Too many CSV fields");
        if (i == line.size()) break;
        ++i;
    }
    return fields;
}

struct SaveGuard {
    std::filesystem::path directory;
    ~SaveGuard() { std::error_code ec; std::filesystem::remove_all(directory, ec); }
};
} // namespace

Id ExpenseTracker::add(std::string date, std::string category, std::string description, Money amount) {
    date = trim(date); category = trim(category); description = trim(description);
    if (!validDate(date) || !validText(category, 80) || !validText(description, 500) || amount <= 0)
        throw std::invalid_argument("Invalid expense fields");
    if (expenses_.size() >= maxRows) throw std::overflow_error("Expense limit reached");
    if (nextId_ == 0) throw std::overflow_error("Expense IDs exhausted");
    if (amount > std::numeric_limits<Money>::max() - total()) throw std::overflow_error("Overall total would overflow");
    const Id id = nextId_;
    expenses_.push_back({id, std::move(date), std::move(category), std::move(description), amount});
    total_ += amount;
    nextId_ = id == std::numeric_limits<Id>::max() ? 0 : id + 1;
    return id;
}

bool ExpenseTracker::remove(Id id) {
    const auto it = std::find_if(expenses_.begin(), expenses_.end(), [id](const Expense& e) { return e.id == id; });
    if (it == expenses_.end()) return false;
    total_ -= it->amount;
    expenses_.erase(it);
    return true;
}

std::vector<Expense> ExpenseTracker::byCategory(const std::string& category) const {
    std::vector<Expense> result;
    const auto name = trim(category);
    for (const auto& e : expenses_) if (e.category == name) result.push_back(e);
    return result;
}

Money ExpenseTracker::total() const noexcept {
    return total_;
}

Money ExpenseTracker::categoryTotal(const std::string& category) const {
    Money result = 0;
    const auto name = trim(category);
    for (const auto& e : expenses_) if (e.category == name) result += e.amount;
    return result;
}

void ExpenseTracker::load(const std::filesystem::path& path) {
    auto source = path;
    const std::filesystem::path backup = path.string() + ".bak";
    if (!std::filesystem::exists(source)) {
        if (std::filesystem::exists(backup)) source = backup;
        else { expenses_.clear(); nextId_ = 1; total_ = 0; return; }
    }
    if (!std::filesystem::is_regular_file(source) || std::filesystem::file_size(source) > maxFileBytes)
        throw std::runtime_error("Data path is not a regular file or exceeds 16 MiB");
    std::ifstream in(source, std::ios::binary);
    if (!in) throw std::runtime_error("Cannot open expense file");
    std::string line;
    const auto readLine = [&]() {
        if (!std::getline(in, line)) return false;
        if (!line.empty() && line.back() == '\r') line.pop_back();
        return true;
    };
    if (!readLine() || line != header) throw std::runtime_error("Invalid CSV header");
    ExpenseTracker candidate;
    std::set<Id> ids;
    Id highest = 0;
    std::size_t row = 1;
    while (readLine()) {
        ++row;
        try {
            const auto fields = parseRow(line);
            if (fields.size() != 5) throw std::runtime_error("Expected five CSV fields");
            Id id = 0, raw = 0;
            if (!parseId(fields[0], id) || !ids.insert(id).second || !parseId(fields[4], raw) ||
                raw > static_cast<Id>(std::numeric_limits<Money>::max()))
                throw std::runtime_error("Invalid or duplicate ID / amount");
            candidate.add(fields[1], fields[2], fields[3], static_cast<Money>(raw));
            candidate.expenses_.back().id = id;
            highest = std::max(highest, id);
        } catch (const std::exception& e) {
            throw std::runtime_error("CSV row " + std::to_string(row) + ": " + e.what());
        }
    }
    if (in.bad()) throw std::runtime_error("Error reading expense file");
    // Windows does not allow renaming a file while this stream holds it open.
    in.close();
    candidate.nextId_ = highest == std::numeric_limits<Id>::max() ? 0 : highest + 1;
    // Restore an interrupted replacement only after the backup has passed validation.
    if (source == backup) std::filesystem::rename(backup, path);
    expenses_.swap(candidate.expenses_);
    nextId_ = candidate.nextId_;
    total_ = candidate.total_;
}

void ExpenseTracker::save(const std::filesystem::path& path) const {
    if (path.empty() || path.filename().empty()) throw std::runtime_error("Invalid save path");
    const std::filesystem::path lock = path.string() + ".lock";
    if (!std::filesystem::create_directory(lock)) throw std::runtime_error("Save lock exists; another save or interrupted save needs attention");
    SaveGuard guard{lock};
    const auto temporary = lock / "pending.csv";
    const std::filesystem::path backup = path.string() + ".bak";
    if (std::filesystem::exists(backup)) throw std::runtime_error("Backup exists; preserve and resolve it before saving");
    const bool exists = std::filesystem::exists(path);
    if (exists && !std::filesystem::is_regular_file(path)) throw std::runtime_error("Save destination is not a regular file");
    std::ofstream out(temporary, std::ios::binary | std::ios::trunc);
    if (!out) throw std::runtime_error("Cannot open temporary expense file");
    out << header << '\n';
    for (const auto& e : expenses_)
        out << e.id << ',' << e.date << ',' << quote(e.category) << ',' << quote(e.description) << ',' << e.amount << '\n';
    out.flush();
    if (!out) throw std::runtime_error("Failed to write expense file");
    out.close();
    if (!out) throw std::runtime_error("Failed to close expense file");
    if (std::filesystem::file_size(temporary) > maxFileBytes) throw std::runtime_error("Saved file would exceed 16 MiB");
    if (exists) std::filesystem::rename(path, backup);
    try { std::filesystem::rename(temporary, path); }
    catch (...) {
        if (exists) {
            std::error_code ec;
            std::filesystem::rename(backup, path, ec);
            if (ec) throw std::runtime_error("Save failed; previous data preserved in .bak");
        }
        throw;
    }
    if (exists) {
        std::error_code ec;
        std::filesystem::remove(backup, ec);
        if (ec) throw std::runtime_error("New file saved, but old .bak could not be removed");
    }
}
} // namespace expense
