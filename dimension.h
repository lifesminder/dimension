/*
	Dimension
	(c) LifesMinder

    Module name:
        dimension.cpp
    Purpose:
        This is the main header file of Dimension client
*/

#ifndef DIMENSION_H
#define DIMENSION_H

#include "targetver.h"
// Windows Header Files
#include <windows.h>
#include <CommCtrl.h>
#include <commdlg.h>
#include <objidl.h>
#include <gdiplus.h>
#include <versionhelpers.h>
#include <Richedit.h>
#include <shellapi.h>
//// Windows 10 and later
#include <dwmapi.h>
#include <uxtheme.h>
#include <vsstyle.h>
#pragma comment(lib, "Dwmapi.lib")
#pragma comment(lib, "uxtheme.lib")
#pragma comment(lib, "Gdiplus.lib")
// C RunTime Header Files
#include <stdio.h>
#include <stdlib.h>
#include <malloc.h>
#include <memory.h>
#include <tchar.h>
#include <strsafe.h>
// C++ RunTime Header Files
#include <iostream>
#include <vector>
#include <string>
#include <sstream>
#include <fstream>
#include <map>
#include <any>
// Dimension Header Files
#include "dimengine.h"
#include "theme.h"
#pragma once
#include "Resource.h"

// Special strings
#if defined(__GNUC__)
    #define __in
    #define __out
#endif

#ifndef MAX_LOADSTRING
	#define MAX_LOADSTRING 256
#endif

using namespace std;
using namespace Gdiplus;

// Inter-usable definitions
typedef struct _PathPoint {
    POINT point = { 0, 0 }, controlPoint = { 0, 0 };
    bool smooth = false;
} PathPoint;

typedef struct _Cursor {
    COLORREF color = RGB(0, 0, 0);
    ShapeType tool = SHAPE_NONE;
    POINT start = { 0, 0 }, now = { 0, 0 }, drag = { 0, 0 };
    vector<POINT> points; vector<PathPoint> pathPoints;

    unsigned int size = 1;
    bool drawing = false, dragging = false;
} Cursor;

typedef struct {
    unsigned int type, frame, shape; // <- those are IDs
    vector<Shape> shapes;
} LoggedAction;

// Function prototypes (promises, as Norayr describes)
void CLI();
bool DimCoreIsDark();
void MakeMenuOwnerDraw(HMENU hMenu);
void PaintMenuBar(HWND hwnd);
void UpdateMenu();
void ActionUpdate(int type, LoggedAction action);
int UIRefresh(HWND hWnd, LPARAM lParam = NULL, WPARAM wParam = NULL, int code = 0);
void DrawDarkMenuBar(HWND hwnd, HDC hdc);
COLORREF ShowColorDialog(HWND hwnd);

// Shell Manifest
#if defined _M_IX86
#pragma comment(linker,"/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='x86' publicKeyToken='6595b64144ccf1df' language='*'\"")
#elif defined _M_IA64
#pragma comment(linker,"/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='ia64' publicKeyToken='6595b64144ccf1df' language='*'\"")
#elif defined _M_X64
#pragma comment(linker,"/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='amd64' publicKeyToken='6595b64144ccf1df' language='*'\"")
#else
#pragma comment(linker,"/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")
#endif
#pragma endregion

// UXTheme specifics
enum PreferredAppMode {
    Default,
    AllowDark,
    ForceDark,
    ForceLight,
    Max
};
using SetPreferredAppMode_t = PreferredAppMode(WINAPI*)(PreferredAppMode);
typedef bool (WINAPI* AllowDarkModeForWindow_t)(HWND hwnd, BOOL allow);
typedef void (WINAPI* FlushMenuThemes_t)();

#endif