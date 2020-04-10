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

#include "mediastreamer2/msfilter.h"
#include "mediastreamer2/msticker.h"


struct VoidSourceState {
	int rate;
	int nchannels;
	bool_t send_silence;
};

typedef struct VoidSourceState VoidSourceState;


static void void_source_init(MSFilter *f) {
	VoidSourceState *s = ms_new0(VoidSourceState, 1);
	s->rate = 8000;
	s->nchannels = 1;
	s->send_silence = FALSE;
	f->data = s;
}

static void void_source_uninit(MSFilter *f) {
	ms_free(f->data);
}

static void void_source_process(MSFilter *f) {
	VoidSourceState *s = (VoidSourceState *)f->data;
	mblk_t *m;
	int nsamples;

	if (s->send_silence == TRUE) {
		nsamples = (f->ticker->interval * s->rate) / 1000;
		m = allocb(nsamples * s->nchannels * 2, 0);
		memset(m->b_wptr, 0, nsamples * s->nchannels * 2);
		m->b_wptr += nsamples * s->nchannels * 2;
		ms_queue_put(f->outputs[0], m);
	}
}

static int void_source_set_rate(MSFilter *f, void *arg) {
	VoidSourceState *s = (VoidSourceState *)f->data;
	s->rate = *((int*)arg);
	return 0;
}

static int void_source_get_rate(MSFilter *f, void *arg) {
	VoidSourceState *s = (VoidSourceState *)f->data;
	*((int *)arg) = s->rate;
	return 0;
}

static int void_source_set_nchannels(MSFilter *f, void *arg) {
	VoidSourceState *s = (VoidSourceState *)f->data;
	s->nchannels = *(int *)arg;
	return 0;
}

static int void_source_get_nchannels(MSFilter *f, void *arg) {
	VoidSourceState *s = (VoidSourceState *)f->data;
	*((int *)arg) = s->nchannels;
	return 0;
}

static int void_source_send_silence(MSFilter *f, void *arg) {
	VoidSourceState *s = (VoidSourceState *)f->data;
	s->send_silence = *((bool_t *)arg);
	return 0;
}

MSFilterMethod void_source_methods[] = {
	{ MS_FILTER_SET_SAMPLE_RATE, void_source_set_rate },
	{ MS_FILTER_GET_SAMPLE_RATE, void_source_get_rate },
	{ MS_FILTER_SET_NCHANNELS, void_source_set_nchannels },
	{ MS_FILTER_GET_NCHANNELS, void_source_get_nchannels },
	{ MS_VOID_SOURCE_SEND_SILENCE, void_source_send_silence },
	{ 0, NULL }
};

static void void_sink_process(MSFilter *f){
	int i;
	
	for( i = 0; i < f->desc->ninputs; ++i){
		if (f->inputs[i]) ms_queue_flush(f->inputs[i]);
	}
}

#ifdef _MSC_VER

MSFilterDesc ms_void_source_desc={
	MS_VOID_SOURCE_ID,
	"MSVoidSource",
	N_("A filter that generates silence on its output (useful for beginning some graphs)."),
	MS_FILTER_OTHER,
	NULL,
	0,
	1,
	void_source_init,
	NULL,
	void_source_process,
	NULL,
	void_source_uninit,
	void_source_methods,
	MS_FILTER_IS_PUMP
};

MSFilterDesc ms_void_sink_desc={
	MS_VOID_SINK_ID,
	"MSVoidSink",
	N_("A filter that trashes its input (useful for terminating some graphs)."),
	MS_FILTER_OTHER,
	NULL,
	10,
	0,
	NULL,
	NULL,
	void_sink_process,
	NULL,
	NULL
};

#else

MSFilterDesc ms_void_source_desc={
	.id=MS_VOID_SOURCE_ID,
	.name="MSVoidSource",
	.text=N_("A filter that generates silence on its output (useful for beginning some graphs)."),
	.category=MS_FILTER_OTHER,
	.ninputs=0,
	.noutputs=1,
	.init=void_source_init,
	.process=void_source_process,
	.uninit=void_source_uninit,
	.methods=void_source_methods,
	.flags=MS_FILTER_IS_PUMP
};

MSFilterDesc ms_void_sink_desc={
	.id=MS_VOID_SINK_ID,
	.name="MSVoidSink",
	.text=N_("A filter that trashes its input (useful for terminating some graphs)."),
	.category=MS_FILTER_OTHER,
	.ninputs=10,
	.noutputs=0,
	.process=void_sink_process,
};

#endif

MS_FILTER_DESC_EXPORT(ms_void_source_desc)
MS_FILTER_DESC_EXPORT(ms_void_sink_desc)
