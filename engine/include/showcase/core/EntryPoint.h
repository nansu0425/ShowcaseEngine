#pragma once

#include <showcase/core/Platform.h>

#include <Windows.h>

#define SE_MAIN(AppClass, DescType, appTitle, w, h) \
    int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int) { \
        AppClass app; \
        DescType desc; \
        desc.window.title = appTitle; \
        desc.window.width = w; \
        desc.window.height = h; \
        if (!app.Init(desc)) { \
            showcase::ShowErrorDialog(L"Error", L"Failed to initialize application"); \
            return -1; \
        } \
        int result = app.Run(); \
        app.Shutdown(); \
        return result; \
    }
