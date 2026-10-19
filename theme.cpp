/*
	Dimension
	(c) LifesMinder

	Module name:
		theme.cpp
	Description:
		Theming facility
*/

#include "theme.h"

// MinGW-w64's uxtheme.h does not declare DrawThemeTextEx / DTTOPTS
#ifndef DTT_TEXTCOLOR
#define DTT_TEXTCOLOR (1UL << 0)
typedef int (WINAPI* DTT_CALLBACK_PROC)(HDC, LPWSTR, int, LPRECT, UINT, LPARAM);
typedef struct _DTTOPTS {
    DWORD dwSize;
    DWORD dwFlags;
    COLORREF crText;
    COLORREF crBorder;
    COLORREF crShadow;
    int iTextShadowType;
    POINT ptShadowOffset;
    int iBorderSize;
    int iFontPropId;
    int iColorPropId;
    int iStateId;
    BOOL fApplyOverlay;
    int iGlowSize;
    DTT_CALLBACK_PROC pfnDrawTextCallback;
    LPARAM lParam;
} DTTOPTS, *PDTTOPTS;
extern "C" HRESULT WINAPI DrawThemeTextEx(HTHEME, HDC, int, int, LPCWSTR, int, DWORD, LPRECT, const DTTOPTS*);
#endif

static HBRUSH g_brTextColor = CreateSolidBrush(RGB(255, 255, 255));
static HBRUSH g_brItemBackground = CreateSolidBrush(RGB(43, 43, 43));
static HBRUSH g_brItemBackgroundHot = CreateSolidBrush(RGB(48, 48, 48));
static HBRUSH g_brItemBackgroundSelected = CreateSolidBrush(RGB(53, 53, 53));
static HBRUSH g_brBarBackground = CreateSolidBrush(RGB(43, 43, 43));

void UAHDrawMenuNCBottomLine(HWND hWnd)
{
    MENUBARINFO mbi = { sizeof(mbi) };
    if (!GetMenuBarInfo(hWnd, OBJID_MENU, 0, &mbi))
    {
        return;
    }
    RECT rcClient = { 0 };
    GetClientRect(hWnd, &rcClient);
    MapWindowPoints(hWnd, nullptr, (POINT*)&rcClient, 2);

    RECT rcWindow = { 0 };
    GetWindowRect(hWnd, &rcWindow);
    OffsetRect(&rcClient, -rcWindow.left, -rcWindow.top);
    // the rcBar is offset by the window rect
    RECT rcAnnoyingLine = rcClient;
    rcAnnoyingLine.bottom = rcAnnoyingLine.top;
    rcAnnoyingLine.top--;

    HDC hdc = GetWindowDC(hWnd);
    FillRect(hdc, &rcAnnoyingLine, g_brBarBackground);
    ReleaseDC(hWnd, hdc);
}
bool UAHWndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam, LRESULT* lr) {
    switch (message) {
        case WM_UAHDRAWMENU:
        {
            UAHMENU* pUDM = (UAHMENU*)lParam;
            RECT rc = { 0 };
            // get the menubar rect
            {
                MENUBARINFO mbi = { sizeof(mbi) };
                GetMenuBarInfo(hWnd, OBJID_MENU, 0, &mbi);

                RECT rcWindow;
                GetWindowRect(hWnd, &rcWindow);
                // the rcBar is offset by the window rect
                rc = mbi.rcBar;
                OffsetRect(&rc, -rcWindow.left, -rcWindow.top);
            }
            FillRect(pUDM->hdc, &rc, g_brBarBackground);
            return true;
        }
        case WM_UAHDRAWMENUITEM: {
            UAHDRAWMENUITEM* pUDMI = (UAHDRAWMENUITEM*)lParam;

            HBRUSH* pbrBackground = &g_brItemBackground;
            // get the menu item string
            wchar_t menuString[256] = { 0 };
            MENUITEMINFO mii = { sizeof(mii), MIIM_STRING };
            {
                mii.dwTypeData = menuString;
                mii.cch = (sizeof(menuString) / 2) - 1;

                GetMenuItemInfo(pUDMI->um.hmenu, pUDMI->umi.iPosition, TRUE, &mii);
            }
            // get the item state for drawing
            DWORD dwFlags = DT_CENTER | DT_SINGLELINE | DT_VCENTER;
            int iTextStateID = 0;
            int iBackgroundStateID = 0;
            {
                if ((pUDMI->dis.itemState & ODS_INACTIVE) | (pUDMI->dis.itemState & ODS_DEFAULT)) {
                    // normal display
                    iTextStateID = MPI_NORMAL;
                    iBackgroundStateID = MPI_NORMAL;
                }
                if (pUDMI->dis.itemState & ODS_HOTLIGHT) {
                    // hot tracking
                    iTextStateID = MPI_HOT;
                    iBackgroundStateID = MPI_HOT;
                    pbrBackground = &g_brItemBackgroundHot;
                }
                if (pUDMI->dis.itemState & ODS_SELECTED) {
                    // clicked -- MENU_POPUPITEM has no state for this, though MENU_BARITEM does
                    iTextStateID = MPI_HOT;
                    iBackgroundStateID = MPI_HOT;
                    pbrBackground = &g_brItemBackgroundSelected;
                }
                if ((pUDMI->dis.itemState & ODS_GRAYED) || (pUDMI->dis.itemState & ODS_DISABLED)) {
                    // disabled / grey text
                    iTextStateID = MPI_DISABLED;
                    iBackgroundStateID = MPI_DISABLED;
                }
                if (pUDMI->dis.itemState & ODS_NOACCEL) {
                    dwFlags |= DT_HIDEPREFIX;
                }
            }
            if (!g_menuTheme) {
                g_menuTheme = OpenThemeData(hWnd, L"Menu");
            }

            DTTOPTS opts = { sizeof(opts), DTT_TEXTCOLOR, iTextStateID != MPI_DISABLED ? RGB(255, 255, 255) : RGB(164, 164, 164) };
            FillRect(pUDMI->um.hdc, &pUDMI->dis.rcItem, *pbrBackground);
            DrawThemeTextEx(g_menuTheme, pUDMI->um.hdc, MENU_BARITEM, MBI_NORMAL, menuString, mii.cch, dwFlags, &pUDMI->dis.rcItem, &opts);
            return true;
        }
        case WM_THEMECHANGED: {
            if (g_menuTheme) {
                CloseThemeData(g_menuTheme);
                g_menuTheme = nullptr;
            }
            // continue processing in main wndproc
            return false;
        }
        case WM_UAHMEASUREMENUITEM: {
            UAHMEASUREMENUITEM* pMmi = (UAHMEASUREMENUITEM*)lParam;
            *lr = DefWindowProc(hWnd, message, wParam, lParam);
            return true;
        }
        case WM_UNINITMENUPOPUP: {
            RedrawWindow(hWnd, NULL, NULL, RDW_INVALIDATE | RDW_ERASE | RDW_FRAME | RDW_NOCHILDREN);
            return false;
        }
        case WM_NCPAINT:
        case WM_NCACTIVATE:
            *lr = DefWindowProc(hWnd, message, wParam, lParam);
            UAHDrawMenuNCBottomLine(hWnd);
            return true;
            break;
        default:
            return false;
    }
}