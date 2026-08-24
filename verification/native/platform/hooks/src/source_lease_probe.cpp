#include <axiom/verification/platform_hooks.h>

#include <algorithm>

namespace axiom::verification::platform {

bool SourceLeaseProbe::open(SourceLease lease) {
  const auto scope_closed = std::ranges::any_of(
      closed_scopes_, [&](const auto& scope) {
        return scope.first == lease.scope_kind && scope.second == lease.scope_tag;
      });
  const auto duplicate = std::ranges::any_of(
      records_, [&](const auto& record) { return record.lease.lease_id == lease.lease_id; });
  if (scope_closed || duplicate || lease.lease_id.empty() || lease.scope_tag.empty()) {
    return false;
  }
  records_.push_back({std::move(lease), true});
  return true;
}

bool SourceLeaseProbe::close(std::string_view lease_id) {
  const auto found = std::ranges::find_if(records_, [&](const auto& record) {
    return record.lease.lease_id == lease_id;
  });
  if (found == records_.end() || !found->open) return false;
  found->open = false;
  return true;
}

void SourceLeaseProbe::close_scope(ScopeKind kind, std::string_view scope_tag) {
  const auto duplicate = std::ranges::any_of(closed_scopes_, [&](const auto& scope) {
    return scope.first == kind && scope.second == scope_tag;
  });
  if (!duplicate) closed_scopes_.emplace_back(kind, scope_tag);
}

bool SourceLeaseProbe::all_closed(ScopeKind kind, std::string_view scope_tag) const {
  return std::ranges::none_of(records_, [&](const auto& record) {
    return record.open && record.lease.scope_kind == kind && record.lease.scope_tag == scope_tag;
  });
}

bool SourceLeaseProbe::is_open(std::string_view lease_id) const {
  return std::ranges::any_of(records_, [&](const auto& record) {
    return record.open && record.lease.lease_id == lease_id;
  });
}

}  // namespace axiom::verification::platform
