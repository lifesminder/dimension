/*
    Dimension
    (c) LifesMinder

    Module name:
        dimension.cpp
    Purpose:
        This is the main executable file of Dimension client
*/

#include "dimension.h"

// Global Variables
Engine engine;
HINSTANCE hInst; HWND hwnd;
WCHAR szTitle[MAX_LOADSTRING], szWindowClass[MAX_LOADSTRING];
// Program-specific variables
Cursor cursor;
bool isSaved = false;
wstring currentFile = L"", currentCode = L"";
map<int, wchar_t> flags;
map<wstring, any> options;

// Forward declarations of functions included in this code module
void New();
void Open();
void Save(bool force);
void Export();
LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK Editor(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK About(HWND, UINT, WPARAM, LPARAM);

void CLI(__in LPWSTR lpCmdLine) {
    int argc;
    LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    int realArgc = argc - 1;

    if (argv != NULL && realArgc > 0) {
        // Attach the console
        FILE* f;
        if (!AttachConsole(ATTACH_PARENT_PROCESS))
            AllocConsole();
        freopen_s(&f, "CONOUT$", "w", stdout);

        // Traverse through the arguments.
        // argv[0] is executable, real args start at 1
        for (int i = 1; i < argc; i++) {
            int index = i - 1;
            wstring arg = argv[i];

            if (arg[0] == L'-') {
                if (arg.size() > 1 && arg[1] == L'-') {
                    wstring key = arg.substr(2);
                    options.emplace(key, L"");
                } else if (arg.size() > 1) {
                    flags.emplace(index, arg[1]);
                }
            } else if (arg[0] == L'/') {
                size_t colon = arg.find(L":");
                if (colon != wstring::npos) {
                    wstring key = arg.substr(1, colon - 1), value = arg.substr(colon + 1);
                    options.emplace(key, value);
                }
            }
        }

        // Work with options. To-Do: separate this exact logic
        if (options.find(L"debug") != options.end()) {
            cout << endl;
            cout << "[Dimension] Debug mode is on" << endl;
        }

        LocalFree(argv);
        fclose(f);
    }
}

int
APIENTRY
wWinMain(
    __in HINSTANCE hInstance,
    __in_opt HINSTANCE hPrevInstance,
    __in LPWSTR lpCmdLine,
    __in int nCmdShow
) {
    // These guys are useless (at least for now)
    UNREFERENCED_PARAMETER(hPrevInstance);

    // Parse command line
    CLI(lpCmdLine);

    // Initialize GDI+
    GdiplusStartupInput gdiplusStartupInput;
    ULONG_PTR gdiplusToken;
    GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, NULL);

    // Initialize global strings
    LoadStringW(hInstance, IDS_APP_TITLE, szTitle, MAX_LOADSTRING);
    LoadStringW(hInstance, IDC_DIMENSION, szWindowClass, MAX_LOADSTRING);

    // Register the class
    WNDCLASSEXW wcex;
    wcex.cbSize = sizeof(WNDCLASSEX);
    wcex.style = CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS;
    wcex.lpfnWndProc = WndProc;
    wcex.cbClsExtra = 0;
    wcex.cbWndExtra = 0;
    wcex.hInstance = hInstance;
    wcex.hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_DIMENSION));
    wcex.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wcex.hbrBackground = NULL; // (HBRUSH)(COLOR_WINDOW + 1);
    wcex.lpszMenuName = MAKEINTRESOURCEW(IDC_DIMENSION);
    wcex.lpszClassName = szWindowClass;
    wcex.hIconSm = LoadIcon(wcex.hInstance, MAKEINTRESOURCE(IDI_DIMENSION));
    if (!RegisterClassExW(&wcex))
        return FALSE;

    // Perform application initialization
    hInst = hInstance;
    hwnd = CreateWindowW(
        szWindowClass, szTitle,
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, 0, CW_USEDEFAULT, 0,
        nullptr, nullptr, hInstance, nullptr
    );
    if (hwnd) {
        engine = Engine(hwnd);
    } else {
        return FALSE;
    }

    ShowWindow(hwnd, nCmdShow);
    UpdateWindow(hwnd);
    UIRefresh(hwnd, 0, 0);

    HACCEL hAccelTable = LoadAccelerators(hInstance, MAKEINTRESOURCE(IDC_DIMENSION));
    MSG msg;

    // Main message loop
    while (GetMessage(&msg, nullptr, 0, 0)) {
        if (!TranslateAccelerator(msg.hwnd, hAccelTable, &msg)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }

    GdiplusShutdown(gdiplusToken);
    return (int)msg.wParam;
}

