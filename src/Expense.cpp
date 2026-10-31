#include "Expense.hpp"
#include <charconv>
#include <limits>

namespace expense {
std::string trim(const std::string& text) {
    const auto first = text.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) return {};
    return text.substr(first, text.find_last_not_of(" \t\r\n") - first + 1);
}

bool validText(const std::string& text, std::size_t maxLength) {
    if (trim(text).empty() || text.size() > maxLength) return false;
    for (unsigned char c : text) if (c < 32 || c == 127) return false;
    return true;
}

bool parseId(const std::string& text, Id& result) {
    const auto s = trim(text);
    if (s.empty()) return false;
    for (char c : s) if (c < '0' || c > '9') return false;
    Id value = 0;
    const auto parsed = std::from_chars(s.data(), s.data() + s.size(), value);
    if (parsed.ec != std::errc{} || value == 0) return false;
    result = value;
    return true;
}

bool parseAmount(const std::string& text, Money& result) {
    const auto s = trim(text);
    if (s.empty()) return false;
    const auto dot = s.find('.');
    const auto whole = s.substr(0, dot);
    if (whole.empty()) return false;
    const auto fraction = dot == std::string::npos ? std::string{} : s.substr(dot + 1);
    if (dot != std::string::npos && (fraction.empty() || fraction.size() > 2)) return false;
    Money rupees = 0;
    constexpr Money max = std::numeric_limits<Money>::max();
    for (char c : whole) {
        if (c < '0' || c > '9' || rupees > (max / 100 - (c - '0')) / 10) return false;
        rupees = rupees * 10 + (c - '0');
    }
    Money paise = 0;
    for (char c : fraction) {
        if (c < '0' || c > '9') return false;
        paise = paise * 10 + (c - '0');
    }
    if (fraction.size() == 1) paise *= 10;
    if (rupees > max / 100 || (rupees == max / 100 && paise > max % 100)) return false;
    const Money amount = rupees * 100 + paise;
    if (amount <= 0) return false;
    result = amount;
    return true;
}

std::string formatMoney(Money amount) {
    const auto fraction = amount % 100;
    return std::to_string(amount / 100) + "." + (fraction < 10 ? "0" : "") + std::to_string(fraction);
}

bool validDate(const std::string& date) {
    if (date.size() != 10 || date[4] != '-' || date[7] != '-') return false;
    for (std::size_t i = 0; i < date.size(); ++i) {
        if (i != 4 && i != 7 && (date[i] < '0' || date[i] > '9')) return false;
    }
    const int year = std::stoi(date.substr(0, 4));
    const int month = std::stoi(date.substr(5, 2));
    const int day = std::stoi(date.substr(8, 2));
    if (year == 0 || month < 1 || month > 12) return false;
    const int days[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    const bool leap = year % 4 == 0 && (year % 100 != 0 || year % 400 == 0);
    return day >= 1 && day <= days[month - 1] + (month == 2 && leap ? 1 : 0);
}
} // namespace expense
