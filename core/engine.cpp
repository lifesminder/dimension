/*
	Dimension Engine
	(c) LifesMinder

	Module name:
		engine.cpp
	Purpose:
		This file is the main functionality of the Dimension engine
*/

#include "pch.h"

Engine::Engine(HWND hWnd) {
	this->hWnd = hWnd;
	GdiplusStartupInput gdiplusstartupinput;
	GdiplusStartup(&this->gdiplustoken, &gdiplusstartupinput, NULL);
}
Engine::Engine() {
	this->hWnd = NULL;
	this->gdiplustoken = 0;
}
Engine::~Engine() {
	if (this->gdiplustoken) {
		GdiplusShutdown(this->gdiplustoken);
		this->gdiplustoken = 0;
	}
	if (this->hWnd) {
		this->hWnd = NULL;
	}
}

// Basics
int Engine::CreateFrame(Frame frame) {
	this->frames.push_back(frame);
	return (int)this->frames.size() - 1;
}
int Engine::CreateShape(Shape shape) {
	if (this->currentFrame < this->frames.size()) {
		this->frames[this->currentFrame].shapes.push_back(shape);
		return (int)this->frames[this->currentFrame].shapes.size() - 1;
	}
	return -1;
}
int Engine::CreateShape(int frame, Shape shape) {
	if (frame < (int)this->frames.size()) {
		this->frames[frame].shapes.push_back(shape);
		return (int)this->frames[frame].shapes.size() - 1;
	}
	return -1;
}
int Engine::EditFrame(int id, Frame frame) {
	if (id < (int)this->frames.size()) {
		this->frames[id] = frame;
		return id;
	}
	return -1;
}
int Engine::EditShape(int id, int shapeId, Shape shape) {
	if (id < (int)this->frames.size() && shapeId < (int)this->frames[id].shapes.size()) {
		this->frames[id].shapes[shapeId] = shape;
		return shapeId;
	}
	return -1;
}
int Engine::DeleteFrame(int id) {
	if (id < (int)this->frames.size()) {
		this->frames.erase(this->frames.begin() + id);
		return id;
	}
	return -1;
}
int Engine::DeleteShape(int id, int shapeId) {
	if (id < (int)this->frames.size() && shapeId < (int)this->frames[id].shapes.size()) {
		this->frames[id].shapes.erase(this->frames[id].shapes.begin() + shapeId);
		return shapeId;
	}
	return -1;
}

// Options management
// getter — never stores fallback
any Engine::Get(wstring option, any fallback) {
	auto it = this->options.find(option);
	if (it != this->options.end())
		return it->second;
	return fallback;
}

// setter — always overwrites
void Engine::Set(wstring option, any value) {
	this->options[option] = value;
}

wstring Engine::Option(Shape shape, wstring name) {
	wstring value = L"";
	auto option = shape.options.find(name);
	if (option != shape.options.end()) {
		value = any_cast<wstring>(option->second);
	}
	return value;
}
int Engine::OptionInt(Shape shape, wstring name, int byDefault) {
	return this->Option(shape, name) != L"" ? stoi(this->Option(shape, name)) : byDefault;
}

// Frames
void Engine::NextFrame() {
	if (this->currentFrame + 1 < this->frames.size()) {
		this->currentFrame++;
	} else if (this->GetAs<bool>(L"frameLoop", false)) {
		this->currentFrame = 0;
	}
	InvalidateRect(this->hWnd, NULL, TRUE);
	UpdateWindow(this->hWnd);
}

void Engine::PreviousFrame() {
	if (this->currentFrame > 0) {
		this->currentFrame--;
	} else if (this->GetAs<bool>(L"frameLoop", false)) {
		this->currentFrame = (unsigned int)this->frames.size() - 1;
	}
	InvalidateRect(this->hWnd, NULL, TRUE);
	UpdateWindow(this->hWnd);
}

