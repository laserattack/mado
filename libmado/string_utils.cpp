#include <iostream>
#include <string>

#include "string_utils.hpp"

void trim(std::string &s) {
    s.erase(0, s.find_first_not_of(" \t"));
    s.erase(s.find_last_not_of(" \t") + 1);
}

void print_json_string(const std::string &str, std::ostream &os) {
    os << '"';
    for (char c : str) {
        switch (c) {
        case '"':
            os << "\\\"";
            break;
        case '\\':
            os << "\\\\";
            break;
        default:
            os << c;
        }
    }
    os << '"';
}
