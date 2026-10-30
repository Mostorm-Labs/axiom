#pragma once
#include <algorithm>
#include <canvas/semantic/canonical_commit_record.hpp>
#include <canvas/semantic/codec.hpp>
#include <iterator>
#include <list>
#include <nlohmann/json.hpp>
#include <span>

namespace canvas::sdk {
// Volatile bridge custody only. All allocating publication work precedes apply.
class DataPort final {
  public:
    struct Packet {
        semantic::OperationId id{};
        semantic::DocumentId document{};
        semantic::OperationKind kind{};
        semantic::CanonicalCommitStamp stamp{};
        semantic::ApplySource source{};
        std::uint64_t before = 0, after = 0;
        std::uint8_t durable = 0, synced = 0;
        std::vector<std::uint8_t> bytes;
    };
    bool attached = false, closing = false, fault = false;
    std::size_t maxPackets = 1024, maxBytes = 32 * 1024 * 1024, retainedBytes = 0;
    std::list<Packet> retained, prepared, receipts;
    Packet released;

    bool prepare(std::span<const semantic::Operation> operations) noexcept {
        if (!attached)
            return true;
        prepared.clear();
        if (closing || fault || operations.size() > maxPackets - retained.size())
            return false;
        try {
            std::size_t reserved = retainedBytes;
            for (const auto& op : operations) {
                auto encoded = semantic::SemanticCodec::encodeProtobufOperation(op);
                if (!encoded.ok() || encoded.bytes.size() > maxBytes - reserved) {
                    prepared.clear();
                    return false;
                }
                reserved += encoded.bytes.size();
                Packet p;
                p.id = op.id;
                p.document = op.document_id;
                p.kind = op.kind();
                p.bytes = std::move(encoded.bytes);
                prepared.push_back(std::move(p));
            }
            return true;
        } catch (...) {
            prepared.clear();
            return false;
        }
    }
    const Packet* commit(const semantic::Operation& op,
                         const semantic::CanonicalCommitRecord& record) noexcept {
        if (!attached)
            return nullptr;
        if (prepared.empty() || prepared.front().id != op.id) {
            fault = true;
            return nullptr;
        }
        auto& p = prepared.front();
        p.stamp = record.commit_stamp;
        p.source = record.source;
        p.before = record.before_generation.value();
        p.after = record.after_generation.value();
        retainedBytes += p.bytes.size();
        retained.splice(retained.end(), prepared, prepared.begin());
        return &retained.back();
    }
    const Packet* latest() const noexcept {
        return retained.empty() ? (receipts.empty() ? nullptr : &receipts.back())
                                : &retained.back();
    }
    const Packet* find(const semantic::OperationId& id,
                       const semantic::CanonicalCommitStamp& stamp) const noexcept {
        if (id.isZero() || stamp.ordinal.value() == 0)
            return nullptr;
        if (!retained.empty() && retained.back().id==id && retained.back().stamp==stamp) return &retained.back();
        if (!receipts.empty() && receipts.back().id==id && receipts.back().stamp==stamp) return &receipts.back();
        for (const auto& p : retained)
            if (p.id == id && p.stamp == stamp)
                return &p;
        for (const auto& p : receipts)
            if (p.id == id && p.stamp == stamp)
                return &p;
        return nullptr;
    }
    bool release(const semantic::OperationId& id,
                 const semantic::CanonicalCommitStamp& stamp) noexcept {
        if (id.isZero() || stamp.ordinal.value() == 0)
            return false;
        auto it = std::find_if(retained.begin(), retained.end(), [&](const auto& p) {
            return p.id == id && p.stamp == stamp;
        });
        if (it == retained.end())
            return find(id, stamp) != nullptr;
        auto end = std::next(it);
        for (auto p = retained.begin(); p != end; ++p) {
            retainedBytes -= p->bytes.size();
            std::vector<std::uint8_t>{}.swap(p->bytes);
        }
        released = *it;
        receipts.splice(receipts.end(), retained, retained.begin(), end);
        while (receipts.size() > 4096)
            receipts.pop_front();
        return true;
    }
    bool report(const semantic::OperationId& id,
                const semantic::CanonicalCommitStamp& stamp,
                std::uint8_t durable,
                std::uint8_t synced) noexcept {
        auto* p = const_cast<Packet*>(find(id, stamp));
        if (!p || durable > 3 || synced > 3 || (synced == 3 && durable != 3) ||
            (p->durable == 3 && durable != 3) || (p->synced == 3 && synced != 3))
            return false;
        p->durable = durable;
        p->synced = synced;
        return true;
    }
};
} // namespace canvas::sdk
