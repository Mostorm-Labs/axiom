#define NOMINMAX
#include <axiom/axiom.hpp>
#include <cstdio>
#include <windows.h>

using J = axiom::Json;
static void require(bool value, const char* message) {
    if (!value)
        throw std::runtime_error(message);
}
int main() {
    const auto parent = CreateWindowExW(0,
                                        L"STATIC",
                                        L"Axiom bridge test",
                                        WS_OVERLAPPEDWINDOW,
                                        0,
                                        0,
                                        800,
                                        600,
                                        nullptr,
                                        nullptr,
                                        GetModuleHandleW(nullptr),
                                        nullptr);
    if (!parent)
        return 2;
    try {
        axiom::Options options{parent};
        options.open = {{"documentId", "11112222333344445555666677778888"}};
        axiom::Runtime a(options);
        const auto resources = a.invoke("resource", {{"action", "list"}}).value.at("value");
        require(!resources.at("entries").empty(),
                "external data owner cannot enumerate built-in resources");
        const auto resourceToken = resources.at("entries").front().at("token");
        const auto blob = a.invoke("resource", {{"action", "bytes"}, {"token", resourceToken}});
        require(!blob.bytes.empty(), "external data owner cannot retrieve resource bytes");
        a.attachData({{"capacity", 2}});
        std::vector<J> metadata;
        std::vector<std::vector<uint8_t>> operations;
        unsigned states = 0;
        int reentry = 0;
        const auto listener = a.subscribe(
            AXIOM_EVENT_OPERATION | AXIOM_EVENT_STATE,
            [&](uint32_t kind, const J& event, std::span<const uint8_t> bytes) {
                if (kind == AXIOM_EVENT_STATE) {
                    ++states;
                    return;
                }
                metadata.push_back(event);
                operations.emplace_back(bytes.begin(), bytes.end());
                axiom_buffer output = 0;
                reentry = axiom_call(
                    a.handle(), "{\"command\":\"state\"}", 19, nullptr, 0, &output, nullptr);
                require(output == 0, "reentry allocated output");
            });
        a.dispatchEvents();
        require(states == 1, "initial shell event missing");
        a.createSticky({{"objectId", "00000000000000000000000000004001"}, {"size", 80}});
        a.createSticky(
            {{"objectId", "00000000000000000000000000004002"}, {"size", 80}, {"x", 100}});
        require(metadata.empty(), "callback ran during canonical command");
        a.dispatchEvents();
        require(metadata.size() == 2 && reentry == AXIOM_REENTRANT,
                "operation dispatch/reentry failed");
        require(metadata[0].at("operationId") != metadata[1].at("operationId"),
                "operation IDs lost");
        require(metadata[0].at("commit") != metadata[1].at("commit"), "commit order lost");
        const auto before = a.state();
        bool rejected = false;
        try {
            a.createSticky({{"objectId", "00000000000000000000000000004003"}, {"size", 80}});
        } catch (const axiom::Error&) {
            rejected = true;
        }
        require(rejected && a.state() == before,
                "full publication queue mutated canonical/history");
        a.reportOperationStatus(metadata.back(), 3, 1);
        require(a.state().at("lastCommit").at("localDurable") == 3,
                "Axis status not wired to real owner");
        bool foreign = false;
        auto wrong = metadata.back();
        wrong["binding"] = "18446744073709551615";
        try {
            a.reportOperationStatus(wrong, 3, 3);
        } catch (const axiom::Error&) {
            foreign = true;
        }
        require(foreign, "foreign status accepted");
        axiom::Runtime replica(options);
        const auto replicaResources=replica.resourceManifest();
        require(replicaResources.at("entries").front().at("token").at("instance")!=resourceToken.at("instance"),
            "resource instance identities collide");
        bool wrongResource=false;
        try { (void)replica.readResourceBytes(resourceToken); } catch(const axiom::Error&){wrongResource=true;}
        require(wrongResource,"foreign resource completion accepted");
        J batch = J::array();
        for (std::size_t i = 0; i < operations.size(); ++i)
            batch.push_back(
                {{"operationId", metadata[i].at("operationId")}, {"bytes", operations[i]}});
        batch.push_back(batch[0]);
        batch.push_back(
            {{"operationId", "00000000000000000000000000000001"}, {"bytes", operations[0]}});
        batch.push_back(
            {{"operationId", "00000000000000000000000000000002"}, {"bytes", J::array({255})}});
        replica.attachData();
        const auto outcomes = replica.applyRemoteBatch(batch);
        require(outcomes.size() == 5 && outcomes[0].at("ok") == true &&
                    outcomes[1].at("ok") == true && outcomes[2].at("ok") == true &&
                    outcomes[3].at("ok") == false && outcomes[4].at("ok") == false,
                "mixed remote batch outcomes wrong");
        require(!outcomes[2].contains("commit"), "duplicate remote minted a commit");
        require(replica.state().at("objects") == 2 && replica.dataStatus().at("pending") == 0,
                "remote echoed locally");
        axiom::Runtime recovered(options);recovered.attachData();
        const auto replayed=recovered.invoke("replayBatch",{{"packets",batch}}).value.at("value");
        require(replayed[0].at("source")==4 && replayed[1].at("source")==4 &&
                recovered.state().at("objects")==2 && !recovered.state().at("canUndo").get<bool>() &&
                recovered.dataStatus().at("pending")==0,"local replay was remote/echoed/recorded as History");
        require(recovered.close(),"replay close failed");
        a.unsubscribe(listener);
        const auto count = metadata.size();
        require(!a.close(), "close silently dropped retained operations");
        const auto barrierState=a.state();
        const auto closingCanvas=static_cast<HWND>(a.window());
        SendMessageW(closingCanvas,WM_LBUTTONDOWN,MK_LBUTTON,MAKELPARAM(250,250));
        SendMessageW(closingCanvas,WM_LBUTTONUP,0,MAKELPARAM(300,300));
        require(a.state()==barrierState,"native input continued through close barrier");
        a.releaseOperations(metadata.back());
        a.reportOperationStatus(metadata.front(), 3, 3);
        a.releaseOperations(metadata.front()); // idempotent older custody release
        require(a.dataStatus().at("pending") == 0, "custody release failed");
        a.dispatchEvents();
        require(metadata.size() == count, "callback after unsubscribe");
        const auto closing = a.state();
        rejected = false;
        try {
            a.undo();
        } catch (const axiom::Error&) {
            rejected = true;
        }
        require(rejected && a.state() == closing, "canonical mutation after close barrier");
        require(a.close() && replica.close(), "drained close failed");
        // Real existing Undo/Redo must also reserve publication capacity.
        axiom::Runtime history(options);
        history.attachData({{"capacity", 1}});
        history.createSticky({{"objectId", "00000000000000000000000000005001"}, {"size", 80}});
        auto packet = history.nextOperation();
        const auto historyBefore = history.state();
        rejected = false;
        try {
            history.undo();
        } catch (const axiom::Error&) {
            rejected = true;
        }
        require(rejected && history.state() == historyBefore, "Undo bypassed queue admission");
        history.ink();
        const auto canvas = static_cast<HWND>(history.window());
        SendMessageW(canvas, WM_LBUTTONDOWN, MK_LBUTTON, MAKELPARAM(30, 30));
        SendMessageW(canvas, WM_MOUSEMOVE, MK_LBUTTON, MAKELPARAM(100, 100));
        SendMessageW(canvas, WM_LBUTTONUP, 0, MAKELPARAM(100, 100));
        require(history.state().at("objects") == 1 && history.state().at("activeContacts") == 0,
                "native input bypassed pressure or retained a contact");
        history.select(); // rejected Brush publication must cancel its transient session
        history.releaseOperations(packet.value.at("value"));
        history.undo();
        packet = history.nextOperation();
        require(packet.value.at("value").at("source") == 2, "Undo source missing");
        history.releaseOperations(packet.value.at("value"));
        history.redo();
        packet = history.nextOperation();
        history.releaseOperations(packet.value.at("value"));
        require(history.state().at("objects") == 1 && history.close(), "History changed by bridge");
        axiom::Runtime byteLimit(options);byteLimit.attachData({{"byteCapacity",1}});
        const auto beforeBytes=byteLimit.state();rejected=false;
        try {byteLimit.createSticky({{"objectId","00000000000000000000000000007001"},{"size",80}});}
        catch(const axiom::Error&){rejected=true;}
        require(rejected && byteLimit.state()==beforeBytes && byteLimit.close(),"byte admission mutated canonical state");
        axiom::Runtime retry(options);
        retry.attachData();
        unsigned attempts = 0;
        const auto retried = retry.subscribe(
            AXIOM_EVENT_OPERATION, [&](uint32_t, const J&, std::span<const uint8_t>) {
                if (++attempts == 1)
                    throw std::runtime_error("application queue unavailable");
            });
        retry.createSticky({{"objectId", "00000000000000000000000000006001"}, {"size", 80}});
        rejected = false;
        try {
            retry.dispatchEvents();
        } catch (const axiom::Error&) {
            rejected = true;
        }
        require(rejected && retry.dataStatus().at("pending") == 1, "listener failure lost custody");
        retry.dispatchEvents();
        require(attempts == 2, "listener retry skipped its failed cursor");
        retry.unsubscribe(retried);
        auto retained = retry.nextOperation();
        retry.releaseOperations(retained.value.at("value"));
        require(retry.close(), "retry close failed");
        std::puts("PASS ShellAxisBridge: deferred subscription, identity, custody, overload, "
                  "status, mixed remote, no-echo, close, Undo/Redo");
        DestroyWindow(parent);
        return 0;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "%s\n", error.what());
        DestroyWindow(parent);
        return 1;
    }
}
