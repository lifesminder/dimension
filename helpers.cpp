/*
	Dimension
	(c) LifesMinder

	Module name:
		helpers.cpp
	Description:
		This file defines helper functions used by Dimension client.
*/

#include "dimension.h"

extern HWND hwnd;
extern Engine engine;
extern Cursor cursor;
extern map<int, wchar_t> flags;
extern map<wstring, any> options;
extern vector<LoggedAction> loggedActions;
extern unsigned int currentAction;
extern wstring currentCode;

bool DimCoreIsDark() {
    HKEY hKey;
    long lRes = RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize", 0, KEY_READ, &hKey);
    bool bDarkMode = false;

    // If options contain such flag, return it's value
    auto darkmodeOption = options.find(L"darkmode");
    if (darkmodeOption != options.end()) {
        wstring text = any_cast<wstring>(darkmodeOption->second);
        return (text == L"true");
    }

    // if (!IsWindowsVersionOrGreater(10, 0, 18362)) { // normal dark mode was arrived to Windows since Windows 10 October 2018 update
    if (lRes == ERROR_SUCCESS) {
        DWORD dwValue, dwSize = sizeof(dwValue);
        lRes = RegQueryValueExW(hKey, L"AppsUseLightTheme", NULL, NULL, (LPBYTE)&dwValue, &dwSize);
        if (lRes == ERROR_SUCCESS) {
            bDarkMode = (dwValue == 0);
        }
        RegCloseKey(hKey);
    }
    return bDarkMode;
}

void MakeMenuOwnerDraw(HMENU hMenu) {
    int count = GetMenuItemCount(hMenu);
    for (int i = 0; i < count; i++) {
        MENUITEMINFOW mii{ sizeof(mii) };
        mii.fMask = MIIM_FTYPE | MIIM_SUBMENU;

        if (!GetMenuItemInfoW(hMenu, i, TRUE, &mii))
            continue;

        // Mark as owner-draw
        mii.fType |= MFT_OWNERDRAW;
        SetMenuItemInfoW(hMenu, i, TRUE, &mii);

        // Recurse into submenus
        if (mii.hSubMenu)
            MakeMenuOwnerDraw(mii.hSubMenu);
    }
}
static bool GetMenuBarRect(HWND hwnd, RECT& rc) {
    MENUBARINFO mbi{ sizeof(mbi) };
    if (!GetMenuBarInfo(hwnd, OBJID_MENU, 0, &mbi))
        return false;

    rc = mbi.rcBar;

    POINT p{ rc.left, rc.top };
    ScreenToClient(hwnd, &p);
    rc.left = p.x;
    rc.top = p.y;

    p = { rc.right, rc.bottom };
    ScreenToClient(hwnd, &p);
    rc.right = p.x;
    rc.bottom = p.y;

    return true;
}

int UIRefresh(HWND hWnd, LPARAM lParam, WPARAM wParam, int code) {
    if (DimCoreIsDark()) {
        if (code == 0) {
            BOOL darkTheme = TRUE;

            HRESULT result = DwmSetWindowAttribute(hWnd, 20, &darkTheme, sizeof(darkTheme));
            if (!SUCCEEDED(result)) {
                DwmSetWindowAttribute(hWnd, 19, &darkTheme, sizeof(darkTheme));
            }
            SetWindowPos(hWnd, NULL, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED);
            RedrawWindow(hWnd, NULL, NULL, RDW_INVALIDATE | RDW_ERASE | RDW_FRAME | RDW_ALLCHILDREN);

            HMODULE hUxTheme = LoadLibraryW(L"uxtheme.dll");
            if (!hUxTheme) return false;
            auto SetPreferredAppMode = (SetPreferredAppMode_t)GetProcAddress(hUxTheme, MAKEINTRESOURCEA(135));
            auto AllowDarkModeForWindow = (AllowDarkModeForWindow_t)GetProcAddress(hUxTheme, MAKEINTRESOURCEA(133));
            auto FlushMenuThemes = (FlushMenuThemes_t)GetProcAddress(hUxTheme, MAKEINTRESOURCEA(136));

            if (SetPreferredAppMode) SetPreferredAppMode(DimCoreIsDark() ? ForceDark : ForceLight);
            if (AllowDarkModeForWindow) AllowDarkModeForWindow(hWnd, TRUE);
            if (FlushMenuThemes) FlushMenuThemes();

            auto fn = (AllowDarkModeForWindow_t)GetProcAddress(hUxTheme, MAKEINTRESOURCEA(133));
            EnumChildWindows(hWnd, [](HWND child, LPARAM lParam) -> BOOL {
                auto AllowDarkModeForWindow = (AllowDarkModeForWindow_t)lParam;
                WCHAR className[256] = {};
                GetClassNameW(child, className, 256);
                AllowDarkModeForWindow(child, TRUE);
                if (wcscmp(className, L"ComboBox") == 0) SetWindowTheme(child, L"DarkMode_CFD", NULL);
                else SetWindowTheme(child, L"DarkMode_Explorer", NULL);
                return TRUE;
            }, (LPARAM)fn);
            FreeLibrary(hUxTheme);
        } else {
            static HBRUSH bg = CreateSolidBrush(RGB(32, 32, 32));
            switch (code) {
                case WM_CTLCOLORDLG:
                case WM_CTLCOLORSTATIC: 
                case WM_CTLCOLORBTN: {
                    HDC hdc = (HDC)wParam;
                    SetTextColor(hdc, RGB(240, 240, 240));
                    SetBkMode(hdc, TRANSPARENT);
                    return (INT_PTR)bg;
                }
            }
        }
    }
    return true;
}

