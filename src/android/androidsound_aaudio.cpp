/*
 * androidsound_aaudio.cpp - Android Media plugin for Linphone, based on AAudio APIs.
 *
 * Copyright (C) 2017  Belledonne Communications, Grenoble, France
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

#include <mediastreamer2/msfilter.h>
#include <mediastreamer2/msjava.h>
#include <mediastreamer2/msticker.h>
#include <mediastreamer2/mssndcard.h>
#include <mediastreamer2/devices.h>

#include <sys/types.h>
#include <string.h>
#include <jni.h>
#include <dlfcn.h>

#include <aaudio/AAudio.h>

static const int flowControlIntervalMs = 5000;
static const int flowControlThresholdMs = 40;

static int DeviceFavoriteSampleRate = 44100;

#ifdef __cplusplus
extern "C" {
#endif
JNIEXPORT void JNICALL Java_org_linphone_mediastream_MediastreamerAndroidContext_setDeviceFavoriteSampleRate(JNIEnv* env, jclass thiz, jint samplerate) {
	DeviceFavoriteSampleRate = (int)samplerate;
}
JNIEXPORT void JNICALL Java_org_linphone_mediastream_MediastreamerAndroidContext_setDeviceFavoriteBufferSize(JNIEnv* env, jclass thiz, jint buffersize) {
	// Unused
}
#ifdef __cplusplus
}
#endif

static MSSndCard *android_snd_card_new(MSSndCardManager *m);
static MSFilter *ms_android_snd_read_new(MSFactory *factory);
static MSFilter *ms_android_snd_write_new(MSFactory* factory);

struct AAudioContext {
	AAudioContext() {
		samplerate = DeviceFavoriteSampleRate;
		nchannels = 1;
		builtin_aec = false;
	}

	~AAudioContext() {
		AAudioStreamBuilder_delete(builder);
	}

	int samplerate;
	int nchannels;
	bool builtin_aec;

	AAudioStreamBuilder *builder;
	AAudioStream *stream;	
};

struct AAudioInputContext {
	AAudioInputContext() {
		qinit(&q);
		ms_mutex_init(&mutex,NULL);
		mTickerSynchronizer = NULL;
		aec = NULL;
		mAvSkew = 0;
	}

	~AAudioInputContext() {
		flushq(&q,0);
		ms_mutex_destroy(&mutex);
	}
	
	void setContext(AAudioContext *context) {
		aaudio_context = context;
	}
	
	AAudioContext *aaudio_context;

	queue_t q;
	ms_mutex_t mutex;
	MSTickerSynchronizer *mTickerSynchronizer;
	MSFilter *mFilter;
	int64_t read_samples;
	int32_t samplesPerFrame;
	double mAvSkew;
};

struct AAudioOutputContext {
	AAudioOutputContext(MSFilter *f) {
		mFilter = f;
		ms_flow_controlled_bufferizer_init(&buffer, f, DeviceFavoriteSampleRate, 1);
		ms_mutex_init(&mutex,NULL);
	}

	~AAudioOutputContext() {
		ms_flow_controlled_bufferizer_uninit(&buffer);
		ms_mutex_destroy(&mutex);
	}
	
	void setContext(AAudioContext *context) {
		aaudio_context = context;
		ms_flow_controlled_bufferizer_set_samplerate(&buffer, aaudio_context->samplerate);
		ms_flow_controlled_bufferizer_set_nchannels(&buffer, aaudio_context->nchannels);
		ms_flow_controlled_bufferizer_set_max_size_ms(&buffer, flowControlThresholdMs);
		ms_flow_controlled_bufferizer_set_flow_control_interval_ms(&buffer, flowControlIntervalMs);
	}

	AAudioContext *aaudio_context;

	MSFilter *mFilter;
	MSFlowControlledBufferizer buffer;
	int32_t samplesPerFrame;
	ms_mutex_t mutex;
};

static AAudioContext* aaudio_context_init() {
	AAudioContext* ctx = new AAudioContext();
	return ctx;
}

static void android_snd_card_detect(MSSndCardManager *m) {
	SoundDeviceDescription* d = NULL;
	MSDevicesInfo *devices = NULL;
	devices = ms_factory_get_devices_info(m->factory);
	d = ms_devices_info_get_sound_device_description(devices);
	MSSndCard *card = android_snd_card_new(m);
	ms_snd_card_manager_add_card(m, card);
}

static void android_native_snd_card_init(MSSndCard *card) {

}

static void android_native_snd_card_uninit(MSSndCard *card) {
	AAudioContext *ctx = (AAudioContext*)card->data;
	ms_warning("Deletion of AAudio context [%p]", ctx);
}

static AAudioInputContext* aaudio_input_context_init() {
	AAudioInputContext* ictx = new AAudioInputContext();
	return ictx;
}

static void android_snd_read_init(MSFilter *obj) {
	AAudioInputContext *ictx = aaudio_input_context_init();
	obj->data = ictx;	
}

static aaudio_data_callback_result_t aaudio_recorder_callback(AAudioStream *stream, void *userData, void *audioData, int32_t numFrames) {
	AAudioInputContext *ictx = (AAudioInputContext *)userData;
	ictx->read_samples += numFrames * ictx->samplesPerFrame;

	int32_t bufferSize = sizeof(int16_t) * numFrames * ictx->samplesPerFrame;
	mblk_t *m = allocb(bufferSize, 0);
	memcpy(m->b_wptr, audioData, bufferSize);
	m->b_wptr += bufferSize;

	ms_mutex_lock(&ictx->mutex);
	ictx->mAvSkew = ms_ticker_synchronizer_update(ictx->mTickerSynchronizer, ictx->read_samples, (unsigned int)ictx->aaudio_context->samplerate);
	putq(&ictx->q, m);
	ms_mutex_unlock(&ictx->mutex);

	return AAUDIO_CALLBACK_RESULT_CONTINUE;	
}

static void android_snd_read_preprocess(MSFilter *obj) {
	AAudioInputContext *ictx = (AAudioInputContext*) obj->data;
	ictx->mFilter = obj;
	ictx->read_samples = 0;

	aaudio_result_t result = AAudio_createStreamBuilder(&ictx->aaudio_context->builder);
	AAudioStreamBuilder_setDeviceId(ictx->aaudio_context->builder, AAUDIO_UNSPECIFIED); // Default recorder
	AAudioStreamBuilder_setDirection(ictx->aaudio_context->builder, AAUDIO_DIRECTION_INPUT);
	AAudioStreamBuilder_setSharingMode(ictx->aaudio_context->builder, AAUDIO_SHARING_MODE_EXCLUSIVE); // If EXCLUSIVE mode isn't available the builder will fall back to SHARED mode.
	AAudioStreamBuilder_setSampleRate(ictx->aaudio_context->builder, ictx->aaudio_context->samplerate);
	AAudioStreamBuilder_setChannelCount(ictx->aaudio_context->builder, ictx->aaudio_context->nchannels);
	AAudioStreamBuilder_setFormat(ictx->aaudio_context->builder, AAUDIO_FORMAT_PCM_I16);
	AAudioStreamBuilder_setDataCallback(ictx->aaudio_context->builder, aaudio_recorder_callback, ictx);
	AAudioStreamBuilder_setPerformanceMode(ictx->aaudio_context->builder, AAUDIO_PERFORMANCE_MODE_LOW_LATENCY);
	//AAudioStreamBuilder_setSamplesPerFrame(ictx->aaudio_context->builder, AAUDIO_UNSPECIFIED); // Let the device decide
	//AAudioStreamBuilder_setBufferCapacityInFrames(ictx->aaudio_context->builder, AAUDIO_UNSPECIFIED); // Let the device decide
	
	result = AAudioStreamBuilder_openStream(ictx->aaudio_context->builder, &ictx->aaudio_context->stream);
	int32_t framesPerBust = AAudioStream_getFramesPerBurst(ictx->aaudio_context->stream);
	// Set the buffer size to the burst size - this will give us the minimum possible latency
	AAudioStream_setBufferSizeInFrames(ictx->aaudio_context->stream, framesPerBust);
	ictx->samplesPerFrame = AAudioStream_getSamplesPerFrame(ictx->aaudio_context->stream);

	result = AAudioStream_requestStart(ictx->aaudio_context->stream);
}

static void android_snd_read_process(MSFilter *obj) {
	AAudioInputContext *ictx = (AAudioInputContext*) obj->data;
	mblk_t *m;

	ms_mutex_lock(&ictx->mutex);
	while ((m = getq(&ictx->q)) != NULL) {
		ms_queue_put(obj->outputs[0], m);
	}
	ms_mutex_unlock(&ictx->mutex);
	if (obj->ticker->time % 5000 == 0) {
		ms_message("sound/wall clock skew is average=%g ms", ictx->mAvSkew);
	}
}

static void android_snd_read_postprocess(MSFilter *obj) {
	AAudioInputContext *ictx = (AAudioInputContext*)obj->data;

	aaudio_result_t result = AAudioStream_requestStop(ictx->aaudio_context->stream);
	result = AAudioStream_close(ictx->aaudio_context->stream);

	ms_ticker_set_synchronizer(obj->ticker, NULL);
	ms_mutex_lock(&ictx->mutex);
	ms_ticker_synchronizer_destroy(ictx->mTickerSynchronizer);
	ictx->mTickerSynchronizer = NULL;
	ms_mutex_unlock(&ictx->mutex);
}

static void android_snd_read_uninit(MSFilter *obj) {
	AAudioInputContext *ictx = (AAudioInputContext*)obj->data;
	delete ictx;
}

static int android_snd_read_set_sample_rate(MSFilter *obj, void *data) {
	return -1; /*don't accept custom sample rates, use recommended rate always*/
}

