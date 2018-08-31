/*
 *  androidvideo_camera.cpp
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

#include "androidvideo_camera.h"

namespace AndroidVideo {
	AndroidVideoCamera::AndroidVideoCamera(MSFilter *f) : AndroidVideoAbstract(f) {
		this->mJavaEnv = ms_get_jni_env();
		this->mHelperClass = getHelperClassGlobalRef(this->mJavaEnv);
	}

	void AndroidVideoCamera::videoCaptureInit() {
	}

	void AndroidVideoCamera::videoCapturePreprocess() {
		this->lock();

		ms_message("Preprocessing of Android VIDEO capture filter");

		ms_video_init_framerate_controller(&this->mFpsControl, this->mFps);
		ms_video_init_average_fps(&this->mAverageFps, this->mFpsContext);

		ms_message("Starting Android camera '%d' (rotation:%d)", ((AndroidWebcamConfig*)this->mWebcam->data)->id, this->mRotation);
		jobject cam = this->mJavaEnv->CallStaticObjectMethod(this->mHelperClass,
				this->mMethodStartRecording,
				((AndroidWebcamConfig*)this->mWebcam->data)->id,
				this->mHwCapableSize.width,
				this->mHwCapableSize.height,
				(jint)30,
				this->mRotationSavedDuringVSize,
				(jlong)this);
		this->mAndroidCamera = this->mJavaEnv->NewGlobalRef(cam);

		if (this->mPreviewWindow) {
			this->mJavaEnv->CallStaticVoidMethod(this->mHelperClass, this->mMethodSetPreviewDisplaySurface, this->mAndroidCamera, this->mPreviewWindow);
		}

		ms_message("Preprocessing of Android VIDEO capture filter done");

		this->unlock();
	}

	void AndroidVideoCamera::videoCaptureProcess() {
		this->lock();

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

	void AndroidVideoCamera::videoCapturePostprocess() {
		this->lock();

		ms_message("Postprocessing of Android VIDEO capture filter");

		if (this->mAndroidCamera) {
			this->mJavaEnv->CallStaticVoidMethod(this->mHelperClass, this->mMethodStopRecording, this->mAndroidCamera);
			this->mJavaEnv->DeleteGlobalRef(this->mAndroidCamera);
		}

		this->mAndroidCamera = 0;
		this->mPreviewWindow = 0;

		if (this->mFrame){
			freemsg(this->mFrame);
			this->mFrame = nullptr;
		}

		this->unlock();
	}

	void AndroidVideoCamera::videoCaptureUninit() {
	}

	void AndroidVideoCamera::putImage(jbyteArray frame) {
		this->lock();

		if (!this->mAndroidCamera) {
			this->unlock();
			return;
		}

		if (!ms_video_capture_new_frame(&this->mFpsControl, ms_ticker_get_time(this->mFilter->ticker))) {
			this->unlock();
			return;
		}

		if (this->mRotation != UNDEFINED_ROTATION && this->mRotationSavedDuringVSize != this->mRotation) {
			ms_warning("Rotation has changed (new value: %d) since vsize was run (old value: %d)."
						"Will produce inverted images. Use set_device_orientation() then update call.\n",
				this->mRotation, this->mRotationSavedDuringVSize);
		}

		int image_rotation_correction = this->computeImageRotationCorrection();

		jboolean isCopied;
		jbyte* jinternal_buff = this->mJavaEnv->GetByteArrayElements(frame, &isCopied);
		if (isCopied) {
			ms_warning("The video frame received from Java has been copied");
		}

		int y_cropping_offset=0, cbcr_cropping_offset=0;
		MSVideoSize targetSize;
		this->mUseDownscaling ? targetSize.width = this->mRequestedSize.width * 2 : targetSize.width = this->mRequestedSize.width;
		this->mUseDownscaling ? targetSize.height = this->mRequestedSize.height * 2 : targetSize.height = this->mRequestedSize.height;

		this->computeCroppingOffsets(targetSize, &y_cropping_offset, &cbcr_cropping_offset);

		int width = this->mHwCapableSize.width;
		int height = this->mHwCapableSize.height;

		uint8_t* y_src = (uint8_t*)(jinternal_buff + y_cropping_offset);
		uint8_t* cbcr_src = (uint8_t*) (jinternal_buff + width * height + cbcr_cropping_offset);


		/* Warning note: image_rotation_correction == 90 does not imply portrait mode !
		(incorrect function naming).
		It only implies one thing: image needs to rotated by that amount to be correctly
		displayed.
		*/
		mblk_t* yuv_block = copy_ycbcrbiplanar_to_true_yuv_with_rotation_and_down_scale_by_2(this->mAllocator, y_src
															, cbcr_src
															, image_rotation_correction
															, this->mUsedSize.width
															, this->mUsedSize.height
															, this->mHwCapableSize.width
															, this->mHwCapableSize.width
															, false
															, this->mUseDownscaling);
		if (yuv_block) {
			if (this->mFrame)
				freemsg(this->mFrame);
			this->mFrame = yuv_block;
		}
		this->unlock();

		// JNI_ABORT free the buffer without copying back the possible changes
		this->mJavaEnv->ReleaseByteArrayElements(frame, jinternal_buff, JNI_ABORT);
	}
}