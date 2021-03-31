/*
 * Copyright (c) 2010-2020 Belledonne Communications SARL.
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


#include "mediastreamer2/msvideorouter.h"
#include "mediastreamer2/msticker.h"
#include "vp8rtpfmt.h"

#define ROUTER_MAX_CHANNELS 20
#define ROUTER_MAX_OUTPUT_CHANNELS ROUTER_MAX_CHANNELS*(ROUTER_MAX_CHANNELS-1)


static bool_t is_vp8_key_frame(mblk_t *m){
	uint8_t *p;
	
	if (m->b_cont){
		/* When data comes directly from the VP8 encoder, the VP8 payload is the second of the mblk_t chain.*/
		return !(m->b_cont->b_rptr[0] & 1);
	}
	p = vp8rtpfmt_skip_payload_descriptor(m);
	
	if (!p) {
		ms_warning("MSVideoSwitcher: invalid vp8 payload descriptor.");
		return FALSE;
	}
	return !(p[0] & 1);
}

static bool_t is_h264_key_frame(mblk_t *frame){
	/*TODO*/
	return FALSE;
}

static bool_t is_key_frame_dummy(mblk_t *frame){
	return TRUE;
}

typedef bool_t (*is_key_frame_func_t)(mblk_t *frame);


enum _IOState{
	STOPPED,
	RUNNING
};


typedef enum _IOState IOState;

typedef struct _InputContext{
	IOState state;
	uint8_t ignore_cseq;
	uint16_t cur_seq; /* to detect discontinuties*/
	uint32_t cur_ts; /* current timestamp, to detect beginning of frames */
	int seq_set;
	int key_frame_requested;
	mblk_t *key_frame_start;
}InputContext;

typedef struct _OutputContext{
	IOState state;
	uint32_t out_ts;
	uint32_t adjusted_out_ts;
	uint16_t out_seq;
	int router_source;
	int router_output;
}OutputContext;

typedef struct RouterState{
	InputContext input_contexts[ROUTER_MAX_CHANNELS];
	OutputContext output_contexts[ROUTER_MAX_OUTPUT_CHANNELS];
	int inputs[ROUTER_MAX_CHANNELS];
	is_key_frame_func_t is_key_frame;
} RouterState;

static void router_init(MSFilter *f){
	RouterState *s=ms_new0(RouterState,1);
	for(int i=0; i<ROUTER_MAX_CHANNELS;i++) {
		s->inputs[i] = -1;
	}
	s->is_key_frame=is_key_frame_dummy;
	f->data=s;
}

static void router_uninit(MSFilter *f){
	RouterState *s=(RouterState *)f->data;
	ms_free(s);
}

static int router_configure_output(MSFilter *f, void *data){
	RouterState *s=(RouterState *)f->data;
	int pin=*(int*)data;
	ms_filter_lock(f);

	int i;
	int k;
	
	for(i=0, k=0; i<ROUTER_MAX_CHANNELS;i++) {
		if (s->inputs[i] == 1) {
			for (;k<f->desc->noutputs;k++) {
				// configure outputs with the new input pin
				if (s->output_contexts[i].router_source == -1){
					s->output_contexts[i].router_source = pin;
					s->output_contexts[i].router_output = i;
				}
			}
			for (;k<f->desc->noutputs;k++) {
				// configure outputs with the existed input pin
				if (s->output_contexts[i].router_source == -1){
					s->output_contexts[i].router_source = i;
					s->output_contexts[i].router_output = pin;
				}
			}
		}
	}
	
	s->inputs[pin] = 1;

	ms_filter_unlock(f);
	ms_message("%s: router configure output for pin %i", f->desc->name, pin);
	return 0;
}

static int router_unconfigure_output(MSFilter *f, void *data){
	RouterState *s=(RouterState *)f->data;
	int pin=*(int*)data;
	ms_filter_lock(f);

	s->inputs[pin] = -1;

	int i;
	for(i=0;i<f->desc->noutputs;++i) {
		if (s->output_contexts[i].router_source == pin || s->output_contexts[i].router_output == pin){
			s->output_contexts[i].router_source = -1;
			s->output_contexts[i].router_output = -1;
		}
	}
	ms_filter_unlock(f);
	ms_message("%s: router unconfigure output for pin %i", f->desc->name, pin);
	return 0;
}

