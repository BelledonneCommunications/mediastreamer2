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

namespace AndroidVideo {

const int UNDEFINED_ROTATION = -1;

struct AndroidWebcamConfig {
	int id;
	int frontFacing;
	int orientation;
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
										mAndroidCamera(0), mPreviewWindow(0), mFrame(nullptr), mFilter(f), mWebcam(nullptr) {
		ms_message("Creating AndroidVideoAbstract for Android VIDEO capture filter");
		ms_mutex_init(&mMutex, nullptr);
		mJavaEnv = ms_get_jni_env();
		mAllocator = ms_yuv_buf_allocator_new();
		snprintf(mFpsContext, sizeof(mFpsContext), "Captured mean fps=%%f");
		f->data = this;
	};

	virtual ~AndroidVideoAbstract() {
		ms_message("Deleting AndroidVideoAbstract");
		if (mFrame != nullptr) {
			freeb(mFrame);
		}
		ms_yuv_buf_allocator_free(mAllocator);
		ms_mutex_destroy(&mMutex);
	};

	// Filter methods
	virtual void videoCaptureInit() = 0;
	virtual void videoCapturePreprocess() = 0;
	virtual void videoCaptureProcess() = 0;
	virtual void videoCapturePostprocess() = 0;
	virtual void videoCaptureUninit() = 0;

	// Other methods
	virtual int videoCaptureSetFps(void *arg) {
		this->mFps = *((float*)arg);
		snprintf(this->mFpsContext, sizeof(this->mFpsContext), "Captured mean fps=%%f, expected=%f", this->mFps);
		ms_video_init_framerate_controller(&this->mFpsControl, this->mFps);
		ms_video_init_average_fps(&this->mAverageFps, this->mFpsContext);
		return 0;
	};

	virtual int videoCaptureGetFps(void *arg) {
		*((float*)arg) = ms_average_fps_get(&this->mAverageFps);
		return 0;
	};