// Drawing
void Engine::Draw() {
	PAINTSTRUCT ps;
	if (this->hWnd) {
		HDC hdc = BeginPaint(this->hWnd, &ps);

		// Scene drawing structure
		RECT clientRect;
		GetClientRect(this->hWnd, &clientRect);
		int clientWidth = clientRect.right - clientRect.left, clientHeight = clientRect.bottom - clientRect.top;
		const int canvasW = this->GetAs<int>(L"width", 800), canvasH = this->GetAs<int>(L"height", 600);
		int canvasX = (clientWidth - canvasW) / 2, canvasY = (clientHeight - canvasH) / 2;

		// Double-buffer
		HDC memDC = CreateCompatibleDC(hdc);
		HBITMAP memBitmap = CreateCompatibleBitmap(hdc, clientWidth, clientHeight);
		HGDIOBJ oldBitmap = SelectObject(memDC, memBitmap);

		// Clear with GDI first
		bool darkTheme = this->GetAs<bool>(L"darkTheme", false);
		HBRUSH clearBrush = CreateSolidBrush(darkTheme ? RGB(43, 43, 43) : RGB(240, 240, 240));
		RECT fullRect = { 0, 0, clientWidth, clientHeight };
		FillRect(memDC, &fullRect, clearBrush);
		DeleteObject(clearBrush);

		Graphics graphics(memDC);
		graphics.SetSmoothingMode(SmoothingModeAntiAlias);
		graphics.SetTextRenderingHint(TextRenderingHintAntiAlias);
		graphics.SetPixelOffsetMode(PixelOffsetModeHighQuality);

		// Scene differentiator
		SolidBrush sceneBrush(Color(255, 255, 255, 255));
		Pen scenePen(Color(255, 255, 255, 255), 1);
		graphics.FillRectangle(&sceneBrush, canvasX, canvasY, canvasW, canvasH);
		graphics.DrawRectangle(&scenePen, canvasX, canvasY, canvasW, canvasH);

		if (this->frames.size() > 0 && this->currentFrame < this->frames.size()) {
			Frame& frame = this->frames[this->currentFrame];

			for (int i = 0; i < frame.shapes.size(); i++) {
				Shape shape = frame.shapes[i];

				int x = this->OptionInt(shape, L"x") + canvasX,
					y = this->OptionInt(shape, L"y") + canvasY;
				unsigned int width = this->OptionInt(shape, L"width"),
							 height = this->OptionInt(shape, L"height");

				Color color = Color(
					this->OptionInt(shape, L"a", 255),
					this->OptionInt(shape, L"r"), this->OptionInt(shape, L"g"), this->OptionInt(shape, L"b")
				);
				SolidBrush brush(color);
				Pen pen(color, (REAL)this->OptionInt(shape, L"size"));

				switch (shape.type) {
					case SHAPE_RECTANGLE:
						graphics.DrawRectangle(&pen, x, y, width, height);
						break;
					case SHAPE_ELLIPSE:
						graphics.DrawEllipse(&pen, x, y, width, height);
						break;
					case SHAPE_LINE:
						graphics.DrawLine(&pen, x, y, this->OptionInt(shape, L"x2") + canvasX, this->OptionInt(shape, L"y2") + canvasY);
						break;
					case SHAPE_TEXT: {
						wstring fontName = this->Option(shape, L"font");
						wstring contentPre = this->Option(shape, L"content");
						int size = this->OptionInt(shape, L"fontSize", 16);
						if (fontName == L"") fontName = L"Arial";
						if (contentPre == L"") throw 3;

						FontFamily fontFamily(fontName.data());
						Font font(&fontFamily, (REAL)size, FontStyleRegular, UnitPixel);
						PointF point((REAL)x, (REAL)y);
						graphics.DrawString(contentPre.data(), -1, &font, point, &brush);
						break;
					}
					case SHAPE_PATH: {
						GraphicsPath path;
						int j = 1; float prevX = 0, prevY = 0;
						while (true) {
							wstring xi = L"x" + to_wstring(j), yi = L"y" + to_wstring(j), opt = this->Option(shape, xi);
							if (opt == L"") break;

							float x2 = (float)(this->OptionInt(shape, xi) + canvasX),
								  y2 = (float)(this->OptionInt(shape, yi) + canvasY);
							wstring cx = this->Option(shape, L"cx" + to_wstring(j)),
									cy = this->Option(shape, L"cy" + to_wstring(j));

							if (j == 1) { // first point
								prevX = x2; prevY = y2;
							} else if (cx != L"") { // bezier — needs previous control point too
								float cx1 = (float)(this->OptionInt(shape, L"cx" + to_wstring(j - 1)) + canvasX);
								float cy1 = (float)(this->OptionInt(shape, L"cy" + to_wstring(j - 1)) + canvasY);
								float cx2 = (float)(this->OptionInt(shape, L"cx" + to_wstring(j)) + canvasX);
								float cy2 = (float)(this->OptionInt(shape, L"cy" + to_wstring(j)) + canvasY);
								path.AddBezier(prevX, prevY, cx1, cy1, cx2, cy2, x2, y2);
								prevX = x2; prevY = y2;
							} else { // straight line
								path.AddLine(prevX, prevY, x2, y2);
								prevX = x2; prevY = y2;
							}
							j++;
						}
						graphics.DrawPath(&pen, &path);
						break;
					}
				}
			}
		}

		BitBlt(hdc, 0, 0, clientWidth, clientHeight, memDC, 0, 0, SRCCOPY);

		SelectObject(memDC, oldBitmap);
		DeleteObject(memBitmap);
		DeleteDC(memDC);

		EndPaint(this->hWnd, &ps);
	}
}

// Lexer
static wstring ReadBlock(wistringstream& stream) {
	wstring block;
	wchar_t ch;
	while (stream.get(ch)) if (ch == L'{') break;
	while (stream.get(ch)) { if (ch == L'}') break; block += ch; }
	return block;
}

static wstring AccumulateArgs(wistringstream& stream, wstring token) {
	if (token.find(L'(') != wstring::npos && token.find(L')') == wstring::npos) {
		wchar_t ch;
		while (stream.get(ch)) { token += ch; if (ch == L')') break; }
	}
	return token;
}

