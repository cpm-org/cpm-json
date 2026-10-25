#include <consumer/settings.hpp>

#include <cpm/test.hpp>

TEST_CASE("settings are read from JSONC") {
    const Settings settings = read_settings(R"({
        // the service
        "name": "api",
        "port": 9000,
        "plugins": ["auth", "metrics",],
    })");
    CHECK(settings.name == "api");
    CHECK(settings.port == 9000);
    CHECK(settings.plugins == (std::vector<std::string>{"auth", "metrics"}));
    CHECK(!settings.log_file.has_value());

    SUBCASE("and written back") {
        CHECK(read_settings(write_settings(settings)).plugins == settings.plugins);
    }
}

TEST_CASE("bad settings say where they are wrong") {
    CHECK_THROWS_WITH(read_settings(R"({"name": "api", "plugins": ["auth", 7]})"),
                      "at /plugins/1: expected a string, found 7");
    CHECK_THROWS_WITH(read_settings(R"({"name": "api",
  "port": 70000})"),
                      "at /port: expected an integer from 0 to 65535, found 70000");
    CHECK_THROWS_WITH(read_settings("{\"name\": \"api\"\n  \"port\": 1}"),
                      "line 2, column 3: expected ',' or '}' after an object member, found a string");
}
