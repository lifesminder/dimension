/*
	Dimension Engine
	(c) LifesMinder

	Module name:
		export.cpp
	Purpose:
		This file defines exporting facilities to bitmap and full video
*/

#include "pch.h"
#include <mfapi.h>
#include <mfidl.h>
#include <mfreadwrite.h>
#pragma comment(lib, "mfplat.lib")
#pragma comment(lib, "mfreadwrite.lib")
#pragma comment(lib, "mfuuid.lib")

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

// Video renderer
// To-Do: add more options and codecs if appliable, gradually use open-source encoders
//		  instead of Windows Media Foundation
bool Engine::ExportVideo(wstring path) {
	if (this->frames.empty()) {
		MessageBox(this->hWnd, TEXT("There are no frames to export"), TEXT("DimEngine"), MB_ICONINFORMATION | MB_OK);
		return false;
	}

	// H.264 requires even dimensions
	const UINT width = (UINT)this->GetAs<int>(L"width", 800) & ~1u, height = (UINT)this->GetAs<int>(L"height", 600) & ~1u;
	UINT fps = this->GetAs<unsigned int>(L"fps", 12);
	if (fps == 0) fps = 12;
	const UINT64 frameDuration = 10000000ULL / fps; // 100-ns units
	const UINT bitrate = 8000000;

	bool ok = false, mfStarted = SUCCEEDED(MFStartup(MF_VERSION));
	IMFSinkWriter* writer = nullptr;
	IMFMediaType *outType = nullptr, *inType = nullptr;
	DWORD stream = 0;
	const unsigned int savedFrame = this->currentFrame;
	Bitmap* bmp = nullptr;

	if (!mfStarted) goto done;
	if (FAILED(MFCreateSinkWriterFromURL(path.c_str(), nullptr, nullptr, &writer))) goto done;

	// Output: H.264
	if (FAILED(MFCreateMediaType(&outType))) goto done;
	outType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
	outType->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_H264);
	outType->SetUINT32(MF_MT_AVG_BITRATE, bitrate);
	outType->SetUINT32(MF_MT_INTERLACE_MODE, MFVideoInterlace_Progressive);
	MFSetAttributeSize(outType, MF_MT_FRAME_SIZE, width, height);
	MFSetAttributeRatio(outType, MF_MT_FRAME_RATE, fps, 1);
	MFSetAttributeRatio(outType, MF_MT_PIXEL_ASPECT_RATIO, 1, 1);
	if (FAILED(writer->AddStream(outType, &stream))) goto done;

	// Input: top-down 32-bit RGB
	if (FAILED(MFCreateMediaType(&inType))) goto done;
	inType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
	inType->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_RGB32);
	inType->SetUINT32(MF_MT_INTERLACE_MODE, MFVideoInterlace_Progressive);

	// No stride attribute: MF then treats RGB32 as bottom-up, and the rows are flipped when copied
	MFSetAttributeSize(inType, MF_MT_FRAME_SIZE, width, height);
	MFSetAttributeRatio(inType, MF_MT_FRAME_RATE, fps, 1);
	MFSetAttributeRatio(inType, MF_MT_PIXEL_ASPECT_RATIO, 1, 1);
	if (FAILED(writer->SetInputMediaType(stream, inType, nullptr))) goto done;
	if (FAILED(writer->BeginWriting())) goto done;

	bmp = new Bitmap(this->GetAs<int>(L"width", 800), this->GetAs<int>(L"height", 600));
	{
		const DWORD frameBytes = width * height * 4;
		LONGLONG time = 0;
		ok = true;

		for (unsigned int i = 0; i < this->frames.size() && ok; i++) {
			this->currentFrame = i;
			this->Draw(bmp);

			BitmapData data;
			Rect area(0, 0, (INT)width, (INT)height);
			if (bmp->LockBits(&area, ImageLockModeRead, PixelFormat32bppRGB, &data) != Ok) { ok = false; break; }

			IMFMediaBuffer* buffer = nullptr;
			IMFSample* sample = nullptr;
			BYTE* dst = nullptr;
			if (SUCCEEDED(MFCreateMemoryBuffer(frameBytes, &buffer)) && SUCCEEDED(buffer->Lock(&dst, nullptr, nullptr))) {
				// GDI+ rows are top-down (row y at Scan0 + y * Stride); write them bottom-up
				for (UINT y = 0; y < height; y++) {
					const BYTE* srcRow = (const BYTE*)data.Scan0 + (ptrdiff_t)y * data.Stride;
					memcpy(dst + (size_t)(height - 1 - y) * width * 4, srcRow, (size_t)width * 4);
				}
				buffer->Unlock();
				buffer->SetCurrentLength(frameBytes);

				if (SUCCEEDED(MFCreateSample(&sample))) {
					sample->AddBuffer(buffer);
					sample->SetSampleTime(time);
					sample->SetSampleDuration((LONGLONG)frameDuration);
					ok = SUCCEEDED(writer->WriteSample(stream, sample));
					sample->Release();
				} else ok = false;
			} else ok = false;

			if (buffer) buffer->Release();
			bmp->UnlockBits(&data);
			time += (LONGLONG)frameDuration;
		}

		if (ok) ok = SUCCEEDED(writer->Finalize());
	}

done:
	this->currentFrame = savedFrame;
	delete bmp;
	if (inType) inType->Release();
	if (outType) outType->Release();
	if (writer) writer->Release();
	if (mfStarted) MFShutdown();

	if (!ok) {
		MessageBox(this->hWnd, TEXT("Error while exporting the video"), TEXT("DimEngine"), MB_ICONERROR | MB_OK);
	}

	return ok;
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