	virtual int videoCaptureSetVsize(void *arg) {
		lock();

		this->mRequestedSize = *(MSVideoSize*)arg;

		// always request landscape mode, orientation is handled later
		if (this->mRequestedSize.height > this->mRequestedSize.width) {
			int tmp = this->mRequestedSize.height;
			this->mRequestedSize.height = this->mRequestedSize.width;
			this->mRequestedSize.width = tmp;
		}

		jmethodID method = this->mJavaEnv->GetStaticMethodID(this->mHelperClass,"selectNearestResolutionAvailable", "(III)[I");

		// find neareast hw-available resolution (using jni call);
		jobject resArray = this->mJavaEnv->CallStaticObjectMethod(this->mHelperClass, method, ((AndroidWebcamConfig*)this->mWebcam->data)->id, this->mRequestedSize.width, this->mRequestedSize.height);

		if (!resArray) {
			ms_mutex_unlock(&this->mMutex);
			ms_error("Failed to retrieve camera '%d' supported resolutions\n", ((AndroidWebcamConfig*)this->mWebcam->data)->id);
			return -1;
		}

		// handle result :
		//   - 0 : width
		//   - 1 : height
		//   - 2 : useDownscaling
		jint res[3];
		this->mJavaEnv->GetIntArrayRegion((jintArray)resArray, 0, 3, res);
		ms_message("Camera selected resolution is: %dx%d (requested: %dx%d) with downscaling?%d\n", res[0], res[1], this->mRequestedSize.width, this->mRequestedSize.height, res[2]);
		this->mHwCapableSize.width =  res[0];
		this->mHwCapableSize.height = res[1];
		this->mUseDownscaling = res[2];

		int rqSize = this->mRequestedSize.width * this->mRequestedSize.height;
		int hwSize = this->mHwCapableSize.width * this->mHwCapableSize.height;
		double downscale = this->mUseDownscaling ? 0.5 : 1;

		// if hw supplies a smaller resolution, modify requested size accordingly
		if ((hwSize * downscale * downscale) < rqSize) {
			ms_message("Camera cannot produce requested resolution %dx%d, will supply smaller one: %dx%d\n",
				this->mRequestedSize.width, this->mRequestedSize.height, (int) (res[0] * downscale), (int) (res[1]*downscale));
			this->mUsedSize.width = (int) (this->mHwCapableSize.width * downscale);
			this->mUsedSize.height = (int) (this->mHwCapableSize.height * downscale);
		} else if ((hwSize * downscale * downscale) > rqSize) {
			ms_message("Camera cannot produce requested resolution %dx%d, will capture a bigger one (%dx%d) and crop it to match encoder requested resolution\n",
				this->mRequestedSize.width, this->mRequestedSize.height, (int)(res[0] * downscale), (int)(res[1] * downscale));
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

		unlock();
		return 0;
	};

	virtual int videoCaptureGetVsize(void *arg) {
		*(MSVideoSize*)arg = this->mUsedSize;
		return 0;
	};

	virtual int videoCaptureGetPixFmt(void *arg) {
		*(MSPixFmt*)arg=MS_YUV420P;
		return 0;
	};

	virtual int videoSetNativePreviewWindow(void *arg) {
		lock();

		jobject w = (jobject)*((unsigned long*)arg);

		if (w == this->mPreviewWindow) {
			unlock();

			return 0;
		}

		jmethodID method = this->mJavaEnv->GetStaticMethodID(this->mHelperClass,"setPreviewDisplaySurface", "(Ljava/lang/Object;Ljava/lang/Object;)V");

		if (this->mAndroidCamera) {
			if (this->mPreviewWindow == 0) {
				ms_message("Preview capture window set for the 1st time (win: %p rotation:%d)\n", w, this->mRotation);
			} else {
				ms_message("Preview capture window changed (oldwin: %p newwin: %p rotation:%d)\n", this->mPreviewWindow, w, this->mRotation);

				this->mJavaEnv->CallStaticVoidMethod(this->mHelperClass,
							this->mJavaEnv->GetStaticMethodID(this->mHelperClass,"stopRecording", "(Ljava/lang/Object;)V"),
							this->mAndroidCamera);
				this->mJavaEnv->DeleteGlobalRef(this->mAndroidCamera);
				/*this->mAndroidCamera = this->mJavaEnv->NewGlobalRef(
				this->mJavaEnv->CallStaticObjectMethod(this->mHelperClass,
							this->mJavaEnv->GetStaticMethodID(this->mHelperClass,"startRecording", "(IIIIIJ)Ljava/lang/Object;"),
							((AndroidWebcamConfig*)this->mWebcam->data)->id,
							this->mHwCapableSize.width,
							this->mHwCapableSize.height,
							(jint)30,
							(this->mRotation != UNDEFINED_ROTATION) ? this->mRotation:0,
							(jlong)&this));*/ // TODO
			}
			// if previewWindow AND camera are valid => set preview window
			if (w && this->mAndroidCamera)
				this->mJavaEnv->CallStaticVoidMethod(this->mHelperClass, method, this->mAndroidCamera, w);
		} else {
			ms_message("Preview capture window set but camera not created yet; remembering it for later use\n");
		}
		this->mPreviewWindow = w;

		unlock();

		return 0;
	};

	virtual int videoGetNativePreviewWindow(void *arg) {
		*((unsigned long *)arg) = (unsigned long)this->mPreviewWindow;

		return 0;
	};

	virtual int videoSetDeviceRotation(void *arg) {
		this->mRotation=*((int*)arg);
		ms_message("%s : %d\n", __FUNCTION__, this->mRotation);

		return 0;
	};

	virtual int videoCaptureSetAutofocus(void *arg) {
		jmethodID method = this->mJavaEnv->GetStaticMethodID(this->mHelperClass,"activateAutoFocus", "(Ljava/lang/Object;)V");
		this->mJavaEnv->CallStaticObjectMethod(this->mHelperClass, method, this->mAndroidCamera);

		return 0;
	};

	// JNI
	virtual void putImage(jbyteArray frame) = 0;

protected:
	AndroidVideoAbstract(const AndroidVideoAbstract&) = delete;
	AndroidVideoAbstract() = delete;

	void lock() {
		ms_mutex_lock(&this->mMutex);
	};

	void unlock() {
		ms_mutex_unlock(&this->mMutex);
	};

	// Tools
	int computeImageRotationCorrection() {
		int result;
		AndroidWebcamConfig* conf = (AndroidWebcamConfig*)this->mWebcam->data;

		if (conf->frontFacing) {
			ms_debug("%s: %d + %d\n", __FUNCTION__, ((AndroidWebcamConfig*)this->mWebcam->data)->orientation, this->mRotation);
			result = ((AndroidWebcamConfig*)this->mWebcam->data)->orientation +  this->mRotation;
		} else {
			ms_debug("%s: %d - %d\n", __FUNCTION__, ((AndroidWebcamConfig*)this->mWebcam->data)->orientation,  this->mRotation);
			result = ((AndroidWebcamConfig*)this->mWebcam->data)->orientation -  this->mRotation;
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
};

}

#endif //androidvideo_abstract_include