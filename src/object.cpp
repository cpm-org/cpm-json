#include <cpm/json/value.hpp>

#include <format>
#include <functional>
#include <utility>

namespace cpm::json {

namespace {

// Objects up to this size are searched member by member, which beats hashing for them.
constexpr std::size_t linear_limit = 8;

std::size_t hash_of(std::string_view key) noexcept { return std::hash<std::string_view>{}(key); }

}  // namespace

namespace detail {

std::size_t MemberIndex::find(std::string_view key, const std::vector<Member>& members) const noexcept {
    const std::size_t mask = slots_.size() - 1;
    for (std::size_t slot = hash_of(key) & mask;; slot = (slot + 1) & mask) {
        const std::size_t position = slots_[slot];
        if (position == npos) return npos;
        if (members[position].key() == key) return position;
    }
}

void MemberIndex::add(std::size_t position, const std::vector<Member>& members) {
    // Keep at most half of the slots in use, so that searches stay short.
    if (2 * members.size() > slots_.size()) {
        rebuild(members);
    } else {
        place(position, members);
    }
}

void MemberIndex::rebuild(const std::vector<Member>& members) {
    std::size_t size = 16;
    while (size < 2 * members.size()) size *= 2;
    slots_.assign(size, npos);
    for (std::size_t position = 0; position < members.size(); ++position) place(position, members);
}

void MemberIndex::place(std::size_t position, const std::vector<Member>& members) noexcept {
    const std::size_t mask = slots_.size() - 1;
    std::size_t slot = hash_of(members[position].key()) & mask;
    while (slots_[slot] != npos) slot = (slot + 1) & mask;
    slots_[slot] = position;
}

}  // namespace detail

Object::Object() noexcept = default;

Object::Object(std::initializer_list<Member> members) {
    for (const Member& member : members) insert_or_assign(member.key(), member.value);
}

Object::Object(const Object& other) : members_(other.members_), index_(other.index_) {}
Object::Object(Object&& other) noexcept : members_(std::move(other.members_)), index_(std::move(other.index_)) {
    other.index_.clear();
}

// Members can't be assigned (their keys are fixed), so assignment swaps in a new set of members.
Object& Object::operator=(const Object& other) {
    Object copy(other);
    swap(copy);
    return *this;
}

Object& Object::operator=(Object&& other) noexcept {
    Object moved(std::move(other));
    swap(moved);
    return *this;
}

Object::~Object() = default;

void Object::swap(Object& other) noexcept {
    members_.swap(other.members_);
    std::swap(index_, other.index_);
}

std::size_t Object::size() const noexcept { return members_.size(); }
bool Object::empty() const noexcept { return members_.empty(); }

void Object::clear() noexcept {
    members_.clear();
    index_.clear();
}

Object::iterator Object::begin() noexcept { return members_.data(); }
Object::iterator Object::end() noexcept { return members_.data() + members_.size(); }
Object::const_iterator Object::begin() const noexcept { return members_.data(); }
Object::const_iterator Object::end() const noexcept { return members_.data() + members_.size(); }

std::size_t Object::position(std::string_view key) const noexcept {
    if (index_.active()) return index_.find(key, members_);
    for (std::size_t i = 0; i < members_.size(); ++i) {
        if (members_[i].key() == key) return i;
    }
    return detail::MemberIndex::npos;
}

Value* Object::find(std::string_view key) noexcept {
    const std::size_t i = position(key);
    return i == detail::MemberIndex::npos ? nullptr : &members_[i].value;
}

const Value* Object::find(std::string_view key) const noexcept {
    const std::size_t i = position(key);
    return i == detail::MemberIndex::npos ? nullptr : &members_[i].value;
}

bool Object::contains(std::string_view key) const noexcept { return position(key) != detail::MemberIndex::npos; }

Value& Object::at(std::string_view key) {
    if (Value* value = find(key)) return *value;
    throw LookupError(std::format("no member \"{}\"", key));
}

const Value& Object::at(std::string_view key) const {
    if (const Value* value = find(key)) return *value;
    throw LookupError(std::format("no member \"{}\"", key));
}

Value& Object::insert_or_assign(std::string key, Value value) {
    if (const std::size_t i = position(key); i != detail::MemberIndex::npos) {
        members_[i].value = std::move(value);
        return members_[i].value;
    }
    members_.emplace_back(std::move(key), std::move(value));
    if (index_.active()) {
        index_.add(members_.size() - 1, members_);
    } else if (members_.size() > linear_limit) {
        index_.rebuild(members_);
    }
    return members_.back().value;
}

bool Object::erase(std::string_view key) {
    const std::size_t i = position(key);
    if (i == detail::MemberIndex::npos) return false;
    // Members can't be assigned, so the others move into a new vector.
    std::vector<Member> kept;
    kept.reserve(members_.size() - 1);
    for (std::size_t j = 0; j < members_.size(); ++j) {
        if (j != i) kept.push_back(std::move(members_[j]));
    }
    members_.swap(kept);
    if (members_.size() > linear_limit) index_.rebuild(members_);
    else index_.clear();
    return true;
}

bool operator==(const Object& a, const Object& b) {
    if (a.size() != b.size()) return false;
    for (const Member& member : a) {
        const Value* other = b.find(member.key());
        if (other == nullptr || !(*other == member.value)) return false;
    }
    return true;
}

}  // namespace cpm::json
