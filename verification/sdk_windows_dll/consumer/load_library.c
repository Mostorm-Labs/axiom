#define WIN32_LEAN_AND_MEAN
#include <axiom/api.h>
#include <stdio.h>
#include <string.h>
#include <windows.h>
#define CHECK(x)                                                                                   \
    do {                                                                                           \
        if (!(x)) {                                                                                \
            fprintf(stderr, "FAIL LoadLibrary %d: %s\n", __LINE__, #x);                            \
            return 1;                                                                              \
        }                                                                                          \
    } while (0)
typedef int (*create_fn)(const axiom_options*, axiom_runtime*, axiom_buffer*);
typedef int (*close_fn)(axiom_runtime);
typedef int (*release_fn)(axiom_buffer);
typedef uint32_t (*version_fn)(void);
int main(void) {
    WNDCLASSW cls = {0};
    HWND w;
    int i;
    cls.hInstance = GetModuleHandleW(NULL);
    cls.lpfnWndProc = DefWindowProcW;
    cls.lpszClassName = L"AxiomPureCConsumer";
    CHECK(RegisterClassW(&cls));
    w = CreateWindowW(cls.lpszClassName,
                      L"C ABI test",
                      WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN,
                      0,
                      0,
                      500,
                      350,
                      NULL,
                      NULL,
                      cls.hInstance,
                      NULL);
    CHECK(w);
    ShowWindow(w, SW_SHOWNOACTIVATE);
    for (i = 0; i < 3; ++i) {
        HMODULE dll =
            LoadLibraryExW(L"axiom.dll",
                           NULL,
                           LOAD_LIBRARY_SEARCH_APPLICATION_DIR | LOAD_LIBRARY_SEARCH_DEFAULT_DIRS);
        create_fn create;
        close_fn close;
        release_fn release;
        version_fn version;
        axiom_options options = {0};
        axiom_runtime runtime = 0;
        axiom_buffer error = 0;
        CHECK(dll);
        /* Copy FARPROC bytes: no ISO C object/function pointer cast warning under /WX. */
        {
            FARPROC p = GetProcAddress(dll, "axiom_create");
            memcpy(&create, &p, sizeof(create));
        }
        {
            FARPROC p = GetProcAddress(dll, "axiom_close");
            memcpy(&close, &p, sizeof(close));
        }
        {
            FARPROC p = GetProcAddress(dll, "axiom_buffer_release");
            memcpy(&release, &p, sizeof(release));
        }
        {
            FARPROC p = GetProcAddress(dll, "axiom_abi_version");
            memcpy(&version, &p, sizeof(version));
        }
        CHECK(create && close && release && version && version() == 1);
        options.size = sizeof(options);
        options.abi_version = 99;
        options.parent_window = w;
        CHECK(create(&options, &runtime, &error) == AXIOM_INVALID_ARGUMENT && runtime == 0);
        options.abi_version = 1;
        CHECK(create(&options, &runtime, &error) == AXIOM_OK);
        CHECK(error == 0);
        CHECK(close(runtime) == AXIOM_OK);
        CHECK(close(runtime) == AXIOM_INVALID_HANDLE);
        CHECK(FreeLibrary(dll));
    }
    DestroyWindow(w);
    puts("PASS pure C LoadLibrary/unload/reload and ABI version rejection");
    return 0;
}
