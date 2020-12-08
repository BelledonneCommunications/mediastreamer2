/*
 * Copyright (c) 2010-2019 Belledonne Communications SARL.
 *
 * This file is part of mediastreamer2.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include "mediastreamer2/msvideo.h"
#include "mediastreamer2/msfilter.h"
#include "mediastreamer2/mswebcam.h"
#include "mediastreamer2/msjava.h"
#include "mediastreamer2/msticker.h"

#include <jni.h>
#include <math.h>

static int android_sdk_version = 5;

static const char* AndroidApi9WrapperPath = "org/linphone/mediastream/video/capture/AndroidVideoApi9JniWrapper";
static const char* AndroidApi8WrapperPath = "org/linphone/mediastream/video/capture/AndroidVideoApi8JniWrapper";
static const char* AndroidApi5WrapperPath = "org/linphone/mediastream/video/capture/AndroidVideoApi5JniWrapper";
static const char* VersionPath 			  = "org/linphone/mediastream/Version";

#define UNDEFINED_ROTATION -1

/************************ Data structures              ************************/
// Struct holding Android's cameras properties
struct AndroidWebcamConfig {
	int id;
	int frontFacing;
	int orientation;
};

struct AndroidReaderContext {
	AndroidReaderContext(MSFilter *f, MSWebCam *cam):filter(f), webcam(cam),frame(0),fps(5){
		ms_message("[Legacy Capture] Creating AndroidReaderContext for Android VIDEO capture filter");
		ms_mutex_init(&mutex,NULL);
		androidCamera = 0;
		previewWindow = 0;
		rotation = rotationSavedDuringVSize = UNDEFINED_ROTATION;
		allocator = ms_yuv_buf_allocator_new();
		snprintf(fps_context, sizeof(fps_context), "Captured mean fps=%%f");
	};

	~AndroidReaderContext(){
		if (frame != 0) {
			freeb(frame);
		}
		ms_yuv_buf_allocator_free(allocator);
		ms_mutex_destroy(&mutex);
	};

	MSFrameRateController fpsControl;
	MSAverageFPS averageFps;

	MSFilter *filter;
	MSWebCam *webcam;

	mblk_t *frame;
	float fps;
	MSVideoSize requestedSize, hwCapableSize, usedSize;
	ms_mutex_t mutex;
	int rotation, rotationSavedDuringVSize;
	int useDownscaling;
	char fps_context[64];
	MSYuvBufAllocator *allocator;

	jobject androidCamera;
	jobject previewWindow;
	jclass helperClass;
};

/************************ Private helper methods       ************************/
static jclass getHelperClassGlobalRef(JNIEnv *env);
static int compute_image_rotation_correction(AndroidReaderContext* d, int rotation);
static void compute_cropping_offsets(MSVideoSize hwSize, MSVideoSize outputSize, int* yoff, int* cbcroff);
static AndroidReaderContext *getContext(MSFilter *f);


/************************ MS2 filter methods           ************************/
static int video_capture_set_fps(MSFilter *f, void *arg){
	AndroidReaderContext* d = (AndroidReaderContext*) f->data;
	d->fps=*((float*)arg);
	snprintf(d->fps_context, sizeof(d->fps_context), "Captured mean fps=%%f, expected=%f", d->fps);
	ms_video_init_framerate_controller(&d->fpsControl, d->fps);
	ms_video_init_average_fps(&d->averageFps, d->fps_context);
	return 0;
}

static int video_capture_set_autofocus(MSFilter *f, void* data){
	JNIEnv *env = ms_get_jni_env();
	AndroidReaderContext* d = (AndroidReaderContext*) f->data;
	jmethodID method = env->GetStaticMethodID(d->helperClass,"activateAutoFocus", "(Ljava/lang/Object;)V");
	env->CallStaticObjectMethod(d->helperClass, method, d->androidCamera);

	return 0;
}

static int video_capture_get_fps(MSFilter *f, void *arg){
	AndroidReaderContext* d = (AndroidReaderContext*) f->data;
	*((float*)arg) = ms_average_fps_get(&d->averageFps);
	return 0;
}

