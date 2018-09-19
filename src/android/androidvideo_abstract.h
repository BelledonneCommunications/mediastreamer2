/*
 *  androidvideo_abstract.h
 *
 *  mediastreamer2 library - modular sound and video processing and streaming
 *  This is this and abstract video capture filter for Android.
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

#ifndef androidvideo_abstract_include
#define androidvideo_abstract_include

#include <jni.h>

#include "mediastreamer2/msjava.h"
#include "mediastreamer2/msfilter.h"
#include "mediastreamer2/msticker.h"
#include "mediastreamer2/msvideo.h"
#include "mediastreamer2/mswebcam.h"

//#ifdef API_ANDROID >= 24
#include <camera/NdkCameraManager.h>
#include <camera/NdkCameraDevice.h>
//#endif

extern MSWebCamDesc ms_android_video_capture_desc;

namespace AndroidVideo {

static const int UNDEFINED_ROTATION = -1;
static const char* AndroidWrapperPath = "org/linphone/mediastream/video/capture/AndroidVideoJniWrapper";
struct AndroidWebcamConfig {
	int id;
	int frontFacing;
	int orientation;
	char idChar[64] = {0};
};

class AndroidVideoAbstract {
protected:
	int mRotation, mRotationSavedDuringVSize;
	int mUseDownscaling;
	char mFpsContext[64];
	float mFps;

	jobject mAndroidCamera;
	jobject mPreviewWindow;
	jclass mHelperClass;
	JNIEnv *mJavaEnv;

	mblk_t *mFrame;
	ms_mutex_t mMutex;

	MSAverageFPS mAverageFps;
	MSFilter *mFilter;
	MSFrameRateController mFpsControl;
	MSVideoSize mRequestedSize, mHwCapableSize, mUsedSize;
	MSWebCam *mWebcam;
	MSYuvBufAllocator *mAllocator;

public:
	AndroidVideoAbstract(MSFilter *f) : mRotation(UNDEFINED_ROTATION), mRotationSavedDuringVSize(UNDEFINED_ROTATION), mFps(5),
										mAndroidCamera(nullptr), mPreviewWindow(nullptr), mFrame(nullptr), mFilter(f), mWebcam(nullptr) {
		ms_debug("Creating AndroidVideoAbstract for Android VIDEO capture filter");
		ms_mutex_init(&this->mMutex, nullptr);
		this->mJavaEnv = ms_get_jni_env();
		this->mAllocator = ms_yuv_buf_allocator_new();
		if (!this->mHelperClass) {
			this->mHelperClass = getHelperClassGlobalRef(this->mJavaEnv);
		}

		snprintf(this->mFpsContext, sizeof(this->mFpsContext), "Captured mean fps=%%f");
		f->data = this;
	};

	virtual ~AndroidVideoAbstract() {
		ms_debug("Deleting AndroidVideoAbstract");
		if (this->mFrame) {
			freeb(this->mFrame);
			this->mFrame = nullptr;
		}
		/*if (this->mHelperClass && this->mJavaEnv) {
			this->mJavaEnv->DeleteGlobalRef(this->mHelperClass);
			this->mHelperClass = nullptr;
		}*/
		ms_yuv_buf_allocator_free(this->mAllocator);
		ms_mutex_destroy(&this->mMutex);
	};

	// Static methods
	static jclass getHelperClassGlobalRef(JNIEnv *env) {
		ms_debug("getHelperClassGlobalRef (env: %p)", env);
		const char* className = AndroidWrapperPath;

		jclass c = env->FindClass(className);
		if (c == 0) {
			ms_error("Could not load class '%s'", className);
			return nullptr;
		} else {
			jclass globalRef = reinterpret_cast<jclass>(env->NewGlobalRef(c));
			env->DeleteLocalRef(c);
			return globalRef;
		}
	}

	/**
	 * Camera detect function for deprecated API Camera
	 */
	static void cameraDetect(MSWebCamManager *obj) {
		ms_message("CameraDetect: Detecting Android VIDEO cards");
		JNIEnv *env = ms_get_jni_env();
		jclass helperClass = getHelperClassGlobalRef(env);

		if (helperClass == nullptr) return;

		// create 3 int arrays - assuming 2 webcams at most
		jintArray indexes = env->NewIntArray(2);
		jintArray frontFacing = env->NewIntArray(2);
		jintArray orientation = env->NewIntArray(2);

		jmethodID method = env->GetStaticMethodID(helperClass,"detectCameras", "([I[I[I)I");

		int count = env->CallStaticIntMethod(helperClass, method, indexes, frontFacing, orientation);

		ms_message("%d cards detected", count);
		for(int i=0; i<count; i++) {
			MSWebCam *cam = ms_web_cam_new(&ms_android_video_capture_desc);
			AndroidVideo::AndroidWebcamConfig* c = new AndroidVideo::AndroidWebcamConfig();
			env->GetIntArrayRegion(indexes, i, 1, &c->id);
			env->GetIntArrayRegion(frontFacing, i, 1, &c->frontFacing);
			env->GetIntArrayRegion(orientation, i, 1, &c->orientation);
			cam->data = c;
			cam->name = ms_strdup("Android video name");
			char* idstring = static_cast<char*>(ms_malloc(15));
			snprintf(idstring, 15, "Android%d", c->id);
			cam->id = idstring;
			ms_web_cam_manager_add_cam(obj, cam);
			ms_message("CameraDetect: camera created: id=%d frontFacing=%d orientation=%d [msid:%s]\n",
				c->id, c->frontFacing, c->orientation, idstring);
		}
		env->DeleteLocalRef(indexes);
		env->DeleteLocalRef(frontFacing);
		env->DeleteLocalRef(orientation);

		env->DeleteGlobalRef(helperClass);
		ms_message("CameraDetect: Detection of Android VIDEO cards done");
	}

	/**
	 * Camera detect function for API Camera2
	 */
	static void camera2Detect(MSWebCamManager *obj) {
		//#ifdef API_ANDROID >= 24
		ms_message("Camera2Detect: Detecting Android VIDEO cards");
		ACameraIdList *cameraIds = nullptr;
		ACameraManager *cameraManager = ACameraManager_create();
		ACameraManager_getCameraIdList(cameraManager, &cameraIds);

		for (int i = 0 ; i < cameraIds->numCameras ; ++i) {
			ACameraMetadata* metadataObj;
			ACameraManager_getCameraCharacteristics(cameraManager, cameraIds->cameraIds[i], &metadataObj);

			ACameraMetadata_const_entry lensInfo = { 0 };
			ACameraMetadata_const_entry orientationInfo = { 0 };
			ACameraMetadata_getConstEntry(metadataObj, ACAMERA_LENS_FACING, &lensInfo);
			ACameraMetadata_getConstEntry(metadataObj, ACAMERA_SENSOR_ORIENTATION, &orientationInfo);

			auto facing = static_cast<acamera_metadata_enum_android_lens_facing_t>(lensInfo.data.u8[0]);
			int32_t orientation = orientationInfo.data.i32[0];

			MSWebCam *cam = ms_web_cam_new(&ms_android_video_capture_desc);
			AndroidVideo::AndroidWebcamConfig* conf = new AndroidVideo::AndroidWebcamConfig();
			conf->id = i;
			conf->frontFacing = (facing == ACAMERA_LENS_FACING_FRONT);
			conf->orientation = orientation;
			strcpy(conf->idChar, cameraIds->cameraIds[i]);
			cam->data = conf;
			cam->name = ms_strdup("Android video name");
			char* idstring = static_cast<char*>(ms_malloc(15));
			snprintf(idstring, 15, "Android%d", conf->id);
			cam->id = idstring;
			ms_web_cam_manager_add_cam(obj, cam);
			ms_message("Camera2Detect: camera created: id=%d frontFacing=%d orientation=%d [msid:%s]\n",
				conf->id, conf->frontFacing, conf->orientation, idstring);
		}
		ACameraManager_deleteCameraIdList(cameraIds);
		ACameraManager_delete(cameraManager);
		ms_message("Camera2Detect: Detection of Android VIDEO cards done");
		//#else
		//ms_error("Camera2Detect: API Camera2 not available on this device.");
		//#endif
	}

	static bool apiCamera2Available() {
		//#ifdef API_ANDROID >= 24
		ACameraIdList *cameraIds = nullptr;
		ACameraManager *cameraManager = ACameraManager_create();
		ACameraManager_getCameraIdList(cameraManager, &cameraIds);

		for (int i = 0 ; i < cameraIds->numCameras ; ++i) {
			ACameraMetadata* metadataObj;
			ACameraManager_getCameraCharacteristics(cameraManager, cameraIds->cameraIds[i], &metadataObj);
			ACameraMetadata_const_entry infoHardware = { 0 };
			ACameraMetadata_getConstEntry(metadataObj, ACAMERA_INFO_SUPPORTED_HARDWARE_LEVEL, &infoHardware);

			auto level = static_cast<acamera_metadata_enum_android_info_supported_hardware_level_t>(infoHardware.data.u8[0]);

			// Return directly if one camera is not compatible
			if (level != ACAMERA_INFO_SUPPORTED_HARDWARE_LEVEL_FULL &&
				level != ACAMERA_INFO_SUPPORTED_HARDWARE_LEVEL_3 &&
				level != ACAMERA_INFO_SUPPORTED_HARDWARE_LEVEL_LIMITED) {
				ACameraManager_deleteCameraIdList(cameraIds);
				ACameraManager_delete(cameraManager);
				ms_message("AndroidVideo: Camera2 API level full not available for this device.");
				return false;
			}
		}
		ms_message("AndroidVideo: Camera2 API level full detected on this device.");
		ACameraManager_deleteCameraIdList(cameraIds);
		ACameraManager_delete(cameraManager);
		return true;
		//#else
		//return false;
		//#endif
	}

	// Setters∕Getters
	void setWebcam(MSWebCam *webcam) {
		this->mWebcam = webcam;
	}

	// Filter methods
	virtual void videoCaptureInit() = 0;
	virtual void videoCapturePreprocess() = 0;
	virtual void videoCaptureProcess() = 0;
	virtual void videoCapturePostprocess() = 0;
	virtual void videoCaptureUninit() = 0;

	// Other methods

	virtual int videoCaptureSetFps(void *arg) {
		this->mFps = *static_cast<float*>(arg);
		snprintf(this->mFpsContext, sizeof(this->mFpsContext), "Captured mean fps=%%f, expected=%f", this->mFps);
		ms_video_init_framerate_controller(&this->mFpsControl, this->mFps);
		ms_video_init_average_fps(&this->mAverageFps, this->mFpsContext);
		return 0;
	};

	virtual int videoCaptureGetFps(void *arg) {
		*static_cast<float*>(arg) = ms_average_fps_get(&this->mAverageFps);
		return 0;
	};

	virtual int videoCaptureGetVsize(void *arg) {
		*static_cast<MSVideoSize*>(arg) = this->mUsedSize;
		return 0;
	};

	virtual int videoCaptureGetPixFmt(void *arg) {
		*static_cast<MSPixFmt*>(arg) = MS_YUV420P;
		return 0;
	};

	virtual int videoGetNativePreviewWindow(void *arg) {
		*static_cast<unsigned long *>(arg) = (unsigned long)this->mPreviewWindow;
		return 0;
	};

	virtual int videoSetDeviceRotation(void *arg) {
		this->mRotation = *static_cast<int*>(arg);
		ms_message("%s : %d\n", __FUNCTION__, this->mRotation);
		return 0;
	};

	virtual int videoCaptureSetVsize(void *arg) = 0;
	virtual int videoSetNativePreviewWindow(void *arg) = 0;
	virtual int videoCaptureSetAutofocus(void *arg) = 0;

	// JNI
	virtual void putImage(jbyteArray frame) = 0;

