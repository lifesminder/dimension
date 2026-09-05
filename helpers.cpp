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
            engine.Set(L"darkTheme", darkTheme ? true : false);

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