static int video_capture_set_vsize(MSFilter *f, void* data){
	AndroidReaderContext* d = (AndroidReaderContext*) f->data;
	ms_mutex_lock(&d->mutex);

	d->requestedSize=*(MSVideoSize*)data;

	// always request landscape mode, orientation is handled later
	if (d->requestedSize.height > d->requestedSize.width) {
		int tmp = d->requestedSize.height;
		d->requestedSize.height = d->requestedSize.width;
		d->requestedSize.width = tmp;
	}

	JNIEnv *env = ms_get_jni_env();

	jmethodID method = env->GetStaticMethodID(d->helperClass,"selectNearestResolutionAvailable", "(III)[I");

	// find neareast hw-available resolution (using jni call);
	jobject resArray = env->CallStaticObjectMethod(d->helperClass, method, ((AndroidWebcamConfig*)d->webcam->data)->id, d->requestedSize.width, d->requestedSize.height);

	if (!resArray) {
		ms_mutex_unlock(&d->mutex);
		ms_error("[Legacy Capture] Failed to retrieve camera '%d' supported resolutions\n", ((AndroidWebcamConfig*)d->webcam->data)->id);
		return -1;
	}

	// handle result :
	//   - 0 : width
	//   - 1 : height
	//   - 2 : useDownscaling
	jint res[3];
	env->GetIntArrayRegion((jintArray)resArray, 0, 3, res);
	ms_message("[Legacy Capture] Camera selected resolution is: %dx%d (requested: %dx%d) with downscaling?%d\n", res[0], res[1], d->requestedSize.width, d->requestedSize.height, res[2]);
	d->hwCapableSize.width =  res[0];
	d->hwCapableSize.height = res[1];
	d->useDownscaling = res[2];

	int rqSize = d->requestedSize.width * d->requestedSize.height;
	int hwSize = d->hwCapableSize.width * d->hwCapableSize.height;
	double downscale = d->useDownscaling ? 0.5 : 1;

	// if hw supplies a smaller resolution, modify requested size accordingly
	if ((hwSize * downscale * downscale) < rqSize) {
		ms_message("[Legacy Capture] Camera cannot produce requested resolution %dx%d, will supply smaller one: %dx%d\n",
			d->requestedSize.width, d->requestedSize.height, (int) (res[0] * downscale), (int) (res[1]*downscale));
		d->usedSize.width = (int) (d->hwCapableSize.width * downscale);
		d->usedSize.height = (int) (d->hwCapableSize.height * downscale);
	} else if ((hwSize * downscale * downscale) > rqSize) {
		if (d->requestedSize.width > d->hwCapableSize.width || d->requestedSize.height > d->hwCapableSize.height) {
			ms_message("[Legacy Capture] Camera cannot produce requested resolution %dx%d, will capture a bigger one (%dx%d)\n",
				d->requestedSize.width, d->requestedSize.height, (int)(res[0] * downscale), (int)(res[1] * downscale));
			d->usedSize.width = d->requestedSize.width = d->hwCapableSize.width;
			d->usedSize.height = d->requestedSize.height = d->hwCapableSize.height;
		} else {
			ms_message("[Legacy Capture] Camera cannot produce requested resolution %dx%d, will capture a bigger one (%dx%d) and crop it to match encoder requested resolution\n",
				d->requestedSize.width, d->requestedSize.height, (int)(res[0] * downscale), (int)(res[1] * downscale));
			d->usedSize.width = d->requestedSize.width;
			d->usedSize.height = d->requestedSize.height;
		}
	} else {
		d->usedSize.width = d->requestedSize.width;
		d->usedSize.height = d->requestedSize.height;
	}

	// is phone held |_ to cam orientation ?
	if (d->rotation == UNDEFINED_ROTATION || compute_image_rotation_correction(d, d->rotation) % 180 != 0) {
		if (d->rotation == UNDEFINED_ROTATION) {
			ms_error("[Legacy Capture] To produce a correct image, Mediastreamer MUST be aware of device's orientation BEFORE calling 'configure_video_source'\n");
			ms_warning("[Legacy Capture] Capture filter do not know yet about device's orientation.\n"
				"Current assumption: device is held perpendicular to its webcam (ie: portrait mode for a phone)\n");
			d->rotationSavedDuringVSize = 0;
		} else {
			d->rotationSavedDuringVSize = d->rotation;
		}
		bool camIsLandscape = d->hwCapableSize.width > d->hwCapableSize.height;
		bool useIsLandscape = d->usedSize.width > d->usedSize.height;

		// if both are landscape or both portrait, swap
		if (camIsLandscape == useIsLandscape) {
			int t = d->usedSize.width;
			d->usedSize.width = d->usedSize.height;
			d->usedSize.height = t;
			ms_message("[Legacy Capture] Swapped resolution width and height to : %dx%d\n", d->usedSize.width, d->usedSize.height);
		}
	} else {
		d->rotationSavedDuringVSize = d->rotation;
	}

	if (d->usedSize.width != 0 && d->usedSize.height != 0) {
		ms_filter_notify(f, MS_CAMERA_PREVIEW_SIZE_CHANGED, &d->usedSize);
	}

	ms_mutex_unlock(&d->mutex);
	return 0;
}

