#include <showcase/core/Platform.h>
#include <showcase/editor/EditorApp.h>

#include <Windows.h>

int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int) {
    showcase::EditorApp app;
    showcase::EditorAppDesc desc;
    desc.window.title = L"ShowcaseEngine";
    desc.window.width = 1600;
    desc.window.height = 900;

    if (!app.Init(desc)) {
        showcase::ShowErrorDialog(L"Error", L"Failed to initialize application");
        return -1;
    }

    int result = app.Run();
    app.Shutdown();
    return result;
}