static int android_snd_read_get_sample_rate(MSFilter *obj, void *data) {
	int *n = (int*)data;
	AAudioInputContext *ictx = (AAudioInputContext*)obj->data;
	*n = ictx->aaudio_context->samplerate;
	return 0;
}

static int android_snd_read_set_nchannels(MSFilter *obj, void *data) {
	int *n = (int*)data;
	AAudioInputContext *ictx = (AAudioInputContext*)obj->data;
	ictx->aaudio_context->nchannels = *n;
	return 0;
}

static int android_snd_read_get_nchannels(MSFilter *obj, void *data) {
	int *n = (int*)data;
	AAudioInputContext *ictx = (AAudioInputContext*)obj->data;
	*n = ictx->aaudio_context->nchannels;
	return 0;
}

static int android_snd_read_hack_speaker_state(MSFilter *f, void *arg) {
	return 0;
}

static MSFilterMethod android_snd_read_methods[] = {
	{MS_FILTER_SET_SAMPLE_RATE, android_snd_read_set_sample_rate},
	{MS_FILTER_GET_SAMPLE_RATE, android_snd_read_get_sample_rate},
	{MS_FILTER_SET_NCHANNELS, android_snd_read_set_nchannels},
	{MS_FILTER_GET_NCHANNELS, android_snd_read_get_nchannels},
	{MS_AUDIO_CAPTURE_FORCE_SPEAKER_STATE, android_snd_read_hack_speaker_state},
	{0,NULL}
};

