/*
 *  androidvideo.cpp
 *
 *  mediastreamer2 library - modular sound and video processing and streaming
 *  This is the video capture filter for Android.
 *  It uses one of the JNI wrappers to access Android video capture API.
 *  See:
 *    org.linphone.mediastream.video.capture.AndroidVideoApi9JniWrapper
 *    org.linphone.mediastream.video.capture.AndroidVideoApi8JniWrapper
 *    org.linphone.mediastream.video.capture.AndroidVideoApi5JniWrapper
 *
 *  Copyright (C) 2010 - 2018  Belledonne Communications, Grenoble, France
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
#include "mediastreamer2/msticker.h"
#include "mediastreamer2/msvideo.h"
#include "mediastreamer2/mswebcam.h"

#include <jni.h>
#include <math.h>

static int android_sdk_version = 5;

static const char* AndroidApi9WrapperPath = "org/linphone/mediastream/video/capture/AndroidVideoApi9JniWrapper";
static const char* AndroidApi8WrapperPath = "org/linphone/mediastream/video/capture/AndroidVideoApi8JniWrapper";
static const char* AndroidApi5WrapperPath = "org/linphone/mediastream/video/capture/AndroidVideoApi5JniWrapper";
static const char* VersionPath 			  = "org/linphone/mediastream/Version";

/************************ Private helper methods       ************************/
static AndroidVideo::AndroidVideoAbstract* getAndroidVideoCamera(JNIEnv *env, MSFilter *f);
static jclass getHelperClassGlobalRef(JNIEnv *env);

/************************ MS2 filter methods           ************************/
static int video_capture_set_fps(MSFilter *f, void *arg) {
	return ((AndroidVideo::AndroidVideoAbstract*)f->data)->videoCaptureSetFps(arg);
}

static int video_capture_set_autofocus(MSFilter *f, void* arg) {
	return ((AndroidVideo::AndroidVideoAbstract*)f->data)->videoCaptureSetAutofocus(arg);
}

static int video_capture_get_fps(MSFilter *f, void *arg) {
	return ((AndroidVideo::AndroidVideoAbstract*)f->data)->videoCaptureGetFps(arg);
}

static int video_capture_set_vsize(MSFilter *f, void* arg) {
	return ((AndroidVideo::AndroidVideoAbstract*)f->data)->videoCaptureSetVsize(arg);
}

static int video_capture_get_vsize(MSFilter *f, void* arg){
	return ((AndroidVideo::AndroidVideoAbstract*)f->data)->videoCaptureGetVsize(arg);
}

static int video_capture_get_pix_fmt(MSFilter *f, void *arg){
	return ((AndroidVideo::AndroidVideoAbstract*)f->data)->videoCaptureGetPixFmt(arg);
}

static int video_set_native_preview_window(MSFilter *f, void *arg) {
	return ((AndroidVideo::AndroidVideoAbstract*)f->data)->videoSetNativePreviewWindow(arg);
}

static int video_get_native_preview_window(MSFilter *f, void *arg) {
	return ((AndroidVideo::AndroidVideoAbstract*)f->data)->videoGetNativePreviewWindow(arg);
}

static int video_set_device_rotation(MSFilter* f, void* arg) {
	return ((AndroidVideo::AndroidVideoAbstract*)f->data)->videoSetDeviceRotation(arg);
}

void video_capture_preprocess(MSFilter *f){
	((AndroidVideo::AndroidVideoAbstract*)f->data)->videoCapturePreprocess();
}

static void video_capture_process(MSFilter *f){
	AndroidReaderContext* d = getContext(f);

	ms_mutex_lock(&d->mutex);

	// If frame not ready, return
	if (d->frame == 0) {
		ms_mutex_unlock(&d->mutex);
		return;
	}

	ms_video_update_average_fps(&d->averageFps, f->ticker->time);

	ms_queue_put(f->outputs[0],d->frame);
	d->frame = 0;
	ms_mutex_unlock(&d->mutex);
}

static void video_capture_postprocess(MSFilter *f){
	ms_message("Postprocessing of Android VIDEO capture filter");
	AndroidReaderContext* d = getContext(f);
	JNIEnv *env = ms_get_jni_env();

	ms_mutex_lock(&d->mutex);

	if (d->androidCamera) {
		jmethodID method = env->GetStaticMethodID(d->helperClass,"stopRecording", "(Ljava/lang/Object;)V");

		env->CallStaticVoidMethod(d->helperClass, method, d->androidCamera);
		env->DeleteGlobalRef(d->androidCamera);
	}
	d->androidCamera = 0;
	d->previewWindow = 0;
	if (d->frame){
		freemsg(d->frame);
		d->frame=NULL;
	}
	ms_mutex_unlock(&d->mutex);
}

static void video_capture_init(MSFilter *f) {
	AndroidVideo::AndroidVideoAbstract *androidCamera = getAndroidVideoCamera(ms_get_jni_env(), f);
	ms_message("Init of Android VIDEO capture filter (%p)", androidCamera);
	androidCamera->videoCaptureInit();
}

