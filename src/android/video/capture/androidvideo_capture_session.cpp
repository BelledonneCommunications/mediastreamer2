/*
 *  androidvideo_capture_session.cpp
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

#include "androidvideo_capture_session.h"

#include "androidvideo_utils.h"

namespace AndroidVideo {
	AndroidVideoCaptureSession::AndroidVideoCaptureSession(AndroidVideoCamera2 *avc, ACameraCaptureSession_stateCallbacks *cbSession, AImageReader_ImageListener *cbImage, bool preview) {
		this->mAndroidVideoCamera2 = avc;

		this->mCameraSession = nullptr;
		this->mOutputTarget = nullptr;
		this->mSessionOutput = nullptr;
		this->mSessionOutputContainer = nullptr;
		this->mSessionReady = false;
		this->mSessionStop = true;
		this->mSessionReset = false;

		this->mCaptureRequest = nullptr;
		this->mRequestRepeat = true;

		this->mCaptureSessionCallback = cbSession;
		this->mImageCallback = cbImage;

		this->mImageReader = nullptr;

		this->mPreview = preview;
	}

	AImageReader* AndroidVideoCaptureSession::getImageReader() {
		return this->mImageReader;
	}

	void AndroidVideoCaptureSession::setWindow(ANativeWindow *window) {
		this->mWindow = window;
	}

	void AndroidVideoCaptureSession::init() {
		this->initWindow();
		this->initSession();
		this->initRequest();
	}

	void AndroidVideoCaptureSession::repeatingRequest() {
		if (this->mCameraSession && this->mRequestRepeat && this->mCaptureRequest) {
			if (this->checkReturnCameraStatus(ACameraCaptureSession_setRepeatingRequest(this->mCameraSession, nullptr, 1, &this->mCaptureRequest, nullptr)) == ACAMERA_OK) {
				this->mRequestRepeat = false;
			}
		}
	}

	void AndroidVideoCaptureSession::stopRepeatingRequest() {
		if (this->mCameraSession) {
			if (this->mImageReader) {
				this->checkReturnMediaStatus(AImageReader_setImageListener(this->mImageReader, nullptr));
			}
			if (this->checkReturnCameraStatus(ACameraCaptureSession_stopRepeating(this->mCameraSession)) == ACAMERA_OK) {
				this->mRequestRepeat = true;
			}
			this->mSessionStop = true;
		}
	}

	void AndroidVideoCaptureSession::abortCapture() {
		if (!this->mCameraSession || this->checkReturnCameraStatus(ACameraCaptureSession_abortCaptures(this->mCameraSession)) == ACAMERA_OK) {
			this->mSessionReset = true;
			this->mCameraSession = nullptr;
		}
		this->mSessionReady = false;
		this->mRequestRepeat = true;
	}

	// Helper
	void AndroidVideoCaptureSession::initWindow() {
		if (this->mAndroidVideoCamera2->mCameraDevice) {
			//TODO change size for preview
			if (this->checkReturnMediaStatus(AImageReader_new(this->mAndroidVideoCamera2->mUsedSize.width, this->mAndroidVideoCamera2->mUsedSize.height, AIMAGE_FORMAT_YUV_420_888, 4, &this->mImageReader)) != AMEDIA_OK) {
				this->mImageReader = nullptr;
				return;
			}
			if (!this->mPreview && this->checkReturnMediaStatus(AImageReader_getWindow(this->mImageReader, &this->mWindow)) != AMEDIA_OK) {
				this->uninitWindow();
				return;
			}
			if (this->checkReturnMediaStatus(AImageReader_setImageListener(this->mImageReader, this->mImageCallback)) != AMEDIA_OK) {
				this->uninitWindow();
				return;
			}
		}
	}

	void AndroidVideoCaptureSession::uninitWindow() {
		if (this->mImageReader) {
			AImageReader_delete(this->mImageReader);
			this->mImageReader = nullptr;
		}
		if (!this->mPreview && this->mWindow) {
			ANativeWindow_release(this->mWindow);
			this->mWindow = nullptr;
		}
	}

	void AndroidVideoCaptureSession::initSession() {
		if (this->mAndroidVideoCamera2->mCameraDevice && this->mWindow) {
			if (this->checkReturnCameraStatus(ACaptureSessionOutput_create(this->mWindow, &this->mSessionOutput)) != ACAMERA_OK) {
				this->mSessionOutput = nullptr;
				this->uninitSession();
				return;
			}
			if (this->checkReturnCameraStatus(ACaptureSessionOutputContainer_create(&this->mSessionOutputContainer)) != ACAMERA_OK) {
				this->mSessionOutputContainer = nullptr;
				this->uninitSession();
				return;
			}
			if (this->checkReturnCameraStatus(ACaptureSessionOutputContainer_add(this->mSessionOutputContainer, this->mSessionOutput)) != ACAMERA_OK) {
				this->uninitSession();
				return;
			}
			if (this->checkReturnCameraStatus(ACameraDevice_createCaptureSession(this->mAndroidVideoCamera2->mCameraDevice, this->mSessionOutputContainer, this->mCaptureSessionCallback, &this->mCameraSession)) != ACAMERA_OK) {
				this->uninitSession();
				this->mCameraSession = nullptr;
				return;
			}
			this->mSessionStop = false;
			this->mSessionReady = true;
		}
	}

	void AndroidVideoCaptureSession::sessionReady() {
		this->mAndroidVideoCamera2->lock();
		this->mSessionReady = true;
		if (this->mSessionStop) {
			this->uninitRequest();
			this->uninitSession();
			this->uninitWindow();
		}
		if (!this->mPreview && this->mSessionReset) {
			this->mSessionReset = false;
			this->initSession();
			this->initRequest();
		}
		this->mAndroidVideoCamera2->unlock();
	}

	void AndroidVideoCaptureSession::sessionClosed() {
		this->mAndroidVideoCamera2->lock();
		this->mCameraSession = nullptr;
		this->mAndroidVideoCamera2->unlock();
	}

	void AndroidVideoCaptureSession::uninitSession() {
		if (this->mSessionOutputContainer) {
			ACaptureSessionOutputContainer_free(this->mSessionOutputContainer);
			this->mSessionOutputContainer = nullptr;
		}
		if (this->mSessionOutput) {
			ACaptureSessionOutput_free(this->mSessionOutput);
			this->mSessionOutput = nullptr;
		}
		if (this->mCameraSession) {
			ACameraCaptureSession_close(this->mCameraSession);
			this->mCameraSession = nullptr;
		}

		this->mSessionStop = false;
	}

	void AndroidVideoCaptureSession::initRequest() {
		if (this->mAndroidVideoCamera2->mCameraDevice && this->mWindow) {
			if (this->checkReturnCameraStatus(ACameraDevice_createCaptureRequest(this->mAndroidVideoCamera2->mCameraDevice, (this->mPreview) ? TEMPLATE_PREVIEW : TEMPLATE_RECORD, &this->mCaptureRequest)) != ACAMERA_OK) {
				this->mCaptureRequest = nullptr;
				this->uninitRequest();
				return;
			}
			if (this->checkReturnCameraStatus(ACameraOutputTarget_create(this->mWindow, &this->mOutputTarget)) != ACAMERA_OK) {
				this->mOutputTarget = nullptr;
				this->uninitRequest();
				return;
			}
			if (this->checkReturnCameraStatus(ACaptureRequest_addTarget(this->mCaptureRequest, this->mOutputTarget)) != ACAMERA_OK) {
				this->uninitRequest();
				return;
			}
		}
	}
	void AndroidVideoCaptureSession::uninitRequest() {
		if (this->mOutputTarget) {
			ACameraOutputTarget_free(this->mOutputTarget);
			this->mOutputTarget = nullptr;
		}
		if (this->mCaptureRequest) {
			ACaptureRequest_free(this->mCaptureRequest);
			this->mCaptureRequest = nullptr;
		}
	}

	camera_status_t AndroidVideoCaptureSession::checkReturnCameraStatus(camera_status_t status) {
		return AndroidVideo::checkReturnCameraStatus(status, "AndroidVideoCaptureSession");
	}

	media_status_t AndroidVideoCaptureSession::checkReturnMediaStatus(media_status_t status) {
		return AndroidVideo::checkReturnMediaStatus(status, "AndroidVideoCaptureSession");
	}
}