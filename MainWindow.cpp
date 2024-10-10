// MainWindow.cpp
#include "MainWindow.hpp"
#include <psapi.h>      // For EnumProcesses, GetModuleBaseName, GetProcessMemoryInfo
#include <tchar.h>      // For TCHAR
#include <uxtheme.h>    // For SetWindowTheme
#include <shellapi.h>   // For Shell APIs
#include <memory>
#include <vector>

// Define LVM_GETITEMDATA if not defined
#ifndef LVM_GETITEMDATA
#define LVM_FIRST 0x1000
#define LVM_GETITEMDATA (LVM_FIRST + 25)
#endif

#pragma comment(lib, "Comctl32.lib")
#pragma comment(lib, "uxtheme.lib")
#pragma comment(lib, "Psapi.lib")

const wchar_t* MainWindow::CLASS_NAME = L"myWindowClass";

// Define a global background brush (black)
static HBRUSH g_hbrBackground = CreateSolidBrush(RGB(0, 0, 0)); // Black

// Constructor
MainWindow::MainWindow(HINSTANCE hInstance)
    : hInstance(hInstance), hwnd(nullptr), hListView(nullptr), hProcessThread(nullptr), refreshTimerId(1) {}

// Destructor
MainWindow::~MainWindow() {
    if (hListView) {
        // Delete all ProcessInfo pointers
        int itemCount = ListView_GetItemCount(hListView);
        for (int i = 0; i < itemCount; ++i) {
            ProcessInfo* pInfo = reinterpret_cast<ProcessInfo*>(SendMessage(hListView, LVM_GETITEMDATA, static_cast<WPARAM>(i), 0));
            delete pInfo;
        }
        DestroyWindow(hListView);  // Ensure ListView is destroyed
        hListView = nullptr;
    }

    if (hProcessThread) {
        WaitForSingleObject(hProcessThread, INFINITE);  // Wait for the thread to finish
        CloseHandle(hProcessThread);  // Close the thread handle
        hProcessThread = nullptr;
    }

    // Delete the background brush
    if (g_hbrBackground) {
        DeleteObject(g_hbrBackground);
        g_hbrBackground = NULL;
    }
}

// Create and register window class
bool MainWindow::create(int nCmdShow) {
    WNDCLASSEX wc = { 0 }; // Zero-initialize the structure

    wc.cbSize = sizeof(WNDCLASSEX);
    wc.style = CS_HREDRAW | CS_VREDRAW; // Redraw on horizontal and vertical changes
    wc.lpfnWndProc = MainWindow::WndProc;
    wc.cbClsExtra = 0;
    wc.cbWndExtra = sizeof(MainWindow*); // Allocate space for the pointer
    wc.hInstance = hInstance;
    wc.hIcon = LoadIcon(NULL, IDI_APPLICATION);
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = g_hbrBackground; // Set black background
    wc.lpszMenuName = NULL;
    wc.lpszClassName = CLASS_NAME;
    wc.hIconSm = LoadIcon(NULL, IDI_APPLICATION);

    if (!RegisterClassEx(&wc)) {
        MessageBox(NULL, L"Window Registration Failed!", L"Error!", MB_ICONEXCLAMATION | MB_OK);
        return false;
    }

    // Create a window with default size; it will be maximized shortly
    hwnd = CreateWindowEx(
        WS_EX_CLIENTEDGE,
        CLASS_NAME,
        L"Task Manager - GUI",
        WS_OVERLAPPEDWINDOW | WS_VISIBLE | WS_MAXIMIZEBOX | WS_MINIMIZEBOX | WS_SIZEBOX,
        CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
        NULL, NULL, hInstance, this); // Pass 'this' as lpParam

    if (hwnd == NULL) {
        MessageBox(NULL, L"Window Creation Failed!", L"Error!", MB_ICONEXCLAMATION | MB_OK);
        return false;
    }

    // Maximize the window to full screen
    ShowWindow(hwnd, SW_MAXIMIZE);
    UpdateWindow(hwnd);

    // Initialize ListView
    initListView();
    populateProcessList();  // Launch the thread for populating processes

    // Set timer to refresh process list every 5 seconds
    SetTimer(hwnd, refreshTimerId, 5000, NULL);

    return true;
}

// Show the window
void MainWindow::show(int nCmdShow) const {
    ShowWindow(hwnd, nCmdShow);
}

// Update the window
void MainWindow::update() const {
    UpdateWindow(hwnd);
}

// Comparison function for sorting
int CALLBACK MainWindow::CompareProc(LPARAM lParam1, LPARAM lParam2, LPARAM lParamSort) {
    // lParam1 and lParam2 are pointers to ProcessInfo
    ProcessInfo* pInfo1 = reinterpret_cast<ProcessInfo*>(lParam1);
    ProcessInfo* pInfo2 = reinterpret_cast<ProcessInfo*>(lParam2);

    if (pInfo1 && pInfo2) {
        // Example: sort by process name
        return pInfo1->name.compare(pInfo2->name);
    }

    return 0;
}