protected:
	AndroidVideoAbstract(const AndroidVideoAbstract&) = delete;
	AndroidVideoAbstract() = delete;

	virtual void lock() {
		ms_mutex_lock(&this->mMutex);
	};

	virtual void unlock() {
		ms_mutex_unlock(&this->mMutex);
	};

	// Tools
	int computeImageRotationCorrection() {
		int result;
		AndroidWebcamConfig* conf = static_cast<AndroidWebcamConfig*>(this->mWebcam->data);

		if (conf->frontFacing) {
			ms_debug("%s: %d + %d\n", __FUNCTION__, static_cast<AndroidWebcamConfig*>(this->mWebcam->data)->orientation, this->mRotation);
			result = static_cast<AndroidWebcamConfig*>(this->mWebcam->data)->orientation +  this->mRotation;
		} else {
			ms_debug("%s: %d - %d\n", __FUNCTION__, static_cast<AndroidWebcamConfig*>(this->mWebcam->data)->orientation,  this->mRotation);
			result = static_cast<AndroidWebcamConfig*>(this->mWebcam->data)->orientation -  this->mRotation;
		}
		while(result < 0)
			result += 360;
		return result % 360;
	};

	void computeCroppingOffsets(MSVideoSize outputSize, int* yoff, int* cbcroff) {
		// if hw <= out -> return
		if (this->mHwCapableSize.width * this->mHwCapableSize.height <= outputSize.width * outputSize.height) {
			*yoff = 0;
			*cbcroff = 0;
			return;
		}

		int halfDiffW = (this->mHwCapableSize.width - ((outputSize.width>outputSize.height)?outputSize.width:outputSize.height)) / 2;
		int halfDiffH = (this->mHwCapableSize.height - ((outputSize.width<outputSize.height)?outputSize.width:outputSize.height)) / 2;

		*yoff = this->mHwCapableSize.width * halfDiffH + halfDiffW;
		*cbcroff = this->mHwCapableSize.width * halfDiffH * 0.5 + halfDiffW;
	};

	void setVsizeHelper() {
		// always request landscape mode, orientation is handled later
		if (this->mRequestedSize.height > this->mRequestedSize.width) {
			int tmp = this->mRequestedSize.height;
			this->mRequestedSize.height = this->mRequestedSize.width;
			this->mRequestedSize.width = tmp;
		}

		int rqSize = this->mRequestedSize.width * this->mRequestedSize.height;
		int hwSize = this->mHwCapableSize.width * this->mHwCapableSize.height;
		double downscale = this->mUseDownscaling ? 0.5 : 1;

		// if hw supplies a smaller resolution, modify requested size accordingly
		if ((hwSize * downscale * downscale) < rqSize) {
			ms_message("Camera cannot produce requested resolution %dx%d, will supply smaller one: %dx%d\n",
				this->mRequestedSize.width, this->mRequestedSize.height, (int)(this->mHwCapableSize.width * downscale),
				(int)(this->mHwCapableSize.height * downscale));
			this->mUsedSize.width = (int) (this->mHwCapableSize.width * downscale);
			this->mUsedSize.height = (int) (this->mHwCapableSize.height * downscale);
		} else if ((hwSize * downscale * downscale) > rqSize) {
			ms_message("Camera cannot produce requested resolution %dx%d, will capture a bigger one (%dx%d) and crop it to match encoder requested resolution\n",
				this->mRequestedSize.width, this->mRequestedSize.height, (int)(this->mHwCapableSize.width * downscale),
				(int)(this->mHwCapableSize.height * downscale));
			this->mUsedSize.width = this->mRequestedSize.width;
			this->mUsedSize.height = this->mRequestedSize.height;
		} else {
			this->mUsedSize.width = this->mRequestedSize.width;
			this->mUsedSize.height = this->mRequestedSize.height;
		}

		// is phone held |_ to cam orientation ?
		if (this->mRotation == UNDEFINED_ROTATION || computeImageRotationCorrection() % 180 != 0) {
			if (this->mRotation == UNDEFINED_ROTATION) {
				ms_error("To produce a correct image, Mediastreamer MUST be aware of device's orientation BEFORE calling 'configure_video_source'\n");
				ms_warning("Capture filter do not know yet about device's orientation.\n"
					"Current assumption: device is held perpendicular to its webcam (ie: portrait mode for a phone)\n");
				this->mRotationSavedDuringVSize = 0;
			} else {
				this->mRotationSavedDuringVSize = this->mRotation;
			}
			bool camIsLandscape = this->mHwCapableSize.width > this->mHwCapableSize.height;
			bool useIsLandscape = this->mUsedSize.width > this->mUsedSize.height;

			// if both are landscape or both portrait, swap
			if (camIsLandscape == useIsLandscape) {
				int t = this->mUsedSize.width;
				this->mUsedSize.width = this->mUsedSize.height;
				this->mUsedSize.height = t;
				ms_message("Swapped resolution width and height to : %dx%d\n", this->mUsedSize.width, this->mUsedSize.height);
			}
		} else {
			this->mRotationSavedDuringVSize = this->mRotation;
		}
	}

};

}

#endif //androidvideo_abstract_include