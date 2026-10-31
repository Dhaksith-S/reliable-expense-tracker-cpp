#pragma once

#include <cstdint>
#include <string>

namespace expense {
using Money = std::int64_t;
using Id = std::uint64_t;

struct Expense {
    Id id;
    std::string date;
    std::string category;
    std::string description;
    Money amount;
};

std::string trim(const std::string& text);
bool validDate(const std::string& date);
bool validText(const std::string& text, std::size_t maxLength);
bool parseAmount(const std::string& text, Money& result);
bool parseId(const std::string& text, Id& result);
std::string formatMoney(Money amount);
} // namespace expense