static void router_channel_update_input(RouterState *s, int pin, MSQueue *q){
	InputContext *input_context = &s->input_contexts[pin];
	mblk_t *m;
	
	input_context->key_frame_start = NULL;
	
	for(m = ms_queue_peek_first(q); !ms_queue_end(q, m); m = ms_queue_peek_next(q,m)){
		uint32_t new_ts = mblk_get_timestamp_info(m);
		uint16_t new_seq = mblk_get_cseq(m);
		//uint8_t marker = mblk_get_marker_info(m);
		
		if (!input_context->seq_set){
			input_context->state = STOPPED;
			input_context->key_frame_requested = TRUE;
		}else if (!input_context->ignore_cseq && (new_seq != input_context->cur_seq + 1)){
			ms_warning("MSVideoSwitcher: Sequence discontinuity detected on pin %i, key-frame requested", pin);
			input_context->state = STOPPED;
			input_context->key_frame_requested = TRUE;
		}
		if (input_context->key_frame_requested){
			if (!input_context->seq_set || input_context->cur_ts != new_ts){
				/* Possibly a beginning of frame ! */
				if (s->is_key_frame(m)){
					ms_message("MSVideoSwitcher: key frame detected on pin %i", pin);
					input_context->state = RUNNING;
					input_context->key_frame_start = m;
					input_context->key_frame_requested = FALSE;
				}
			}
		}
		input_context->cur_ts = new_ts;
		input_context->cur_seq = new_seq;
		input_context->seq_set = 1;
	}
}

static void router_transfer(MSFilter *f, MSQueue *input,  MSQueue *output, OutputContext *output_context, mblk_t *start){
	mblk_t *m;
	if (ms_queue_empty(input)) return;
	
	if (start == NULL) start = ms_queue_peek_first(input);
	
	for(m = start; !ms_queue_end(input, m) ; m = ms_queue_peek_next(input,m)){
		mblk_t *o = dupmsg(m);
		
		if (mblk_get_timestamp_info(m) != output_context->out_ts){
			/* Each time we observe a new input timestamp, we must select a new output timestamp */
			output_context->out_ts = mblk_get_timestamp_info(m);
			output_context->adjusted_out_ts = (uint32_t) (f->ticker->time * 90LL);
		}
		
		/* We need to set sequence number for what we send out, otherwise the VP8 decoder won't be able
		 * to verify the integrity of the stream*/
		
		mblk_set_timestamp_info(o, output_context->adjusted_out_ts);
		mblk_set_cseq(o, output_context->out_seq++);
		mblk_set_marker_info(o, mblk_get_marker_info(m));
		
		ms_queue_put(output, o);
	}
}

static void router_preprocess(MSFilter *f){
}

static void router_postprocess(MSFilter *f){
}

static void router_process(MSFilter *f){
	RouterState *s=(RouterState *)f->data;
	int i;

	ms_filter_lock(f);

	/* First update channel states according to their received packets */
	for(i=0;i<f->desc->ninputs;++i){
		MSQueue *q=f->inputs[i];
		InputContext *input_context=&s->input_contexts[i];
		if (q) {
			router_channel_update_input(s, i, q);
			if (!ms_queue_empty(q) && input_context->key_frame_requested){
				if (input_context->state == STOPPED){
					ms_filter_notify(f, MS_VIDEO_ROUTER_SEND_PLI, &i);
				}else{
					ms_filter_notify(f,MS_VIDEO_ROUTER_SEND_FIR, &i);
				}
			}
		}
	}

	/*fill outputs from inputs according to rules below*/
	for(i=0;i<f->desc->noutputs;++i){
		MSQueue * q = f->outputs[i];
		OutputContext *output_context = &s->output_contexts[i];
		InputContext *input_context;

		if (q){
			if (output_context->router_source != -1){
				input_context = &s->input_contexts[output_context->router_source];
				if (input_context->state == RUNNING){
					router_transfer(f, f->inputs[output_context->router_source], q, output_context, NULL);
				}
			}
		}
	}
	/*purge all inputs, we are done*/
	for(i=0;i<f->desc->ninputs;++i){
		MSQueue *q=f->inputs[i];
		if (q) ms_queue_flush(q);
	}
	ms_filter_unlock(f);
}

