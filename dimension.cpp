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
map<int, wchar_t> flags; map<wstring, any> options;
vector<LoggedAction> loggedActions; unsigned int currentAction = (loggedActions.size() > 0) ? loggedActions.size() : 0;

// Forward declarations of functions included in this code module
void New();
void Open();
void Save(bool force);
void Export();
void ExportVideo();
LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
LRESULT CALLBACK ExportProc(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK Editor(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK About(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK ColorsProcess(HWND, UINT, WPARAM, LPARAM);

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
    UNREFERENCED_PARAMETER(lpCmdLine); // <- this is done, because it's unusable for parsing, at least... for now
    CLI();

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
        engine.isV2 = true;
        engine.isDarkTheme = DimCoreIsDark();
        UIRefresh(hwnd, 0, 0);
        UpdateMenu();
    } else {
        return FALSE;
    }

    ShowWindow(hwnd, nCmdShow);
    UpdateWindow(hwnd);

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
        if (cursor.pathPoints.size() < 2) {
            cursor.pathPoints.clear();
            cursor.points.clear();
            return;
        }

        RECT clientRect;
        GetClientRect(hwnd, &clientRect);
        int canvasW = engine.GetAs<int>(L"width", 800);
        int canvasH = engine.GetAs<int>(L"height", 600);
        int cx = (clientRect.right - canvasW) / 2;
        int cy = (clientRect.bottom - canvasH) / 2;

        Shape sh = { .id = -1, .type = SHAPE_PATH };
        for (int i = 0; i < (int)cursor.pathPoints.size(); i++) {
            sh.options[L"x" + to_wstring(i + 1)] = to_wstring(cursor.pathPoints[i].point.x - cx);
            sh.options[L"y" + to_wstring(i + 1)] = to_wstring(cursor.pathPoints[i].point.y - cy);
            if (cursor.pathPoints[i].smooth) {
                sh.options[L"cx" + to_wstring(i + 1)] = to_wstring(cursor.pathPoints[i].controlPoint.x - cx);
                sh.options[L"cy" + to_wstring(i + 1)] = to_wstring(cursor.pathPoints[i].controlPoint.y - cy);
            }
        }
        sh.options[L"r"] = to_wstring(GetRValue(cursor.foreground));
        sh.options[L"g"] = to_wstring(GetGValue(cursor.foreground));
        sh.options[L"b"] = to_wstring(GetBValue(cursor.foreground));
        sh.options[L"size"] = to_wstring(cursor.size);
        if (cursor.useBackground) {
            sh.options[L"fill"] = wstring(L"1");
            sh.options[L"br"] = to_wstring(GetRValue(cursor.background));
            sh.options[L"bg"] = to_wstring(GetGValue(cursor.background));
            sh.options[L"bb"] = to_wstring(GetBValue(cursor.background));
        }

        if (engine.frames.empty()) engine.CreateFrame(Frame());
        engine.CreateShape(sh);
        currentCode = engine.Cast();
        cursor.pathPoints.clear();
        cursor.points.clear();
        isSaved = false;

        // Log the path creation
        vector<Shape> shs; shs.push_back(sh);
        ActionUpdate(2, { .type = 5000, .frame = (unsigned int)engine.currentFrame, .shape = 0, .shapes = shs });

        InvalidateRect(hwnd, NULL, FALSE);
        UpdateMenu();
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
                case ID_EXPORT: DialogBox(hInst, MAKEINTRESOURCE(IDD_EXPORTER), hWnd, ExportProc); break;
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
                    UpdateMenu();
                    break;
                }
                //// Timeline
                case ID_TIMELINE_PLAY: (engine.isPlaying) ? engine.Pause() : engine.Play(); break;
                case ID_TIMELINE_STOP: engine.Pause(); break;
                case ID_TIMELINE_NEXT: engine.NextFrame(); break;
                case ID_TIMELINE_PREVIOUS: engine.PreviousFrame(); break;
                case ID_TIMELINE_NEW: {
                    engine.CreateFrame(Frame());
                    engine.currentFrame = (int)engine.frames.size() - 1; // go to newly created frame
                    InvalidateRect(hWnd, NULL, FALSE);
                    ActionUpdate(2, { .type = ID_TIMELINE_NEW, .frame = engine.currentFrame });
                    UpdateMenu();
                    break;
                }
                case ID_TIMELINE_EXTEND: {
                    engine.frames[engine.currentFrame].length++;
                    InvalidateRect(hWnd, NULL, FALSE);
                    ActionUpdate(2, { .type = ID_TIMELINE_EXTEND, .frame = engine.currentFrame });
                    UpdateMenu();
                    break;
                }
                case ID_TIMELINE_DUPLICATE: {
                    if (engine.frames.empty()) break;
                    Frame copy = engine.frames[engine.currentFrame]; // duplicate current, not next
                    engine.CreateFrame(copy);
                    engine.currentFrame = (int)engine.frames.size() - 1;
                    InvalidateRect(hWnd, NULL, FALSE);
                    ActionUpdate(2, { .type = ID_TIMELINE_DUPLICATE, .frame = (unsigned int)engine.currentFrame, .shape = 0, .shapes = copy.shapes });
                    UpdateMenu();
                    break;
                }
                case ID_TIMELINE_DELETE: {
                    if (engine.frames.empty()) break;
                    Frame dying = engine.frames[engine.currentFrame]; // save before delete
                    unsigned int deletedFrame = engine.currentFrame;
                    engine.DeleteFrame(engine.currentFrame);
                    if (engine.currentFrame >= (int)engine.frames.size())
                        engine.currentFrame = max(0, (int)engine.frames.size() - 1);
                    if (engine.frames.empty()) {
                        engine.CreateFrame(Frame());
                        engine.currentFrame = 0;
                    }
                    InvalidateRect(hWnd, NULL, FALSE);
                    ActionUpdate(2, { .type = ID_TIMELINE_DELETE, .frame = deletedFrame, .shape = 0, .shapes = dying.shapes });
                    UpdateMenu();
                    break;
                }
                case ID_TIMELINE_LOOP: {
                    bool doFrameLoop = engine.GetAs<bool>(L"frameLoop", false);
                    doFrameLoop = !doFrameLoop;
                    engine.Set(L"frameLoop", doFrameLoop);
                    CheckMenuItem(GetMenu(hWnd), ID_TIMELINE_LOOP, doFrameLoop ? MF_CHECKED : MF_UNCHECKED);
                    UpdateMenu();
                    break;
                }
                //// Brush
                case ID_BRUSH_INCREASE: cursor.size += 1; break;
                case ID_BRUSH_DECREASE: cursor.size = (cursor.size > 1) ? --cursor.size : 1; break;
                case ID_BRUSH_RESET: cursor.size = 1; break;
                case ID_BRUSH_COLORS: DialogBox(hInst, MAKEINTRESOURCE(IDD_COLORS), hWnd, ColorsProcess); break;
                //// Shape operations
                case ID_ACTION_UNDO: ActionUpdate(0, {}); break;
                case ID_ACTION_REDO: ActionUpdate(1, {}); break;
                case ID_SHAPE_SELECT_ALL: {
                    if (engine.frames.size() > 0) {
                        auto& shapes = engine.frames[engine.currentFrame].shapes;
                        if (shapes.size() > 0) {
                            for (int i = (int)shapes.size() - 1; i >= 0; i--) {
                                if (!shapes[i].selected) {
                                    shapes[i].selected = true;
                                }
                            }
                            InvalidateRect(hWnd, NULL, FALSE);
                            UpdateMenu();
                        }
                    }
                    break;
                }
                case ID_SHAPE_DELETE: {
                    if (engine.frames.size() > 0) {
                        auto& shapes = engine.frames[engine.currentFrame].shapes;
                        if (shapes.size() > 0) {
                            vector<Shape> shs;
                            for (int i = (int)shapes.size() - 1; i >= 0; i--) {
                                if (shapes[i].selected) {
                                    shs.push_back(shapes[i]);
                                    shapes.erase(shapes.begin() + i);
                                }
                            }
                            InvalidateRect(hWnd, NULL, FALSE);
                            ActionUpdate(2, { .type = ID_SHAPE_DELETE, .frame = (unsigned int)engine.currentFrame, .shape = 0, .shapes = shs });
                            UpdateMenu();
                        }
                    }
                    break;
                }
                // Help
                case IDM_ABOUT: DialogBox(hInst, MAKEINTRESOURCE(IDD_ABOUTBOX), hWnd, About); break;
                case ID_HELP_WIKI: ShellExecuteW(NULL, TEXT("open"), TEXT("http://git.lmd.vg/ash/dimension/wiki/Welcome"), NULL, NULL, SW_SHOW); break;
                case ID_HELP_CHANGELOG: ShellExecuteW(NULL, TEXT("open"), TEXT("http://git.lmd.vg/ash/dimension/src/branch/master/CHANGELOG.md"), NULL, NULL, SW_SHOW); break;
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
            if (wParam == 1) {
                static unsigned int ticksOnFrame = 0;
                unsigned int length = 1;
                if (!engine.frames.empty())
                    length = max(1u, engine.frames[engine.currentFrame].length);

                ticksOnFrame++;
                if (ticksOnFrame >= length) {
                    ticksOnFrame = 0;
                    engine.NextFrame();
                }
            }
            UpdateMenu();
            break;
        }
        case WM_LBUTTONDOWN: {
            cursor.dragging = false;
            cursor.drag = { LOWORD(lParam), HIWORD(lParam) };
            cursor.start.x = cursor.now.x = LOWORD(lParam);
            cursor.start.y = cursor.now.y = HIWORD(lParam);

            if (cursor.tool == SHAPE_NONE) {
                RECT rc; GetClientRect(hWnd, &rc);
                int canvasW = engine.GetAs<int>(L"width", 800), canvasH = engine.GetAs<int>(L"height", 600);
                int cx = (rc.right - canvasW) / 2, cy = (rc.bottom - canvasH) / 2;
                int px = cursor.start.x - cx, py = cursor.start.y - cy;

                if (!engine.frames.empty()) {
                    auto& shapes = engine.frames[engine.currentFrame].shapes;
                    // single-select: clear first
                    for (auto& s : shapes)
                        s.selected = false;
                    for (int i = (int)shapes.size() - 1; i >= 0; i--) {
                        // topmost first
                        if (engine.HitTest(shapes[i], px, py)) {
                            shapes[i].selected = true;
                            break;
                        }
                    }
                }
            } else if (cursor.tool == SHAPE_PATH) {
                cursor.drawing = true;
                cursor.drag = { LOWORD(lParam), HIWORD(lParam) };
            } else {
                cursor.drawing = (cursor.tool != SHAPE_NONE);
            }
            InvalidateRect(hWnd, NULL, FALSE);
            UpdateMenu();
            return 0;
        }
        case WM_MOUSEMOVE: {
            cursor.now.x = LOWORD(lParam);
            cursor.now.y = HIWORD(lParam);
            if (cursor.drawing) {
                if (cursor.tool == SHAPE_PATH && (wParam & MK_LBUTTON)) {
                    cursor.dragging = true;
                }
                InvalidateRect(hWnd, NULL, FALSE);
                UpdateMenu();
            }
            return 0;
        }
        case WM_LBUTTONUP: {
            if (!cursor.drawing) return 0;
            if (cursor.tool != SHAPE_PATH) cursor.drawing = false;

            // get canvas offset from engine
            RECT clientRect;
            GetClientRect(hWnd, &clientRect);
            int clientWidth = clientRect.right, clientHeight = clientRect.bottom;
            int canvasW = engine.GetAs<int>(L"width", 800), canvasH = engine.GetAs<int>(L"height", 600);
            int cx = (clientWidth - canvasW) / 2, cy = (clientHeight - canvasH) / 2;

            // check for path being
            if (cursor.tool == SHAPE_PATH) {
                // check being close to first point
                if (cursor.pathPoints.size() >= 2) {
                    int dx = (int)LOWORD(lParam) - cursor.pathPoints[0].point.x, dy = (int)HIWORD(lParam) - cursor.pathPoints[0].point.y;
                    if (dx * dx + dy * dy < 15 * 15) { // ???
                        FinishPath();
                        return 0;
                    }
                }

                PathPoint p = { .point = cursor.drag, .smooth = cursor.dragging };
                p.controlPoint = (cursor.dragging) ? POINT{ cursor.now.x, cursor.now.y } : cursor.drag;
                cursor.pathPoints.push_back(p);
                cursor.points.push_back(cursor.drag);
                InvalidateRect(hWnd, NULL, FALSE);
                cursor.dragging = false;
            }

            // build shape — subtract canvas offset so coords are canvas-relative
            Shape sh = { .id = -1, .type = cursor.tool };

            if (cursor.tool == SHAPE_LINE) {
                // lines keep exact start/end points, no min/max normalization
                sh.options[L"x"] = to_wstring(cursor.start.x - cx);
                sh.options[L"y"] = to_wstring(cursor.start.y - cy);
                sh.options[L"x2"] = to_wstring(cursor.now.x - cx);
                sh.options[L"y2"] = to_wstring(cursor.now.y - cy);
            } else {
                int x1 = min(cursor.start.x, cursor.now.x) - cx, y1 = min(cursor.start.y, cursor.now.y) - cy;
                int x2 = max(cursor.start.x, cursor.now.x) - cx, y2 = max(cursor.start.y, cursor.now.y) - cy;
                sh.options[L"x"] = to_wstring(x1);
                sh.options[L"y"] = to_wstring(y1);
                sh.options[L"width"] = to_wstring(x2 - x1);
                sh.options[L"height"] = to_wstring(y2 - y1);
            }

            // Remaining parameters, fetched from our cursor
            sh.options[L"r"] = to_wstring(GetRValue(cursor.foreground));
            sh.options[L"g"] = to_wstring(GetGValue(cursor.foreground));
            sh.options[L"b"] = to_wstring(GetBValue(cursor.foreground));
            sh.options[L"size"] = to_wstring(cursor.size);
            if (cursor.useBackground) {
                sh.options[L"fill"] = wstring(L"1");
                sh.options[L"br"] = to_wstring(GetRValue(cursor.background));
                sh.options[L"bg"] = to_wstring(GetGValue(cursor.background));
                sh.options[L"bb"] = to_wstring(GetBValue(cursor.background));
            }

            // If there's no any frame, attach one like in Adobe Animate (Flash)
            if (engine.frames.empty()) {
                Frame frame;
                engine.CreateFrame(frame);
            }

            engine.CreateShape(sh);
            currentCode = engine.Cast();
            isSaved = false;
            InvalidateRect(hWnd, NULL, FALSE);

            vector<Shape> shs;
            shs.push_back(sh);
            ActionUpdate(2, { .type = 5000, .frame = (unsigned int)engine.currentFrame, .shape = 0, .shapes = shs });

            UpdateMenu();
            return 0;
        }
        case WM_LBUTTONDBLCLK: {
            if (cursor.tool != SHAPE_PATH || !cursor.drawing) return 0;
            FinishPath();
            UpdateMenu();
            return 0;
        }
        case WM_PAINT: {
            if (engine.isV2) {
                PAINTSTRUCT ps;
                HDC hdc = BeginPaint(hWnd, &ps);
                engine.DrawV2(hdc);
                EndPaint(hWnd, &ps);
            } else {
                engine.Draw();
            }

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
                HGDIOBJ oldPen = SelectObject(hdc, rbPen), oldBrush = SelectObject(hdc, GetStockObject(NULL_BRUSH));

                switch (cursor.tool) {
                    case SHAPE_NONE:
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
                        if (cursor.drawing && cursor.pathPoints.size() > 0) {
                            MoveToEx(hdc, cursor.pathPoints[0].point.x, cursor.pathPoints[0].point.y, NULL);

                            for (int i = 1; i < (int)cursor.pathPoints.size(); i++) {
                                PathPoint previous = cursor.pathPoints[i - 1], current = cursor.pathPoints[i];
                                if (previous.smooth || current.smooth) {
                                    // outgoing handle of previous point
                                    POINT c1 = previous.smooth ? previous.controlPoint : previous.point;
                                    // incoming handle of current = reflection of its stored (outgoing) handle
                                    POINT c2 = current.smooth
                                        ? POINT{ 2 * current.point.x - current.controlPoint.x,
                                                 2 * current.point.y - current.controlPoint.y }
                                    : current.point;
                                    POINT points[3] = { c1, c2, current.point };
                                    MoveToEx(hdc, previous.point.x, previous.point.y, NULL);
                                    PolyBezierTo(hdc, points, sizeof(points) / sizeof(points[0]));
                                }
                                else {
                                    MoveToEx(hdc, previous.point.x, previous.point.y, NULL);
                                    LineTo(hdc, current.point.x, current.point.y);
                                }
                            }

                            // preview segment from last placed point to the point being dragged
                            PathPoint last = cursor.pathPoints.back();
                            SetROP2(hdc, R2_NOTXORPEN);
                            MoveToEx(hdc, last.point.x, last.point.y, NULL);
                            if (cursor.dragging) {
                                // outgoing handle of last point
                                POINT c1 = last.smooth ? last.controlPoint : last.point;
                                // incoming handle of the point being dragged, reflection of current cursor and across anchor (draggable cursor)
                                POINT c2 = { 2 * cursor.drag.x - cursor.now.x, 2 * cursor.drag.y - cursor.now.y };
                                POINT seg[3] = { c1, c2, cursor.drag };
                                PolyBezierTo(hdc, seg, 3);
                                MoveToEx(hdc, cursor.drag.x, cursor.drag.y, NULL);
                                LineTo(hdc, cursor.now.x, cursor.now.y);
                            } else {
                                LineTo(hdc, cursor.now.x, cursor.now.y);
                            }
                        }
                        break;
                    }
                }

                SelectObject(hdc, oldPen);
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
        int confirm = MessageBoxW(hwnd, L"Do you wish to save this file before creating new one?", szTitle, MB_YESNOCANCEL | MB_ICONINFORMATION);
        if (confirm == IDCANCEL) return;
        if (confirm == IDYES) Save(true);
    }

    currentFile = L"";
    currentCode = L"";
    engine.Clean();
    engine.Parse(currentCode.data());
    loggedActions.clear();
    currentAction = 0;
    isSaved = true;
}
void Open() {
    OPENFILENAMEW ofn;
    WCHAR szFileName[MAX_PATH] = L"";
    ZeroMemory(&ofn, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = hwnd;
    ofn.lpstrFilter = L"Text Files (*.txt)\0*.txt\0All Files (*.*)\0*.*\0";
    ofn.lpstrFile = szFileName;
    ofn.nMaxFile = MAX_PATH;
    ofn.Flags = OFN_EXPLORER | OFN_FILEMUSTEXIST | OFN_HIDEREADONLY;
    ofn.lpstrDefExt = L"txt";

    if (GetOpenFileNameW(&ofn)) {
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
        OPENFILENAMEW ofn;
        WCHAR szFileName[MAX_PATH] = L"";
        ZeroMemory(&ofn, sizeof(ofn));
        ofn.lStructSize = sizeof(ofn);
        ofn.hwndOwner = hwnd;
        ofn.lpstrFilter = L"Text Files (*.txt)\0*.txt\0All Files (*.*)\0*.*\0";
        ofn.lpstrFile = szFileName;
        ofn.nMaxFile = MAX_PATH;
        ofn.lpstrTitle = (!force) ? L"Save As" : L"Save";
        ofn.Flags = OFN_EXPLORER | OFN_PATHMUSTEXIST | OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT;
        ofn.lpstrDefExt = L"txt";
        if (!GetSaveFileNameW(&ofn))
            return;

        filePath = wstring(szFileName);
        currentFile = filePath;
    }

    wofstream stream(filePath.c_str(), ios::out | ios::trunc);
    if (stream.good()) {
        stream << engine.Cast();
        isSaved = true;
    }
    stream.close();
}
void Export() {
    OPENFILENAMEW ofn;
    WCHAR szFileName[MAX_PATH] = L"";
    ZeroMemory(&ofn, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = hwnd;
    ofn.lpstrFilter = L"JPG/JPEG File\0*.jpeg\0";
    ofn.lpstrFile = szFileName;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrTitle = L"Export";
    ofn.Flags = OFN_EXPLORER | OFN_PATHMUSTEXIST | OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT;
    ofn.lpstrDefExt = L"jpg";

    if (GetSaveFileNameW(&ofn)) {
        wstring path = wstring(szFileName);
        engine.Export(path, engine.currentFrame);
    }
}

void ExportVideo() {
    OPENFILENAMEW ofn;
    WCHAR szFileName[MAX_PATH] = L"";
    ZeroMemory(&ofn, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = hwnd;
    ofn.lpstrFilter = L"MP4 Video\0*.mp4\0";
    ofn.lpstrFile = szFileName;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrTitle = L"Export video";
    ofn.Flags = OFN_EXPLORER | OFN_PATHMUSTEXIST | OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT;
    ofn.lpstrDefExt = L"mp4";

    if (GetSaveFileNameW(&ofn)) {
        engine.ExportVideo(wstring(szFileName));
        InvalidateRect(hwnd, NULL, TRUE);
    }
}

// Message handler for the editor
INT_PTR CALLBACK Editor(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam) {
    UNREFERENCED_PARAMETER(lParam);
    switch (message) {
        case WM_CTLCOLORDLG:
        case WM_CTLCOLORSTATIC:
        case WM_CTLCOLORBTN: {
            if (DimCoreIsDark())
                return UIRefresh(hDlg, lParam, wParam, message);
            return 0;
        }
        case WM_INITDIALOG: {
            UIRefresh(hDlg, lParam, wParam);
            currentCode = engine.Cast();

            // Create a rich edit control for editor, since from dialog we can't yet
            LoadLibrary(TEXT("msftedit.dll"));
            HWND hEdit = CreateWindowEx(
                0, TEXT("RICHEDIT50W"), TEXT(""),
                ES_MULTILINE | ES_AUTOVSCROLL | ES_WANTRETURN | WS_VISIBLE | WS_CHILD | WS_TABSTOP | WS_VSCROLL,
                10, 10, 442, 233,
                hDlg, (HMENU)IDC_EDITOR, hInst, NULL
            );
            SendMessage(hEdit, EM_SETTARGETDEVICE, (WPARAM)NULL, 0);

            if (engine.isV2) {
                wstring code = engine.Interpret();
                SetDlgItemTextW(hDlg, IDC_EDITOR, code.c_str());
            } else {
                SetDlgItemTextW(hDlg, IDC_EDITOR, currentCode.c_str());
            }
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

INT_PTR CALLBACK ExportProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam) {
    UNREFERENCED_PARAMETER(lParam);

    switch (message) {
        case WM_CTLCOLORDLG:
        case WM_CTLCOLORSTATIC:
        case WM_CTLCOLORBTN: {
            if (DimCoreIsDark())
                return UIRefresh(hDlg, lParam, wParam, message);
            return 0;
        }
        case WM_INITDIALOG: {
            UIRefresh(hDlg, lParam, wParam);
            return (INT_PTR)TRUE;
        }
        case WM_COMMAND: {
            switch (LOWORD(wParam)) {
                case ID_EXPORT_CURRENT: Export(); break;
                case ID_EXPORT_VIDEO: ExportVideo(); break;
                case IDOK:
                case IDCANCEL: {
                    EndDialog(hDlg, LOWORD(wParam));
                    return (INT_PTR)TRUE;
                }
            }
            break;
        }
    }
    return (INT_PTR)FALSE;
}

INT_PTR CALLBACK ColorsProcess(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam) {
    UNREFERENCED_PARAMETER(lParam);
    static COLORREF colorForeground, colorBackground;
    static bool useBackground;

    switch (message) {
        case WM_CTLCOLORDLG:
        case WM_CTLCOLORSTATIC:
        case WM_CTLCOLORBTN: {
            if (DimCoreIsDark())
                return UIRefresh(hDlg, lParam, wParam, message);
            return 0;
        }
        case WM_INITDIALOG: {
            UIRefresh(hDlg, lParam, wParam);
            colorForeground = cursor.foreground;
            colorBackground = cursor.background;
            useBackground = cursor.useBackground;
            CheckDlgButton(hDlg, ID_EDIT_COLOR_ISBACKGROUND, useBackground ? BST_CHECKED : BST_UNCHECKED);
            EnableWindow(GetDlgItem(hDlg, ID_EDIT_COLORS_CHANGE_BACK), useBackground);
            return (INT_PTR)TRUE;
        }
        case WM_COMMAND: {
            switch (LOWORD(wParam)) {
                case ID_EDIT_COLORS_CHANGE: {
                    colorForeground = ShowColorDialog(hDlg);
                    break;
                }
                case ID_EDIT_COLORS_CHANGE_BACK: {
                    colorBackground = ShowColorDialog(hDlg);
                    break;
                }
                case ID_EDIT_COLOR_ISBACKGROUND: {
                    useBackground = IsDlgButtonChecked(hDlg, ID_EDIT_COLOR_ISBACKGROUND) == BST_CHECKED;
                    EnableWindow(GetDlgItem(hDlg, ID_EDIT_COLORS_CHANGE_BACK), useBackground);
                    break;
                }
                case IDOK: {
                    cursor.foreground = colorForeground;
                    cursor.background = colorBackground;
                    cursor.useBackground = useBackground;
                    EndDialog(hDlg, LOWORD(wParam));
                    return (INT_PTR)TRUE;
                }
                case IDCANCEL: {
                    EndDialog(hDlg, LOWORD(wParam));
                    return (INT_PTR)TRUE;
                }
            }
            break;
        }
    }
    return (INT_PTR)FALSE;
}

INT_PTR CALLBACK About(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam) {
    UNREFERENCED_PARAMETER(lParam);
    switch (message) {
        case WM_CTLCOLORDLG:
        case WM_CTLCOLORSTATIC:
        case WM_CTLCOLORBTN: {
            if (DimCoreIsDark())
                return UIRefresh(hDlg, lParam, wParam, message);
            return 0;
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
