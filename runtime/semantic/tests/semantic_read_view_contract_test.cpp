#include "canvas/semantic/semantic_read_view.hpp"

#include <gtest/gtest.h>

#include <cstddef>
#include <optional>
#include <type_traits>
#include <utility>
#include <vector>

namespace canvas::semantic {
namespace {

class ProbeObjectStore final : public ObjectStore {
  public:
    explicit ProbeObjectStore(std::vector<ObjectRecord> records)
        : records_(std::move(records)) {}

    [[nodiscard]] std::size_t size() const noexcept override {
        ++size_calls;
        return records_.size();
    }

    [[nodiscard]] bool contains(const ObjectId& id) const noexcept override {
        ++contains_calls;
        for (const auto& record : records_) {
            if (record.id == id) {
                return true;
            }
        }
        return false;
    }

    [[nodiscard]] const ObjectRecord* find(const ObjectId& id) const noexcept override {
        ++find_calls;
        for (const auto& record : records_) {
            if (record.id == id) {
                return &record;
            }
        }
        return nullptr;
    }

    [[nodiscard]] std::vector<ObjectRecord> allObjects() const override {
        ++all_objects_calls;
        return records_;
    }

    [[nodiscard]] std::vector<ObjectRecord> children(
        const std::optional<ObjectId>& parent_id) const override {
        ++children_calls;
        std::vector<ObjectRecord> result;
        for (const auto& record : records_) {
            if (record.placement.parent_id == parent_id) {
                result.push_back(record);
            }
        }
        return result;
    }

    mutable std::size_t size_calls = 0;
    mutable std::size_t contains_calls = 0;
    mutable std::size_t find_calls = 0;
    mutable std::size_t all_objects_calls = 0;
    mutable std::size_t children_calls = 0;

  private:
    std::vector<ObjectRecord> records_;
};

[[nodiscard]] ObjectRecord makeRecord(
    std::uint64_t id,
    std::optional<ObjectId> parent_id = std::nullopt) {
    ObjectRecord record{};
    record.id = ObjectId::fromUint64(id);
    record.placement.parent_id = parent_id;
    return record;
}

template <typename T>
concept HasInsert = requires(T& value, ObjectRecord record) {
    value.insert(std::move(record));
};

template <typename T>
concept HasErase = requires(T& value, const ObjectId& id) {
    value.erase(id);
};

template <typename T>
concept HasMutableStoreEscape = requires(T& value) {
    { value.objects() } -> std::same_as<ObjectStore&>;
};

static_assert(std::is_constructible_v<
              SemanticReadView, const ObjectStore&, SemanticGeneration>);
static_assert(std::is_same_v<
              decltype(std::declval<const SemanticReadView&>().find(
                  std::declval<const ObjectId&>())),
              const ObjectRecord*>);
static_assert(!HasInsert<SemanticReadView>);
static_assert(!HasErase<SemanticReadView>);
static_assert(!HasMutableStoreEscape<SemanticReadView>);

TEST(SemanticReadViewContract, CapturesStrongSemanticGeneration) {
    ProbeObjectStore store({});
    const SemanticGeneration generation(17U);
    const SemanticReadView view(store, generation);

    EXPECT_EQ(view.generation(), generation);
}

TEST(SemanticReadViewContract, DelegatesCanonicalReadOperations) {
    const ObjectId parent = ObjectId::fromUint64(1U);
    const ObjectId child = ObjectId::fromUint64(2U);
    const ObjectId sibling = ObjectId::fromUint64(3U);
    ProbeObjectStore store({
        makeRecord(1U),
        makeRecord(2U, parent),
        makeRecord(3U),
    });
    const SemanticReadView view(store, SemanticGeneration(4U));

    EXPECT_EQ(view.size(), 3U);
    EXPECT_TRUE(view.contains(child));
    EXPECT_FALSE(view.contains(ObjectId::fromUint64(99U)));

    const ObjectRecord* found = view.find(sibling);
    ASSERT_NE(found, nullptr);
    EXPECT_EQ(found->id, sibling);
    EXPECT_EQ(view.find(ObjectId::fromUint64(100U)), nullptr);

    const auto all = view.allObjects();
    ASSERT_EQ(all.size(), 3U);
    EXPECT_EQ(all[0].id, parent);
    EXPECT_EQ(all[1].id, child);
    EXPECT_EQ(all[2].id, sibling);

    const auto children = view.children(parent);
    ASSERT_EQ(children.size(), 1U);
    EXPECT_EQ(children.front().id, child);

    EXPECT_EQ(store.size_calls, 1U);
    EXPECT_EQ(store.contains_calls, 2U);
    EXPECT_EQ(store.find_calls, 2U);
    EXPECT_EQ(store.all_objects_calls, 1U);
    EXPECT_EQ(store.children_calls, 1U);
}

} // namespace
} // namespace canvas::semantic