// Window Procedure to handle messages, including resizing and custom draw
LRESULT CALLBACK MainWindow::WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    MainWindow* window = nullptr;

    if (msg == WM_NCCREATE) {
        CREATESTRUCT* createStruct = reinterpret_cast<CREATESTRUCT*>(lParam);
        window = reinterpret_cast<MainWindow*>(createStruct->lpCreateParams);
        SetWindowLongPtr(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(window));
    }
    else {
        window = reinterpret_cast<MainWindow*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
    }

    if (window) {
        switch (msg) {
        case WM_SIZE: {
            // Get the new client area dimensions
            RECT rcClient;
            GetClientRect(hwnd, &rcClient);
            int newWidth = rcClient.right - rcClient.left;
            int newHeight = rcClient.bottom - rcClient.top;

            // Resize the ListView to fill the client area
            MoveWindow(window->hListView, 0, 0, newWidth, newHeight, TRUE);

            // Adjust column widths based on the new width
            // Column 0: 50%, Column 1: 25%, Column 2: 25%
            int column0Width = newWidth * 50 / 100;
            int column1Width = newWidth * 25 / 100;
            int column2Width = newWidth - column0Width - column1Width - 8; // Adjust for padding/margin

            ListView_SetColumnWidth(window->hListView, 0, column0Width);
            ListView_SetColumnWidth(window->hListView, 1, column1Width);
            ListView_SetColumnWidth(window->hListView, 2, column2Width);
            break;
        }

        case WM_UPDATE_LISTVIEW: {
            // Extract process info
            ProcessInfo* pInfo = reinterpret_cast<ProcessInfo*>(lParam);

            // Insert process name into the first column of the ListView
            LVITEM lvItem = { 0 }; // Zero-initialize
            lvItem.mask = LVIF_TEXT | LVIF_PARAM;
            lvItem.iItem = ListView_GetItemCount(window->hListView);  // Insert at the end of the list
            lvItem.iSubItem = 0;  // Process Name in the first column
            lvItem.pszText = const_cast<LPWSTR>(pInfo->name.c_str());
            lvItem.lParam = reinterpret_cast<LPARAM>(pInfo); // Store pointer in lParam
            lvItem.iIndent = 0;

            int index = ListView_InsertItem(window->hListView, &lvItem);

            if (index != -1) {
                // Set the process ID in the second column
                TCHAR processIDText[16] = { 0 };
                _stprintf_s(processIDText, _T("%u"), pInfo->id);
                ListView_SetItemText(window->hListView, index, 1, processIDText);

                // Set the memory usage in the third column
                TCHAR memoryText[32] = { 0 };
                _stprintf_s(memoryText, _T("%llu"), static_cast<unsigned long long>(pInfo->memoryKB));
                ListView_SetItemText(window->hListView, index, 2, memoryText);
            }

            break;
        }

        case WM_NOTIFY: {
            LPNMHDR pnmh = reinterpret_cast<LPNMHDR>(lParam);
            if (pnmh->hwndFrom == window->hListView) {
                if (pnmh->code == NM_CUSTOMDRAW) {
                    LPNMLVCUSTOMDRAW lplvcd = reinterpret_cast<LPNMLVCUSTOMDRAW>(lParam);

                    switch (lplvcd->nmcd.dwDrawStage) {
                    case CDDS_PREPAINT:
                        return CDRF_NOTIFYITEMDRAW;

                    case CDDS_ITEMPREPAINT:
                        // Set text and background colors
                        lplvcd->clrText = RGB(255, 255, 255);      // White text
                        lplvcd->clrTextBk = RGB(0, 0, 0);          // Black background
                        return CDRF_DODEFAULT;

                    case CDDS_SUBITEM | CDDS_ITEMPREPAINT:
                        // Optionally, customize subitem colors or fonts here
                        return CDRF_DODEFAULT;

                    default:
                        return CDRF_DODEFAULT;
                    }
                }
                else if (pnmh->code == LVN_COLUMNCLICK) {
                    LPNMLISTVIEW pListView = reinterpret_cast<LPNMLISTVIEW>(lParam);
                    int clickedColumn = pListView->iSubItem;

                    // Sort the items based on the clicked column
                    ListView_SortItemsEx(window->hListView,
                        MainWindow::CompareProc,
                        reinterpret_cast<LPARAM>(window->hListView));
                }
                else if (pnmh->code == NM_RCLICK) {
                    // Get cursor position
                    POINT pt;
                    GetCursorPos(&pt);

                    // Determine which item was clicked
                    LVHITTESTINFO hitTest = { 0 };
                    hitTest.pt = pt;
                    ScreenToClient(window->hListView, &hitTest.pt);
                    ListView_HitTest(window->hListView, &hitTest);

                    if (hitTest.flags & LVHT_ONITEM) {
                        // Select the item
                        ListView_SetItemState(window->hListView, hitTest.iItem, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);

                        // Create context menu
                        HMENU hMenu = CreatePopupMenu();
                        if (hMenu) {
                            InsertMenu(hMenu, -1, MF_BYPOSITION, ID_KILL_PROCESS, L"Kill Process");

                            // Get screen coordinates for the popup menu
                            POINT ptScreen = pt;
                            ClientToScreen(window->hListView, &ptScreen);

                            // Track the popup menu
                            TrackPopupMenu(hMenu, TPM_RIGHTBUTTON, ptScreen.x, ptScreen.y, 0, hwnd, NULL);
                            DestroyMenu(hMenu);
                        }
                    }
                }
            }
            break;
        }

        case WM_COMMAND: {
            if (LOWORD(wParam) == ID_KILL_PROCESS) {
                // Get the selected item
                int selected = ListView_GetNextItem(window->hListView, -1, LVNI_SELECTED);
                if (selected != -1) {
                    // Get the ProcessInfo pointer from the item's lParam
                    ProcessInfo* pInfo = reinterpret_cast<ProcessInfo*>(SendMessage(window->hListView, LVM_GETITEMDATA, static_cast<WPARAM>(selected), 0));
                    if (pInfo) {
                        // Confirm termination
                        wchar_t confirmMsg[256];
                        swprintf_s(confirmMsg, L"Are you sure you want to terminate process \"%s\" (ID %u)?", pInfo->name.c_str(), pInfo->id);
                        int response = MessageBox(hwnd, confirmMsg, L"Confirm Termination", MB_YESNO | MB_ICONWARNING);
                        if (response == IDYES) {
                            // Open the process with terminate rights
                            HANDLE hProcess = OpenProcess(PROCESS_TERMINATE, FALSE, pInfo->id);
                            if (hProcess) {
                                // Terminate the process
                                if (TerminateProcess(hProcess, 1)) {
                                    MessageBox(hwnd, L"Process terminated successfully.", L"Success", MB_OK | MB_ICONINFORMATION);
                                    // Refresh the process list
                                    window->refreshProcessList();
                                }
                                else {
                                    MessageBox(hwnd, L"Failed to terminate the process.", L"Error", MB_OK | MB_ICONERROR);
                                }
                                CloseHandle(hProcess);
                            }
                            else {
                                MessageBox(hwnd, L"Unable to open the process for termination.", L"Error", MB_OK | MB_ICONERROR);
                            }
                        }
                    }
                }
            }
            break;
        }

        case WM_TIMER: {
            if (wParam == window->refreshTimerId) {
                window->refreshProcessList();
            }
            break;
        }

        case WM_CLOSE:
            DestroyWindow(hwnd);
            break;

        case WM_DESTROY:
            PostQuitMessage(0);
            break;

        default:
            return DefWindowProc(hwnd, msg, wParam, lParam);
        }
    }
    else {
        return DefWindowProc(hwnd, msg, wParam, lParam);
    }

    return 0;
}