static map<wstring, wstring> ParseArgs(const wstring& token) {
	map<wstring, wstring> args;
	size_t open = token.find(L'('), close = token.rfind(L')');
	if (open == wstring::npos || close == wstring::npos) return args;
	wistringstream s(token.substr(open + 1, close - open - 1));
	wstring attr;
	while (s >> attr) {
		if (!attr.empty() && attr.back() == L',') attr.pop_back();
		size_t eq = attr.find(L'=');
		if (eq == wstring::npos) continue;
		args[Trim(attr.substr(0, eq))] = Trim(attr.substr(eq + 1));
	}
	return args;
}
void Engine::Lex(__in LPWSTR code) {
	wistringstream stream(code);
	wstring token;

	while (stream >> token) {
		token = Trim(token);
		if (token.empty()) continue;

		if (token == L"frame") {
			Frame frame;
			frame.number = (unsigned int)this->frames.size();
			wstring block = ReadBlock(stream);
			wistringstream blockStream(block);
			wstring blockToken;
			while (blockStream >> blockToken) {
				blockToken = Trim(AccumulateArgs(blockStream, blockToken));
				if (blockToken.empty()) continue;
				Shape sh = DimShape(blockToken);
				if (sh.type == SHAPE_NONE) continue;
				sh.id = (int)frame.shapes.size();
				frame.shapes.push_back(sh);
			}
			this->frames.push_back(frame);

		} else if (token == L"scene") {
			wstring block = ReadBlock(stream);
			auto args = ParseArgs(L"scene(" + block + L")");
			for (auto& kv : args) {
				try {
					this->Set(kv.first, stoi(kv.second));
				} catch (...) {
					this->Set(kv.first, kv.second);
				}
			}
		} else {
			if (this->frames.empty()) {
				Frame frame; frame.number = 0;
				this->frames.push_back(frame);
			}
			token = AccumulateArgs(stream, token);
			Shape sh = DimShape(token);
			if (sh.type == SHAPE_NONE) continue;
			sh.id = (int)this->frames[0].shapes.size();
			this->frames[0].shapes.push_back(sh);
		}
	}
}

void Engine::Parse(__in LPWSTR code) {
	try {
		this->Lex(code);
		InvalidateRect(this->hWnd, NULL, TRUE);
		UpdateWindow(this->hWnd);
		// MessageBoxW(this->hWnd, code, TEXT("DimensionEngine v0"), MB_ICONINFORMATION | MB_OK);
	}
	catch (int error) {
		WCHAR message[MAX_PATH];
		swprintf_s(message, L"Error: %d", error);
		MessageBox(this->hWnd, message, TEXT("Engine"), MB_ICONERROR | MB_OK);
	}
}

void Engine::Clean() {
	this->frames.clear();
	this->currentFrame = 0;
}

wstring Engine::Cast() {
	wostringstream codeStream;

	// Scene
	codeStream << "scene {\r\n";
	if (this->options.find(L"width") == this->options.end()) this->options[L"width"] = 800;
	if (this->options.find(L"height") == this->options.end()) this->options[L"height"] = 600;

	for (auto op = this->options.begin(); op != this->options.begin(); ++op) {
		bool last = (next(op) == this->options.end());
		wstring value = any_cast<wstring>(op->second);
		codeStream << "\t" << op->first << "=" << value << "\r\n";
	}
	codeStream << "}\r\n";

	if (this->frames.size() > 0) {
		codeStream << "\r\n";
		for (int i = 0; i < this->frames.size(); i++) {
			// Frames
			Frame frame = this->frames[i];
			codeStream << "frame {\r\n";

			for (int j = 0; j < frame.shapes.size(); j++) {
				// Frame shapes
				Shape shape = frame.shapes[j];
				if (shape.type != SHAPE_NONE) {
					codeStream << L"\t"; // prettify

					switch (shape.type) {
						case SHAPE_RECTANGLE: codeStream << L"rectangle"; break;
						case SHAPE_ELLIPSE: codeStream << L"ellipse"; break;
						case SHAPE_LINE: codeStream << L"line"; break;
						case SHAPE_TEXT: codeStream << L"text"; break;
						case SHAPE_PATH: codeStream << L"path"; break;
					}

					if (shape.options.size() > 0) {
						codeStream << L"(";
						for (auto it = shape.options.begin(); it != shape.options.end(); ++it) {
							bool last = (next(it) == shape.options.end());
							wstring value = any_cast<wstring>(it->second);

							bool isNumber = !value.empty() && all_of(value.begin(), value.end(), [](wchar_t c) {
								return iswdigit(c) || c == L'-' || c == L'.';
							});

							codeStream << it->first + L"=";
							codeStream << value; // (!isNumber) ? L'\"' + value + L'\"' : value;

							if (!last)
								codeStream << L", ";
						}
						codeStream << L")";
					}
					codeStream << L"\r\n";
				}
			}

			codeStream << L"}\r\n";
		}
	}

	return codeStream.str();
}