MSFilterDesc android_snd_aaudio_read_desc = {
	MS_FILTER_PLUGIN_ID,
	"MSAAudioRecorder",
	"android sound source",
	MS_FILTER_OTHER,
	NULL,
	0,
	1,
	android_snd_read_init,
	android_snd_read_preprocess,
	android_snd_read_process,
	android_snd_read_postprocess,
	android_snd_read_uninit,
	android_snd_read_methods
};

static MSFilter* ms_android_snd_read_new(MSFactory *factory) {
	MSFilter *f = ms_factory_create_filter_from_desc(factory, &android_snd_aaudio_read_desc);
	return f;
}

static MSFilter *android_snd_card_create_reader(MSSndCard *card) {
	MSFilter *f = ms_android_snd_read_new(ms_snd_card_get_factory(card));
	AAudioInputContext *ictx = static_cast<AAudioInputContext*>(f->data);
	ictx->setContext((AAudioContext*)card->data);
	return f;
}

static MSFilter *android_snd_card_create_writer(MSSndCard *card) {
	MSFilter *f = ms_android_snd_write_new(ms_snd_card_get_factory(card));
	AAudioOutputContext *octx = static_cast<AAudioOutputContext*>(f->data);
	octx->setContext((AAudioContext*)card->data);
	return f;
}

