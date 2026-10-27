#pragma once
#include "canvas/runtime/runtime_composition.hpp"
#include "canvas/semantic/codec.hpp"
#include <nlohmann/json.hpp>
#include <stdexcept>
#include <string>

#if defined(AXIOM_FC08_QUALIFICATION_HOOKS)
#include "qualification_checkpoint.hpp"
namespace canvas::qualification {
class Access final {
  template<class Owner>
  static Handle bind(Owner& owner, const std::shared_ptr<Observer>& observer,
                     std::array<std::uint8_t,16> identity, std::uint64_t epoch) {
    deactivateGroup(owner.fc08Qualification_.get());
    auto state = std::make_shared<State>();
    state->owner = &owner; state->identity = identity; state->epoch = epoch;
    state->observer = observer; owner.fc08Qualification_ = state;
    return Handle(state);
  }
 public:
  struct Registration final { Handle engine; Handle history; Handle command; };
  static Registration bind(runtime::RuntimeComposition& composition,
                           const std::shared_ptr<Observer>& observer,
                           std::array<std::uint8_t,16> identity, std::uint64_t epoch) {
    auto& owner = *composition.runtime_;
    if (owner.closed_) throw std::logic_error("cannot register a closed owner");
    Registration registration{bind(owner.operationEngine_,observer,identity,epoch),
        bind(owner.history_,observer,identity,epoch),bind(owner,observer,identity,epoch)};
    owner.fc08Qualification_->related = {owner.operationEngine_.fc08Qualification_,owner.history_.fc08Qualification_};
    return registration;
  }
  static const auto& entries(const interaction::EditorHistory& history) noexcept { return history.entries_; }
  static const auto& emitted(const interaction::EditorHistory& history) noexcept { return history.emittedOperationIds_; }
  static std::size_t cursor(const interaction::EditorHistory& history) noexcept { return history.cursor_; }
};
} // namespace canvas::qualification
#endif

namespace fc08 {
using Json = nlohmann::json;
inline std::string hex(std::span<const std::uint8_t> bytes) {
  constexpr char digits[] = "0123456789abcdef";
  std::string out; out.reserve(bytes.size()*2);
  for (const auto b:bytes) { out.push_back(digits[b>>4]); out.push_back(digits[b&15]); }
  return out;
}
inline std::string id(canvas::foundation::ObjectId value) { return hex(value.bytes); }
inline canvas::foundation::ObjectId literal(std::uint64_t number) {
  canvas::foundation::ObjectId value{};
  for (unsigned i=0;i<8;++i) value.bytes[15-i]=static_cast<std::uint8_t>(number>>(8*i));
  return value;
}
inline std::string operation(const canvas::semantic::Operation& value) {
  const auto encoded = canvas::semantic::SemanticCodec::encodeProtobufOperation(value);
  if (!encoded.ok() || encoded.bytes.size()>65536) throw std::logic_error("invalid/oversized small baseline observation");
  return hex(encoded.bytes);
}
inline std::string object(const canvas::semantic::ObjectRecord& value) {
  using namespace canvas::semantic;
  return operation({OperationId{canvas::foundation::ObjectId::fromUint64(9000)},
      DocumentId{canvas::foundation::ObjectId::fromUint64(1)},1,1,InsertObjectsOp{{value}}});
}
inline Json manifestEntry(const canvas::semantic::ResourceManifestEntry& entry) {
  return {{"id",id(entry.resource_id.value)}, {"kind",static_cast<unsigned>(entry.resource_kind)},
    {"revision",entry.resource_revision},{"content_hash",entry.content_hash},{"byte_length",entry.byte_length},
    {"decode_version",entry.decode_version},{"layout_version",entry.layout_version},{"metadata",hex(entry.metadata)}};
}
inline Json manifest(const canvas::semantic::ResourceManifest& value) {
  Json out{{"schema_version",value.schema_version},{"revision",value.manifest_revision},
    {"entries",Json::array()},{"retired",Json::array()}};
  for (const auto& item:value.entries) out["entries"].push_back(manifestEntry(item));
  for (const auto& item:value.retired_entries) out["retired"].push_back({{"id",id(item.resource_id.value)},
    {"revision",item.retired_revision},{"hash",item.last_content_hash},
    {"removal_operation",id(item.removal_operation_id.value())},{"preimage",manifestEntry(item.preimage)},
    {"lineage",hex(item.lineage_token)}});
  return out;
}
} // namespace fc08

