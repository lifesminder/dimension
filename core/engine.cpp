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

	// Initialize GDI+
	GdiplusStartupInput gdiplusstartupinput;
	GdiplusStartup(&this->gdiplustoken, &gdiplusstartupinput, NULL);

	// Initialize Direct2d
	HRESULT result = S_OK;
	result = D2D1CreateFactory(D2D1_FACTORY_TYPE_MULTI_THREADED, &this->factory);
	if (!SUCCEEDED(result)) {
		MessageBox(this->hWnd, TEXT("Cannot initialize factory"), TEXT("DimEngine"), MB_ICONERROR | MB_OK);
		exit(1);
	}

	D2D1_RENDER_TARGET_PROPERTIES properties = D2D1::RenderTargetProperties(
		D2D1_RENDER_TARGET_TYPE_DEFAULT, // To-Do: understand Software vs Hardware stuff
		D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_IGNORE),
		0.0f, 0.0f,
		D2D1_RENDER_TARGET_USAGE_GDI_COMPATIBLE,
		D2D1_FEATURE_LEVEL_DEFAULT
	);
	result = this->factory->CreateDCRenderTarget(&properties, &this->target);
	if (SUCCEEDED(result)) {
		RECT rc; GetClientRect(this->hWnd, &rc);
		result = this->target->BindDC(GetDC(hWnd), &rc);
		if (!SUCCEEDED(result)) {
			MessageBox(this->hWnd, TEXT("Cannot bind the device context"), TEXT("DimEngine"), MB_ICONERROR | MB_OK);
			exit(1);
		}
	} else {
		wchar_t msg[256];
		swprintf_s(msg, L"Cannot create render target\nReason: 0x % 08X", static_cast<unsigned int>(result));
		MessageBoxW(this->hWnd, msg, L"DimEngine", MB_ICONERROR | MB_OK);
		exit(1);
	}
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
	if (this->factory) this->factory = NULL;
	if (this->target) this->target = NULL;
	if (this->bitmapTarget) this->bitmapTarget = NULL;
	if (this->bitmap) this->bitmap = NULL;
}

// Basics
int Engine::CreateFrame(Frame frame) {
	this->project.frames.push_back(frame);
	return (int)this->project.frames.size() - 1;
}
int Engine::CreateShape(Shape shape) {
	if (this->currentFrame < this->project.frames.size()) {
		this->project.frames[this->currentFrame].shapes.push_back(shape);
		return (int)this->project.frames[this->currentFrame].shapes.size() - 1;
	}
	return -1;
}
int Engine::CreateShape(int frame, Shape shape) {
	if (frame < (int)this->project.frames.size()) {
		this->project.frames[frame].shapes.push_back(shape);
		return (int)this->project.frames[frame].shapes.size() - 1;
	}
	return -1;
}
int Engine::EditFrame(int id, Frame frame) {
	if (id < (int)this->project.frames.size()) {
		this->project.frames[id] = frame;
		return id;
	}
	return -1;
}
int Engine::EditShape(int id, int shapeId, Shape shape) {
	if (id < (int)this->project.frames.size() && shapeId < (int)this->project.frames[id].shapes.size()) {
		this->project.frames[id].shapes[shapeId] = shape;
		return shapeId;
	}
	return -1;
}
int Engine::DeleteFrame(int id) {
	if (id < (int)this->project.frames.size()) {
		this->project.frames.erase(this->project.frames.begin() + id);
		return id;
	}
	return -1;
}
int Engine::DeleteShape(int id, int shapeId) {
	if (id < (int)this->project.frames.size() && shapeId < (int)this->project.frames[id].shapes.size()) {
		this->project.frames[id].shapes.erase(this->project.frames[id].shapes.begin() + shapeId);
		return shapeId;
	}
	return -1;
}

// Options management
// getter — never stores fallback
any Engine::Get(wstring option, any fallback) {
	auto it = this->project.options.find(option);
	if (it != this->project.options.end())
		return it->second;
	return fallback;
}

