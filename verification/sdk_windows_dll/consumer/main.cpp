#define NOMINMAX
// clang-format off
#include <windows.h>
#include <commdlg.h>
// clang-format on
#include <axiom/axiom.hpp>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <thread>

struct App {
    axiom::Runtime* runtime = nullptr;
    std::function<void()> pending;
    bool closing = false;
};
enum {
    Ink = 100,
    Select,
    Box,
    Lasso,
    Eraser,
    Partial,
    Hand,
    Text,
    CmdRectangle,
    CmdEllipse,
    Triangle,
    Sticky,
    Line,
    Arrow,
    Undo,
    Redo,
    Fit,
    Delete,
    Group,
    Ungroup,
    EnterGroup,
    LeaveGroup,
    Section,
    ReleaseSection,
    Import,
    Replace,
    Crop,
    Done,
    Cancel,
    Black,
    Blue,
    Title,
    Heading,
    Subheading,
    Body,
    Bold,
    Italic,
    Underline,
    Strike,
    Bulleted,
    Left,
    Center,
    Right,
    FontFirst = 200
};
void action(App& a, int id) {
    auto& r = *a.runtime;
    switch (id) {
    case Ink:
        r.ink();
        break;
    case Select:
        r.select();
        r.selectionMode(0);
        break;
    case Box:
        r.select();
        r.selectionMode(1);
        break;
    case Lasso:
        r.select();
        r.selectionMode(2);
        break;
    case Eraser:
    case Partial:
        r.erase();
        r.control("eraser", {{"mode", id == Eraser ? 1 : 2}});
        break;
    case Hand:
        r.control("tool", {{"value", 3}});
        break;
    case Text:
        r.text();
        break;
    case CmdRectangle:
        r.rectangle();
        break;
    case CmdEllipse:
        r.ellipse();
        break;
    case Triangle:
        r.triangle();
        break;
    case Sticky:
        r.sticky();
        break;
    case Line:
        r.line();
        break;
    case Arrow:
        r.arrow();
        break;
    case Undo:
        r.undo();
        break;
    case Redo:
        r.redo();
        break;
    case Fit:
        r.fit();
        break;
    case Delete:
        r.remove();
        break;
    case Group: {
        auto s = r.state();
        r.section({{"action", 10},
                   {"objects", s.at("selected")},
                   {"generation", s.at("generation")},
                   {"selectionRevision", s.at("selectionRevision")}});
        break;
    }
    case Ungroup: {
        auto s = r.state();
        r.section({{"action", 11},
                   {"objectId", s.at("primary")},
                   {"generation", s.at("generation")},
                   {"selectionRevision", s.at("selectionRevision")}});
        break;
    }
    case EnterGroup:
        r.section({{"action", 8}, {"objectId", r.state().at("primary")}});
        break;
    case LeaveGroup:
        r.section({{"action", 9}});
        break;
    case Section:
        r.section({{"action", 0}, {"title", "Section"}, {"boundary", {40, 40, 400, 300}}});
        break;
    case ReleaseSection:
        r.section({{"action", 2}, {"objectId", r.state().at("primary")}});
        break;
    case Import:
    case Replace: {
        wchar_t file[32768]{};
        OPENFILENAMEW o{};
        o.lStructSize = sizeof(o);
        o.hwndOwner = GetParent(static_cast<HWND>(r.window()));
        o.lpstrFile = file;
        o.nMaxFile = 32768;
        o.lpstrFilter =
            L"Images (*.png;*.jpg;*.jpeg;*.webp)\0*.png;*.jpg;*.jpeg;*.webp\0All files\0*.*\0\0";
        o.Flags = OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR;
        if (!GetOpenFileNameW(&o))
            break;
        const auto path = std::filesystem::path(file);
        std::ifstream stream(path, std::ios::binary);
        std::vector<uint8_t> bytes{std::istreambuf_iterator<char>(stream), {}};
        auto ext = path.extension().wstring();
        for (auto& ch : ext)
            ch = static_cast<wchar_t>(towlower(ch));
        r.image({{"action", id == Import ? "import" : "replace"},
                 {"mime",
                  ext == L".png"    ? "image/png"
                  : ext == L".webp" ? "image/webp"
                                    : "image/jpeg"}},
                bytes);
        break;
    }
    case Crop:
        r.image({{"action", "crop"},
                 {"objectId", r.image({{"action", "read"}}).at("value").at("objectId")}});
        break;
    case Done:
    case Cancel:
        r.image({{"action", id == Done ? "done" : "cancel"},
                 {"token", r.image({{"action", "readCrop"}}).at("value").at("token")}});
        break;
    case Black:
    case Blue:
        r.textFormat(
            {{"color", id == Black ? axiom::Json{0, 0, 0, 1} : axiom::Json{0.1, 0.3, 0.9, 1}}});
        break;
    case Title:
    case Heading:
    case Subheading:
    case Body:
        r.textFormat({{"fontSize",
                       id == Title        ? 32
                       : id == Heading    ? 24
                       : id == Subheading ? 20
                                          : 16}});
        break;
    case Bold:
    case Italic:
    case Underline:
    case Strike:
    case Bulleted: {
        const auto field = id == Bold        ? "bold"
                           : id == Italic    ? "italic"
                           : id == Underline ? "underline"
                           : id == Strike    ? "strikethrough"
                                             : "bulleted";
        auto format = r.textSnapshot().at("format");
        r.textFormat({{field, !format.value(field, false)}});
        break;
    }
    case Left:
    case Center:
    case Right:
        r.textFormat({{"alignment", id == Left ? 1 : id == Center ? 2 : 3}});
        break;
    default:
        if (id >= FontFirst) {
            auto fonts = r.fonts().at("entries");
            auto index = static_cast<size_t>(id - FontFirst);
            if (index < fonts.size())
                r.textFormat({{"font", fonts[index].at("id")}});
        }
        break;
    }
}
LRESULT CALLBACK appProc(HWND w, UINT m, WPARAM a, LPARAM b) {
    auto* app = reinterpret_cast<App*>(GetWindowLongPtrW(w, GWLP_USERDATA));
    if (m == WM_NCCREATE) {
        app = static_cast<App*>(reinterpret_cast<CREATESTRUCTW*>(b)->lpCreateParams);
        SetWindowLongPtrW(w, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(app));
    }
    if (app && app->runtime) {
        if (m == WM_COMMAND) {
            const int id = LOWORD(a);
            app->pending = [app, id] { action(*app, id); };
            return 0;
        }
        if (m == WM_SIZE && LOWORD(b) && HIWORD(b)) {
            try {
                app->runtime->resize(static_cast<float>(LOWORD(b)) * 96 / GetDpiForWindow(w),
                                     static_cast<float>(HIWORD(b)) * 96 / GetDpiForWindow(w),
                                     static_cast<float>(GetDpiForWindow(w)) / 96);
            } catch (const axiom::Error&) {
            }
            return 0;
        }
        if (m == WM_CLOSE) {
            app->closing = true;
            return 0;
        }
    }
    return DefWindowProcW(w, m, a, b);
}
void menu(HWND w, axiom::Runtime& r) {
    HMENU bar = CreateMenu();
    const auto add = [&](const wchar_t* name,
                         std::initializer_list<std::pair<int, const wchar_t*>> items) {
        auto popup = CreatePopupMenu();
        for (auto [id, label] : items)
            AppendMenuW(popup, MF_STRING, static_cast<UINT_PTR>(id), label);
        AppendMenuW(bar, MF_POPUP, reinterpret_cast<UINT_PTR>(popup), name);
    };
    add(L"Tools",
        {{Ink, L"Ink"},
         {Select, L"Select"},
         {Box, L"Rectangle select"},
         {Lasso, L"Lasso"},
         {Eraser, L"Object eraser"},
         {Partial, L"Partial eraser"},
         {Hand, L"Hand"},
         {Text, L"Text"}});
    add(L"Create",
        {{CmdRectangle, L"Rectangle"},
         {CmdEllipse, L"Ellipse"},
         {Triangle, L"Triangle"},
         {Sticky, L"Sticky"},
         {Line, L"Orthogonal line"},
         {Arrow, L"Orthogonal arrow"},
         {Section, L"Section"}});
    add(L"Edit",
        {{Undo, L"Undo"},
         {Redo, L"Redo"},
         {Delete, L"Delete"},
         {Fit, L"Fit content"},
         {Group, L"Group"},
         {Ungroup, L"Ungroup"},
         {EnterGroup, L"Enter group"},
         {LeaveGroup, L"Leave group"},
         {ReleaseSection, L"Release section"}});
    add(L"Image",
        {{Import, L"Import"},
         {Replace, L"Replace selected"},
         {Crop, L"Crop"},
         {Done, L"Done"},
         {Cancel, L"Cancel"}});
    add(L"Text format",
        {{Black, L"Black"},
         {Blue, L"Blue"},
         {Title, L"Title 32"},
         {Heading, L"Heading 24"},
         {Subheading, L"Subheading 20"},
         {Body, L"Body 16"},
         {Bold, L"Bold"},
         {Italic, L"Italic"},
         {Underline, L"Underline"},
         {Strike, L"Strikethrough"},
         {Bulleted, L"Bulleted"},
         {Left, L"Left"},
         {Center, L"Center"},
         {Right, L"Right"}});
    auto fonts = CreatePopupMenu();
    int index = FontFirst;
    const auto catalog = r.fonts().at("entries");
    for (const auto& f : catalog) {
        const auto family = f.at("family").get<std::string>();
        std::wstring label(family.begin(), family.end());
        AppendMenuW(fonts, MF_STRING, static_cast<UINT_PTR>(index++), label.c_str());
    }
    AppendMenuW(bar, MF_POPUP, reinterpret_cast<UINT_PTR>(fonts), L"Font");
    SetMenu(w, bar);
}

