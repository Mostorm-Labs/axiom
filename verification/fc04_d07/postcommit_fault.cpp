// Isolated audit process. Fail the first real C++ allocation AFTER canonical
// generation advances. No production hooks or allocation policy are changed.
#include "canvas/runtime/runtime_composition.hpp"

#include <cstdio>
#include <cstdlib>
#include <exception>
#include <new>
#include <array>
#include <string_view>
#if defined(_WIN32)
#include <intrin.h>
#endif

namespace canvas::ink_playground {
class InkPlaygroundHistoryTestAccess {
  public:
    static auto& history(interaction::CanvasEditingRuntime& r) {
        return r.history_;
    }
    static bool recover(interaction::CanvasEditingRuntime& r) {
        return r.recoverHistoryScene();
    }
};
} // namespace canvas::ink_playground
namespace {
canvas::interaction::CanvasEditingRuntime* subject = nullptr;
std::uint64_t before = 0;
bool armed = false;
bool injected = false;
bool sweepArmed = false;
bool sweepInjected = false;
std::size_t sweepCountdown = 0;
std::size_t sweepIndex = 0;
bool sweepTrace = false;
void observe(const char* label) noexcept {
    const auto* object =
        subject->qualificationObjectRecord(canvas::foundation::ObjectId::fromUint64(1));
    std::fprintf(stderr,
                 "%s before_generation=%llu generation=%llu ordinal=%llu history=%zu scene=%llu "
                 "tx=%.0f objects=%zu\n",
                 label,
                 static_cast<unsigned long long>(before),
                 static_cast<unsigned long long>(subject->semanticGeneration().value()),
                 static_cast<unsigned long long>(subject->canonicalCommitOrdinal()),
                 canvas::ink_playground::InkPlaygroundHistoryTestAccess::history(*subject).size(),
                 static_cast<unsigned long long>(subject->qualificationSceneGeneration()),
                 object ? object->transform.tx : -1.0,
                 subject->semanticObjectCount());
    std::fflush(stderr);
}
} // namespace
void* operator new(std::size_t size) {
    if (sweepArmed) {
        if (sweepCountdown == 0) {
#if defined(_WIN32)
            if (sweepTrace && sweepIndex == 100) __debugbreak();
#endif
            sweepArmed = false;
            sweepInjected = true;
            throw std::bad_alloc();
        }
        --sweepCountdown;
    }
    if (armed && subject && subject->semanticGeneration().value() > before) {
        armed = false;
        injected = true;
        observe("POSTCOMMIT_ALLOCATION_INJECTED");
        throw std::bad_alloc();
    }
    if (void* p = std::malloc(size ? size : 1))
        return p;
    throw std::bad_alloc();
}
void operator delete(void* p) noexcept {
    std::free(p);
}
void operator delete(void* p, std::size_t) noexcept {
    std::free(p);
}
int main(int argc, char** argv) {
    using namespace canvas;
    std::set_terminate([] {
        if (subject)
            observe("TERMINATE_REACHED");
        std::_Exit(86); // Capture the fatal boundary without an OS error dialog.
    });
    runtime::RuntimeComposition c;
    if (!c.open() || !c.bindSurface(64, 64))
        return 2;
    const auto id = [](std::uint64_t n) { return foundation::ObjectId::fromUint64(n); };
    semantic::ObjectRecord object;
    object.id = id(1);
    object.kind = semantic::ObjectKind::kShape;
    object.kind_version = 1;
    object.placement.order_key = semantic::OrderKey({1});
    object.content = semantic::ShapeContent{1, 20, 20};
    semantic::Operation seed{semantic::OperationId{id(500)},
                             semantic::DocumentId{id(1)},
                             1,
                             1,
                             semantic::InsertObjectsOp{{object}}};
    if (c.applyExternalOperation(seed, semantic::ApplySource::kRemoteSync).disposition !=
        semantic::ApplyDisposition::kApplied)
        return 3;
    subject = &c.qualificationRuntime();
    before = subject->semanticGeneration().value();
    semantic::Operation command{semantic::OperationId{id(501)},
                                semantic::DocumentId{id(1)},
                                1,
                                1,
                                semantic::SetTransformsOp{{{id(1), {1, 0, 0, 1, 200, 30}}}}};
    if (argc > 1 && std::string_view(argv[1]) == "explicit-intent-noalloc") {
        semantic::Operation color{semantic::OperationId{id(502)}, semantic::DocumentId{id(1)},
            1, 1, semantic::PatchPropertiesOp{{{id(1), 3,
                semantic::PropertyPatchAction::kSet, semantic::PropertyValue{0.5F}}}}};
        const std::array operations{command, color};
        if (!subject->beginExplicitIntent() || !subject->prepareExplicitIntent(operations)) return 4;
        armed = true;
        const bool accepted = subject->commitExplicitIntent();
        armed = false;
        observe("INTENT_RETURNED");
        const bool safe = accepted && !injected &&
            subject->semanticGeneration().value() == before + 2U &&
            subject->qualificationSceneGeneration() == before + 2U &&
            ink_playground::InkPlaygroundHistoryTestAccess::history(*subject).size() == 1U;
        subject = nullptr;
        return safe ? 0 : 1;
    }
    if (argc > 1 && (std::string_view(argv[1]) == "explicit-intent-prepare-sweep" ||
                     std::string_view(argv[1]) == "trace-prepare-sweep")) {
        sweepTrace = std::string_view(argv[1]) == "trace-prepare-sweep";
        semantic::Operation color{semantic::OperationId{id(502)}, semantic::DocumentId{id(1)},
            1, 1, semantic::PatchPropertiesOp{{{id(1), 3,
                semantic::PropertyPatchAction::kSet, semantic::PropertyValue{0.5F}}}}};
        const std::array operations{command, color};
        const auto originalOrdinal = subject->canonicalCommitOrdinal();
        for (std::size_t n = 0; n < 4096; ++n) {
            std::fprintf(stderr, "PREPARE_SWEEP_INDEX=%zu\n", n);
            std::fflush(stderr);
            if (!subject->beginExplicitIntent()) return 5;
            sweepCountdown = n;
            sweepIndex = n;
            sweepInjected = false;
            sweepArmed = true;
            const bool prepared = subject->prepareExplicitIntent(operations);
            sweepArmed = false;
            subject->abortExplicitIntent();
            const auto* actual = subject->qualificationObjectRecord(id(1));
            if (subject->semanticGeneration().value() != before ||
                subject->canonicalCommitOrdinal() != originalOrdinal ||
                subject->qualificationSceneGeneration() != before ||
                ink_playground::InkPlaygroundHistoryTestAccess::history(*subject).size() != 0U ||
                !actual || actual->transform.tx != object.transform.tx) return 6;
            if (!sweepInjected) {
                std::fprintf(stderr, "PREPARE_SWEEP allocations=%zu prepared=%d\n", n, prepared);
                subject = nullptr;
                return prepared ? 0 : 7;
            }
        }
        subject = nullptr;
        return 8;
    }
    if (argc > 1) {
        auto a = object, b = object;
        a.id = id(2);
        b.id = id(3);
        a.placement.order_key = semantic::OrderKey({2});
        b.placement.order_key = semantic::OrderKey({3});
        command.payload = semantic::InsertObjectsOp{{a, b}};
    }
    armed = true;
    const bool accepted =
        ink_playground::InkPlaygroundHistoryTestAccess::history(*subject).executeUndoable(command);
    armed = false;
    observe("COMMAND_RETURNED");
    std::fprintf(stderr, "accepted=%d injected=%d\n", accepted, injected);
    // A permitted outcome would retain truthful History after commit or reject
    // before any canonical mutation. Terminate is a real failure, not success.
    const bool safe =
        accepted ? ink_playground::InkPlaygroundHistoryTestAccess::history(*subject).size() == 1
                 : subject->semanticGeneration().value() == before;
    const bool recovered = ink_playground::InkPlaygroundHistoryTestAccess::recover(*subject);
    observe("RECOVERY_RETURNED");
    subject = nullptr;
    return injected && safe && recovered ? 0 : 1;
}