static int router_set_fmt(MSFilter *f, void *data){
	const MSFmtDescriptor *fmt=(MSFmtDescriptor*)data;
	RouterState *s=(RouterState *)f->data;
	if (fmt){
		if (strcasecmp(fmt->encoding,"VP8")==0){
			s->is_key_frame=is_vp8_key_frame;
		}else if (strcasecmp(fmt->encoding,"H264")==0){
			s->is_key_frame=is_h264_key_frame;
		}else{
			ms_error("%s: unsupported format %s", f->desc->name, fmt->encoding);
			return -1;
		}
	}
	return 0;
}

static int router_set_local_member_pin(MSFilter *f, void *data) {
	RouterState *s=(RouterState *)f->data;
	MSVideoRouterPinControl *pc = (MSVideoRouterPinControl*)data;
	if (pc->pin >= 0 && pc->pin < f->desc->ninputs){
		s->input_contexts[pc->pin].ignore_cseq = (unsigned char)pc->enabled;
		ms_message("MSVideoRouter: Pin #%i local member attribute: %i", pc->pin, pc->enabled);
		return 0;
	}
	ms_error("%s: invalid argument to MS_VIDEO_ROUTER_SET_AS_LOCAL_MEMBER", f->desc->name);
	return -1;
}

static int router_notify_pli(MSFilter *f, void *data){
	RouterState *s=(RouterState *)f->data;
	int pin = *(int*)data;
	ms_filter_notify(f, MS_VIDEO_ROUTER_SEND_PLI, &pin);
	return 0;
}

static int router_notify_fir(MSFilter *f, void *data){
	RouterState *s=(RouterState *)f->data;
	int pin = *(int*)data;
	ms_filter_notify(f, MS_VIDEO_ROUTER_SEND_FIR, &pin);
	return 0;
}

static MSFilterMethod methods[]={
	{	MS_VIDEO_ROUTER_CONFIGURE_OUTPUT , router_configure_output },
	{	MS_VIDEO_ROUTER_UNCONFIGURE_OUTPUT , router_unconfigure_output },
	{	MS_VIDEO_ROUTER_SET_AS_LOCAL_MEMBER , router_set_local_member_pin },
	{	MS_VIDEO_ROUTER_NOTIFY_PLI, router_notify_pli },
	{	MS_VIDEO_ROUTER_NOTIFY_FIR, router_notify_fir },
	{	MS_FILTER_SET_INPUT_FMT, router_set_fmt },
	{0,NULL}
};


#ifdef _MSC_VER


MSFilterDesc ms_video_router_desc={
	MS_VIDEO_ROUTER_ID,
	"MSVideoRouter",
	N_("A filter that router video streams from several inputs, used for video conferencing."),
	MS_FILTER_OTHER,
	NULL,
	ROUTER_MAX_CHANNELS,
	ROUTER_MAX_OUTPUT_CHANNELS,
	router_init,
	router_preprocess,
	router_process,
	router_postprocess,
	router_uninit,
	methods,
};


#else


MSFilterDesc ms_video_router_desc={
	.id=MS_VIDEO_ROUTER_ID,
	.name="MSVideoRouter",
	.text=N_("A filter that router video streams from several inputs, used for video conferencing."),
	.category=MS_FILTER_OTHER,
	.ninputs=ROUTER_MAX_CHANNELS,
	.noutputs=ROUTER_MAX_OUTPUT_CHANNELS,
	.init=router_init,
	.preprocess=router_preprocess,
	.process=router_process,
	.postprocess=router_postprocess,
	.uninit=router_uninit,
	.methods=methods,
};


#endif

MS_FILTER_DESC_EXPORT(ms_video_router_desc)
