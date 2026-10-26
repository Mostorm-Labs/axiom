#include "fc08_container_fixture.hpp"
#include "canvas/interaction/editor_history.hpp"
#if __has_include("undoable_command.hpp")
#include "undoable_command.hpp"
#define FC08_REAL_HISTORY_ADMISSION 1
#endif
TEST(SectionHistory,RealPrecommitHistoryEntryExists) {
#if !defined(FC08_REAL_HISTORY_ADMISSION)
    FAIL() << "Real undoable command admission/inverse preparation is not implemented";
#else
    EXPECT_TRUE(canvas::interaction::supportsUndoableCommand(canvas::semantic::OperationKind::kEditContainerTopology));
#endif
}
