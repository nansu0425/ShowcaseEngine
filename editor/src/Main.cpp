#include <showcase/editor/EditorApp.h>

#include <Windows.h>

int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int) {
    showcase::EditorApp app;
    showcase::EditorAppDesc desc;
    desc.window.title = L"ShowcaseEngine";
    desc.window.width = 1600;
    desc.window.height = 900;

    if (!app.Init(desc)) {
        MessageBoxW(nullptr, L"Failed to initialize application", L"Error", MB_OK | MB_ICONERROR);
        return -1;
    }

    int result = app.Run();
    app.Shutdown();
    return result;
}
