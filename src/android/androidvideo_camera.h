/*
 *  androidvideo_camera.h
 *
 *  mediastreamer2 library - modular sound and video processing and streaming
 *  This is the video capture filter for Android using deprecated API android.hardware.Camera.
 *  It uses one of the JNI wrappers to access Android video capture API(5,8,9).
 *
 *  Copyright (C) 2010-2018  Belledonne Communications, Grenoble, France
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

#ifndef androidvideo_camera_include
#define androidvideo_camera_include

#include "androidvideo_abstract.h"

namespace AndroidVideo {
class AndroidVideoCamera : public AndroidVideoAbstract {
	private:
		jmethodID mMethodActivateAutoFocus;
		jmethodID mMethodSelectNearestResolutionAvailable;
		jmethodID mMethodSetPreviewDisplaySurface;
		jmethodID mMethodStartRecording;
		jmethodID mMethodStopRecording;
	public:
		AndroidVideoCamera(MSFilter *f);
		~AndroidVideoCamera();

		// Filter methods
		void videoCaptureInit();
		void videoCapturePreprocess();
		void videoCaptureProcess();
		void videoCapturePostprocess();
		void videoCaptureUninit();

		// Other methods
		int videoCaptureSetVsize(void *arg);
		int videoSetNativePreviewWindow(void *arg);
		int videoCaptureSetAutofocus(void *arg);

		void putImage(jbyteArray frame);
		void initJNIMethod();
	private:
		AndroidVideoCamera(const AndroidVideoCamera&) = delete;
		AndroidVideoCamera() = delete;
};
}

#endif // androidvideo_camera_include