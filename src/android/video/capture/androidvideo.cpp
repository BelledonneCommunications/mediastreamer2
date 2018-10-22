/*
 *  androidvideo.cpp
 *
 *  mediastreamer2 library - modular sound and video processing and streaming
 *  This is the video capture filter for Android.
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

#include "androidvideo_abstract.h"
#include "androidvideo_camera.h"
#include "androidvideo_camera2.h"
#include "mediastreamer2/msfilter.h"
#include "mediastreamer2/msjava.h"
#include "mediastreamer2/msvideo.h"
#include "mediastreamer2/mswebcam.h"

#include <jni.h>
#include <math.h>

static const char* VersionPath = "org/linphone/mediastream/Version";

/************************ Private helper methods ************************/
static AndroidVideo::AndroidVideoAbstract* getAndroidVideoCamera(JNIEnv *env, MSFilter *f);

/************************ Helper methods ************************/
static int getAndroidSdkVersion(JNIEnv *env) {
	jclass version = env->FindClass(VersionPath);
	jmethodID method = env->GetStaticMethodID(version, "sdk", "()I");
	int android_sdk_version = env->CallStaticIntMethod(version, method);
	ms_message("AndroidVideo: Android SDK version found is %i", android_sdk_version);
	env->DeleteLocalRef(version);
	return android_sdk_version;
}

static AndroidVideo::AndroidVideoAbstract* getAndroidVideoCamera(JNIEnv *env, MSFilter *f) {
	int android_sdk_version = getAndroidSdkVersion(env);

	// Android API equal or sup 24 and API Camera2 is available we use this API
	if (android_sdk_version >= 24 && AndroidVideo::AndroidVideoAbstract::apiCamera2Available()) {
		return new AndroidVideo::AndroidVideoCamera2(f);
	} else {
		return new AndroidVideo::AndroidVideoCamera(f);
	}
}

/************************ MS2 filter methods ************************/
static int video_capture_set_fps(MSFilter *f, void *arg) {
	return static_cast<AndroidVideo::AndroidVideoAbstract*>(f->data)->videoCaptureSetFps(arg);
}

static int video_capture_set_autofocus(MSFilter *f, void* arg) {
	return static_cast<AndroidVideo::AndroidVideoAbstract*>(f->data)->videoCaptureSetAutofocus(arg);
}

static int video_capture_get_fps(MSFilter *f, void *arg) {
	return static_cast<AndroidVideo::AndroidVideoAbstract*>(f->data)->videoCaptureGetFps(arg);
}

static int video_capture_set_vsize(MSFilter *f, void* arg) {
	return static_cast<AndroidVideo::AndroidVideoAbstract*>(f->data)->videoCaptureSetVsize(arg);
}

static int video_capture_get_vsize(MSFilter *f, void* arg){
	return static_cast<AndroidVideo::AndroidVideoAbstract*>(f->data)->videoCaptureGetVsize(arg);
}

static int video_capture_get_pix_fmt(MSFilter *f, void *arg){
	return static_cast<AndroidVideo::AndroidVideoAbstract*>(f->data)->videoCaptureGetPixFmt(arg);
}

static int video_set_native_preview_window(MSFilter *f, void *arg) {
	return static_cast<AndroidVideo::AndroidVideoAbstract*>(f->data)->videoSetNativePreviewWindow(arg);
}

static int video_get_native_preview_window(MSFilter *f, void *arg) {
	return static_cast<AndroidVideo::AndroidVideoAbstract*>(f->data)->videoGetNativePreviewWindow(arg);
}

static int video_set_device_rotation(MSFilter* f, void* arg) {
	return static_cast<AndroidVideo::AndroidVideoAbstract*>(f->data)->videoSetDeviceRotation(arg);
}

void video_capture_preprocess(MSFilter *f) {
	static_cast<AndroidVideo::AndroidVideoAbstract*>(f->data)->videoCapturePreprocess();
}

static void video_capture_process(MSFilter *f) {
	static_cast<AndroidVideo::AndroidVideoAbstract*>(f->data)->videoCaptureProcess();
}