static void android_snd_write_init(MSFilter *obj){
	AAudioOutputContext *octx = new AAudioOutputContext(obj);
	obj->data = octx;
}

static void android_snd_write_uninit(MSFilter *obj){
	AAudioOutputContext *octx = (AAudioOutputContext*)obj->data;
	delete octx;
}

static int android_snd_write_set_sample_rate(MSFilter *obj, void *data) {
	return -1; /*don't accept custom sample rates, use recommended rate always*/
}

static int android_snd_write_get_sample_rate(MSFilter *obj, void *data) {
	int *n = (int*)data;
	AAudioOutputContext *octx = (AAudioOutputContext*)obj->data;
	*n = octx->aaudio_context->samplerate;
	return 0;
}

static int android_snd_write_set_nchannels(MSFilter *obj, void *data) {
	int *n = (int*)data;
	AAudioOutputContext *octx = (AAudioOutputContext*)obj->data;
	octx->aaudio_context->nchannels = *n;
	ms_flow_controlled_bufferizer_set_nchannels(&octx->buffer, octx->aaudio_context->nchannels);
	return 0;
}

static int android_snd_write_get_nchannels(MSFilter *obj, void *data) {
	int *n = (int*)data;
	AAudioOutputContext *octx = (AAudioOutputContext*)obj->data;
	*n = octx->aaudio_context->nchannels;
	return 0;
}

static aaudio_data_callback_result_t aaudio_player_callback(AAudioStream *stream, void *userData, void *audioData, int32_t numFrames) {
	AAudioOutputContext *octx = (AAudioOutputContext*)userData;

	detectUnderrunAndAdjustBufferSize(octx, stream);

	ms_mutex_lock(&octx->mutex);
	int ask = sizeof(int16_t) * numFrames * octx->samplesPerFrame;
	int avail = ms_flow_controlled_bufferizer_get_avail(&octx->buffer);
	
	ms_flow_controlled_bufferizer_read(&octx->buffer, (uint8_t *)audioData, avail);
	if (avail < ask) {
		memset(static_cast<int16_t *>(audioData) + avail, 0, ask - avail);
	}
	ms_mutex_unlock(&octx->mutex);

	return AAUDIO_CALLBACK_RESULT_CONTINUE;	
}

static void android_snd_write_preprocess(MSFilter *obj) {
	AAudioOutputContext *octx = (AAudioOutputContext*)obj->data;

	aaudio_result_t result = AAudio_createStreamBuilder(&octx->aaudio_context->builder);
	AAudioStreamBuilder_setDeviceId(octx->aaudio_context->builder, AAUDIO_UNSPECIFIED); // Default speaker
	AAudioStreamBuilder_setDirection(octx->aaudio_context->builder, AAUDIO_DIRECTION_OUTPUT);
	AAudioStreamBuilder_setSharingMode(octx->aaudio_context->builder, AAUDIO_SHARING_MODE_EXCLUSIVE); // If EXCLUSIVE mode isn't available the builder will fall back to SHARED mode.
	AAudioStreamBuilder_setSampleRate(octx->aaudio_context->builder, octx->aaudio_context->samplerate);
	AAudioStreamBuilder_setChannelCount(octx->aaudio_context->builder, octx->aaudio_context->nchannels);
	AAudioStreamBuilder_setFormat(octx->aaudio_context->builder, AAUDIO_FORMAT_PCM_I16);
	AAudioStreamBuilder_setDataCallback(octx->aaudio_context->builder, aaudio_player_callback, octx);
	AAudioStreamBuilder_setPerformanceMode(octx->aaudio_context->builder, AAUDIO_PERFORMANCE_MODE_LOW_LATENCY);
	AAudioStreamBuilder_setSamplesPerFrame(octx->aaudio_context->builder, AAUDIO_UNSPECIFIED); // Let the device decide
	AAudioStreamBuilder_setBufferCapacityInFrames(octx->aaudio_context->builder, AAUDIO_UNSPECIFIED); // Let the device decide
	
	result = AAudioStreamBuilder_openStream(octx->aaudio_context->builder, &octx->aaudio_context->stream);
	int32_t framesPerBust = AAudioStream_getFramesPerBurst(octx->aaudio_context->stream);
	// Set the buffer size to the burst size - this will give us the minimum possible latency
	AAudioStream_setBufferSizeInFrames(octx->aaudio_context->stream, framesPerBust);
	octx->samplesPerFrame = AAudioStream_getSamplesPerFrame(octx->aaudio_context->stream);

	result = AAudioStream_requestStart(octx->aaudio_context->stream);
}

