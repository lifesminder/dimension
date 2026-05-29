/*
	Dimension Engine
	(c) LifesMinder

	Module name:
		export.cpp
	Purpose:
		This file defines exporting facilities to bitmap and
		full video
*/

#include "pch.h"

int GetEncoderCLSID(const WCHAR *format, CLSID *clsid) {
	UINT num = 0, size = 0;
	GetImageEncodersSize(&num, &size);
	if (size == 0)
		return -1;

	ImageCodecInfo* codecInfo = (ImageCodecInfo*)malloc(size);
	if (codecInfo == NULL)
		return -1;

	GetImageEncoders(num, size, codecInfo);
	for (int j = 0; j < num; ++j) {
		if (wcscmp(codecInfo[j].MimeType, format) == 0) {
			// this matches the requested format, pass it on
			*clsid = codecInfo[j].Clsid;
			free(codecInfo);
			return j;
		}
	}
	free(codecInfo);
	return -1;
}

void Engine::Export(wstring path, int frame) {
	Bitmap* bmp = new Bitmap(this->GetAs<int>(L"width", 800), this->GetAs<int>(L"height", 600));
	if (bmp) {
		// Draw the selected frame to the bitmap
		if (frame == -1) {
			MessageBox(this->hWnd, TEXT("The rendering to video is not supported yet"), TEXT("DimEngine"), MB_ICONINFORMATION | MB_OK);
			delete bmp;
			return;
		} else {
			this->Draw(bmp);
		}

		// Export as JPEG. To-Do: expand to different formats
		CLSID clsid;
		GetEncoderCLSID(L"image/jpeg", &clsid);

		Status status = bmp->Save(path.data(), &clsid, NULL);
		if (status != Ok) {
			MessageBox(this->hWnd, TEXT("Error while saving the file"), TEXT("DimEngine"), MB_ICONERROR | MB_OK);
		}
	}

	delete bmp;
}