//
//  FUNCTION: WndProc(HWND, UINT, WPARAM, LPARAM)
//
//  PURPOSE: Processes messages for the main window.
//
//  WM_COMMAND  - process the application menu
//  WM_PAINT    - Paint the main window
//  WM_DESTROY  - post a quit message and return
//
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
    LRESULT lr = 0;
    auto FinishPath = [&]() {
        cursor.drawing = false;
        if (cursor.points.size() < 2) { cursor.points.clear(); return; }

        RECT clientRect;
        GetClientRect(hwnd, &clientRect);
        unsigned int canvasW = engine.GetAs<int>(L"width", 800), canvasH = engine.GetAs<int>(L"height", 600);
        int cx = (clientRect.right - canvasW) / 2, cy = (clientRect.bottom - canvasH) / 2;

        Shape sh = { .id = -1, .type = SHAPE_PATH };
        for (int i = 0; i < (int)cursor.points.size(); i++) {
            sh.options[L"x" + to_wstring(i + 1)] = to_wstring(cursor.points[i].x - cx);
            sh.options[L"y" + to_wstring(i + 1)] = to_wstring(cursor.points[i].y - cy);
        }
        sh.options[L"r"] = to_wstring(GetRValue(cursor.color));
        sh.options[L"g"] = to_wstring(GetGValue(cursor.color));
        sh.options[L"b"] = to_wstring(GetBValue(cursor.color));
        sh.options[L"size"] = to_wstring(cursor.size);

        if (engine.frames.empty()) engine.CreateFrame(Frame());
        engine.CreateShape(sh);
        currentCode = engine.Cast();
        cursor.points.clear();
        isSaved = false;
        InvalidateRect(hwnd, NULL, FALSE);
    };

    if (DimCoreIsDark() && UAHWndProc(hwnd, message, wParam, lParam, &lr)) {
        return lr;
    }

    switch (message) {
        case WM_ACTIVATE: {
            CheckMenuRadioItem(GetMenu(hWnd), ID_TOOLBOX_CURSOR, ID_TOOLBOX_PATH, ID_TOOLBOX_CURSOR, MF_BYCOMMAND);
            break;
        }
        case WM_SIZE: {
            SetWindowPos(hWnd, NULL, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED);
            UIRefresh(hWnd, lParam, wParam);
            break;
        }
        case WM_COMMAND: {
            int wmId = LOWORD(wParam);
            switch (wmId) {
                // File
                case IDM_NEW: New(); break;
                case IDM_OPEN: Open(); break;
                case IDM_SAVE: Save(true); break;
                case IDM_SAVEAS: Save(false); break;
                case IDM_EDIT: DialogBox(hInst, MAKEINTRESOURCE(IDD_EDITOR), hWnd, Editor); break;
                //// Export
                case ID_EXPORT_CURRENT: Export(); break;
                case IDM_EXIT: DestroyWindow(hWnd); break;
                // Edit
                //// Toolbox
                case ID_TOOLBOX_CURSOR:
                case ID_TOOLBOX_RECTANGLE:
                case ID_TOOLBOX_ELLIPSE:
                case ID_TOOLBOX_LINE:
                case ID_TOOLBOX_PATH: {
                    switch (wmId) {
                        case ID_TOOLBOX_CURSOR: cursor.tool = SHAPE_NONE; break;
                        case ID_TOOLBOX_RECTANGLE: cursor.tool = SHAPE_RECTANGLE; break;
                        case ID_TOOLBOX_ELLIPSE: cursor.tool = SHAPE_ELLIPSE; break;
                        case ID_TOOLBOX_LINE: cursor.tool = SHAPE_LINE; break;
                        case ID_TOOLBOX_PATH: cursor.tool = SHAPE_PATH; break;
                    }
                    CheckMenuRadioItem(GetMenu(hWnd), ID_TOOLBOX_CURSOR, ID_TOOLBOX_PATH, wmId, MF_BYCOMMAND);
                    break;
                }
                //// Timeline
                case ID_TIMELINE_PLAY: {
                    if (engine.isPlaying) {
                        engine.Pause();
                    } else {
                        engine.Play();
                    }
                    break;
                }
                case ID_TIMELINE_STOP: {
                    engine.Pause();
                    CheckMenuItem(GetMenu(hWnd), ID_TIMELINE_PLAY, MF_CHECKED);
                    break;
                }
                case ID_TIMELINE_NEXT: engine.NextFrame(); break;
                case ID_TIMELINE_PREVIOUS: engine.PreviousFrame(); break;
                case ID_TIMELINE_NEW: {
                    engine.CreateFrame(Frame());
                    engine.currentFrame = (int)engine.frames.size() - 1; // go to newly created frame
                    InvalidateRect(hWnd, NULL, FALSE);
                    break;
                }
                case ID_TIMELINE_DUPLICATE: {
                    if (engine.frames.empty()) break;
                    Frame copy = engine.frames[engine.currentFrame]; // duplicate current, not next
                    engine.CreateFrame(copy);
                    engine.currentFrame = (int)engine.frames.size() - 1;
                    InvalidateRect(hWnd, NULL, FALSE);
                    break;
                }
                case ID_TIMELINE_DELETE: {
                    if (engine.frames.empty()) break;
                    engine.DeleteFrame(engine.currentFrame);
                    if (engine.currentFrame >= (int)engine.frames.size())
                        engine.currentFrame = max(0, (int)engine.frames.size() - 1);
                    if (engine.frames.empty()) {
                        engine.CreateFrame(Frame());
                        engine.currentFrame = 0;
                    }
                    InvalidateRect(hWnd, NULL, FALSE);
                    break;
                }
                case ID_TIMELINE_LOOP: {
                    bool doFrameLoop = engine.GetAs<bool>(L"frameLoop", false);
                    doFrameLoop = !doFrameLoop;
                    engine.Set(L"frameLoop", doFrameLoop);
                    CheckMenuItem(GetMenu(hWnd), ID_TIMELINE_LOOP, doFrameLoop ? MF_CHECKED : MF_UNCHECKED);
                    break;
                }
                //// Brush
                case ID_BRUSH_INCREASE: cursor.size += 1; break;
                case ID_BRUSH_DECREASE: cursor.size = (cursor.size > 1) ? --cursor.size : 1; break;
                case ID_BRUSH_RESET: cursor.size = 1; break;
                case ID_BRUSH_COLOR: cursor.color = ShowColorDialog(hWnd); break;
                // Help
                case IDM_ABOUT: DialogBox(hInst, MAKEINTRESOURCE(IDD_ABOUTBOX), hWnd, About); break;
                default: return DefWindowProc(hWnd, message, wParam, lParam);
            }
            break;
        }
        case WM_CHAR: {
            if (wParam == VK_ESCAPE && cursor.drawing) {
                cursor.drawing = false;
                InvalidateRect(hWnd, NULL, FALSE);
            }
            if (wParam == VK_RETURN) {
                if (cursor.tool == SHAPE_PATH && cursor.drawing)
                    FinishPath();
            }
            return 0;
        }
        case WM_TIMER: {
            if (wParam == 1)
                engine.NextFrame();
            break;
        }
        case WM_LBUTTONDOWN: {
            cursor.start.x = cursor.now.x = LOWORD(lParam);
            cursor.start.y = cursor.now.y = HIWORD(lParam);
            if (cursor.tool == SHAPE_PATH) {
                cursor.drawing = true;
                cursor.points.push_back({ LOWORD(lParam), HIWORD(lParam) });
            } else {
                cursor.drawing = (cursor.tool != SHAPE_NONE);
            }
            InvalidateRect(hWnd, NULL, FALSE);
            return 0;
        }
        case WM_MOUSEMOVE: {
            cursor.now.x = LOWORD(lParam);
            cursor.now.y = HIWORD(lParam);
            if (cursor.drawing)
                InvalidateRect(hWnd, NULL, FALSE);
            return 0;
        }
        case WM_LBUTTONUP: {
            if (!cursor.drawing) return 0;
            if (cursor.tool == SHAPE_PATH) return 0;
            cursor.drawing = false;

            // get canvas offset from engine
            RECT clientRect;
            GetClientRect(hWnd, &clientRect);
            int clientWidth = clientRect.right, clientHeight = clientRect.bottom;
            int canvasW = engine.GetAs<int>(L"width", 800), canvasH = engine.GetAs<int>(L"height", 600);

            // build shape — subtract canvas offset so coords are canvas-relative
            Shape sh = { .id = -1, .type = cursor.tool };
            int cx = (clientWidth - canvasW) / 2, cy = (clientHeight - canvasH) / 2;

            if (cursor.tool == SHAPE_LINE) {
                // lines keep exact start/end points, no min/max normalization
                sh.options[L"x"] = to_wstring(cursor.start.x - cx);
                sh.options[L"y"] = to_wstring(cursor.start.y - cy);
                sh.options[L"x2"] = to_wstring(cursor.now.x - cx);
                sh.options[L"y2"] = to_wstring(cursor.now.y - cy);
            } else {
                int x1 = min(cursor.start.x, cursor.now.x) - cx;
                int y1 = min(cursor.start.y, cursor.now.y) - cy;
                int x2 = max(cursor.start.x, cursor.now.x) - cx;
                int y2 = max(cursor.start.y, cursor.now.y) - cy;
                sh.options[L"x"] = to_wstring(x1);
                sh.options[L"y"] = to_wstring(y1);
                sh.options[L"width"] = to_wstring(x2 - x1);
                sh.options[L"height"] = to_wstring(y2 - y1);
            }

            // Remaining parameters, fetched from our cursor
            sh.options[L"r"] = to_wstring(GetRValue(cursor.color));
            sh.options[L"g"] = to_wstring(GetGValue(cursor.color));
            sh.options[L"b"] = to_wstring(GetBValue(cursor.color));
            sh.options[L"size"] = to_wstring(cursor.size);

            // If there's no any frame, attach one like in Adobe Animate (Flash)
            if (engine.frames.empty()) {
                Frame frame;
                engine.CreateFrame(frame);
            }

            engine.CreateShape(sh);
            currentCode = engine.Cast();
            isSaved = false;
            InvalidateRect(hWnd, NULL, FALSE);
            return 0;
        }
        case WM_LBUTTONDBLCLK: {
            if (cursor.tool != SHAPE_PATH || !cursor.drawing) return 0;
            FinishPath();
            return 0;
        }
        case WM_PAINT: {
            engine.Draw();
            // rubber band overlay
            if (cursor.drawing && cursor.tool != SHAPE_NONE) {
                RECT cr;
                GetClientRect(hWnd, &cr);

                // clamp to canvas bounds
                int x1 = min(cursor.start.x, cursor.now.x), y1 = min(cursor.start.y, cursor.now.y);
                int x2 = max(cursor.start.x, cursor.now.x), y2 = max(cursor.start.y, cursor.now.y);

                HDC hdc = GetDC(hWnd);
                SetROP2(hdc, R2_NOTXORPEN);
                HPEN rbPen = CreatePen(PS_DOT, 1, RGB(0, 0, 0));
                HGDIOBJ oldBrush = SelectObject(hdc, GetStockObject(NULL_BRUSH));

                switch (cursor.tool) {
                    case SHAPE_RECTANGLE:
                        Rectangle(hdc, x1, y1, x2, y2);
                        break;
                    case SHAPE_ELLIPSE:
                        Ellipse(hdc, x1, y1, x2, y2);
                        break;
                    case SHAPE_LINE:
                        MoveToEx(hdc, cursor.start.x, cursor.start.y, NULL);
                        LineTo(hdc, cursor.now.x, cursor.now.y);
                        break;
                    case SHAPE_PATH: {
                        if (cursor.drawing && cursor.points.size() > 0) {
                            HPEN rbPen = CreatePen(PS_DOT, 1, RGB(0, 0, 0));
                            SelectObject(hdc, rbPen);

                            MoveToEx(hdc, cursor.points[0].x, cursor.points[0].y, NULL);
                            for (int i = 1; i < (int)cursor.points.size(); i++)
                                LineTo(hdc, cursor.points[i].x, cursor.points[i].y);

                            // preview line to current mouse
                            SetROP2(hdc, R2_NOTXORPEN);
                            LineTo(hdc, cursor.now.x, cursor.now.y);
                        }
                        break;
                    }
                }

                SelectObject(hdc, oldBrush);
                DeleteObject(rbPen);
                ReleaseDC(hWnd, hdc);
            }
            break;
        }
        case WM_DESTROY: {
            FreeConsole();
            PostQuitMessage(0);
            break;
        }
        default: return DefWindowProc(hWnd, message, wParam, lParam);
    }
    return 0;
}