static void android_snd_write_process(MSFilter *obj) {
	AAudioOutputContext *octx = (AAudioOutputContext*)obj->data;
	ms_mutex_lock(&octx->mutex);
	ms_flow_controlled_bufferizer_put_from_queue(&octx->buffer, obj->inputs[0]);
	ms_mutex_unlock(&octx->mutex);
}

static void android_snd_write_postprocess(MSFilter *obj) {
	AAudioOutputContext *octx = (AAudioOutputContext*)obj->data;

	aaudio_result_t result = AAudioStream_requestStop(octx->aaudio_context->stream);
	result = AAudioStream_close(octx->aaudio_context->stream);
}

static MSFilterMethod android_snd_write_methods[] = {
	{MS_FILTER_SET_SAMPLE_RATE, android_snd_write_set_sample_rate},
	{MS_FILTER_GET_SAMPLE_RATE, android_snd_write_get_sample_rate},
	{MS_FILTER_SET_NCHANNELS, android_snd_write_set_nchannels},
	{MS_FILTER_GET_NCHANNELS, android_snd_write_get_nchannels},
	{0,NULL}
};

MSFilterDesc android_snd_aaudio_write_desc = {
	MS_FILTER_PLUGIN_ID,
	"MSAAudioPlayer",
	"android sound output",
	MS_FILTER_OTHER,
	NULL,
	1,
	0,
	android_snd_write_init,
	android_snd_write_preprocess,
	android_snd_write_process,
	android_snd_write_postprocess,
	android_snd_write_uninit,
	android_snd_write_methods
};

static MSFilter* ms_android_snd_write_new(MSFactory* factory) {
	MSFilter *f = ms_factory_create_filter_from_desc(factory, &android_snd_aaudio_write_desc);
	return f;
}

MSSndCardDesc android_native_snd_aaudio_card_desc = {
	"AAudio",
	android_snd_card_detect,
	android_native_snd_card_init,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	android_snd_card_create_reader,
	android_snd_card_create_writer,
	android_native_snd_card_uninit
};

static MSSndCard* android_snd_card_new(MSSndCardManager *m) {
	MSSndCard* card = NULL;
	SoundDeviceDescription *d = NULL;
	MSDevicesInfo *devices = NULL;

	card = ms_snd_card_new(&android_native_snd_aaudio_card_desc);
	card->name = ms_strdup("android aaudio sound card");

	devices = ms_factory_get_devices_info(m->factory);
	d = ms_devices_info_get_sound_device_description(devices);

	AAudioContext *context = aaudio_context_init();
	card->capabilities |= MS_SND_CARD_CAP_BUILTIN_ECHO_CANCELLER; // Let's test is AAudio supports hardware AEC
	context->builtin_aec = true;
	card->latency = d->delay;
	card->data = context;
	if (d->recommended_rate){
		context->samplerate = d->recommended_rate;
	}
	return card;
}
