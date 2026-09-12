/*
	Dimension Engine V2
	(c) LifesMinder

	Module name:
		enginev2.cpp
	Purpose:
		This file is the main functionality of the Dimension engine
*/

#include "pch.h"

wstring InterpretShape(Shape shape) {
	wostringstream code;

	// Parse shape type
	switch (shape.type) {
		case SHAPE_RECTANGLE: code << L"rectangle"; break;
		case SHAPE_ELLIPSE: code << L"ellipse"; break;
		case SHAPE_LINE: code << L"line"; break;
		case SHAPE_TEXT: code << L"text"; break;
		case SHAPE_PATH: code << L"path"; break;
		default: code << L"shape"; break;
	}

	// Append options
	if (shape.options.size() > 0) {
		code << "(";
		for (auto it = shape.options.begin(); it != shape.options.end(); ++it) {
			bool last = (next(it) == shape.options.end());
			wstring value = any_cast<wstring>(it->second);

			bool isNumber = !value.empty() && all_of(value.begin(), value.end(), [](wchar_t c) {
				return iswdigit(c) || c == L'-' || c == L'.';
				});

			code << it->first + L"=";
			if (isNumber) code << value;
			else code << L'"' << value << L'"';

			if (!last) code << L", ";
		}
		code << ")";
	}

	return code.str();
}

wstring Engine::Interpret() {
	wstring code = L"";

	// The beginnings of artwork
	code += L"shape {\n";
	for (auto op = this->options.begin(); op != this->options.end(); ++op) {
		code += L"\t" + op->first + L"=";
		try {
			code += to_wstring(any_cast<int>(op->second));
		} catch (...) {
			try {
				code += any_cast<wstring>(op->second);
			} catch (...) {
				try {
					code += any_cast<bool>(op->second) ? L"true" : L"false";
				} catch (...) {}
			}
		}
		if (next(op) != this->options.end()) {
			code += L",";
		}
		code += L"\r\n";
	}
	code += L"}\n";

	// Frames
	if (this->frames.size() > 0) {
		code += L"\nframes {\n";
		for (auto fr = this->frames.begin(); fr != this->frames.end(); ++fr) {
			code += L"\t[";
			auto& shapes = fr->shapes;
			if (shapes.size() > 0) {
				code += L"\n";
				for (auto sh = shapes.begin(); sh != shapes.end(); ++sh) {
					code += L"\t\t" + InterpretShape(*sh);
					if (next(sh) != shapes.end()) code += L",";
					code += L"\n";
				}
				code += L"\t";
			}
			code += L"]";
			if (next(fr) != this->frames.end()) {
				code += L",";
			}
			code += L"\n";
		}
		code += L"}\n";
	}

	return code;
}

