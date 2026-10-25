// cpm-json: JSON for C++20. See README.md for a guide and docs/spec.md for the specification.
//
//   #include <cpm/json.hpp>
//   namespace json = cpm::json;
//
//   json::Value config = json::parse(text, json::jsonc);   // comments and trailing commas allowed
//   int port = config.at("port").get<int>();                // TypeError when it isn't an int
//   config.as_object().insert_or_assign("debug", true);
//   std::string out = json::write(config, {.indent = 2});
#pragma once

#include <cpm/json/builder.hpp>
#include <cpm/json/emit.hpp>
#include <cpm/json/error.hpp>
#include <cpm/json/handler.hpp>
#include <cpm/json/number.hpp>
#include <cpm/json/pointer.hpp>
#include <cpm/json/read.hpp>
#include <cpm/json/traits.hpp>
#include <cpm/json/value.hpp>
#include <cpm/json/write.hpp>
