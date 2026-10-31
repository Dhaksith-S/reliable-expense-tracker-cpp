#pragma once

#include "Expense.hpp"
#include <filesystem>
#include <vector>

namespace expense {
class ExpenseTracker {
public:
    Id add(std::string date, std::string category, std::string description, Money amount);
    bool remove(Id id);
    const std::vector<Expense>& all() const noexcept { return expenses_; }
    std::vector<Expense> byCategory(const std::string& category) const;
    Money categoryTotal(const std::string& category) const;
    Money total() const noexcept;
    // Missing files mean an empty tracker. Invalid files leave existing state intact.
    void load(const std::filesystem::path& path);
    void save(const std::filesystem::path& path) const;

private:
    std::vector<Expense> expenses_;
    Money total_ = 0;
    Id nextId_ = 1; // Zero means exhausted.
};
} // namespace expense
