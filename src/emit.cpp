#include <cpm/json/emit.hpp>
#include <cpm/json/write.hpp>

#include <algorithm>
#include <vector>

namespace cpm::json {

void emit(const Value& value, Handler& handler, bool sort_keys) {
    switch (value.kind()) {
        case Kind::null: handler.null(); return;
        case Kind::boolean: handler.boolean(value.as_boolean()); return;
        case Kind::number: handler.number(value.as_number()); return;
        case Kind::string: handler.string(value.as_string()); return;
        case Kind::array:
            handler.begin_array();
            for (const Value& element : value.as_array()) emit(element, handler, sort_keys);
            handler.end_array();
            return;
        case Kind::object: {
            auto member_events = [&](const Member& member) {
                handler.key(member.key());
                emit(member.value, handler, sort_keys);
            };
            handler.begin_object();
            const Object& object = value.as_object();
            if (sort_keys) {
                std::vector<const Member*> members;
                members.reserve(object.size());
                for (const Member& member : object) members.push_back(&member);
                std::sort(members.begin(), members.end(),
                          [](const Member* a, const Member* b) { return a->key() < b->key(); });
                for (const Member* member : members) member_events(*member);
            } else {
                for (const Member& member : object) member_events(member);
            }
            handler.end_object();
            return;
        }
    }
}

std::string write(const Value& value, const WriteOptions& options) {
    std::string out;
    StringSink sink(out);
    write(value, sink, options);
    return out;
}

void write(const Value& value, Sink& sink, const WriteOptions& options) {
    Writer writer(sink, options);
    emit(value, writer, options.sort_keys);
}

}  // namespace cpm::json
