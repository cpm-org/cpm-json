#include <cpm/json/error.hpp>
#include <cpm/json/pointer.hpp>

#include <format>
#include <utility>

namespace cpm::json {

namespace {

std::string with_path(const std::string& path, const std::string& reason) {
    return path.empty() ? reason : std::format("at {}: {}", path, reason);
}

}  // namespace

ParseError::ParseError(Code code, Location location, std::string reason)
    : Error(std::format("line {}, column {}: {}", location.line, location.column, reason)),
      code_(code),
      location_(location),
      reason_(std::move(reason)) {}

TypeError::TypeError(std::string reason) : TypeError(std::string(), std::move(reason)) {}

TypeError::TypeError(const TypeError& inner, std::string_view token)
    : TypeError("/" + escape_pointer_token(token) + inner.path_, inner.reason_) {}

TypeError::TypeError(std::string path, std::string reason)
    : Error(with_path(path, reason)), path_(std::move(path)), reason_(std::move(reason)) {}

}  // namespace cpm::json
