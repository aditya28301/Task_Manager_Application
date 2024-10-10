// main.cpp
#include <windows.h>
#include "MainWindow.hpp"

// Link against the Common Controls and UxTheme libraries via pragma
#pragma comment(lib, "Comctl32.lib")
#pragma comment(lib, "uxtheme.lib")
#pragma comment(lib, "Psapi.lib")

// Entry point
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    MainWindow mainWindow(hInstance);

    if (!mainWindow.create(nCmdShow)) {
        return 0;
    }

    mainWindow.show(nCmdShow);
    mainWindow.update();

    // Message loop
    MSG Msg = { 0 }; // Zero-initialize
    while (GetMessage(&Msg, NULL, 0, 0) > 0) {
        TranslateMessage(&Msg);
        DispatchMessage(&Msg);
    }

    return static_cast<int>(Msg.wParam);
}