static int video_capture_get_vsize(MSFilter *f, void* data){
	AndroidReaderContext* d = (AndroidReaderContext*) f->data;
	*(MSVideoSize*)data=d->usedSize;
	return 0;
}

static int video_capture_get_pix_fmt(MSFilter *f, void *data){
	*(MSPixFmt*)data=MS_YUV420P;
	return 0;
}

// Java will give us a pointer to capture preview surface.
static int video_set_native_preview_window(MSFilter *f, void *arg) {
	AndroidReaderContext* d = (AndroidReaderContext*) f->data;

	ms_mutex_lock(&d->mutex);

	jobject w = (jobject)*((unsigned long*)arg);

	if (w == d->previewWindow) {
		ms_mutex_unlock(&d->mutex);
		return 0;
	}

	JNIEnv *env = ms_get_jni_env();

	jmethodID method = env->GetStaticMethodID(d->helperClass,"setPreviewDisplaySurface", "(Ljava/lang/Object;Ljava/lang/Object;)V");

	if (d->androidCamera) {
		if (d->previewWindow == 0) {
			ms_message("[Legacy Capture] Preview capture window set for the 1st time (win: %p rotation:%d)\n", w, d->rotation);
		} else {
			ms_message("[Legacy Capture] Preview capture window changed (oldwin: %p newwin: %p rotation:%d)\n", d->previewWindow, w, d->rotation);

			env->CallStaticVoidMethod(d->helperClass,
						env->GetStaticMethodID(d->helperClass,"stopRecording", "(Ljava/lang/Object;)V"),
						d->androidCamera);
			env->DeleteGlobalRef(d->androidCamera);
			d->androidCamera = env->NewGlobalRef(
			env->CallStaticObjectMethod(d->helperClass,
						env->GetStaticMethodID(d->helperClass,"startRecording", "(IIIIIJ)Ljava/lang/Object;"),
						((AndroidWebcamConfig*)d->webcam->data)->id,
						d->hwCapableSize.width,
						d->hwCapableSize.height,
						(jint)30,
						(d->rotation != UNDEFINED_ROTATION) ? d->rotation:0,
						(jlong)d));
		}
		// if previewWindow AND camera are valid => set preview window
		if (w && d->androidCamera)
			env->CallStaticVoidMethod(d->helperClass, method, d->androidCamera, w);

		if (d->usedSize.width != 0 && d->usedSize.height != 0) {
			ms_filter_notify(f, MS_CAMERA_PREVIEW_SIZE_CHANGED, &d->usedSize);
		}
	} else {
		ms_message("[Legacy Capture] Preview capture window set but camera not created yet; remembering it for later use\n");
	}
	d->previewWindow = w;

	ms_mutex_unlock(&d->mutex);
	return 0;
}

static int video_get_native_preview_window(MSFilter *f, void *arg) {
	AndroidReaderContext* d = (AndroidReaderContext*) f->data;
	*((unsigned long *)arg) = (unsigned long)d->previewWindow;
	return 0;
}

static int video_set_device_rotation(MSFilter* f, void* arg) {
	AndroidReaderContext* d = (AndroidReaderContext*) f->data;
	d->rotation=*((int*)arg);
	ms_message("%s : %d\n", __FUNCTION__, d->rotation);
	return 0;
}