// File basic routines
void New() {
    if (!isSaved && currentCode != L"") {
        int confirm = MessageBox(hwnd, L"Do you wish to save this file before creating new one?", szTitle, MB_YESNOCANCEL | MB_ICONINFORMATION);
        if (confirm == IDCANCEL) return;
        if (confirm == IDYES) Save(true);
    }

    currentFile = L"";
    currentCode = L"";
    engine.Clean();
    engine.Parse(currentCode.data());
    isSaved = true;
}
void Open() {
    OPENFILENAME ofn;
    WCHAR szFileName[MAX_PATH] = L"";
    ZeroMemory(&ofn, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = hwnd;
    ofn.lpstrFilter = TEXT("Text Files (*.txt)\0*.txt\0All Files (*.*)\0*.*\0");
    ofn.lpstrFile = szFileName;
    ofn.nMaxFile = MAX_PATH;
    ofn.Flags = OFN_EXPLORER | OFN_FILEMUSTEXIST | OFN_HIDEREADONLY;
    ofn.lpstrDefExt = TEXT("txt");

    if (GetOpenFileName(&ofn)) {
        currentFile = szFileName;
        wifstream stream(szFileName, ios::in);
        if (stream.is_open()) {
            wstring code = L"", line;
            while (getline(stream, line)) {
                if (!line.empty() && line.back() == L'\r')
                    line.pop_back();
                code += line + L"\r\n";
            }
            currentCode = code;
            if (!currentCode.empty()) {
                engine.Parse(currentCode.data());
                engine.Draw();
            }
        }
        stream.close();
    }
}
void Save(bool force) {
    wstring filePath = currentFile;

    if (!force || currentFile.empty()) {
        OPENFILENAME ofn;
        WCHAR szFileName[MAX_PATH] = L"";
        ZeroMemory(&ofn, sizeof(ofn));
        ofn.lStructSize = sizeof(ofn);
        ofn.hwndOwner = hwnd;
        ofn.lpstrFilter = TEXT("Text Files (*.txt)\0*.txt\0All Files (*.*)\0*.*\0");
        ofn.lpstrFile = szFileName;
        ofn.nMaxFile = MAX_PATH;
        ofn.lpstrTitle = (!force) ? TEXT("Save As") : TEXT("Save");
        ofn.Flags = OFN_EXPLORER | OFN_PATHMUSTEXIST | OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT;
        ofn.lpstrDefExt = TEXT("txt");
        if (!GetSaveFileName(&ofn))
            return;

        filePath = wstring(szFileName);
        currentFile = filePath;
    }

    wofstream stream(filePath, ios::out | ios::trunc);
    if (stream.good()) {
        stream << engine.Cast();
        isSaved = true;
    }
    stream.close();
}
void Export() {
    OPENFILENAME ofn;
    WCHAR szFileName[MAX_PATH] = L"";
    ZeroMemory(&ofn, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = hwnd;
    ofn.lpstrFilter = TEXT("JPG/JPEG File\0*.jpeg\0");
    ofn.lpstrFile = szFileName;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrTitle = TEXT("Export");
    ofn.Flags = OFN_EXPLORER | OFN_PATHMUSTEXIST | OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT;
    ofn.lpstrDefExt = TEXT("jpg");
    if (GetSaveFileName(&ofn)) {
        wstring path = wstring(szFileName);
        engine.Export(path, engine.currentFrame);
    }
}

// Message handler for the editor
INT_PTR CALLBACK Editor(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam) {
    UNREFERENCED_PARAMETER(lParam);
    switch (message) {
        case WM_CTLCOLORDLG:
        case WM_CTLCOLORSTATIC:
        case WM_CTLCOLORBTN: {
            return UIRefresh(hDlg, lParam, wParam, message);
        }
        case WM_INITDIALOG: {
            UIRefresh(hDlg, lParam, wParam);
            currentCode = engine.Cast();

            // Create a rich edit control for editor, since from dialog we can't yet
            LoadLibrary(TEXT("msftedit.dll"));
            HWND hEdit = CreateWindowEx(
                0, MSFTEDIT_CLASS,
                TEXT(""),
                ES_MULTILINE | ES_AUTOVSCROLL | ES_WANTRETURN | WS_VISIBLE | WS_CHILD | WS_TABSTOP | WS_VSCROLL,
                10, 10, 442, 233,
                hDlg, (HMENU)IDC_EDITOR, hInst, NULL
            );
            SendMessage(hEdit, EM_SETTARGETDEVICE, (WPARAM)NULL, 0);

            SetDlgItemTextW(hDlg, IDC_EDITOR, currentCode.c_str());
            return (INT_PTR)TRUE;
        }
        case WM_COMMAND: {
            if (LOWORD(wParam) == IDOK) {
                HWND hEdit = GetDlgItem(hDlg, IDC_EDITOR);
                int length = GetWindowTextLengthW(hEdit) + 1;
                vector<WCHAR> buffer(length);
                GetDlgItemTextW(hDlg, IDC_EDITOR, buffer.data(), length);
                currentCode = wstring(buffer.data());
                engine.Clean();
                engine.Parse(currentCode.data());
                currentCode = engine.Cast();
                InvalidateRect(hwnd, NULL, TRUE);
                UpdateWindow(hwnd);
                isSaved = false;
                EndDialog(hDlg, LOWORD(wParam));
            } else if (LOWORD(wParam) == IDCANCEL) {
                EndDialog(hDlg, LOWORD(wParam));
            }
            return (INT_PTR)TRUE;
        }
    }
    return (INT_PTR)FALSE;
}

// Message handler for about box.
INT_PTR CALLBACK About(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam) {
    UNREFERENCED_PARAMETER(lParam);
    switch (message) {
        case WM_CTLCOLORDLG:
        case WM_CTLCOLORSTATIC:
        case WM_CTLCOLORBTN: {
            return UIRefresh(hDlg, lParam, wParam, message);
        }
        case WM_INITDIALOG: {
            SetClassLongPtr(hDlg, GCLP_HBRBACKGROUND, (LONG_PTR)CreateSolidBrush(RGB(32, 32, 32)));
            UIRefresh(hDlg, lParam, wParam);

            char programTitle[1024];
#if defined(DIM_CODENAME)
            sprintf_s(programTitle, "%s %s %s", DIM_TITLE, DIM_CODENAME, DIM_VERSION);
#else
            sprintf_s(programTitle, "%s %s", DIM_TITLE, DIM_VERSION);
#endif
            SetWindowTextA(GetDlgItem(hDlg, IDC_DIM_TITLE), programTitle);

            return (INT_PTR)TRUE;
        }
        case WM_COMMAND: {
            if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL) {
                EndDialog(hDlg, LOWORD(wParam));
                return (INT_PTR)TRUE;
            }
            break;
        }
    }
    return (INT_PTR)FALSE;
}