void Engine::DrawV2(HDC hdc) {
	bool targeted = (this->target != nullptr);

	RECT rc; GetClientRect(hWnd, &rc);
	HRESULT hr = this->target->BindDC(hdc, &rc);
	if (FAILED(hr))
		return;

	this->target->SetAntialiasMode(D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);

	const int canvasW = this->GetAs<int>(L"width", 800), canvasH = this->GetAs<int>(L"height", 600);
	const int clientWidth = rc.right - rc.left, clientHeight = rc.bottom - rc.top;
	const int canvasX = (clientWidth - canvasW) / 2, canvasY = (clientHeight - canvasH) / 2;
	D2D1_RECT_F sceneRRect = D2D1::RectF(0, 0, static_cast<float>(canvasW), static_cast<float>(canvasH));
	D2D1_SIZE_F desiredSize = D2D1::SizeF(static_cast<float>(clientWidth), static_cast<float>(clientHeight));

	// Create stuff
	HRESULT compatibleResult = this->target->CreateCompatibleRenderTarget(&this->bitmapTarget);
	if (!SUCCEEDED(compatibleResult)) {
		MessageBox(this->hWnd, TEXT("Error while creating compatible render target"), TEXT("Dimension"), MB_ICONERROR | MB_OK);
		exit(1);
	}
	this->target->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::Black), &this->brush);

	// Initialize the scene
	bitmapTarget->BeginDraw();
	bitmapTarget->SetTransform(D2D1::Matrix3x2F::Translation(static_cast<float>(canvasX), static_cast<float>(canvasY)));
	bitmapTarget->Clear(this->isDarkTheme ? D2D1::ColorF(43.0f / 255.0f, 43.0f / 255.0f, 43.0f / 255.0f) : D2D1::ColorF::WhiteSmoke);

	// Draw the scene
	ID2D1SolidColorBrush* sceneBrush = nullptr;
	bitmapTarget->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::White), &sceneBrush);
	bitmapTarget->FillRectangle(&sceneRRect, sceneBrush);
	sceneBrush->Release();

	// And then the objects
	if (this->frames.size() > 0 && this->currentFrame < this->frames.size()) {
		Frame& frame = this->frames[this->currentFrame];

		for (int i = 0; i < frame.shapes.size(); i++) {
			Shape shape = frame.shapes[i];

			int x = this->OptionInt(shape, L"x"), y = this->OptionInt(shape, L"y");
			unsigned int width = this->OptionInt(shape, L"width"), height = this->OptionInt(shape, L"height");

			this->brush->SetColor(D2D1::ColorF(
				this->OptionInt(shape, L"r", 255) / 255.0f,
				this->OptionInt(shape, L"g", 255) / 255.0f,
				this->OptionInt(shape, L"b", 255) / 255.0f,
				this->OptionInt(shape, L"a", 255) / 255.0f
			));
			ID2D1SolidColorBrush* brush = this->brush;

			switch (shape.type) {
				case SHAPE_RECTANGLE: {
					bitmapTarget->DrawRectangle(D2D1::RectF((float)x, (float)y, (float)(x + width), (float)(y + height)), brush, (float)this->OptionInt(shape, L"size"));
					break;
				}
				case SHAPE_ELLIPSE: {
					D2D1_ELLIPSE ellipse = D2D1::Ellipse(
						D2D1::Point2F(x + width / 2.0f, y + height / 2.0f),
						width / 2.0f,
						height / 2.0f
					);
					bitmapTarget->DrawEllipse(&ellipse, brush, (float)this->OptionInt(shape, L"size"));
					break;
				}
				case SHAPE_LINE: {
					bitmapTarget->DrawLine(
						D2D1::Point2F((float)x, (float)y),
						D2D1::Point2F((float)this->OptionInt(shape, L"x2"), (float)this->OptionInt(shape, L"y2")),
						brush,
						(float)this->OptionInt(shape, L"size")
					);
					break;
				}
				case SHAPE_PATH: {
					ID2D1PathGeometry* path = nullptr;
					ID2D1GeometrySink* sink = nullptr;
					HRESULT hr = factory->CreatePathGeometry(&path);
					if (FAILED(hr))
						break;

					hr = path->Open(&sink);
					if (FAILED(hr)) {
						path->Release();
						break;
					}

					int j = 1;
					float prevX = 0.0f, prevY = 0.0f;
					bool started = false;
					while (true) {
						wstring xi = L"x" + to_wstring(j), yi = L"y" + to_wstring(j), opt = this->Option(shape, xi);
						if (opt == L"")
							break;

						float x2 = static_cast<float>(this->OptionInt(shape, xi)), y2 = static_cast<float>(this->OptionInt(shape, yi));
						wstring cx = this->Option(shape, L"cx" + to_wstring(j)), prevCx = this->Option(shape, L"cx" + to_wstring(j - 1));
						if (!started) {
							// First point
							sink->BeginFigure(D2D1::Point2F(x2, y2), D2D1_FIGURE_BEGIN_HOLLOW);
							prevX = x2; prevY = y2;
							started = true;
						} else if (cx != L"") {
							// Bezier
							float cx1, cy1;
							if (prevCx != L"") {
								cx1 = static_cast<float>(this->OptionInt(shape, L"cx" + to_wstring(j - 1)));
								cy1 = static_cast<float>(this->OptionInt(shape, L"cy" + to_wstring(j - 1)));
							} else {
								cx1 = prevX;
								cy1 = prevY;
							}

							float hx = static_cast<float>(this->OptionInt(shape, L"cx" + to_wstring(j))),
								  hy = static_cast<float>(this->OptionInt(shape, L"cy" + to_wstring(j)));

							// Reflect the second control point
							float cx2 = 2.0f * x2 - hx, cy2 = 2.0f * y2 - hy;
							D2D1_BEZIER_SEGMENT bezier = D2D1::BezierSegment(D2D1::Point2F(cx1, cy1), D2D1::Point2F(cx2, cy2), D2D1::Point2F(x2, y2));
							sink->AddBezier(bezier);
							prevX = x2; prevY = y2;
						} else {
							// Straight line
							sink->AddLine(D2D1::Point2F(x2, y2));
							prevX = x2; prevY = y2;
						}
						j++;
					}

					if (started)
						sink->EndFigure(D2D1_FIGURE_END_OPEN);
					sink->Close();
					sink->Release();
					bitmapTarget->DrawGeometry(path, brush, (float)this->OptionInt(shape, L"size"));
					path->Release();
					break;
				}
				default: break;
			}
		}
	}

	hr = bitmapTarget->EndDraw();
	if (SUCCEEDED(hr)) {
		hr = bitmapTarget->GetBitmap(&this->bitmap);
		target->BeginDraw();
		target->DrawBitmap(this->bitmap, D2D1::RectF(0, 0, desiredSize.width, desiredSize.height));
		hr = target->EndDraw();
		if (hr == D2DERR_RECREATE_TARGET) {
			// Target needs to be recreated.
		}
	}
}