static void video_capture_uninit(MSFilter *f) {
	ms_message("Uninit of Android VIDEO capture filter");
	AndroidVideo::AndroidVideoAbstract *androidCamera = (AndroidVideo::AndroidVideoAbstract*)f->data;
	delete androidCamera;
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
		NULL,
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

/* Webcam methods */
/*static void video_capture_detect(MSWebCamManager *obj);
static void video_capture_cam_init(MSWebCam *cam){
	ms_message("Android VIDEO capture filter cam init");
}

static MSFilter *video_capture_create_reader(MSWebCam *obj){
	ms_message("Instanciating Android VIDEO capture MS filter");

	MSFilter* lFilter = ms_factory_create_filter_from_desc(ms_web_cam_get_factory(obj), &ms_video_capture_desc);
	getContext(lFilter)->webcam = obj;

	return lFilter;
}

MSWebCamDesc ms_android_video_capture_desc={
		"AndroidVideoCapture",
		&video_capture_detect,
		&video_capture_cam_init,
		&video_capture_create_reader,
		NULL
};

static void video_capture_detect(MSWebCamManager *obj){
	ms_message("Detecting Android VIDEO cards");
	JNIEnv *env = ms_get_jni_env();
	jclass helperClass = getHelperClassGlobalRef(env);

	if (helperClass==NULL) return;

	// create 3 int arrays - assuming 2 webcams at most
	jintArray indexes = (jintArray)env->NewIntArray(2);
	jintArray frontFacing = (jintArray)env->NewIntArray(2);
	jintArray orientation = (jintArray)env->NewIntArray(2);

	jmethodID method = env->GetStaticMethodID(helperClass,"detectCameras", "([I[I[I)I");

	int count = env->CallStaticIntMethod(helperClass, method, indexes, frontFacing, orientation);

	ms_message("%d cards detected", count);
	for(int i=0; i<count; i++) {
		MSWebCam *cam = ms_web_cam_new(&ms_android_video_capture_desc);
		AndroidWebcamConfig* c = new AndroidWebcamConfig();
		env->GetIntArrayRegion(indexes, i, 1, &c->id);
		env->GetIntArrayRegion(frontFacing, i, 1, &c->frontFacing);
		env->GetIntArrayRegion(orientation, i, 1, &c->orientation);
		cam->data = c;
		cam->name = ms_strdup("Android video name");
		char* idstring = (char*) ms_malloc(15);
		snprintf(idstring, 15, "Android%d", c->id);
		cam->id = idstring;
		ms_web_cam_manager_add_cam(obj,cam);
		ms_message("camera created: id=%d frontFacing=%d orientation=%d [msid:%s]\n", c->id, c->frontFacing, c->orientation, idstring);
	}
	env->DeleteLocalRef(indexes);
	env->DeleteLocalRef(frontFacing);
	env->DeleteLocalRef(orientation);

	env->DeleteGlobalRef(helperClass);
	ms_message("Detection of Android VIDEO cards done");
}*/

/************************ JNI methods ************************/
#ifdef __cplusplus
extern "C" {
#endif

JNIEXPORT void JNICALL Java_org_linphone_mediastream_video_capture_AndroidVideoApi5JniWrapper_putImage(JNIEnv* env,
		jclass thiz, jlong nativePtr, jbyteArray frame) {
	AndroidVideo::AndroidVideoAbstract* androidVideo = (AndroidVideo::AndroidVideoAbstract*)nativePtr;
	androidVideo->putImage(frame);
}

#ifdef __cplusplus
}
#endif

static AndroidVideo::AndroidVideoAbstract* getAndroidVideoCamera(JNIEnv *env, MSFilter *f) {
	jclass version = env->FindClass(VersionPath);
	jmethodID method = env->GetStaticMethodID(version,"sdk", "()I");
	android_sdk_version = env->CallStaticIntMethod(version, method);
	ms_message("Android SDK version found is %i", android_sdk_version);
	env->DeleteLocalRef(version);

	// Android API less than 21 we use deprecated API camera
	//if (android_sdk_version < 21) {
		return new AndroidVideo::AndroidVideoCamera(f);
	//} else {
	//	return new AndroidVideo::AndroidVideoCamera2(f);
	//}
}

static jclass getHelperClassGlobalRef(JNIEnv *env) {
	ms_message("getHelperClassGlobalRef (env: %p)", env);
	const char* className;
	// FindClass only returns local references.

	// Find the current Android SDK version
	jclass version = env->FindClass(VersionPath);
	jmethodID method = env->GetStaticMethodID(version,"sdk", "()I");
	android_sdk_version = env->CallStaticIntMethod(version, method);
	ms_message("Android SDK version found is %i", android_sdk_version);
	env->DeleteLocalRef(version);

	if (android_sdk_version >= 9) {
		className = AndroidApi9WrapperPath;
	} else if (android_sdk_version >= 8) {
		className = AndroidApi8WrapperPath;
	} else {
		className = AndroidApi5WrapperPath;
	}
	jclass c = env->FindClass(className);
	if (c == 0) {
		ms_error("Could not load class '%s' (%d)", className, android_sdk_version);
		return NULL;
	} else {
		jclass globalRef = reinterpret_cast<jclass>(env->NewGlobalRef(c));
		env->DeleteLocalRef(c);
		return globalRef;
	}
}