void DrawDarkMenuBar(HWND hwnd, HDC hdc) {
    RECT rc;
    if (!GetMenuBarRect(hwnd, rc))
        return;

    HBRUSH bg = CreateSolidBrush(RGB(32, 32, 32));
    FillRect(hdc, &rc, bg);
    DeleteObject(bg);
}

COLORREF ShowColorDialog(HWND hwnd) { // backported exactly as on Astgh :3
    CHOOSECOLOR cc; static COLORREF crCustClr[16];
    ZeroMemory(&cc, sizeof(cc));
    cc.lStructSize = sizeof(cc);
    cc.hwndOwner = hwnd;
    cc.lpCustColors = (LPDWORD)crCustClr;
    cc.rgbResult = cursor.color; // <- this is the only change
    cc.Flags = CC_FULLOPEN | CC_RGBINIT;
    ChooseColor(&cc);
    return cc.rgbResult;
}

void UpdateMenu() {
    HMENU hMenu = GetMenu(hwnd);
    if (!hMenu)
        return;

    bool hasShapes = false, hasSelectedShapes = false;
    if (!engine.frames.empty() && engine.currentFrame >= 0 && engine.currentFrame < (int)engine.frames.size()) {
        auto& shapes = engine.frames[engine.currentFrame].shapes;
        hasShapes = !shapes.empty();
        for (const auto& shape : shapes) {
            if (shape.selected) {
                hasSelectedShapes = true;
                break;
            }
        }
    }

    EnableMenuItem(hMenu, ID_ACTION_UNDO, MF_BYCOMMAND | (currentAction > 0 ? MF_ENABLED : MF_GRAYED));
    EnableMenuItem(hMenu, ID_ACTION_REDO, MF_BYCOMMAND | (currentAction < (unsigned int)loggedActions.size() ? MF_ENABLED : MF_GRAYED));
    EnableMenuItem(hMenu, ID_SHAPE_SELECT_ALL, MF_BYCOMMAND | (hasShapes ? MF_ENABLED : MF_GRAYED));
    EnableMenuItem(hMenu, ID_SHAPE_DELETE, MF_BYCOMMAND | (hasSelectedShapes ? MF_ENABLED : MF_GRAYED));
    DrawMenuBar(hwnd);
}

void ActionUpdate(int type, LoggedAction action = {}) {
    switch (type) {
        case 0:
        case 1: { // undo & redo
            if (loggedActions.empty()) break;

            if (type == 0) {
                if (currentAction == 0) break;
                currentAction--;
            } else {
                if (currentAction >= (unsigned int)loggedActions.size()) break;
            }

            LoggedAction& a = loggedActions[currentAction];
            switch (a.type) {
                case ID_TIMELINE_NEW:
                case ID_TIMELINE_DUPLICATE: {
                    if (type == 0) {
                        engine.DeleteFrame((int)engine.frames.size() - 1);
                        engine.currentFrame = max(0, (int)engine.frames.size() - 1);
                    } else {
                        Frame fr; fr.shapes = a.shapes;
                        engine.CreateFrame(fr);
                        engine.currentFrame = (int)engine.frames.size() - 1;
                    }
                    break;
                }
                case ID_TIMELINE_EXTEND: {
                    if (a.frame < (unsigned int)engine.frames.size()) {
                        auto& len = engine.frames[a.frame].length;
                        if (type == 0) {
                            if (len > 1)
                                len--;
                        } else {
                            len++;
                        }
                        engine.currentFrame = a.frame;
                    }
                    break;
                }
                case ID_TIMELINE_DELETE: {
                    if (type == 0) {
                        Frame fr; fr.shapes = a.shapes;
                        size_t pos = min((size_t)a.frame, engine.frames.size());
                        engine.frames.insert(engine.frames.begin() + pos, fr);
                        engine.currentFrame = (int)pos;
                    } else {
                        engine.DeleteFrame(a.frame);
                        if (engine.frames.empty()) engine.CreateFrame(Frame());
                        engine.currentFrame = min((int)a.frame, (int)engine.frames.size() - 1);
                    }
                    break;
                }
                case ID_SHAPE_DELETE: {
                    if (a.frame < (unsigned int)engine.frames.size()) {
                        if (type == 0) {
                            for (auto& s : a.shapes)
                                engine.frames[a.frame].shapes.push_back(s);
                        } else {
                            auto& shapes = engine.frames[a.frame].shapes;
                            for (int i = (int)shapes.size() - 1; i >= 0; i--) {
                                for (auto& s : a.shapes) {
                                    if (shapes[i].id == s.id) {
                                        shapes.erase(shapes.begin() + i);
                                        break;
                                    }
                                }
                            }
                        }
                    }
                    break;
                }
                case 5000: {
                    if (a.frame < (unsigned int)engine.frames.size()) {
                        if (type == 0) {
                            auto& shapes = engine.frames[a.frame].shapes;
                            if (!shapes.empty()) shapes.pop_back();
                        } else {
                            for (auto& s : a.shapes)
                                engine.frames[a.frame].shapes.push_back(s);
                        }
                    }
                    break;
                }
            }

            if (type == 1) currentAction++; // advance after redo
            if (engine.currentFrame >= (int)engine.frames.size())
                engine.currentFrame = max(0, (int)engine.frames.size() - 1);
            currentCode = engine.Cast();
            InvalidateRect(hwnd, NULL, FALSE);

            UpdateMenu();
            break;
        }
        case 2: { // add
            if (currentAction < (int)loggedActions.size())
                loggedActions.erase(loggedActions.begin() + currentAction, loggedActions.end());
            if (action.frame == 0 && action.type != ID_TIMELINE_NEW) // don't override explicit frame
                action.frame = engine.currentFrame;
            loggedActions.push_back(action);
            currentAction = (int)loggedActions.size();
            break;
        }
    }
}