void MainWindow::initListView() {
    // Ensure that common controls are initialized
    INITCOMMONCONTROLSEX icex = { 0 }; // Zero-initialize
    icex.dwSize = sizeof(icex);
    icex.dwICC = ICC_LISTVIEW_CLASSES;
    if (!InitCommonControlsEx(&icex)) {
        MessageBox(hwnd, L"Failed to initialize common controls!", L"Error", MB_ICONERROR);
        return;
    }

    // Create the ListView with grid lines and full-row selection
    hListView = CreateWindowEx(0, WC_LISTVIEW, L"",
        WS_CHILD | WS_VISIBLE | LVS_REPORT | LVS_SINGLESEL | LVS_SHOWSELALWAYS,
        0, 0, 0, 0, // Initial size set to 0; will be resized
        hwnd, NULL, hInstance, NULL);

    if (!hListView) {
        MessageBox(hwnd, L"Failed to create ListView!", L"Error", MB_ICONERROR);
        return;
    }

    // Enable extended styles: full-row select and grid lines
    ListView_SetExtendedListViewStyleEx(hListView, LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES, LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES);

    // Set a larger font for the ListView items
    LOGFONT lf = { 0 }; // Zero-initialize
    lf.lfHeight = -14;  // Font size (14 points), negative for height in pixels
    lf.lfWeight = FW_NORMAL;  // Normal weight
    wcscpy_s(lf.lfFaceName, LF_FACESIZE, L"Segoe UI"); // Modern font
    HFONT hFont = CreateFontIndirect(&lf);
    SendMessage(hListView, WM_SETFONT, reinterpret_cast<WPARAM>(hFont), TRUE);

    // Set ListView background color to black and text color to white
    ListView_SetBkColor(hListView, RGB(0, 0, 0));        // Black background
    ListView_SetTextColor(hListView, RGB(255, 255, 255)); // White text

    // Add columns to the ListView
    LVCOLUMN lvCol = { 0 }; // Zero-initialize
    lvCol.mask = LVCF_TEXT | LVCF_WIDTH | LVCF_SUBITEM;

    // First column: Process Name
    wchar_t col1[] = L"Process Name";
    lvCol.pszText = col1;
    lvCol.cx = 400;  // Set appropriate width
    lvCol.iSubItem = 0;
    ListView_InsertColumn(hListView, 0, &lvCol);

    // Second column: Process ID
    wchar_t col2[] = L"Process ID";
    lvCol.pszText = col2;
    lvCol.cx = 150;  // Set appropriate width
    lvCol.iSubItem = 1;
    ListView_InsertColumn(hListView, 1, &lvCol);

    // Third column: Memory Usage (KB)
    wchar_t col3[] = L"Memory Usage (KB)";
    lvCol.pszText = col3;
    lvCol.cx = 200;  // Set appropriate width
    lvCol.iSubItem = 2;
    ListView_InsertColumn(hListView, 2, &lvCol);

    // Customize header background color (optional)
    HWND hHeader = ListView_GetHeader(hListView);
    if (hHeader) {
        // Enable visual styles for header to match the application theme
        SetWindowTheme(hHeader, L"", L"");
    }
}

