/*
    Dimension
    (c) LifesMinder

    Module name:
        dialogs.cpp
    Purpose:
        This is the separate dialogs definition file of Dimension client
*/

#include "dimension.h"

// Externs
extern HWND hwnd;
extern HINSTANCE hInst;
extern WCHAR szTitle[MAX_LOADSTRING], szWindowClass[MAX_LOADSTRING];
extern Engine engine;
extern Cursor cursor;
extern wstring currentFile, currentCode;
extern vector<LoggedAction> loggedActions;
extern unsigned int currentAction;
extern bool isSaved;

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
            }
            else {
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
            }
            else if (LOWORD(wParam) == IDCANCEL) {
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
