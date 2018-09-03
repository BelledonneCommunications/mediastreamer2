/*
 *  androidvideo_camera2.cpp
 *
 *  mediastreamer2 library - modular sound and video processing and streaming
 *  This is the video capture filter for Android using deprecated API android.hardware.Camera.
 *  It uses one of the JNI wrappers to access Android video capture API(5,8,9).
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

#include "androidvideo_camera2.h"

#include <camera/NdkCameraMetadata.h>
#include <camera/NdkCameraMetadataTags.h>
#include <camera/NdkCaptureRequest.h>
#include <camera/NdkCameraCaptureSession.h>
#include <camera/NdkCameraError.h>
#include <media/NdkImage.h>

#include <vector>

extern MSWebCamDesc ms_android_video_capture_desc;

using namespace std;

namespace AndroidVideo {
	AndroidVideoCamera2::AndroidVideoCamera2(MSFilter *f) : AndroidVideoAbstract(f) {
		ms_message("Creating AndroidVideoCamera2 for Android VIDEO capture filter");
		this->mCameraManager = ACameraManager_create();
	}

	AndroidVideoCamera2::~AndroidVideoCamera2() {
		ms_message("Deleting AndroidVideoCamera2");
		if (this->mCameraManager) ACameraManager_delete(this->mCameraManager);
	}

	// Filter methods

	void AndroidVideoCamera2::videoCaptureInit() {
		this->lock();
		this->mUseDownscaling = 0;
		this->unlock();
	}

	void AndroidVideoCamera2::videoCapturePreprocess() {

	}

	void AndroidVideoCamera2::videoCaptureProcess() {

	}

	void AndroidVideoCamera2::videoCapturePostprocess() {

	}

	void AndroidVideoCamera2::videoCaptureUninit() {

	}

	// Callbacks

	void cameraDeviceStateCallback(void* context, ACameraDevice* device, int error) {
		switch (error) {
			case ERROR_CAMERA_IN_USE :
				break;
			case ERROR_MAX_CAMERAS_IN_USE :
				break;
			case ERROR_CAMERA_DISABLED :
				break;
			case ERROR_CAMERA_DEVICE :
				break;
			case ERROR_CAMERA_SERVICE :
				break;
			default:
				break;
		}
	}

	// Other methods

	int AndroidVideoCamera2::videoCaptureSetVsize(void *arg) {
		ACameraMetadata *deviceCaract;
		ACameraMetadata_const_entry deviceEntry;
		vector<int> hardwareCapabilitiesHeight;
		vector<int> hardwareCapabilitiesWidth;

		this->lock();

		/************** Get Hardware capabilities **************/
		camera_status_t status = ACameraManager_getCameraCharacteristics(this->mCameraManager, this->mWebcam->id, &deviceCaract);
		switch (status) {
			case ACAMERA_OK:
				ms_debug("AndroidVideoCamera2: Camera characteristics getted.");
				break;
			case ACAMERA_ERROR_INVALID_PARAMETER :
			case ACAMERA_ERROR_CAMERA_DISCONNECTED :
			case ACAMERA_ERROR_NOT_ENOUGH_MEMORY :
			case ACAMERA_ERROR_UNKNOWN :
				ms_error("AndroidVideoCamera2: Error during characteristics getting.");
				this->unlock();
				return -1;
			default:
				break;
		}

		ACameraMetadata_getConstEntry(deviceCaract, ACAMERA_SCALER_AVAILABLE_STREAM_CONFIGURATIONS, &deviceEntry);

		// Entry format (format, width, height, input)
		for (unsigned int i = 0 ; i < deviceEntry.count ; i += 4) {
			// Skip if Input value
			if (deviceEntry.data.i32[i + 3]) continue;

			// Check JPEG format
			if (deviceEntry.data.i32[i + 0] == AIMAGE_FORMAT_JPEG) {
				hardwareCapabilitiesHeight.push_back(deviceEntry.data.i32[i + 2]);
				hardwareCapabilitiesWidth.push_back(deviceEntry.data.i32[i + 1]);
				ms_message("AndroidVideoCamera2: Camera output spec: width=%i - height=%i", deviceEntry.data.i32[i + 1], deviceEntry.data.i32[i + 2]);
			}
		}

		ACameraMetadata_free(deviceCaract);

		if (hardwareCapabilitiesHeight.size() <= 0) {
			ms_error("AndroidVideoCamera2: No size capabilities found for camera %s", this->mWebcam->id);
			this->unlock();
			return 0;
		}
		/*******************************************************/
		this->mHwCapableSize.height = hardwareCapabilitiesHeight.at(0);
		this->mHwCapableSize.width = hardwareCapabilitiesWidth.at(0);

		this->mRequestedSize = *static_cast<MSVideoSize*>(arg);

		this->setVsizeHelper();

		this->unlock();
		return 0;
	}

	int AndroidVideoCamera2::videoSetNativePreviewWindow(void *arg) {
		return 0;
	}

	int AndroidVideoCamera2::videoCaptureSetAutofocus(void *arg) {
		return 0;
	}

	void AndroidVideoCamera2::putImage(jbyteArray frame) {
		this->lock();

		this->unlock();
	}
}