DWORD WINAPI MainWindow::ProcessThread(LPVOID lpParameter) {
    MainWindow* window = reinterpret_cast<MainWindow*>(lpParameter);

    DWORD processIDs[1024] = { 0 }; // Zero-initialize
    DWORD cbNeeded = 0, processCount = 0;

    // Enumerate processes
    if (!EnumProcesses(processIDs, sizeof(processIDs), &cbNeeded)) {
        MessageBox(NULL, L"Failed to enumerate processes!", L"Error!", MB_ICONERROR);
        return 1;
    }

    processCount = cbNeeded / sizeof(DWORD);

    for (DWORD i = 0; i < processCount; i++) {
        if (processIDs[i] != 0) {
            HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, processIDs[i]);

            if (hProcess) {
                TCHAR processName[MAX_PATH] = _T("<unknown>");

                HMODULE hMod = NULL;
                DWORD cbNeededModule = 0;

                // Get the process name
                if (EnumProcessModules(hProcess, &hMod, sizeof(hMod), &cbNeededModule)) {
                    GetModuleBaseName(hProcess, hMod, processName, sizeof(processName) / sizeof(TCHAR));
                }

                // Get memory usage
                PROCESS_MEMORY_COUNTERS pmc;
                SIZE_T memoryUsageKB = 0;
                if (GetProcessMemoryInfo(hProcess, &pmc, sizeof(pmc))) {
                    memoryUsageKB = pmc.WorkingSetSize / 1024;
                }

                // Allocate memory for process info
                ProcessInfo* pInfo = new ProcessInfo{ processName, processIDs[i], memoryUsageKB };

                // Post the process info to the main thread
                PostMessage(window->hwnd, WM_UPDATE_LISTVIEW, 0, reinterpret_cast<LPARAM>(pInfo));

                CloseHandle(hProcess);
            }
        }
    }

    return 0;
}

// This function starts the process enumeration thread
void MainWindow::populateProcessList() {
    // Using Windows API threading (CreateThread):
    if (hProcessThread) {
        // If a previous thread is running, wait for it
        WaitForSingleObject(hProcessThread, INFINITE);
        CloseHandle(hProcessThread);
        hProcessThread = nullptr;
    }

    hProcessThread = CreateThread(
        NULL,                  // Default security attributes
        0,                     // Default stack size
        ProcessThread,         // Thread function name
        this,                  // Parameter to pass to the thread (this pointer)
        0,                     // Default creation flags
        NULL);                 // No thread identifier

    if (hProcessThread == NULL) {
        MessageBox(NULL, L"Failed to create thread!", L"Error!", MB_ICONERROR);
    }
}

void MainWindow::refreshProcessList() {
    // Clear existing items and delete associated ProcessInfo pointers
    int itemCount = ListView_GetItemCount(hListView);
    for (int i = 0; i < itemCount; ++i) {
        ProcessInfo* pInfo = reinterpret_cast<ProcessInfo*>(SendMessage(hListView, LVM_GETITEMDATA, static_cast<WPARAM>(i), 0));
        delete pInfo;
    }
    ListView_DeleteAllItems(hListView);
    // Repopulate the process list
    populateProcessList();
}