void video_capture_preprocess(MSFilter *f){
	ms_message("[Legacy Capture] Preprocessing of Android VIDEO capture filter");

	AndroidReaderContext *d = getContext(f);
	ms_mutex_lock(&d->mutex);

	ms_video_init_framerate_controller(&d->fpsControl, d->fps);
	ms_video_init_average_fps(&d->averageFps, d->fps_context);

	JNIEnv *env = ms_get_jni_env();

	jmethodID method = env->GetStaticMethodID(d->helperClass,"startRecording", "(IIIIIJ)Ljava/lang/Object;");

	ms_message("[Legacy Capture] Starting Android camera '%d' (rotation:%d)", ((AndroidWebcamConfig*)d->webcam->data)->id, d->rotation);
	jobject cam = env->CallStaticObjectMethod(d->helperClass, method,
			((AndroidWebcamConfig*)d->webcam->data)->id,
			d->hwCapableSize.width,
			d->hwCapableSize.height,
			(jint)30,
			d->rotationSavedDuringVSize,
			(jlong)d);
	d->androidCamera = env->NewGlobalRef(cam);

	if (d->previewWindow) {
		method = env->GetStaticMethodID(d->helperClass,"setPreviewDisplaySurface", "(Ljava/lang/Object;Ljava/lang/Object;)V");
		env->CallStaticVoidMethod(d->helperClass, method, d->androidCamera, d->previewWindow);
	}
	ms_message("[Legacy Capture] Preprocessing of Android VIDEO capture filter done");

	if (d->frame) {
		freemsg(d->frame);
		d->frame = NULL;
	}
	ms_mutex_unlock(&d->mutex);
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
	mblk_set_timestamp_info(d->frame, f->ticker->time * 90);

	ms_queue_put(f->outputs[0],d->frame);
	d->frame = 0;
	ms_mutex_unlock(&d->mutex);
}

static void video_capture_postprocess(MSFilter *f){
	ms_message("[Legacy Capture] Postprocessing of Android VIDEO capture filter");
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
	if (d->frame) {
		freemsg(d->frame);
		d->frame = NULL;
	}
	ms_mutex_unlock(&d->mutex);
}

static void video_capture_init(MSFilter *f) {
	AndroidReaderContext* d = new AndroidReaderContext(f, 0);
	ms_message("[Legacy Capture] Init of Android VIDEO capture filter (%p)", d);
	JNIEnv *env = ms_get_jni_env();
	d->helperClass = getHelperClassGlobalRef(env);
	f->data = d;
}

static void video_capture_uninit(MSFilter *f) {
	ms_message("[Legacy Capture] Uninit of Android VIDEO capture filter");
	AndroidReaderContext* d = getContext(f);
	JNIEnv *env = ms_get_jni_env();
	env->DeleteGlobalRef(d->helperClass);
	delete d;
}

