/*
 * Copyright (c) 2020 Belledonne Communications SARL.
 *
 * This file is part of mediastreamer2.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef _MS_MEDIA_FOUNDATION_CAP_H
#define _MS_MEDIA_FOUNDATION_CAP_H

#ifndef WINVER
 // Set WINVER to get MF_DEVSOURCE attributes
#define WINVER _WIN32_WINNT_WIN7
#endif

#define _WINSOCKAPI_
#include <windows.h>
#include <mfidl.h>	// IMFMediaSource
#include <mfreadwrite.h>	// IMFSourceReaderCallback
#include <mfapi.h>	// MFCreateAttributes
#include <mfobjects.h>	// ActivateObject
#include <stdlib.h>	// wchar => char
#include <stdio.h>

#include <shlwapi.h>	// QITAB

#include "mediastreamer2/msfilter.h"
#include "mediastreamer2/msvideo.h"

class MSMFoundationCap : public IMFSourceReaderCallback {
	CRITICAL_SECTION mCriticalSection;
	CONDITION_VARIABLE mIsFlushed;
	long mReferenceCount;
	IMFSourceReader* mSourceReader;	// The source
	MSYuvBufAllocator * mAllocator;
	bool_t mRunning;	// The reading process is running or not. Set it to FALSE to stop it.
	bool_t mHaveFrame;	// Used to know if the current frame is usable
	float mFPS;
	unsigned int mPlaneSize;	// Optimization to avoid to compute it on each frame
public:
	MSMFoundationCap();
	~MSMFoundationCap();

//-----------------------------------------  Getters/Setters
	void setVSize(MSVideoSize vsize);
	void setDeviceName(const std::string &name);
	void setFPS(float fps);
	float getFPS() const;
	int getDeviceOrientation() const;
	void setDeviceOrientation(int orientation);

//----------------------------------------  Mediastreamer Interface

	void activate();
	void start();
	void feed(MSFilter * filter);
	void stop();
	void deactivate();

//----------------------------------------  Actions

	HRESULT setCaptureDevice(const std::string& name);	// Set the current Capture device name
	HRESULT setSourceReader(IMFActivate *device);		// Set the reader source
	HRESULT getMediaConfiguration(IMFMediaType *pType, GUID *videoFormat, LONG *stride, UINT32 * frameWidth, UINT32 * frameHeight);// Get data from MediaType
	void setMediaConfiguration(const GUID &videoFormat, const LONG &stride, const UINT32 &frameWidth, const UINT32 &frameHeight);// Set internal data and do updates
	bool_t isTimeToSend(uint64_t tickerTime);// Wheck if we have to put the frame in ms queue

//----------------------------------------  Variables

	GUID mVideoFormat;  // MFVideoFormat_NV12 is only supported. Another filter will lead to trying to force the media to MFVideoFormat_NV12
	int mPixelFormat;	// Store MSPixFmt
	int mOrientation;
	LONG mStride;
	UINT32 mWidth;
	UINT32 mHeight;
	int mBytesPerPixel;	// Computed as : abs(mStride) / mWidth	
	BYTE* mRawData;
	DWORD mRawDataLength;// Contains a copy of the internal buffer of MediaFoundation
	std::string mDeviceName;
	MSAverageFPS mAvgFPS;
	MSFrameRateController mFramerateController;

//----------------------------------------	Special Microsoft Section

	// the class must implement the methods from IUnknown
	STDMETHODIMP QueryInterface(REFIID iid, void** ppv);
	STDMETHODIMP_(ULONG) AddRef();
	STDMETHODIMP_(ULONG) Release();

	//  the class must implement the methods from IMFSourceReaderCallback
	STDMETHODIMP OnReadSample(HRESULT status, DWORD streamIndex, DWORD streamFlags, LONGLONG timeStamp, IMFSample *sample);
	STDMETHODIMP OnEvent(DWORD, IMFMediaEvent *);
	STDMETHODIMP OnFlush(DWORD);
	static const char *pixFmtToString(const GUID &fmt);	// Helper to return the name of the format
};

#endif //_MS_MEDIA_FOUNDATION_CAP_H	