static void video_capture_postprocess(MSFilter *f){
	static_cast<AndroidVideo::AndroidVideoAbstract*>(f->data)->videoCapturePostprocess();
}

static void video_capture_init(MSFilter *f) {
	AndroidVideo::AndroidVideoAbstract *androidCamera = getAndroidVideoCamera(ms_get_jni_env(), f);
	ms_message("AndroidVideo: Init of Android VIDEO capture filter (%p)", androidCamera);
	androidCamera->videoCaptureInit();
}

static void video_capture_uninit(MSFilter *f) {
	ms_message("AndroidVideo: Uninit of Android VIDEO capture filter");
	AndroidVideo::AndroidVideoAbstract *androidCamera = static_cast<AndroidVideo::AndroidVideoAbstract*>(f->data);
	androidCamera->videoCaptureUninit();
	delete androidCamera;
	f->data = nullptr;
}

static MSFilterMethod video_capture_methods[]={
		{	MS_FILTER_SET_FPS,	&video_capture_set_fps},
		{	MS_FILTER_GET_FPS,	&video_capture_get_fps},
		{	MS_FILTER_SET_VIDEO_SIZE, &video_capture_set_vsize},
		{	MS_FILTER_GET_VIDEO_SIZE, &video_capture_get_vsize},
		{	MS_FILTER_GET_PIX_FMT, &video_capture_get_pix_fmt},
		{	MS_VIDEO_DISPLAY_SET_NATIVE_WINDOW_ID , &video_set_native_preview_window },//preview is managed by capture filter
		{	MS_VIDEO_DISPLAY_GET_NATIVE_WINDOW_ID , &video_get_native_preview_window },
		{	MS_VIDEO_CAPTURE_SET_DEVICE_ORIENTATION, &video_set_device_rotation },
		{	MS_VIDEO_CAPTURE_SET_AUTOFOCUS, &video_capture_set_autofocus },
		{	0,0 }
};

MSFilterDesc ms_video_capture_desc={
		MS_ANDROID_VIDEO_READ_ID,
		"MSAndroidVideoCapture",
		N_("A filter that captures Android video."),
		MS_FILTER_OTHER,
		nullptr,
		0,
		1,
		video_capture_init,
		video_capture_preprocess,
		video_capture_process,
		video_capture_postprocess,
		video_capture_uninit,
		video_capture_methods
};

MS_FILTER_DESC_EXPORT(ms_video_capture_desc)

/************************ Webcam methods ************************/
static void video_capture_cam_init(MSWebCam *cam) {
	ms_message("AndroidVideo: Android VIDEO capture filter cam init");
}

static MSFilter *video_capture_create_reader(MSWebCam *obj) {
	ms_message("AndroidVideo: Instanciating Android VIDEO capture MS filter");

	MSFilter* lFilter = ms_factory_create_filter_from_desc(ms_web_cam_get_factory(obj), &ms_video_capture_desc);
	static_cast<AndroidVideo::AndroidVideoAbstract*>(lFilter->data)->setWebcam(obj);

	return lFilter;
}

static void video_capture_detect(MSWebCamManager *obj) {
	if (getAndroidSdkVersion(ms_get_jni_env()) >= 24) {
		AndroidVideo::AndroidVideoAbstract::camera2Detect(obj);
	} else {
		AndroidVideo::AndroidVideoAbstract::cameraDetect(obj);
	}
}

MSWebCamDesc ms_android_video_capture_desc = {
		"AndroidVideoCapture",
		&video_capture_detect,
		&video_capture_cam_init,
		&video_capture_create_reader,
		nullptr
};

/************************ JNI methods ************************/
extern "C" {
	JNIEXPORT void JNICALL Java_org_linphone_mediastream_video_capture_AndroidVideoJniWrapper_putImage(JNIEnv* env,
			jclass thiz, jlong nativePtr, jbyteArray frame) {
		AndroidVideo::AndroidVideoAbstract* androidVideo = reinterpret_cast<AndroidVideo::AndroidVideoAbstract*>(nativePtr);
		androidVideo->putImage(frame);
	}
}