// setter — always overwrites
void Engine::Set(wstring option, any value) {
	this->project.options[option] = value;
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
void Engine::Play() {
	int fps = this->GetAs<int>(L"fps", 12);
	if (fps < 1) fps = 12;
	SetTimer(this->hWnd, 1, 1000 / fps, NULL);
	this->isPlaying = true;
}
void Engine::Pause() {
	KillTimer(this->hWnd, 1);
	this->isPlaying = false;
}
void Engine::Stop() {
	KillTimer(this->hWnd, 1);
	this->isPlaying = false;
	this->currentFrame = 0;
	InvalidateRect(this->hWnd, NULL, FALSE);
}

void Engine::NextFrame() {
	if (this->currentFrame + 1 < this->project.frames.size()) {
		this->currentFrame++;
	} else if (this->GetAs<bool>(L"frameLoop", false)) {
		this->currentFrame = 0;
	} else {
		this->Pause();
	}
	InvalidateRect(this->hWnd, NULL, TRUE);
	UpdateWindow(this->hWnd);
}

void Engine::PreviousFrame() {
	if (this->currentFrame > 0) {
		this->currentFrame--;
	} else if (this->GetAs<bool>(L"frameLoop", false)) {
		this->currentFrame = (unsigned int)this->project.frames.size() - 1;
	} else {
		this->Pause();
	}
	InvalidateRect(this->hWnd, NULL, TRUE);
	UpdateWindow(this->hWnd);
}

// Bounding box of a shape in canvas coordinates (used for the selection rectangle).
// Rectangles/ellipses use x,y,width,height; lines use both end points; paths use every
// anchor and control handle; text is estimated from its font size and content.
bool Engine::ShapeBounds(const Shape& shape, float& left, float& top, float& right, float& bottom) {
	bool any = false;
	auto include = [&](float px, float py) {
		if (!any) { left = right = px; top = bottom = py; any = true; }
		else { left = min(left, px); right = max(right, px); top = min(top, py); bottom = max(bottom, py); }
	};

	switch (shape.type) {
		case SHAPE_RECTANGLE:
		case SHAPE_ELLIPSE: {
			float x = (float)this->OptionInt(shape, L"x"), y = (float)this->OptionInt(shape, L"y");
			include(x, y);
			include(x + this->OptionInt(shape, L"width"), y + this->OptionInt(shape, L"height"));
			break;
		}
		case SHAPE_LINE: {
			include((float)this->OptionInt(shape, L"x"), (float)this->OptionInt(shape, L"y"));
			include((float)this->OptionInt(shape, L"x2"), (float)this->OptionInt(shape, L"y2"));
			break;
		}
		case SHAPE_TEXT: {
			float x = (float)this->OptionInt(shape, L"x"), y = (float)this->OptionInt(shape, L"y");
			int size = this->OptionInt(shape, L"fontSize", 16);
			include(x, y);
			include(x + this->Option(shape, L"content").size() * size * 0.6f, y + size * 1.3f);
			break;
		}
		case SHAPE_PATH: {
			for (int j = 1; ; j++) {
				wstring xi = L"x" + to_wstring(j);
				if (this->Option(shape, xi) == L"") break;
				float px = (float)this->OptionInt(shape, xi), py = (float)this->OptionInt(shape, L"y" + to_wstring(j));
				include(px, py);
				if (this->Option(shape, L"cx" + to_wstring(j)) != L"") {
					float hx = (float)this->OptionInt(shape, L"cx" + to_wstring(j)), hy = (float)this->OptionInt(shape, L"cy" + to_wstring(j));
					include(hx, hy);                          // outgoing handle
					include(2.0f * px - hx, 2.0f * py - hy);  // reflected incoming handle
				}
			}
			break;
		}
		default: break;
	}

	// account for the stroke width
	if (any) {
		float half = max(1, this->OptionInt(shape, L"size", 1)) / 2.0f;
		left -= half; top -= half; right += half; bottom += half;
	}
	return any;
}

// Drawing
void Engine::Draw(__out_opt Bitmap *target) {
	PAINTSTRUCT ps;
	bool targeted = (target != nullptr);

	if (this->hWnd) {
		HDC hdc = BeginPaint(this->hWnd, &ps);

		// Scene drawing structure
		RECT clientRect;
		GetClientRect(this->hWnd, &clientRect);
		const int canvasW = this->GetAs<int>(L"width", 800), canvasH = this->GetAs<int>(L"height", 600);
		int clientWidth = (targeted) ? canvasW : clientRect.right - clientRect.left,
			clientHeight = (targeted) ? canvasH : clientRect.bottom - clientRect.top;
		int canvasX = (targeted) ? 0 : (clientWidth - canvasW) / 2,
			canvasY = (targeted) ? 0 : (clientHeight - canvasH) / 2;

		// Double-buffer
		HDC memDC = CreateCompatibleDC(hdc);
		HBITMAP memBitmap = CreateCompatibleBitmap(hdc, clientWidth, clientHeight);
		HGDIOBJ oldBitmap = SelectObject(memDC, memBitmap);

		// Clear with GDI first
		HBRUSH clearBrush = CreateSolidBrush(this->isDarkTheme ? RGB(43, 43, 43) : RGB(240, 240, 240));
		RECT fullRect = { 0, 0, clientWidth, clientHeight };
		FillRect(memDC, &fullRect, clearBrush);
		DeleteObject(clearBrush);

		Graphics* graphics = (target != nullptr) ? new Graphics(target) : new Graphics(memDC);
		graphics->SetSmoothingMode(SmoothingModeAntiAlias);
		graphics->SetTextRenderingHint(TextRenderingHintAntiAlias);
		graphics->SetPixelOffsetMode(PixelOffsetModeHighQuality);

		// Scene differentiator
		SolidBrush sceneBrush(Color(255, 255, 255, 255));
		Pen scenePen(Color(255, 255, 255, 255), 1);
		graphics->FillRectangle(&sceneBrush, canvasX, canvasY, canvasW, canvasH);
		graphics->DrawRectangle(&scenePen, canvasX, canvasY, canvasW, canvasH);

		if (this->project.frames.size() > 0 && this->currentFrame < this->project.frames.size()) {
			Frame& frame = this->project.frames[this->currentFrame];

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
				Pen selectPen(Color(0, 0, 255), 1.0f);

				if (shape.selected) {
					float l, t, r, b;
					if (this->ShapeBounds(shape, l, t, r, b)) {
						graphics->DrawRectangle(&selectPen, l - 5 + canvasX, t - 5 + canvasY, (r - l) + 10, (b - t) + 10);
					}
				}

				SolidBrush fillBrush(Color(
					this->OptionInt(shape, L"br"), this->OptionInt(shape, L"bg"), this->OptionInt(shape, L"bb")));
				bool fill = this->Option(shape, L"fill") == L"1";

				switch (shape.type) {
					case SHAPE_NONE:
						continue;
					case SHAPE_RECTANGLE:
						if (fill) graphics->FillRectangle(&fillBrush, x, y, width, height);
						graphics->DrawRectangle(&pen, x, y, width, height);
						break;
					case SHAPE_ELLIPSE:
						if (fill) graphics->FillEllipse(&fillBrush, x, y, width, height);
						graphics->DrawEllipse(&pen, x, y, width, height);
						break;
					case SHAPE_LINE:
						graphics->DrawLine(&pen, x, y, this->OptionInt(shape, L"x2") + canvasX, this->OptionInt(shape, L"y2") + canvasY);
						break;
					case SHAPE_TEXT: {
						wstring fontName = this->Option(shape, L"font"), contentPre = this->Option(shape, L"content");
						int size = this->OptionInt(shape, L"fontSize", 16);
						if (fontName == L"") fontName = L"Arial";
						if (contentPre == L"") throw 3;

						FontFamily fontFamily(fontName.data());
						Font font(&fontFamily, (REAL)size, FontStyleRegular, UnitPixel);
						PointF point((REAL)x, (REAL)y);
						graphics->DrawString(contentPre.data(), -1, &font, point, &brush);
						break;
					}
					case SHAPE_PATH: {
						GraphicsPath path;
						int j = 1; float prevX = 0, prevY = 0;
						while (true) {
							wstring xi = L"x" + to_wstring(j), yi = L"y" + to_wstring(j), opt = this->Option(shape, xi);
							if (opt == L"") break;

							float x2 = (float)(this->OptionInt(shape, xi) + canvasX), y2 = (float)(this->OptionInt(shape, yi) + canvasY);
							wstring cx = this->Option(shape, L"cx" + to_wstring(j)), prevCx = this->Option(shape, L"cx" + to_wstring(j - 1));

							if (j == 1) { // first point
								prevX = x2; prevY = y2;
							} else if (cx != L"") { // bezier — needs previous control point too
								float cx1 = (prevCx != L"")
									? (float)(this->OptionInt(shape, L"cx" + to_wstring(j - 1)) + canvasX)
									: prevX;
								float cy1 = (prevCx != L"")
									? (float)(this->OptionInt(shape, L"cy" + to_wstring(j - 1)) + canvasY)
									: prevY;
								float hx = (float)(this->OptionInt(shape, L"cx" + to_wstring(j)) + canvasX);
								float hy = (float)(this->OptionInt(shape, L"cy" + to_wstring(j)) + canvasY);
								float cx2 = 2 * x2 - hx, cy2 = 2 * y2 - hy;
								path.AddBezier(prevX, prevY, cx1, cy1, cx2, cy2, x2, y2);
								prevX = x2; prevY = y2;
							} else { // straight line
								path.AddLine(prevX, prevY, x2, y2);
								prevX = x2; prevY = y2;
							}
							j++;
						}
						if (fill) graphics->FillPath(&fillBrush, &path);
						graphics->DrawPath(&pen, &path);
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

bool Engine::HitTest(Shape shape, int px, int py) {
	int tol = max(this->OptionInt(shape, L"size", 1) / 2 + 4, 4);

	auto distToSeg = [](float px, float py, float ax, float ay, float bx, float by) -> float {
		float dx = bx - ax, dy = by - ay;
		float len2 = dx * dx + dy * dy;
		float t = (len2 > 0) ? ((px - ax) * dx + (py - ay) * dy) / len2 : 0.0f;
		t = max(0.0f, min(1.0f, t));
		float cx = ax + t * dx, cy = ay + t * dy;
		float ex = px - cx, ey = py - cy;
		return sqrtf(ex * ex + ey * ey);
	};

	switch (shape.type) {
		case SHAPE_NONE: {
			break;
		}
		case SHAPE_RECTANGLE: {
			int x = this->OptionInt(shape, L"x"), y = this->OptionInt(shape, L"y");
			int w = this->OptionInt(shape, L"width"), h = this->OptionInt(shape, L"height");
			return px >= x && px <= x + w && py >= y && py <= y + h;
		}
		case SHAPE_ELLIPSE: {
			int x = this->OptionInt(shape, L"x"), y = this->OptionInt(shape, L"y");
			int w = this->OptionInt(shape, L"width"), h = this->OptionInt(shape, L"height");
			if (w <= 0 || h <= 0) return false;
			float rx = w / 2.0f, ry = h / 2.0f;
			float cx = x + rx, cy = y + ry;
			float nx = (px - cx) / rx, ny = (py - cy) / ry;
			return nx * nx + ny * ny <= 1.0f;
		}
		case SHAPE_LINE: {
			float ax = (float)this->OptionInt(shape, L"x"), ay = (float)this->OptionInt(shape, L"y");
			float bx = (float)this->OptionInt(shape, L"x2"), by = (float)this->OptionInt(shape, L"y2");
			return distToSeg((float)px, (float)py, ax, ay, bx, by) <= tol;
		}
		case SHAPE_TEXT: {
			int x = this->OptionInt(shape, L"x"), y = this->OptionInt(shape, L"y");
			int size = this->OptionInt(shape, L"fontSize", 16);
			wstring content = this->Option(shape, L"content");
			int w = (int)content.size() * size / 2 + size;  // rough estimate
			int h = size;
			return px >= x && px <= x + w && py >= y - h && py <= y;
		}
		case SHAPE_PATH: {
			// Flatten the path (straight segments and Beziers, same rules as the renderers)
			vector<pair<float, float>> pts;
			float prevX = 0, prevY = 0;
			for (int j = 1; ; j++) {
				wstring xi = L"x" + to_wstring(j);
				if (this->Option(shape, xi) == L"") break;
				float x2 = (float)this->OptionInt(shape, xi);
				float y2 = (float)this->OptionInt(shape, L"y" + to_wstring(j));
				bool hasHandle = this->Option(shape, L"cx" + to_wstring(j)) != L"";

				if (j == 1) {
					pts.push_back({ x2, y2 });
				} else if (hasHandle) {
					bool prevHandle = this->Option(shape, L"cx" + to_wstring(j - 1)) != L"";
					float c1x = prevHandle ? (float)this->OptionInt(shape, L"cx" + to_wstring(j - 1)) : prevX;
					float c1y = prevHandle ? (float)this->OptionInt(shape, L"cy" + to_wstring(j - 1)) : prevY;
					float hx = (float)this->OptionInt(shape, L"cx" + to_wstring(j));
					float hy = (float)this->OptionInt(shape, L"cy" + to_wstring(j));
					float c2x = 2 * x2 - hx, c2y = 2 * y2 - hy;
					const int steps = 24;
					for (int s = 1; s <= steps; s++) {
						float t = (float)s / steps, u = 1 - t;
						pts.push_back({
							u * u * u * prevX + 3 * u * u * t * c1x + 3 * u * t * t * c2x + t * t * t * x2,
							u * u * u * prevY + 3 * u * u * t * c1y + 3 * u * t * t * c2y + t * t * t * y2
						});
					}
				} else {
					pts.push_back({ x2, y2 });
				}
				prevX = x2; prevY = y2;
			}
			if (pts.empty()) return false;

			// 1. Clicking the stroke always selects, open or closed
			if (pts.size() == 1)
				return hypotf(px - pts[0].first, py - pts[0].second) <= tol;
			for (size_t i = 1; i < pts.size(); i++) {
				if (distToSeg((float)px, (float)py, pts[i - 1].first, pts[i - 1].second, pts[i].first, pts[i].second) <= tol)
					return true;
			}

			// 2. The inside only counts when the path is filled. An open path is filled as if
			//    it were closed by a straight line from its last point back to the first.
			if (this->Option(shape, L"fill") == L"1" && pts.size() >= 3) {
				bool inside = false;
				for (size_t i = 0, k = pts.size() - 1; i < pts.size(); k = i++) {
					float xi = pts[i].first, yi = pts[i].second, xk = pts[k].first, yk = pts[k].second;
					if (((yi > py) != (yk > py)) && (px < (xk - xi) * (py - yi) / (yk - yi) + xi))
						inside = !inside;
				}
				return inside;
			}
			return false;
		}
	}
	return false;
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

		if (token == L"shape") {
			wstring block = ReadBlock(stream), line;
			wistringstream blockStream(block);

			while (getline(blockStream, line, L',')) {
				line = Trim(line);
				if (line.empty()) continue;
				size_t eq = line.find(L'=');
				if (eq == wstring::npos) continue;
				wstring key = Trim(line.substr(0, eq)), value = Trim(line.substr(eq + 1));
				try { this->Set(key, stoi(value)); }
				catch (...) {
					if (value == L"true") this->Set(key, true);
					else if (value == L"false") this->Set(key, false);
					else this->Set(key, value);
				}
			}
		} else if (token == L"frame") {
			Frame frame;
			frame.number = (unsigned int)this->project.frames.size();
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
			this->project.frames.push_back(frame);

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
			if (this->project.frames.empty()) {
				Frame frame; frame.number = 0;
				this->project.frames.push_back(frame);
			}
			token = AccumulateArgs(stream, token);
			Shape sh = DimShape(token);
			if (sh.type == SHAPE_NONE) continue;
			sh.id = (int)this->project.frames[0].shapes.size();
			this->project.frames[0].shapes.push_back(sh);
		}
	}
}

void Engine::Parse(__in LPWSTR code) {
	try {
		this->Lex(code);
		InvalidateRect(this->hWnd, NULL, TRUE);
		UpdateWindow(this->hWnd);
	} catch (int error) {
		WCHAR message[MAX_PATH];
		swprintf_s(message, L"Error: %d", error);
		MessageBox(this->hWnd, message, TEXT("Engine"), MB_ICONERROR | MB_OK);
	}
}

void Engine::Clean() {
	this->project.frames.clear();
	this->currentFrame = 0;
}

wstring Engine::Cast() {
	wostringstream codeStream;

	// Nothing to cast for an empty project; also don't inject defaults into it,
	// which would silently make it "initialized"
	if (this->project.options.empty() && this->project.frames.empty())
		return L"";

	// Scene
	if (this->project.options.find(L"width") == this->project.options.end()) this->project.options[L"width"] = 800;
	if (this->project.options.find(L"height") == this->project.options.end()) this->project.options[L"height"] = 600;
	if (this->project.options.find(L"fps") == this->project.options.end()) this->project.options[L"fps"] = 12;

	codeStream << "scene {\r\n";
	for (auto op = this->project.options.begin(); op != this->project.options.end(); ++op) {
		// To-Do: program-specific options should be masked under different way
		if (op->first == L"darkTheme" || op->first == L"frameLoop") continue;

		auto next_op = next(op);
		while (next_op != this->project.options.end() &&
			(next_op->first == L"darkTheme" || next_op->first == L"frameLoop"))
			++next_op;
		bool last = (next_op == this->project.options.end());

		codeStream << "\t" << op->first << "=";
		try {
			codeStream << any_cast<int>(op->second);
		} catch (...) {
			try {
				codeStream << any_cast<wstring>(op->second);
			}
			catch (...) {}
		}
		if (!last) codeStream << L",";
		codeStream << "\r\n";
	}
	codeStream << "}\r\n";

	if (this->project.frames.size() > 0) {
		codeStream << "\r\n";
		for (int i = 0; i < this->project.frames.size(); i++) {
			// Frames
			Frame frame = this->project.frames[i];
			codeStream << "frame {\r\n";

			for (int j = 0; j < frame.shapes.size(); j++) {
				// Frame shapes
				Shape shape = frame.shapes[j];
				if (shape.type != SHAPE_NONE) {
					codeStream << L"\t"; // prettify

					switch (shape.type) {
						case SHAPE_NONE: continue;
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
							if (isNumber) codeStream << value;
							else codeStream << L'"' << value << L'"';

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