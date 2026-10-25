#pragma once

#include <cpm/json.hpp>

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

struct Settings {
    std::string name;
    std::uint16_t port = 8080;               // optional in the file
    std::vector<std::string> plugins;        // optional in the file
    std::optional<std::string> log_file;     // null or missing: no log file
};

// Settings from JSONC text. Throws cpm::json::ParseError for text that isn't JSONC, and cpm::json::TypeError, with
// the path of the bad value, for settings of the wrong type.
Settings read_settings(std::string_view text);

// The settings as indented JSON.
std::string write_settings(const Settings& settings);

template <>
struct cpm::json::Traits<Settings> {
    static Value to_json(const Settings& settings);
    static Settings from_json(const Value& json);
};
