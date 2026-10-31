#include "ExpenseTracker.hpp"
#include <iomanip>
#include <iostream>
#include <stdexcept>

namespace {
struct EndOfInput {};
std::string prompt(const std::string& label) {
    std::cout << label << std::flush;
    std::string line;
    bool tooLong = false;
    char c = 0;
    // Drain oversized lines so a following menu choice remains independent.
    while (std::cin.get(c)) {
        if (c == '\n') break;
        if (line.size() < 4096) line += c;
        else tooLong = true;
    }
    if (!std::cin && line.empty() && !tooLong) throw EndOfInput{};
    if (tooLong) throw std::invalid_argument("Input exceeds 4096 bytes; line discarded");
    return expense::trim(line);
}

std::string field(const std::string& label, std::size_t limit) {
    while (true) {
        auto s = prompt(label);
        if (expense::validText(s, limit)) return s;
        std::cout << "Invalid field: enter 1-" << limit << " bytes without control characters.\n";
    }
}

std::string shortened(const std::string& s, std::size_t width) {
    if (s.size() <= width) return s;
    auto end = width - 3;
    while (end > 0 && (static_cast<unsigned char>(s[end]) & 0xC0) == 0x80) --end;
    return s.substr(0, end) + "...";
}

void table(const std::vector<expense::Expense>& rows) {
    if (rows.empty()) { std::cout << "No expenses found.\n"; return; }
    std::cout << std::left << std::setw(21) << "ID" << std::setw(12) << "Date"
              << std::setw(19) << "Category" << std::setw(29) << "Description" << std::right << std::setw(21) << "Amount (INR)" << '\n'
              << std::string(102, '-') << '\n';
    for (const auto& e : rows)
        std::cout << std::left << std::setw(21) << e.id << std::setw(12) << e.date
                  << std::setw(19) << shortened(e.category, 18) << std::setw(29) << shortened(e.description, 28)
                  << std::right << std::setw(21) << expense::formatMoney(e.amount) << '\n';
}

void menu() {
    std::cout << "\nReliable Expense Tracker CLI\n"
              << "1. Add Expense\n2. View All Expenses\n3. View Expenses by Category\n"
              << "4. View Category Total\n5. View Overall Total\n6. Delete Expense\n"
              << "7. Save Expenses\n8. Exit\n";
}
} // namespace

int main(int argc, char* argv[]) {
    if (argc == 2 && std::string(argv[1]) == "--help") {
        std::cout << "Usage: expense_tracker [CSV_PATH]\nDefault: expenses.csv in the current directory.\n";
        return 0;
    }
    if (argc > 2) { std::cerr << "Usage: expense_tracker [CSV_PATH]\n"; return 2; }
    const std::filesystem::path path = argc == 2 ? argv[1] : "expenses.csv";
    expense::ExpenseTracker tracker;
    try { tracker.load(path); }
    catch (const std::exception& e) {
        std::cerr << "Cannot load data: " << e.what() << ". File left untouched.\n";
        return 1;
    }
    std::cout << "Loaded " << tracker.all().size() << " expense(s). Amounts are in INR.\n";
    bool dirty = false;
    const auto save = [&]() {
        try { tracker.save(path); dirty = false; std::cout << "Expenses saved.\n"; return true; }
        catch (const std::exception& e) { std::cout << "Save failed: " << e.what() << '\n'; return false; }
    };
    while (true) {
        try {
            menu();
            const auto choice = prompt("Choose [1-8]: ");
            if (choice == "1") {
                std::string date;
                while (true) {
                    date = prompt("Date (YYYY-MM-DD): ");
                    if (expense::validDate(date)) break;
                    std::cout << "Invalid date: use a real date in YYYY-MM-DD (0001-9999).\n";
                }
                const auto category = field("Category: ", 80);
                const auto description = field("Description: ", 500);
                expense::Money amount = 0;
                while (!expense::parseAmount(prompt("Amount (INR, e.g. 125.50): "), amount))
                    std::cout << "Invalid amount: enter a positive value with at most two decimal places, within range.\n";
                const auto id = tracker.add(date, category, description, amount);
                dirty = true;
                std::cout << "Added expense #" << id << ".\n";
            } else if (choice == "2") table(tracker.all());
            else if (choice == "3") table(tracker.byCategory(field("Category: ", 80)));
            else if (choice == "4") {
                const auto category = field("Category: ", 80);
                std::cout << "Category total (" << category << "): INR " << expense::formatMoney(tracker.categoryTotal(category)) << '\n';
            } else if (choice == "5") std::cout << "Overall total: INR " << expense::formatMoney(tracker.total()) << '\n';
            else if (choice == "6") {
                expense::Id id = 0;
                if (!expense::parseId(prompt("Expense ID: "), id)) std::cout << "Invalid ID: enter a positive integer within range.\n";
                else if (tracker.remove(id)) { dirty = true; std::cout << "Expense deleted.\n"; }
                else std::cout << "Expense ID not found.\n";
            } else if (choice == "7") save();
            else if (choice == "8") {
                if (dirty && !save()) { std::cout << "Exit cancelled; unsaved changes remain.\n"; continue; }
                std::cout << "Goodbye.\n"; return 0;
            } else std::cout << "Invalid menu choice: enter 1-8.\n";
        } catch (const EndOfInput&) {
            std::cout << "\nEnd of input; incomplete entry cancelled.\n";
            if (dirty && !save()) { std::cerr << "Unsaved changes could not be persisted.\n"; return 1; }
            std::cout << "Goodbye.\n"; return 0;
        } catch (const std::exception& e) { std::cout << "Error: " << e.what() << '\n'; }
    }
}