// Existing qualification friendship, read-only. There are no store mutators,
// submit substitutes or private command invocations in this observer.
namespace canvas::ink_playground {
class InkPlaygroundHistoryTestAccess final {
 public:
  static fc08::Json snapshot(const interaction::CanvasEditingRuntime& owner) {
    using fc08::Json;
    using foundation::ObjectId;
    if (owner.semanticObjects_.size()>16 || owner.history_.size()>8 ||
        owner.nextHistoryOperationOrdinal_-(std::uint64_t{1}<<63U)>64)
      throw std::logic_error("bounded baseline observer exceeded fixture domain");
    Json result{{"document_id",fc08::id(owner.documentId_.value())},
      {"records",Json::array()},{"indices",Json::array()},{"ledger",Json::array()},
      {"manifest",fc08::manifest(*owner.resourceManifestPort_)},
      {"generation",owner.semanticGeneration_.current().value()},
      {"epoch",owner.canonicalCommitClock_.runtimeEpoch().value()},
      {"ordinal",owner.canonicalCommitClock_.lastCommittedOrdinal().value()},
      {"history_size",owner.history_.size()},{"can_undo",owner.history_.canUndo()},
      {"can_redo",owner.history_.canRedo()}};
    for (const auto& record:owner.semanticObjects_.allObjects()) {
      Json item{{"id",fc08::id(record.id)},{"kind",static_cast<unsigned>(record.kind)},
        {"version",record.kind_version},{"wire",fc08::object(record)}};
      if (const auto* sticky=std::get_if<semantic::StickyContent>(&record.content)) {
        item["width"]=sticky->width; item["height"]=sticky->height;
      }
      result["records"].push_back(std::move(item));
    }
    // All possible parent identities of this small fixed fixture, including
    // candidate IDs that must remain absent after an injected rejection.
    for (unsigned n=100;n<=108;++n) {
      Json children=Json::array();
      const auto parent=n==100?std::optional<ObjectId>{}:std::optional<ObjectId>{fc08::literal(n)};
      for (const auto& child:owner.semanticObjects_.children(parent)) children.push_back(fc08::id(child.id));
      result["indices"].push_back({{"parent",n==100?"root":fc08::id(*parent)},{"children",children}});
    }
    // The complete operation-ID domain generated by this fixture. Empty
    // allocator gaps are omitted: diagnostics/uncommitted gaps are not state.
    for (unsigned lane=0;lane<2;++lane) for (std::uint64_t n=0;n<64;++n) {
      const auto key=semantic::OperationId{ObjectId::fromUint64((lane?(std::uint64_t{1}<<63U):0)+n)};
      if (const auto entry=owner.appliedOperations_.find(key))
        result["ledger"].push_back({{"id",fc08::id(key.value())},{"operation",fc08::operation(entry->canonical_operation)},
          {"fingerprint",entry->fingerprint?Json(fc08::hex(*entry->fingerprint)):Json(nullptr)}});
    }
#if defined(AXIOM_FC08_QUALIFICATION_HOOKS)
    result["history_cursor"]=qualification::Access::cursor(owner.history_);
    result["history_entries"]=Json::array();
    for (const auto& entry:qualification::Access::entries(owner.history_)) {
      Json item{{"forward",fc08::operation(entry.forward)},{"inverse",Json::array()},
        {"before",Json::array()},{"after",Json::array()}};
      for (const auto& inverse:entry.inverse) item["inverse"].push_back(fc08::operation(inverse));
      const auto expectations=[](const auto& values) {
        Json array=Json::array();
        for (const auto& value:values) array.push_back({{"id",fc08::id(value.objectId)},
          {"aspect",static_cast<unsigned>(value.aspect)},{"fields",value.fields},
          {"record",value.record?Json(fc08::object(*value.record)):Json(nullptr)}});
        return array;
      };
      item["before"]=expectations(entry.before); item["after"]=expectations(entry.after);
      result["history_entries"].push_back(std::move(item));
    }
    result["history_emitted"]=Json::array();
    for (const auto operation:qualification::Access::emitted(owner.history_)) result["history_emitted"].push_back(fc08::id(operation.value()));
#else
    // For the <=2-entry OFF predecessor, size + both public predicates have a
    // unique cursor. Full ON entry values are checked on each fault separately.
    if (owner.history_.size()>2) throw std::logic_error("OFF cursor inference outside fixed domain");
    result["history_cursor"]=!owner.history_.canUndo()?0:owner.history_.canRedo()?1:owner.history_.size();
#endif
    return result;
  }
};
} // namespace canvas::ink_playground