static MSFilterMethod video_capture_methods[]={
		{	MS_FILTER_SET_FPS,	&video_capture_set_fps},
		{	MS_FILTER_GET_FPS,	&video_capture_get_fps},
		{	MS_FILTER_SET_VIDEO_SIZE, &video_capture_set_vsize},
		{	MS_FILTER_GET_VIDEO_SIZE, &video_capture_get_vsize},
		{	MS_FILTER_GET_PIX_FMT, &video_capture_get_pix_fmt},
		{	MS_VIDEO_DISPLAY_SET_NATIVE_WINDOW_ID , &video_set_native_preview_window },//preview is managed by capture filter
		{	MS_VIDEO_DISPLAY_GET_NATIVE_WINDOW_ID , &video_get_native_preview_window },
		{   MS_VIDEO_CAPTURE_SET_DEVICE_ORIENTATION, &video_set_device_rotation },
		{   MS_VIDEO_CAPTURE_SET_AUTOFOCUS, &video_capture_set_autofocus },
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
static void video_capture_detect(MSWebCamManager *obj);
static void video_capture_cam_init(MSWebCam *cam){
	ms_message("[Legacy Capture] Android VIDEO capture filter cam init");
}

static MSFilter *video_capture_create_reader(MSWebCam *obj){
	ms_message("[Legacy Capture] Instanciating Android VIDEO capture MS filter");

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

void android_video_capture_detect_cameras_legacy(MSWebCamManager *obj) {
	ms_message("[Legacy Capture] Detecting Android VIDEO cards");

	JNIEnv *env = ms_get_jni_env();
	jclass helperClass = getHelperClassGlobalRef(env);

	if (helperClass == NULL) return;

	jmethodID countMethod = env->GetStaticMethodID(helperClass,"detectCamerasCount", "()I");
	int count = env->CallStaticIntMethod(helperClass, countMethod);
	ms_message("[Legacy Capture] %d cards detected", count);

	jintArray indexes = (jintArray)env->NewIntArray(count);
	jintArray frontFacing = (jintArray)env->NewIntArray(count);
	jintArray orientation = (jintArray)env->NewIntArray(count);

	jmethodID method = env->GetStaticMethodID(helperClass,"detectCameras", "([I[I[I)I");
	env->CallStaticIntMethod(helperClass, method, indexes, frontFacing, orientation);

	bool frontFacingFound = false;
	bool backFacingFound = false;
	for (int i = 0; i < count; i++) {
		if (frontFacingFound && backFacingFound) {
			ms_message("[Legacy Capture] Found one front facing camera and one back facing camera, no need for more.");
			break;
		}

		MSWebCam *cam = ms_web_cam_new(&ms_android_video_capture_desc);
		AndroidWebcamConfig* c = new AndroidWebcamConfig();
		env->GetIntArrayRegion(indexes, i, 1, &c->id);
		env->GetIntArrayRegion(frontFacing, i, 1, &c->frontFacing);
		env->GetIntArrayRegion(orientation, i, 1, &c->orientation);
		cam->data = c;
		cam->name = ms_strdup("Android video name");

		const char *facing = "Back";
		if (c->frontFacing) {
			facing = "Front";
			frontFacingFound = true;
		} else {
			backFacingFound = true;
		}

		char *idstring = bctbx_strdup_printf("%sFacingCamera%d", facing, c->id);
		cam->id = ms_strdup(idstring);
		bctbx_free(idstring);

		if (ms_get_android_sdk_version() >= 26) {
			ms_warning("[Legacy Capture] Compatibility mode detected, prepending camera to prevent NoWebcam to be the first one");
			ms_web_cam_manager_prepend_cam(obj, cam); // Otherwise NoWebcam will be the first one...
		} else {
			ms_web_cam_manager_add_cam(obj, cam);
		}
		ms_message("[Legacy Capture] camera created: id=%d frontFacing=%d orientation=%d [msid:%s]\n", c->id, c->frontFacing, c->orientation, cam->id);
	}
	env->DeleteLocalRef(indexes);
	env->DeleteLocalRef(frontFacing);
	env->DeleteLocalRef(orientation);

	env->DeleteGlobalRef(helperClass);
	ms_message("[Legacy Capture] Detection of Android VIDEO cards done");
}

static void video_capture_detect(MSWebCamManager *obj) {
	bool msAndroidCamera2PluginFound = false;
	const char *plugin_to_find = "libmsandroidcamera2.so";
	bctbx_list_t *plugins_list = ms_get_android_plugins_list();
	bctbx_list_t *it = plugins_list;
	if (it != NULL) {
		for (it = plugins_list; it != NULL; it = bctbx_list_next(it)) {
			const char *plugin_name = (const char*)bctbx_list_get_data(it);
			if (plugin_name && strcmp(plugin_name, plugin_to_find) == 0) {
				msAndroidCamera2PluginFound = true;
				break;
			}
		}
		ms_list_free_with_data(plugins_list, ms_free);
	}

	if (ms_get_android_sdk_version() >= 26 && msAndroidCamera2PluginFound) {
		ms_warning("[Legacy Capture] Android >= 8.0 detected and msAndroidCamera2 plugin found, disabling legacy capture filter");
		return;
	}

	android_video_capture_detect_cameras_legacy(obj);
}

/************************ JNI methods                  ************************/
#ifdef __cplusplus
extern "C" {
#endif

JNIEXPORT void JNICALL Java_org_linphone_mediastream_video_capture_AndroidVideoApi5JniWrapper_putImage(JNIEnv*  env,
		jclass  thiz,jlong nativePtr,jbyteArray frame) {
	AndroidReaderContext* d = (AndroidReaderContext*) nativePtr;

	ms_mutex_lock(&d->mutex);

	if (!d->androidCamera){
		ms_mutex_unlock(&d->mutex);
		return;
	}

	if (!ms_video_capture_new_frame(&d->fpsControl,d->filter->ticker->time)) {
		ms_mutex_unlock(&d->mutex);
		return;
	}

	if (d->rotation != UNDEFINED_ROTATION && d->rotationSavedDuringVSize != d->rotation) {
		ms_warning("[Legacy Capture] Rotation has changed (new value: %d) since vsize was run (old value: %d)."
					"Will produce inverted images. Use set_device_orientation() then update call.\n",
			d->rotation, d->rotationSavedDuringVSize);
	}

	int image_rotation_correction = compute_image_rotation_correction(d, d->rotationSavedDuringVSize);

	MSVideoSize used_size = d->usedSize;
	MSVideoSize capable_size = d->hwCapableSize;
	MSVideoSize requested_size = d->requestedSize;
	int use_downscale = d->useDownscaling;

	ms_mutex_unlock(&d->mutex);

	jboolean isCopied;
	jbyte* jinternal_buff = env->GetByteArrayElements(frame, &isCopied);
	if (isCopied) {
		ms_warning("[Legacy Capture] The video frame received from Java has been copied");
	}

	int y_cropping_offset=0, cbcr_cropping_offset=0;
	MSVideoSize targetSize;
	use_downscale?targetSize.width=requested_size.width*2:targetSize.width=requested_size.width;
	use_downscale?targetSize.height=requested_size.height*2:targetSize.height=requested_size.height;

	compute_cropping_offsets(capable_size, targetSize, &y_cropping_offset, &cbcr_cropping_offset);

	int width = capable_size.width;
	int height = capable_size.height;

	uint8_t* y_src = (uint8_t*)(jinternal_buff + y_cropping_offset);
	uint8_t* cbcr_src = (uint8_t*) (jinternal_buff + width * height + cbcr_cropping_offset);


	/* Warning note: image_rotation_correction == 90 does not imply portrait mode !
	   (incorrect function naming).
	   It only implies one thing: image needs to rotated by that amount to be correctly
	   displayed.
	*/
 	mblk_t* yuv_block = copy_ycbcrbiplanar_to_true_yuv_with_rotation_and_down_scale_by_2(d->allocator, y_src
														, cbcr_src
														, image_rotation_correction
														, used_size.width
														, used_size.height
														, capable_size.width
														, capable_size.width
														, false
														, use_downscale);

	ms_mutex_lock(&d->mutex);

	if (yuv_block) {
		if (d->frame)
			freemsg(d->frame);
		d->frame = yuv_block;
	}

	ms_mutex_unlock(&d->mutex);

	// JNI_ABORT free the buffer without copying back the possible changes
	env->ReleaseByteArrayElements(frame, jinternal_buff, JNI_ABORT);
}

#ifdef __cplusplus
}
#endif

static int compute_image_rotation_correction(AndroidReaderContext* d, int rotation) {
	AndroidWebcamConfig* conf = (AndroidWebcamConfig*)(AndroidWebcamConfig*)d->webcam->data;

	int result;
	if (conf->frontFacing) {
		ms_debug("[Legacy Capture] %s: %d + %d\n", __FUNCTION__, ((AndroidWebcamConfig*)d->webcam->data)->orientation, rotation);
	 	result = ((AndroidWebcamConfig*)d->webcam->data)->orientation + rotation;
	} else {
		ms_debug("[Legacy Capture] %s: %d - %d\n", __FUNCTION__, ((AndroidWebcamConfig*)d->webcam->data)->orientation, rotation);
	 	result = ((AndroidWebcamConfig*)d->webcam->data)->orientation - rotation;
	}
	while(result < 0)
		result += 360;
	return result % 360;
}


static void compute_cropping_offsets(MSVideoSize hwSize, MSVideoSize outputSize, int* yoff, int* cbcroff) {
	// if hw <= out -> return
	if (hwSize.width * hwSize.height <= outputSize.width * outputSize.height) {
		*yoff = 0;
		*cbcroff = 0;
		return;
	}

	int halfDiffW = (hwSize.width - ((outputSize.width>outputSize.height)?outputSize.width:outputSize.height)) / 2;
	int halfDiffH = (hwSize.height - ((outputSize.width<outputSize.height)?outputSize.width:outputSize.height)) / 2;

	*yoff = hwSize.width * halfDiffH + halfDiffW;
	*cbcroff = hwSize.width * halfDiffH * 0.5 + halfDiffW;
}


static jclass getHelperClassGlobalRef(JNIEnv *env) {
	ms_message("[Legacy Capture] getHelperClassGlobalRef (env: %p)", env);
	const char* className;
	// FindClass only returns local references.

	// Find the current Android SDK version
	jclass version = env->FindClass(VersionPath);
	jmethodID method = env->GetStaticMethodID(version,"sdk", "()I");
	android_sdk_version = env->CallStaticIntMethod(version, method);
	ms_message("[Legacy Capture] Android SDK version found is %i", android_sdk_version);
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
		ms_error("[Legacy Capture] Could not load class '%s' (%d)", className, android_sdk_version);
		return NULL;
	} else {
		jclass globalRef = reinterpret_cast<jclass>(env->NewGlobalRef(c));
		env->DeleteLocalRef(c);
		return globalRef;
	}
}

static AndroidReaderContext *getContext(MSFilter *f) {
	return (AndroidReaderContext*) f->data;
}