#define CHECK(x)                                                                                   \
    do {                                                                                           \
        if (!(x)) {                                                                                \
            std::fprintf(stderr, "FAIL %d: %s\n", __LINE__, #x);                                   \
            return 1;                                                                              \
        }                                                                                          \
    } while (false)
int main(int argc, char**) {
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    App app;
    WNDCLASSW cls{};
    cls.lpfnWndProc = appProc;
    cls.hInstance = GetModuleHandleW(nullptr);
    cls.lpszClassName = L"ExternalAxiomDll";
    CHECK(RegisterClassW(&cls));
    const auto window = CreateWindowW(cls.lpszClassName,
                                      L"Axiom DLL example",
                                      WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN,
                                      20,
                                      20,
                                      900,
                                      700,
                                      nullptr,
                                      nullptr,
                                      cls.hInstance,
                                      &app);
    CHECK(window);
    ShowWindow(window, SW_SHOW);
    try {
        int callbackStatus = -1;
        std::vector<uint8_t> lastOperation;
        axiom_runtime live = 0;
        axiom::Options options{window};
        options.onOperation = [&](std::span<const uint8_t> bytes) {
            if (bytes.empty())
                throw std::runtime_error("empty operation");
            lastOperation.assign(bytes.begin(), bytes.end());
            axiom_buffer out = 0;
            callbackStatus =
                axiom_call(live, "{\"command\":\"state\"}", 19, nullptr, 0, &out, nullptr);
            if (out)
                axiom_buffer_release(out);
        };
        axiom::Runtime runtime(std::move(options));
        live = runtime.handle();
        app.runtime = &runtime;
        std::printf("BUILD %s\n", runtime.buildInfo().dump().c_str());
        CHECK(runtime.state().at("objects") == 0);
        if (argc > 1)
            menu(window, runtime);
        if (argc == 1) {
            menu(window, runtime);
            MSG msg{};
            while (IsWindow(window)) {
                while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
                    TranslateMessage(&msg);
                    DispatchMessageW(&msg);
                }
                try {
                    if (app.pending) {
                        app.pending();
                        app.pending = {};
                    }
                    runtime.frame();
                    if (app.closing && runtime.close()) {
                        app.runtime = nullptr;
                        DestroyWindow(window);
                        break;
                    }
                } catch (const axiom::Error& e) {
                    if (e.status != AXIOM_BUSY) {
                        app.pending = {};
                        MessageBoxA(
                            window, e.what(), "Axiom command rejected", MB_OK | MB_ICONERROR);
                    }
                }
                Sleep(8);
            }
            return 0;
        }
        runtime.rectangle();
        const auto canvas = static_cast<HWND>(runtime.window());
        SendMessageW(canvas, WM_LBUTTONDOWN, MK_LBUTTON, MAKELPARAM(30, 30));
        SendMessageW(canvas, WM_MOUSEMOVE, MK_LBUTTON, MAKELPARAM(200, 150));
        SendMessageW(canvas, WM_LBUTTONUP, 0, MAKELPARAM(200, 150));
        runtime.frame();
        CHECK(runtime.state().at("objects") == 1);
        if (argc > 1) {
            CHECK(callbackStatus == AXIOM_REENTRANT);
            runtime.undo();
            CHECK(runtime.state().at("objects") == 0);
            runtime.redo();
            CHECK(runtime.state().at("objects") == 1);
            axiom_buffer retained = 0;
            CHECK(axiom_call(runtime.handle(),
                             "{\"command\":\"state\"}",
                             19,
                             nullptr,
                             0,
                             &retained,
                             nullptr) == AXIOM_OK);
            size_t required = 0;
            CHECK(axiom_buffer_size(retained, &required) == AXIOM_OK && required > 0);
            CHECK(axiom_buffer_copy(retained, nullptr, 0, nullptr) == AXIOM_BUFFER_TOO_SMALL);
            int status = 0;
            std::thread other([&] {
                axiom_buffer out = 0;
                status = axiom_call(
                    runtime.handle(), "{\"command\":\"state\"}", 19, nullptr, 0, &out, nullptr);
            });
            other.join();
            CHECK(status == AXIOM_WRONG_THREAD);
            runtime.registerSource(0x100000001ULL);
            runtime.ink();
            axiom_sample sample{};
            sample.source = 0x100000001ULL;
            sample.pointer = 0x200000001ULL;
            sample.x = 250;
            sample.y = 60;
            sample.sequence = 1;
            sample.timestamp_ns = 1000000;
            auto first = runtime.begin({&sample, 1});
            sample.phase = 2;
            sample.sequence = 2;
            sample.timestamp_ns = 2000000;
            runtime.submit(first, {&sample, 1});
            sample.phase = 0;
            sample.sequence = 3;
            sample.timestamp_ns = 3000000;
            auto secondKey = runtime.begin({&sample, 1});
            CHECK(secondKey.generation != first.generation);
            sample.phase = 2;
            sample.sequence = 4;
            sample.timestamp_ns = 4000000;
            CHECK(axiom_input_submit(runtime.handle(), &first, &sample, 1) == AXIOM_REJECTED);
            CHECK(axiom_close(runtime.handle()) == AXIOM_BUSY);
            axiom::Runtime second({window});
            CHECK(second.state().at("objects") == 0);
            CHECK(axiom_input_submit(second.handle(), &secondKey, &sample, 1) == AXIOM_REJECTED);
            runtime.submit(secondKey, {&sample, 1});
            runtime.unregisterSource(sample.source);
            CHECK(second.close());
            runtime.text();
            runtime.textIntent(0, {{"x", 350}, {"y", 250}, {"width", 160}});
            CHECK(runtime.textSnapshot().at("phase") == 1);
            SendMessageW(canvas, WM_CHAR, L'你', 0);
            SendMessageW(canvas, WM_CHAR, L'好', 0);
            runtime.frame();
            CHECK(runtime.textSnapshot().at("projection").size() == 2);
            SendMessageW(canvas, WM_KILLFOCUS, reinterpret_cast<WPARAM>(window), 0);
            runtime.frame();
            CHECK(runtime.textSnapshot().at("phase") == 1);
            runtime.textIntent(12);
            runtime.textFormat({{"bold", true}, {"italic", true}, {"fontSize", 24.0}});
            CHECK(runtime.textSnapshot().at("format").at("bold") == true);
            runtime.textIntent(2);
            CHECK(runtime.textSnapshot().at("phase") == 0);
            CHECK(!runtime.fonts().at("entries").empty());
            runtime.ellipse();
            SendMessageW(canvas, WM_LBUTTONDOWN, MK_LBUTTON, MAKELPARAM(220, 40));
            SendMessageW(canvas, WM_LBUTTONUP, 0, MAKELPARAM(300, 140));
            runtime.select();
            runtime.selectionMode(1);
            SendMessageW(canvas, WM_LBUTTONDOWN, MK_LBUTTON, MAKELPARAM(10, 10));
            SendMessageW(canvas, WM_LBUTTONUP, 0, MAKELPARAM(320, 170));
            CHECK(runtime.selection().at("value").at("count").get<int>() >= 2);
            auto selected = runtime.state();
            auto grouped =
                runtime.section({{"action", 10},
                                 {"objects", selected.at("selected")},
                                 {"generation", selected.at("generation")},
                                 {"selectionRevision", selected.at("selectionRevision")}});
            auto group = grouped.at("section").at("objectId");
            runtime.section({{"action", 8}, {"objectId", group}});
            CHECK(runtime.selection().at("value").at("groupScope") == group);
            runtime.section({{"action", 9}});
            selected = runtime.state();
            runtime.section({{"action", 11},
                             {"objectId", group},
                             {"generation", selected.at("generation")},
                             {"selectionRevision", selected.at("selectionRevision")}});
            auto section =
                runtime
                    .section(
                        {{"action", 0}, {"title", "DLL Section"}, {"boundary", {0, 0, 500, 350}}})
                    .at("section")
                    .at("objectId");
            runtime.section({{"action", 5}, {"objectId", section}, {"title", "Renamed"}});
            CHECK(runtime.invoke("readSection", {{"objectId", section}})
                      .value.at("value")
                      .at("title") == "Renamed");
            runtime.section({{"action", 2}, {"objectId", section}});
            auto connector = runtime.connector({{"action", "create"},
                                                {"objectId", "000000000000000000000000000c0001"},
                                                {"start", {{"point", {20, 400}}}},
                                                {"end", {{"point", {300, 450}}}}});
            CHECK(runtime.connector({{"action", "read"}, {"objectId", connector.at("objectId")}})
                      .at("value")
                      .at("routing") == 2);
            wchar_t executable[32768]{};
            CHECK(GetModuleFileNameW(nullptr, executable, 32768));
            const auto appDir = std::filesystem::path(executable).parent_path();
            const auto imageBytes = [&](const wchar_t* name) {
                std::ifstream stream(appDir / name, std::ios::binary);
                return std::vector<uint8_t>{std::istreambuf_iterator<char>(stream), {}};
            };
            const auto png = imageBytes(L"sample.png");
            const auto jpg = imageBytes(L"sample.jpg");
            const auto webp = imageBytes(L"sample.webp");
            auto imported = runtime.importImage(png, "image/png");
            CHECK(runtime.readImage().at("content").at("intrinsicWidth") == 4);
            runtime.replaceImage(jpg, "image/jpeg");
            CHECK(runtime.readImage().at("content").at("intrinsicWidth") == 138);
            runtime.undo();
            CHECK(runtime.readImage().at("content").at("intrinsicWidth") == 4);
            runtime.replaceImage(webp, "image/webp");
            CHECK(runtime.readImage().at("content").at("intrinsicWidth") == 550);
            const auto crop = runtime.beginCrop(imported.at("objectId"));
            const auto initial = runtime.readCrop();
            runtime.updateCrop({{"token", crop.at("cropToken")},
                                {"sourceRect", initial.at("content").at("sourceRect")},
                                {"width", initial.at("content").at("width")},
                                {"height", initial.at("content").at("height")},
                                {"scale", 2}});
            CHECK(runtime.readCrop().at("scale") == 2);
            runtime.cancelCrop(crop.at("cropToken"));
            CHECK(!runtime.readCrop().at("active").get<bool>());
            auto checkpoint = runtime.snapshot({{"generation", runtime.state().at("generation")},
                                                {"documentRevision", "9007199254740993"},
                                                {"recoveryFrontier", {8, 1, 16, 1, 24, 1}}});
            CHECK(!checkpoint.bytes.empty());
            axiom::Options restore{window};
            restore.open = {{"documentId", runtime.state().at("documentId")},
                            {"restore",
                             {{"documentRevision", "9007199254740993"},
                              {"recoveryFrontier", {8, 1, 16, 1, 24, 1}},
                              {"checkpointDigest", checkpoint.value.at("checkpointDigest")},
                              {"resourceBinding", "55"}}}};
            restore.snapshot = checkpoint.bytes;
            axiom::Runtime replica(std::move(restore));
            CHECK(replica.state().at("objects") == runtime.state().at("objects"));
            CHECK(!replica.state().at("canUndo").get<bool>());
            runtime.createSticky({{"objectId", "000000000000000000000000000c0002"},
                                  {"x", 550},
                                  {"y", 20},
                                  {"size", 80}});
            CHECK(!lastOperation.empty());
            replica.applyRemote(lastOperation);
            CHECK(replica.state().at("objects") == runtime.state().at("objects"));
            const auto restoredCount = replica.state().at("objects");
            replica.applyRemote(lastOperation);
            CHECK(replica.state().at("objects") == restoredCount);
            CHECK(replica.close());
            runtime.resize(600, 440, 1);
            CHECK(runtime.state().at("surface").at("width") == 600);
            runtime.loseSurface();
            CHECK(!runtime.state().at("surface").at("available").get<bool>());
            runtime.resize(600, 440, 1);
            runtime.frame(true);
            const auto old = runtime.handle();
            CHECK(runtime.close());
            CHECK(axiom_buffer_size(retained, &required) == AXIOM_OK);
            CHECK(axiom_buffer_release(retained) == AXIOM_OK);
            CHECK(axiom_buffer_release(retained) == AXIOM_INVALID_HANDLE);
            axiom_buffer out = 0;
            CHECK(axiom_call(old, "{}", 2, nullptr, 0, &out, nullptr) == AXIOM_INVALID_HANDLE);
            app.runtime = nullptr;
            DestroyWindow(window);
            std::puts(
                "PASS DLL /MD external app, real HWND input, commands, isolated owners, lifecycle");
            return 0;
        }
        MSG msg{};
        while (IsWindow(window)) {
            while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
                TranslateMessage(&msg);
                DispatchMessageW(&msg);
            }
            runtime.frame();
            Sleep(8);
        }
        runtime.abandon();
    } catch (const axiom::Error& e) {
        std::fprintf(stderr, "%s %s\n", e.what(), e.details.dump().c_str());
        DestroyWindow(window);
        return 1;
    } catch (const std::exception& e) {
        std::fprintf(stderr, "%s\n", e.what());
        DestroyWindow(window);
        return 1;
    }
}
