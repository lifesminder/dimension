/*
	Dimension Engine
	(c) LifesMinder

	Module name:
		builder.cpp
	Purpose:
		Builder creates shapes, frames and et cetera via
		functional programming lookalike interface.
*/

#include "pch.h"

wstring Trim(const wstring& s) {
	size_t start = s.find_first_not_of(L" \t\r\n"), end = s.find_last_not_of(L" \t\r\n");
	return (start == wstring::npos) ? L"" : s.substr(start, end - start + 1);
}
wstring ToLower(const wstring& s) {
	wstring result = s;
	transform(result.begin(), result.end(), result.begin(), [](const wchar_t ch) {
		return tolower(ch);
	});
	return result;
}

Shape DimShape(wstring block) {
	Shape sh{ .id = -1 };
	block = Trim(block);

	// Parse arguments if they're passed
	size_t open = block.find(L'(');
	if (open != wstring::npos) {
		// Determine the shape type
		wstring type = ToLower(Trim(block.substr(0, open)));
		if (type == L"rectangle") sh.type = SHAPE_RECTANGLE;
		else if (type == L"ellipse") sh.type = SHAPE_ELLIPSE;
		else if (type == L"line") sh.type = SHAPE_LINE;
		else if (type == L"text") sh.type = SHAPE_TEXT;
		else if (type == L"path") sh.type = SHAPE_PATH;
		else sh.type = SHAPE_NONE;

		size_t close = block.find(L')', open);
		if (close == wstring::npos) {
			sh.type = SHAPE_NONE;
			return sh;
		}
		wstring args = block.substr(open + 1, close - open - 1);

		// Our arguments are named, so we use key=value passes
		wstring attr;
		wistringstream argStream(args);
		while (argStream >> attr) {
			if (!attr.empty() && attr.back() == L',')
				attr.pop_back();

			size_t equals = attr.find(L'=');
			if (equals == wstring::npos) continue;

			wstring key = Trim(attr.substr(0, equals)), value = Trim(attr.substr(equals + 1));
			// handle quoted strings
			if (!value.empty() && (value.front() == L'"' || value.front() == L'\'')) {
				wchar_t quote = value.front();
				value = value.substr(1); // strip opening quote
				if (value.empty() || value.back() != quote) {
					wstring part;
					while (argStream >> part) {
						value += L" " + part;
						if (!part.empty() && part.back() == quote) break;
					}
				}

				// strip closing quote and trailing comma
				if (!value.empty() && value.back() == quote) value.pop_back();
				else if (!value.empty() && value.back() == L',') {
					value.pop_back();
					if (!value.empty() && value.back() == quote) value.pop_back();
				}
			}

			sh.options[key] = value;
		}
	} else {
		// There are no arguments — treat entire block as type name
		wstring type = ToLower(Trim(block));
		if (type == L"rectangle") sh.type = SHAPE_RECTANGLE;
		else if (type == L"ellipse") sh.type = SHAPE_ELLIPSE;
		else if (type == L"line") sh.type = SHAPE_LINE;
		else if (type == L"text") sh.type = SHAPE_TEXT;
		else if (type == L"path") sh.type = SHAPE_PATH;
		else sh.type = SHAPE_NONE;
	}

	return sh;
}