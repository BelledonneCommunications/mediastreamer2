/*
 *  androidvideo_camera2.h
 *
 *  mediastreamer2 library - modular sound and video processing and streaming
 *
 *  Copyright (C) 2018  Belledonne Communications, Grenoble, France
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#ifndef androidvideo_camera2_include
#define androidvideo_camera2_include

#include "androidvideo_abstract.h"

#include <iostream>
#include <typeinfo>

#include <camera/NdkCameraManager.h>
#include <camera/NdkCameraDevice.h>
#include <media/NdkImageReader.h>

#include "androidvideo_capture_session.h"

namespace AndroidVideo {
class AndroidVideoCaptureSession;

class AndroidVideoCamera2 : public AndroidVideoAbstract {
	friend class AndroidVideoCaptureSession;

private:
	// Camera
	ACameraManager *mCameraManager;
	ACameraDevice *mCameraDevice;

	// Window
	ANativeWindow *mWindowSurfaceView;

	// Session
	AndroidVideoCaptureSession *mPreviewSession;
	AndroidVideoCaptureSession *mCaptureSession;

	// Callback
	ACameraDevice_StateCallbacks mDeviceCallback;
	ACameraCaptureSession_stateCallbacks mCaptureSessionCallbackPreview;
	ACameraCaptureSession_stateCallbacks mCaptureSessionCallbackCapture;
	AImageReader_ImageListener mImageCallback;

public:
	AndroidVideoCamera2(MSFilter *f);
	~AndroidVideoCamera2();

	// Filter methods
	void videoCaptureInit();
	void videoCapturePreprocess();
	void videoCaptureProcess();
	void videoCapturePostprocess();
	void videoCaptureUninit();

	// Callbacks
	static void onDisconnected(void* context, ACameraDevice* device);
	static void onError(void* context, ACameraDevice* device, int error);
	static void onSessionActive(void* context, ACameraCaptureSession *session);
	static void onSessionReady(void* context, ACameraCaptureSession *session);
	static void onSessionClosed(void* context, ACameraCaptureSession *session);
	static void onImageAvailable(void* context, AImageReader* reader);

	// Other methods
	int videoCaptureSetVsize(void *arg);
	int videoSetNativePreviewWindow(void *arg);
	int videoCaptureSetAutofocus(void *arg);

	void putImage(jbyteArray frame);
private:
	AndroidVideoCamera2(const AndroidVideoCamera2&) = delete;
	AndroidVideoCamera2() = delete;

	void lock();
	void unlock();

	// Helper
	void setImage();

	void initCamera();
	void uninitCamera();

	camera_status_t checkReturnCameraStatus(camera_status_t status);
	media_status_t checkReturnMediaStatus(media_status_t status);
};
}

#endif // androidvideo_camera2_include