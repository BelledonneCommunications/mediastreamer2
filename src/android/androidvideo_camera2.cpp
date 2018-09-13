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

#include <android/native_window_jni.h>
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
		this->mCameraSession = nullptr;
		this->mCameraDevice = nullptr;

		this->mWindows = nullptr;
		this->mOutputTarget = nullptr;

		this->mImageReader = nullptr;

		this->mSessionOutput = nullptr;
		this->mSessionOutputContainer = nullptr;
		this->mSessionReady = false;
		this->mSessionStop = true;
		this->mSessionReset = false;

		this->mCaptureRequest = nullptr;
		this->mRequestRepeat = true;

		this->mDeviceCallback.context = static_cast<void*>(this);
		this->mDeviceCallback.onDisconnected = this->onDisconnected;
		this->mDeviceCallback.onError = this->onError;

		this->mCaptureSessionCallback.context = static_cast<void*>(this);
		this->mCaptureSessionCallback.onActive = this->onSessionActive;
		this->mCaptureSessionCallback.onClosed = this->onSessionClosed;
		this->mCaptureSessionCallback.onReady = this->onSessionReady;

		this->mCaptureCallbacks.context = static_cast<void*>(this);
		this->mCaptureCallbacks.onCaptureBufferLost = nullptr;
		this->mCaptureCallbacks.onCaptureCompleted = nullptr;
		this->mCaptureCallbacks.onCaptureFailed = nullptr;
		this->mCaptureCallbacks.onCaptureProgressed = nullptr;
		this->mCaptureCallbacks.onCaptureSequenceAborted = nullptr;
		this->mCaptureCallbacks.onCaptureSequenceCompleted = this->onCaptureSequenceCompleted;
		this->mCaptureCallbacks.onCaptureStarted = nullptr;

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
		this->unlock();
	}

	void AndroidVideoCamera2::videoCapturePreprocess() {
		this->lock();
		this->initCamera();
		this->initWindow();
		this->initSession();
		this->initRequest();

		// Check if there is a frame from other capture and delete it
		if (this->mFrame) {
			ms_free(this->mFrame);
			this->mFrame = nullptr;
		}

		this->unlock();
	}

	void AndroidVideoCamera2::videoCaptureProcess() {
		this->lock();
		if (this->mRequestRepeat && this->mCameraSession && this->mSessionReady) {
			if (this->checkReturnCameraStatus(ACameraCaptureSession_setRepeatingRequest(this->mCameraSession, &this->mCaptureCallbacks, 1, &this->mCaptureRequest, nullptr)) == ACAMERA_OK) {
				this->mRequestRepeat = false;
			}
		}

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
		if (this->mCameraSession) {
			if (this->mImageReader) {
				this->checkReturnMediaStatus(AImageReader_setImageListener(this->mImageReader, nullptr));
			}
			if (this->checkReturnCameraStatus(ACameraCaptureSession_stopRepeating(this->mCameraSession)) == ACAMERA_OK) {
				this->mRequestRepeat = true;
			}
		}
		this->unlock();
	}

	void AndroidVideoCamera2::videoCaptureUninit() {
		this->lock();
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
			if (this->mWindows == nullptr) {
				ms_message("Preview capture window set for the 1st time (win: %p rotation:%d)\n", w, this->mRotation);
			} else {
				ms_message("Preview capture window changed (oldwin: %p newwin: %p rotation:%d)\n", this->mPreviewWindow, w, this->mRotation);

				// Stop current session
				if (!this->mCameraSession || this->checkReturnCameraStatus(ACameraCaptureSession_abortCaptures(this->mCameraSession)) == ACAMERA_OK) {
					this->mSessionReset = true;
				}
				this->mSessionReady = false;
				this->mRequestRepeat = true;
				this->mCameraSession = nullptr;
			}
		} else {
			ms_message("Preview capture window set but camera not opened yet; remembering it for later use\n");
		}
		this->mPreviewWindow = w;

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

		if (!this->mImageReader) {
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

		if (this->checkReturnMediaStatus(AImageReader_acquireLatestImage(this->mImageReader, &image)) != AMEDIA_OK) goto end;

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

	void AndroidVideoCamera2::initWindow() {
		if (this->mCameraDevice) {
			if (this->checkReturnMediaStatus(AImageReader_new(this->mUsedSize.width, this->mUsedSize.height, AIMAGE_FORMAT_YUV_420_888, 4, &this->mImageReader)) != AMEDIA_OK) {
				this->mImageReader = nullptr;
				return;
			}
			if (this->checkReturnMediaStatus(AImageReader_getWindow(this->mImageReader, &this->mWindows)) != AMEDIA_OK) {
				this->mWindows = nullptr;
				this->uninitWindow();
				return;
			}
			if (this->checkReturnMediaStatus(AImageReader_setImageListener(this->mImageReader, &this->mImageCallback)) != AMEDIA_OK) {
				this->uninitWindow();
				return;
			}
		}
	}

	void AndroidVideoCamera2::uninitWindow() {
		if (this->mImageReader) {
			AImageReader_delete(this->mImageReader);
			this->mImageReader = nullptr;
		}
		if (this->mWindows) {
			ANativeWindow_release(this->mWindows);
			this->mWindows = nullptr;
		}
	}

	void AndroidVideoCamera2::initSession() {
		if (this->mCameraDevice && this->mWindows) {
			if (this->checkReturnCameraStatus(ACaptureSessionOutput_create(this->mWindows, &this->mSessionOutput)) != ACAMERA_OK) {
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
			if (this->checkReturnCameraStatus(ACameraDevice_createCaptureSession(this->mCameraDevice, this->mSessionOutputContainer, &this->mCaptureSessionCallback, &this->mCameraSession)) != ACAMERA_OK) {
				this->uninitSession();
				this->mCameraSession = nullptr;
				return;
			}
			this->mSessionStop = false;
			this->mSessionReady = true;
		}
	}

	void AndroidVideoCamera2::sessionReady() {
		this->lock();
		this->mSessionReady = true;
		if (this->mSessionStop) {
			this->uninitRequest();
			this->uninitSession();
			this->uninitWindow();
		}
		if (this->mSessionReset) {
			this->mSessionReset = false;
			this->initSession();
			this->initRequest();
		}
		this->unlock();
	}

	void AndroidVideoCamera2::sessionClosed() {
		this->lock();
		this->mCameraSession = nullptr;
		this->unlock();
	}

	void AndroidVideoCamera2::uninitSession() {
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

	void AndroidVideoCamera2::initRequest() {
		if (this->mCameraDevice && this->mWindows) {
			if (this->checkReturnCameraStatus(ACameraDevice_createCaptureRequest(this->mCameraDevice, TEMPLATE_ZERO_SHUTTER_LAG, &this->mCaptureRequest)) != ACAMERA_OK) {
				this->mCaptureRequest = nullptr;
				this->uninitRequest();
				return;
			}
			if (this->checkReturnCameraStatus(ACameraOutputTarget_create(this->mWindows, &this->mOutputTarget)) != ACAMERA_OK) {
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

	void AndroidVideoCamera2::uninitRequest() {
		if (this->mOutputTarget) {
			ACameraOutputTarget_free(this->mOutputTarget);
			this->mOutputTarget = nullptr;
		}
		if (this->mCaptureRequest) {
			ACaptureRequest_free(this->mCaptureRequest);
			this->mCaptureRequest = nullptr;
		}
	}

	camera_status_t AndroidVideoCamera2::checkReturnCameraStatus(camera_status_t status) {
		switch (status) {
			case ACAMERA_OK:
				return ACAMERA_OK;
			default:
			case ACAMERA_ERROR_UNKNOWN :
				ms_error("AndroidVideoCamera2: camera_status_t: Unknown error.");
				break;
			case ACAMERA_ERROR_INVALID_PARAMETER :
				ms_error("AndroidVideoCamera2: camera_status_t: Invalid paramter error.");
				break;
			case ACAMERA_ERROR_CAMERA_DISCONNECTED :
				ms_error("AndroidVideoCamera2: camera_status_t: Error, camera disconnected.");
				break;
			case ACAMERA_ERROR_NOT_ENOUGH_MEMORY :
				ms_error("AndroidVideoCamera2: camera_status_t: Error, not enough memory.");
				break;
			case ACAMERA_ERROR_METADATA_NOT_FOUND :
				ms_error("AndroidVideoCamera2: camera_status_t: Error, metadata not found.");
				break;
			case ACAMERA_ERROR_CAMERA_DEVICE :
				ms_error("AndroidVideoCamera2: camera_status_t: Error with camera device.");
				break;
			case ACAMERA_ERROR_CAMERA_SERVICE :
				ms_error("AndroidVideoCamera2: camera_status_t: Error with camera service.");
				break;
			case ACAMERA_ERROR_SESSION_CLOSED :
				ms_error("AndroidVideoCamera2: camera_status_t: Error, session closed.");
				break;
			case ACAMERA_ERROR_INVALID_OPERATION :
				ms_error("AndroidVideoCamera2: camera_status_t: Error, invalid operation.");
				break;
			case ACAMERA_ERROR_STREAM_CONFIGURE_FAIL :
				ms_error("AndroidVideoCamera2: camera_status_t: Error, stream configuration failed.");
				break;
			case ACAMERA_ERROR_CAMERA_IN_USE :
				ms_error("AndroidVideoCamera2: camera_status_t: Error, camera in use.");
				break;
			case ACAMERA_ERROR_MAX_CAMERA_IN_USE :
				ms_error("AndroidVideoCamera2: camera_status_t: Error, maximum number of cameras in use reached.");
				break;
			case ACAMERA_ERROR_CAMERA_DISABLED :
				ms_error("AndroidVideoCamera2: camera_status_t: Error, camera disabled.");
				break;
			case ACAMERA_ERROR_PERMISSION_DENIED :
				ms_error("AndroidVideoCamera2: camera_status_t: Error, camera permission denied.");
				break;
		}

		return ACAMERA_ERROR_BASE;
	}

	media_status_t AndroidVideoCamera2::checkReturnMediaStatus(media_status_t status) {
		switch (status) {
			case AMEDIA_OK:
				return AMEDIA_OK;
			case AMEDIA_ERROR_MALFORMED :
				ms_error("AndroidVideoCamera2: media_status_t: Error, media malformed.");
				break;
			case AMEDIA_ERROR_UNSUPPORTED :
				ms_error("AndroidVideoCamera2: media_status_t: Error, media unsupported.");
				break;
			case AMEDIA_ERROR_INVALID_OBJECT :
				ms_error("AndroidVideoCamera2: media_status_t: Error, invalid object.");
				break;
			case AMEDIA_ERROR_INVALID_PARAMETER :
				ms_error("AndroidVideoCamera2: media_status_t: Error, invalid parameter.");
				break;
			default:
			case AMEDIA_DRM_NOT_PROVISIONED :
			case AMEDIA_DRM_RESOURCE_BUSY :
			case AMEDIA_DRM_DEVICE_REVOKED :
			case AMEDIA_DRM_SHORT_BUFFER :
			case AMEDIA_DRM_SESSION_NOT_OPENED :
			case AMEDIA_DRM_TAMPER_DETECTED :
			case AMEDIA_DRM_VERIFY_FAILED :
			case AMEDIA_DRM_NEED_KEY :
			case AMEDIA_DRM_LICENSE_EXPIRED :
			case AMEDIA_ERROR_UNKNOWN :
				ms_error("AndroidVideoCamera2: media_status_t: Unknown error: %d.", status);
				break;
		}

		return AMEDIA_ERROR_BASE;
	}

	// Callbacks

	void AndroidVideoCamera2::onDisconnected(void* context, ACameraDevice* device) {
		ms_message("AndroidVideoCamera2: Camera %s disconnected.", ACameraDevice_getId(device));
	}

	void AndroidVideoCamera2::onError(void* context, ACameraDevice* device, int error) {
		// Stop capture if running etc...
	}

	void AndroidVideoCamera2::onSessionActive(void* context, ACameraCaptureSession *session) {
		ms_message("AndroidVideoCamera2: Starting capture requests process");
	}

	void AndroidVideoCamera2::onSessionReady(void* context, ACameraCaptureSession *session) {
		if (context) static_cast<AndroidVideoCamera2*>(context)->sessionReady();
	}

	void AndroidVideoCamera2::onSessionClosed(void* context, ACameraCaptureSession *session) {
		if (context) static_cast<AndroidVideoCamera2*>(context)->sessionClosed();
	}

	void AndroidVideoCamera2::onCaptureSequenceCompleted(void* context, ACameraCaptureSession* session, int sequenceId, int64_t frameNumber) {
		ms_message("AndroidVideoCamera2: COCOCOCOCOCOCOCOCO completed");
	}

	void AndroidVideoCamera2::onImageAvailable(void* context, AImageReader* reader) {
		if (context) static_cast<AndroidVideoCamera2*>(context)->setImage();
	}
}