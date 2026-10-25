#include <consumer/settings.hpp>

namespace json = cpm::json;

namespace {

// Reads `key` into `out` when the object has it. A TypeError gets the key added to its path, so the message says
// which setting is wrong: "at /port: expected an integer from 0 to 65535, found 70000".
template <class T>
void read_member(const json::Value& object, std::string_view key, T& out) {
    const json::Value* value = object.find(key);
    if (value == nullptr) return;
    try {
        out = value->get<T>();
    } catch (const json::TypeError& error) {
        throw json::TypeError(error, key);
    }
}

}  // namespace

json::Value json::Traits<Settings>::to_json(const Settings& settings) {
    return Object{{"name", settings.name},
                  {"port", settings.port},
                  {"plugins", json::to_json(settings.plugins)},
                  {"log_file", json::to_json(settings.log_file)}};
}

Settings json::Traits<Settings>::from_json(const Value& json) {
    Settings settings;
    settings.name = json.at("name").get<std::string>();
    read_member(json, "port", settings.port);
    read_member(json, "plugins", settings.plugins);
    read_member(json, "log_file", settings.log_file);
    return settings;
}

Settings read_settings(std::string_view text) { return json::parse(text, json::jsonc).get<Settings>(); }

std::string write_settings(const Settings& settings) { return json::write(json::to_json(settings), {.indent = 2}); }
