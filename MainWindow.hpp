// MainWindow.hpp
#ifndef MAINWINDOW_HPP 
#define MAINWINDOW_HPP

// Define the minimum required Windows version
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0601 // Windows 7 or higher
#endif

#include <windows.h>
#include <commctrl.h>
#include <thread>
#include <string>

// Custom messages and command IDs
constexpr UINT WM_UPDATE_LISTVIEW = WM_USER + 1;  // Custom message for updating ListView
constexpr UINT ID_KILL_PROCESS = 9001;        // Command ID for killing a process

// Structure to hold process information
struct ProcessInfo {
    std::wstring name;
    DWORD id;
    SIZE_T memoryKB;
};

class MainWindow {
public:
    MainWindow(HINSTANCE hInstance);
    ~MainWindow();

    bool create(int nCmdShow);
    void show(int nCmdShow) const;
    void update() const;

    static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
    static DWORD WINAPI ProcessThread(LPVOID lpParameter); // Thread function
    static int CALLBACK CompareProc(LPARAM lParam1, LPARAM lParam2, LPARAM lParamSort); // Comparison function

private:
    static const wchar_t* CLASS_NAME;
    HWND hwnd;
    HWND hListView;

    HINSTANCE hInstance;
    HANDLE hProcessThread; // Thread handle

    void initListView();
    void populateProcessList(); // Method to gather and display process info
    void refreshProcessList();  // Method to refresh process list

    // Timer ID
    UINT_PTR refreshTimerId;

    // For C++11 thread approach:
    void processTask();  // Function for C++11 std::thread
};

#endif // MAINWINDOW_HPP
