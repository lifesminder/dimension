/*
	Dimension Engine
	(c) LifesMinder
*/

#ifndef DIM_ENGINE_H
#define DIM_ENGINE_H

// Windows Header Files
#include <windows.h>
#include <objidl.h>
#include <gdiplus.h>
#pragma comment(lib, "Gdiplus.lib")
// Dimension EngineV2
#include <d2d1.h>
#include <d2d1_1.h>
#include <d2d1helper.h>
#include <dwrite.h>
#include <wincodec.h>
#pragma comment(lib, "d2d1.lib")
#pragma comment(lib, "dwrite.lib")
// C++ RunTime Header Files
#include <iostream>
#include <sstream>
#include <fstream>
#include <vector>
#include <string>
#include <algorithm>
#include <map>
#include <any>

// Special strings
#if defined(__GNUC__)
    #define __in
    #define __out
#endif

using namespace std;
using namespace Gdiplus;

#ifdef DIM_EXPORTS
#define DIM_API __declspec(dllexport)
#else
#define DIM_API __declspec(dllimport)
#endif

typedef enum {
	SHAPE_NONE,
	SHAPE_RECTANGLE,
	SHAPE_ELLIPSE,
	SHAPE_LINE,
	SHAPE_TEXT,
	SHAPE_PATH,
} ShapeType;

typedef struct _Shape {
	int id;
	ShapeType type;
	map<wstring, any> options;
	volatile bool selected = false;
} Shape;

typedef struct {
	unsigned int number, length;
	vector<Shape> shapes;
	ID2D1Bitmap* bitmap;
} Frame;

#ifdef cplusplus
extern "C" {
#endif

class Engine {
	private:
		HWND hWnd;
		ID2D1Factory* factory = nullptr;
		ID2D1DCRenderTarget* target = nullptr;
		ID2D1BitmapRenderTarget* bitmapTarget = nullptr;
		ID2D1Bitmap* bitmap = nullptr;
		ULONG_PTR gdiplustoken;
		map<wstring, any> options, parameters;

		// Primitives too
		ID2D1SolidColorBrush* brush;

		void Lex(__in LPWSTR code);
		wstring Option(Shape shape, wstring name);
		int OptionInt(Shape shape, wstring name, int byDefault = 0);

	public:
		vector<Frame> frames;
		unsigned int currentFrame = 0, frameTick = 0;
		bool isPlaying = false, isV2 = false, isDarkTheme = false;

		DIM_API Engine(HWND hWnd);
		DIM_API Engine();
		DIM_API ~Engine();

		// EngineV2
		DIM_API wstring Interpret();

		// Basics
		DIM_API int CreateFrame(Frame frame);
		DIM_API int CreateShape(Shape shape);
		DIM_API int CreateShape(int frame, Shape shape);
		DIM_API int EditFrame(int id, Frame frame);
		DIM_API int EditShape(int id, int shapeId, Shape shape);
		DIM_API int DeleteFrame(int id);
		DIM_API int DeleteShape(int id, int shapeId);
		DIM_API bool HitTest(Shape shape, int px, int py);
		// Frames
		DIM_API void Play();
		DIM_API void Pause();
		DIM_API void Stop();
		DIM_API void NextFrame();
		DIM_API void PreviousFrame();
		// Options
		DIM_API any Get(wstring option, any fallback);
		DIM_API void Set(wstring option, any value);
		template<typename T> T GetAs(wstring name, T fallback) {
			auto it = this->options.find(name);
			if (it != this->options.end()) {
				try { return any_cast<T>(it->second); }
				catch (...) {}
			}
			return fallback;
		}
		// Functionality
		DIM_API void DrawV2(HDC hdc);
		DIM_API void Draw(__out_opt Bitmap *target = nullptr);
		DIM_API void Parse(__in LPWSTR code); // To-Do: rename
		DIM_API void Clean();
		DIM_API wstring Cast();
		DIM_API void Export(wstring path, int frame = -1);
};

wstring Trim(const wstring& s);
Shape DimShape(wstring block);

#ifdef cplusplus
}
#endif

#endif
