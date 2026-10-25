// The keys of each open object during a parse, to find a repeated one (docs/spec.md §4.2).
#pragma once

#include <cstddef>
#include <string>
#include <string_view>
#include <unordered_set>
#include <vector>

namespace cpm::json::detail {

class KeyTracker {
public:
    // An object begins.
    void open() {
        if (depth_ == objects_.size()) objects_.emplace_back();
        ++depth_;
    }

    // The innermost object ends. Its set is kept, emptied, for the next object at this depth.
    void close() { objects_[--depth_].clear(); }

    // Records `key` in the innermost object. False when that object already has it.
    bool add(std::string_view key) { return objects_[depth_ - 1].emplace(key).second; }

private:
    std::vector<std::unordered_set<std::string>> objects_;
    std::size_t depth_ = 0;
};

}  // namespace cpm::json::detail
