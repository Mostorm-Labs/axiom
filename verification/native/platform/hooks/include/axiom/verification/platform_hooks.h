#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <string_view>
#include <vector>

namespace axiom::verification::platform {

enum class FaultMode { Activate, Clear, Pulse };
enum class FaultType { SurfaceLost, DeviceLost, PresentCompletionHeld };
enum class FaultState { Active, Cleared, Failed };
enum class GenerationKind { Surface, Metrics, Device };
enum class PublishDisposition { Forwarded, DroppedStaleScope, DroppedStaleGeneration, Cancelled };
enum class ScopeKind { Session, Canvas, Document };
enum class SourceKind { SurfaceBinding, SurfacePresentation, DeviceRecovery, FaultHook, OtherDeclared };

struct FaultRequest {
  std::string fault_id;
  FaultMode mode;
  FaultType type;
  std::uint64_t generation;
};

struct SourceLease {
  std::string lease_id;
  ScopeKind scope_kind;
  std::string scope_tag;
  SourceKind source_kind;
};

struct SourceAttempt {
  std::string attempt_id;
  std::string source_lease_id;
  GenerationKind generation_kind;
  std::uint64_t generation;
  PublishDisposition disposition;
};

struct TappedEvent {
  std::string source_lease_id;
  GenerationKind generation_kind;
  std::uint64_t generation;
};

class NormalizedPlatformPort {
 public:
  virtual ~NormalizedPlatformPort() = default;
  virtual void surface_lost(std::uint64_t generation) = 0;
  virtual void device_lost(std::uint64_t generation) = 0;
  virtual PublishDisposition probe_publish(GenerationKind kind,
                                           std::uint64_t generation) = 0;
};

class SourceLeaseProbe {
 public:
  bool open(SourceLease lease);
  bool close(std::string_view lease_id);
  void close_scope(ScopeKind kind, std::string_view scope_tag);
  bool all_closed(ScopeKind kind, std::string_view scope_tag) const;
  bool is_open(std::string_view lease_id) const;

 private:
  struct Record {
    SourceLease lease;
    bool open{true};
  };
  std::vector<Record> records_;
  std::vector<std::pair<ScopeKind, std::string>> closed_scopes_;
};

class SourceAttemptProbe {
 public:
  void record(SourceAttempt attempt);
  const std::vector<SourceAttempt>& records() const noexcept;

 private:
  std::vector<SourceAttempt> records_;
};

class EventTap {
 public:
  void record(TappedEvent event);
  const std::vector<TappedEvent>& records() const noexcept;

 private:
  std::vector<TappedEvent> records_;
};

class PlatformHooks {
 public:
  PlatformHooks(NormalizedPlatformPort& port, SourceLeaseProbe& leases,
                SourceAttemptProbe& attempts, EventTap& events);

  FaultState apply_fault(const FaultRequest& request);
  bool capture_present_completion(std::string attempt_id,
                                  std::string source_lease_id,
                                  GenerationKind generation_kind,
                                  std::uint64_t generation,
                                  std::function<void()> completion);
  PublishDisposition probe_generation(std::string attempt_id,
                                      std::string source_lease_id,
                                      GenerationKind kind,
                                      std::uint64_t generation);

 private:
  struct HeldCompletion {
    std::string attempt_id;
    std::string source_lease_id;
    GenerationKind generation_kind;
    std::uint64_t generation;
    std::function<void()> completion;
  };

  void release_held_completions();

  NormalizedPlatformPort& port_;
  SourceLeaseProbe& leases_;
  SourceAttemptProbe& attempts_;
  EventTap& events_;
  bool hold_present_completions_{false};
  std::vector<HeldCompletion> held_;
};

}  // namespace axiom::verification::platform
