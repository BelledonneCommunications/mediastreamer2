/*
 *  androidvideo_camera2.cpp
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

#include "androidvideo_camera2.h"

#include <vector>

#include <android/native_window_jni.h>
#include <camera/NdkCameraMetadata.h>
#include <camera/NdkCameraMetadataTags.h>
#include <camera/NdkCaptureRequest.h>
#include <camera/NdkCameraCaptureSession.h>
#include <camera/NdkCameraError.h>
#include <media/NdkImage.h>

#include "androidvideo_utils.h"

using namespace std;

namespace AndroidVideo {
	AndroidVideoCamera2::AndroidVideoCamera2(MSFilter *f) : AndroidVideoAbstract(f) {
		ms_message("Creating AndroidVideoCamera2 for Android VIDEO capture filter");
		this->mCameraManager = ACameraManager_create();
		this->mCameraDevice = nullptr;

		this->mPreviewSession = nullptr;
		this->mCaptureSession = nullptr;

		this->mWindowSurfaceView = nullptr;

		this->mDeviceCallback.context = static_cast<void*>(this);
		this->mDeviceCallback.onDisconnected = this->onDisconnected;
		this->mDeviceCallback.onError = this->onError;

		this->mCaptureSessionCallbackPreview.context = nullptr;
		this->mCaptureSessionCallbackPreview.onActive = this->onSessionActive;
		this->mCaptureSessionCallbackPreview.onClosed = this->onSessionClosed;
		this->mCaptureSessionCallbackPreview.onReady = this->onSessionReady;
		this->mCaptureSessionCallbackCapture = this->mCaptureSessionCallbackPreview;

		this->mImageCallback.context = static_cast<void*>(this);
		this->mImageCallback.onImageAvailable = this->onImageAvailable;
	}

	AndroidVideoCamera2::~AndroidVideoCamera2() {
		ms_message("Deleting AndroidVideoCamera2");
		this->uninitCamera();
		if (this->mCameraManager) ACameraManager_delete(this->mCameraManager);
	}

	// Filter methods

	void AndroidVideoCamera2::videoCaptureInit() {
		this->lock();
		this->mUseDownscaling = 0;
		//this->mPreviewSession = new AndroidVideoCaptureSession(this, &this->mCaptureSessionCallbackPreview, nullptr, true);
		this->mCaptureSession = new AndroidVideoCaptureSession(this, &this->mCaptureSessionCallbackCapture, &this->mImageCallback);
		this->mCaptureSessionCallbackPreview.context = static_cast<void*>(this->mPreviewSession);
		this->mCaptureSessionCallbackCapture.context = static_cast<void*>(this->mCaptureSession);
		this->unlock();
	}

	void AndroidVideoCamera2::videoCapturePreprocess() {
		this->lock();
		this->initCamera();
		this->mCaptureSession->init(); // Preview session will be init when window is get

		// Check if there is a frame from other capture and delete it
		if (this->mFrame) {
			freeb(this->mFrame);
			this->mFrame = nullptr;
		}

		this->unlock();
	}

	void AndroidVideoCamera2::videoCaptureProcess() {
		this->lock();

		//this->mPreviewSession->repeatingRequest();
		this->mCaptureSession->repeatingRequest();

		// If frame not ready, return
		if (this->mFrame == nullptr) {
			this->unlock();
			return;
		}

		ms_video_update_average_fps(&this->mAverageFps, this->mFilter->ticker->time);

		ms_queue_put(this->mFilter->outputs[0], this->mFrame);
		this->mFrame = nullptr;

		this->unlock();
	}

	void AndroidVideoCamera2::videoCapturePostprocess() {
		this->lock();
		//this->mPreviewSession->stopRepeatingRequest();
		this->mCaptureSession->stopRepeatingRequest();
		this->mPreviewWindow = nullptr;
		this->unlock();
	}

	void AndroidVideoCamera2::videoCaptureUninit() {
		this->lock();
		this->mCaptureSessionCallbackPreview.context = nullptr;
		this->mCaptureSessionCallbackCapture.context = nullptr;
		this->uninitCamera();
		this->unlock();
	}

	// Other methods

	int AndroidVideoCamera2::videoCaptureSetVsize(void *arg) {
		ACameraMetadata *deviceCaract;
		ACameraMetadata_const_entry deviceEntry;
		vector<int> hardwareCapabilitiesHeight;
		vector<int> hardwareCapabilitiesWidth;

		this->lock();

		this->mRequestedSize = *static_cast<MSVideoSize*>(arg);

		/************** Get Hardware capabilities **************/
		if (!this->mWebcam || !this->mWebcam ||
				this->checkReturnCameraStatus(ACameraManager_getCameraCharacteristics(this->mCameraManager, static_cast<AndroidVideo::AndroidWebcamConfig*>(this->mWebcam->data)->idChar, &deviceCaract))
				!= ACAMERA_OK) {
			this->unlock();
			return -1;
		}

		if (this->checkReturnCameraStatus(ACameraMetadata_getConstEntry(deviceCaract, ACAMERA_SCALER_AVAILABLE_STREAM_CONFIGURATIONS, &deviceEntry)) != ACAMERA_OK) {
			ACameraMetadata_free(deviceCaract);
			this->unlock();
			return -1;
		}

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

		this->setVsizeHelper();

		this->unlock();
		return 0;
	}

	int AndroidVideoCamera2::videoSetNativePreviewWindow(void *arg) {
		this->lock();

		jobject w = (jobject)*static_cast<unsigned long*>(arg);

		if (w == this->mPreviewWindow) {
			this->unlock();
			return 0;
		}

		if (this->mCameraDevice) {
			if (!this->mPreviewWindow) {
				ms_message("Preview capture window set for the 1st time (win: %p rotation:%d)\n", w, this->mRotation);
			} else {
				ms_message("Preview capture window changed (oldwin: %p newwin: %p rotation:%d)\n", this->mPreviewWindow, w, this->mRotation);
			}
			// Stop current session
			//this->mPreviewSession->abortCapture();
			//this->mCaptureSession->abortCapture();
		} else {
			ms_message("Preview capture window set but camera not opened yet; remembering it for later use\n");
		}

		this->mPreviewWindow = w;
		// We need to get Surface here because we only can get this in interface thread
		this->mWindowSurfaceView = ANativeWindow_fromSurface(this->mJavaEnv, this->mPreviewWindow);
		if (this->mPreviewSession && this->mWindowSurfaceView) {
			this->mPreviewSession->setWindow(this->mWindowSurfaceView);
			//this->mPreviewSession->init();
		}

		this->unlock();
		return 0;
	}

	int AndroidVideoCamera2::videoCaptureSetAutofocus(void *arg) {
		this->lock();
		// Check if option could be set in session or request
		this->unlock();
		return 0;
	}

	void AndroidVideoCamera2::putImage(jbyteArray frame) {
		// Nothing to do
	}

	void AndroidVideoCamera2::lock() {
		AndroidVideoAbstract::lock();
	}

	void AndroidVideoCamera2::unlock() {
		AndroidVideoAbstract::unlock();
	}

	// Helper

	void AndroidVideoCamera2::setImage() {
		int rowStride[3];
		int pixelStride[3];
		MSPicture msPicture;
		MSRect rect, rectSrc;
		int bufferSize[3] = {0};
		uint8_t* bufferPlane[3] = {nullptr};
		AImage* image = nullptr;
		mblk_t* yuvBlock = nullptr;

		this->lock();

		if (!this->mCaptureSession || !this->mCaptureSession->getImageReader()) {
			this->unlock();
			return;
		}

		if (this->mFilter->ticker == nullptr || !ms_video_capture_new_frame(&this->mFpsControl, ms_ticker_get_time(this->mFilter->ticker))) {
			this->unlock();
			return;
		}

		if (this->mRotation != UNDEFINED_ROTATION && this->mRotationSavedDuringVSize != this->mRotation) {
			ms_warning("Rotation has changed (new value: %d) since vsize was run (old value: %d)."
						"Will produce inverted images. Use set_device_orientation() then update call.\n",
				this->mRotation, this->mRotationSavedDuringVSize);
		}

		int image_rotation_correction = this->computeImageRotationCorrection();

		if (this->checkReturnMediaStatus(AImageReader_acquireLatestImage(this->mCaptureSession->getImageReader(), &image)) != AMEDIA_OK) goto end;

		// Get all plane
		for (unsigned int i = 0 ; i < 3 ; i++) {
			int32_t rowStride32;
			int32_t pixelStride32;

			if (this->checkReturnMediaStatus(AImage_getPlaneData(image, i, &bufferPlane[i], &bufferSize[i])) != AMEDIA_OK) goto end;
			if (this->checkReturnMediaStatus(AImage_getPlaneRowStride(image, i, &rowStride32)) != AMEDIA_OK) goto end;
			if (this->checkReturnMediaStatus(AImage_getPlanePixelStride(image, i, &pixelStride32)) != AMEDIA_OK) goto end;

			pixelStride[i] = pixelStride32;
			rowStride[i] = rowStride32;
		}

		yuvBlock = ms_yuv_buf_allocator_get(this->mAllocator, &msPicture, this->mUsedSize.width, this->mUsedSize.height);

		rect.x = 0;
		rect.y = 0;
		rect.w = this->mUsedSize.height;
		rect.h = this->mUsedSize.width;

		{
			AImageCropRect cropRect;
			if (this->checkReturnMediaStatus(AImage_getCropRect(image, &cropRect)) != AMEDIA_OK) goto end;
			rectSrc.h = cropRect.bottom - cropRect.top;
			rectSrc.w = cropRect.right - cropRect.left;
			rectSrc.y = cropRect.top;
			rectSrc.x = cropRect.left;
		}

		// U/V planar
		if (pixelStride[1] == 1) {
			yuvBlock = copy_yuv_with_rotation(this->mAllocator,
				bufferPlane[0],
				bufferPlane[1],
				bufferPlane[2],
				image_rotation_correction,
				this->mUsedSize.width,
				this->mUsedSize.height,
				rowStride[0],
				rowStride[1],
				rowStride[2]);
		} else {
			const uint8_t* bufferCrb = bufferPlane[1];
			if (bufferPlane[2] < bufferPlane[1]) {
				bufferCrb = bufferPlane[2];
			}
			yuvBlock = copy_ycbcrbiplanar_to_true_yuv_with_rotation_and_down_scale_by_2(
				this->mAllocator,
				bufferPlane[0],
				bufferCrb,
				image_rotation_correction,
				this->mUsedSize.width,
				this->mUsedSize.height,
				rowStride[0],
				rowStride[1],
				false,
				false);
		}

		if (yuvBlock) this->mFrame = yuvBlock;

		end:
		if (image) AImage_delete(image);
		this->unlock();
	}

	void AndroidVideoCamera2::initCamera() {
		if (this->mCameraManager && this->mWebcam && this->mWebcam->data &&
				this->checkReturnCameraStatus(ACameraManager_openCamera(
					this->mCameraManager, static_cast<AndroidVideo::AndroidWebcamConfig*>(this->mWebcam->data)->idChar, &this->mDeviceCallback, &this->mCameraDevice)) != ACAMERA_OK) {
			this->mCameraDevice = nullptr;
		}
	}

	void AndroidVideoCamera2::uninitCamera() {
		if (this->mCameraDevice) {
			this->checkReturnCameraStatus(ACameraDevice_close(this->mCameraDevice));
			this->mCameraDevice = nullptr;
		}
	}

	// Callbacks

	void AndroidVideoCamera2::onDisconnected(void* context, ACameraDevice* device) {
		ms_message("AndroidVideoCamera2: Camera %s disconnected.", ACameraDevice_getId(device));
	}

	void AndroidVideoCamera2::onError(void* context, ACameraDevice* device, int error) {
		// Stop capture if running etc...
		ms_error("AndroidVideoCamera2: onError number %d", error);
	}

	void AndroidVideoCamera2::onSessionActive(void* context, ACameraCaptureSession *session) {
		ms_message("AndroidVideoCamera2: Starting capture requests process");
	}

	void AndroidVideoCamera2::onSessionReady(void* context, ACameraCaptureSession *session) {
		if (context) static_cast<AndroidVideoCaptureSession*>(context)->sessionReady();
	}

	void AndroidVideoCamera2::onSessionClosed(void* context, ACameraCaptureSession *session) {
		if (context) {
			static_cast<AndroidVideoCaptureSession*>(context)->sessionClosed();
			delete static_cast<AndroidVideoCaptureSession*>(context);
		}
	}

	void AndroidVideoCamera2::onImageAvailable(void* context, AImageReader* reader) {
		if (context) static_cast<AndroidVideoCamera2*>(context)->setImage();
	}

	camera_status_t AndroidVideoCamera2::checkReturnCameraStatus(camera_status_t status) {
		return AndroidVideo::checkReturnCameraStatus(status, "AndroidVideoCamera2");
	}

	media_status_t AndroidVideoCamera2::checkReturnMediaStatus(media_status_t status) {
		return AndroidVideo::checkReturnMediaStatus(status, "AndroidVideoCamera2");
	}
}