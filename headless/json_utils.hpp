// headless/json_utils.hpp
// Simple header-only JSON builder and minimal parser (no external deps)
#pragma once
#include <cctype>
#include <string>

namespace json_utils {

// ---- Builder ----

inline std::string quote(const std::string& s) {
    return "\"" + s + "\"";
}

inline std::string quote_multiline(const std::string& s) {
    std::string out = "\"";
    for (char c : s) {
        if (c == '"') out += "\\\"";
        else if (c == '\\') out += "\\\\";
        else if (c == '\n') out += "\\n";
        else if (c == '\r') out += "\\r";
        else if (c == '\t') out += "\\t";
        else out += c;
    }
    out += "\"";
    return out;
}

// "key":"val"
inline std::string kv(const std::string& key, const std::string& val) {
    return quote(key) + ":" + quote(val);
}

// "key":123
inline std::string kv(const std::string& key, int val) {
    return quote(key) + ":" + std::to_string(val);
}

// "key":true/false
inline std::string kv(const std::string& key, bool val) {
    return quote(key) + ":" + (val ? "true" : "false");
}

// {contents}
inline std::string obj(const std::string& contents) {
    return "{" + contents + "}";
}

// [contents]
inline std::string arr(const std::string& contents) {
    return "[" + contents + "]";
}

// ---- Parser (minimal, works on flat/single-level JSON lines) ----

// Find the position of the value for a given key in a JSON string.
// Returns std::string::npos if not found.
inline std::size_t find_value_pos(const std::string& json, const std::string& key) {
    std::string search = "\"" + key + "\"";
    auto pos = json.find(search);
    if (pos == std::string::npos) return std::string::npos;
    // skip past key
    pos += search.size();
    // find ':'
    while (pos < json.size() && json[pos] != ':') pos++;
    if (pos >= json.size()) return std::string::npos;
    pos++;  // skip ':'
    // skip whitespace
    while (pos < json.size() && std::isspace(static_cast<unsigned char>(json[pos]))) pos++;
    return pos;
}

// Extract string value for key: "key":"value"
inline std::string get_string(const std::string& json, const std::string& key) {
    auto pos = find_value_pos(json, key);
    if (pos == std::string::npos) return "";
    if (pos >= json.size() || json[pos] != '"') return "";
    pos++;  // skip opening quote
    auto end = json.find('"', pos);
    if (end == std::string::npos) return "";
    return json.substr(pos, end - pos);
}

// Extract integer value for key: "key":123
inline int get_int(const std::string& json, const std::string& key, int default_val = -1) {
    auto pos = find_value_pos(json, key);
    if (pos == std::string::npos) return default_val;
    if (pos >= json.size()) return default_val;
    bool neg = false;
    if (json[pos] == '-') { neg = true; pos++; }
    if (pos >= json.size() || !std::isdigit(static_cast<unsigned char>(json[pos]))) return default_val;
    int val = 0;
    while (pos < json.size() && std::isdigit(static_cast<unsigned char>(json[pos]))) {
        val = val * 10 + (json[pos] - '0');
        pos++;
    }
    return neg ? -val : val;
}

// Extract boolean value for key: "key":true/false
inline bool get_bool(const std::string& json, const std::string& key, bool default_val = false) {
    auto pos = find_value_pos(json, key);
    if (pos == std::string::npos) return default_val;
    if (pos + 4 <= json.size() && json.substr(pos, 4) == "true") return true;
    if (pos + 5 <= json.size() && json.substr(pos, 5) == "false") return false;
    return default_val;
}

}  // namespace json_utils
