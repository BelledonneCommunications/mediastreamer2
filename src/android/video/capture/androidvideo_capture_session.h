/*
 *  androidvideo_capture_session.h
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

#ifndef androidvideo_capture_session_include
#define androidvideo_capture_session_include

#include <camera/NdkCameraDevice.h>
#include <media/NdkImageReader.h>

#include "androidvideo_camera2.h"

namespace AndroidVideo {
class AndroidVideoCamera2;

class AndroidVideoCaptureSession {
private:
	AndroidVideoCamera2 *mAndroidVideoCamera2;

	// Session
	ACameraCaptureSession *mCameraSession;

	// Window
	ANativeWindow *mWindow;
	ACameraOutputTarget *mOutputTarget;
	bool mPreview;

	// Image Reader
	AImageReader* mImageReader;

	// Session
	ACaptureSessionOutput *mSessionOutput;
	ACaptureSessionOutputContainer *mSessionOutputContainer;
	bool mSessionReady;
	bool mSessionStop;
	bool mSessionReset;

	// Request
	ACaptureRequest *mCaptureRequest;
	bool mRequestRepeat;

	// Callback
	ACameraCaptureSession_stateCallbacks *mCaptureSessionCallback;
	AImageReader_ImageListener *mImageCallback;

public:
	AndroidVideoCaptureSession(AndroidVideoCamera2 *avc, ACameraCaptureSession_stateCallbacks *cbSession, AImageReader_ImageListener *cbImage, bool preview = false);
	~AndroidVideoCaptureSession() = default;

	AImageReader* getImageReader();
	void setWindow(ANativeWindow *mWindow);

	// ANativeWindow is given when it's a preview
	void init();

	void repeatingRequest();
	void stopRepeatingRequest();
	void abortCapture();

	void sessionReady();
	void sessionClosed();

private:
	AndroidVideoCaptureSession(const AndroidVideoCaptureSession&) = delete;
	AndroidVideoCaptureSession() = delete;

	// Helper
	void initWindow();
	void uninitWindow();

	void initSession();
	void uninitSession();

	void initRequest();
	void uninitRequest();

	camera_status_t checkReturnCameraStatus(camera_status_t status);
	media_status_t checkReturnMediaStatus(media_status_t status);
};
}

#endif // androidvideo_capture_session_include