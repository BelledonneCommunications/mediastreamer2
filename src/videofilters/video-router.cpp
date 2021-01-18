/*
 * Copyright (c) 2010-20121 Belledonne Communications SARL.
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
#include "video-conference-util.h"
#include <vector>

using namespace std;

#define ROUTER_MAX_CHANNELS 5
#define ROUTER_MAX_INPUT_CHANNELS ROUTER_MAX_CHANNELS+1 // 1 input for static image
#define ROUTER_MAX_OUTPUT_CHANNELS ROUTER_MAX_CHANNELS*(ROUTER_MAX_CHANNELS-1)


class MSRouterState : public MSVideoConferenceFilterState {
public:
	MSRouterState(){
		mInputPins.resize(0);
	}
	~MSRouterState() = default;

	InputContext mInputContexts[ROUTER_MAX_INPUT_CHANNELS] = {
		{
			.state = STOPPED,
			.ignore_cseq = 0,
			.cur_seq = 0,
			.cur_ts = 0,
			.seq_set = 0,
			.key_frame_requested = 0,
			.key_frame_start = NULL,
		}
	};
	OutputContext mOutputContexts[ROUTER_MAX_OUTPUT_CHANNELS] = {
		{
			.state = STOPPED,
			.out_ts = 0,
			.adjusted_out_ts = 0,
			.out_seq = 0,
			.needed_source = -1,
			.current_source = 0,
			.next_source = 0,
			.pin = -1,
		}
	};
	vector<int> mInputPins; /* used pins for input */
};

//-------------------------------------------------------------------------------------

static void router_init(MSFilter *f){
	MSRouterState *s = new MSRouterState();
	f->data=s;
}

static void router_uninit(MSFilter *f){
	MSRouterState *s=(MSRouterState *)f->data;
	delete s;
}

static int router_configure_output(MSFilter *f, void *data){
	MSRouterState *s=(MSRouterState *)f->data;
	int pin=*(int*)data;
	ms_filter_lock(f);

	s->mInputPins.push_back(pin);
	int i;
	unsigned long k;
	// configure (number of inputs -1) outputs with the new input pin
	for(i=0,k=0;i<f->desc->noutputs && k < s->mInputPins.size()-1;i++,k++){
		if (s->mOutputContexts[i].needed_source == -1){
			s->mOutputContexts[i].needed_source = pin;
			s->mOutputContexts[i].next_source = pin;
			s->mOutputContexts[i].pin = s->mInputPins.at(k);
		}
	}
	// configure (number of inputs -1) outputs with other input pins
	for(k=0;i<f->desc->noutputs && k < s->mInputPins.size()-1;i++,k++){
		if (s->mOutputContexts[i].needed_source == -1){
			s->mOutputContexts[i].needed_source = s->mInputPins.at(k);
			s->mOutputContexts[i].next_source = s->mInputPins.at(k);
			s->mOutputContexts[i].pin = pin;
		}
	}

	ms_filter_unlock(f);
	ms_message("%s: router configure output for pin %i", f->desc->name, pin);
	return 0;
}

static int router_unconfigure_output(MSFilter *f, void *data){
	MSRouterState *s=(MSRouterState *)f->data;
	int pin=*(int*)data;
	ms_filter_lock(f);

	unsigned long k;
	for (k=0; k < s->mInputPins.size(); k++) {
		if (s->mInputPins.at(k) == pin) {
			s->mInputPins.erase(s->mInputPins.begin()+k);
			break;
		}
	}

	int i;
	for(i=0;i<f->desc->noutputs;++i) {
		if (s->mOutputContexts[i].needed_source == pin || s->mOutputContexts[i].pin == pin){
			s->mOutputContexts[i].needed_source = -1;
			s->mOutputContexts[i].pin = -1;
		}
	}
	ms_filter_unlock(f);
	ms_message("%s: router unconfigure output for pin %i", f->desc->name, pin);
	return 0;
}

static void router_preprocess(MSFilter *f){
}

static void router_postprocess(MSFilter *f){
}

static void router_process(MSFilter *f){
	MSRouterState *s=(MSRouterState *)f->data;
	int i;

	ms_filter_lock(f);

	/* First update channel states according to their received packets */
	for(i=0;i<f->desc->ninputs;++i){
		MSQueue *q=f->inputs[i];
		InputContext *input_context=&s->mInputContexts[i];
		if (q) {
			filter_channel_update_input(s, input_context, i, q);
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
		OutputContext *output_context = &s->mOutputContexts[i];
		InputContext *input_context;

		if (q){
			mblk_t *key_frame_start = NULL;

			if (f->inputs[output_context->next_source] == NULL){
				ms_warning("%s: next source %i disapeared, choosing static image.", f->desc->name, output_context->next_source);
				output_context->next_source = f->desc->ninputs-1;
			}
			if (output_context->current_source != -1 && f->inputs[output_context->current_source] == NULL){
				ms_warning("%s: current source %i disapeared, choosing static image.", f->desc->name, output_context->current_source);
				output_context->next_source = f->desc->ninputs-1;
				output_context->current_source = -1; /* Invalidate the current source until the switch.*/
			}

			if (output_context->current_source != output_context->next_source){
				/* This output is waiting a key-frame to start */
				input_context = &s->mInputContexts[output_context->next_source];
				if (input_context->key_frame_start != NULL){
					/* The input just got a key frame, we can switch ! */
					output_context->current_source = output_context->next_source;
					key_frame_start = input_context->key_frame_start;
				}else{
					/* else request a key frame */
					if (input_context->key_frame_requested == FALSE){
						ms_message("%s: need key-frame for pin %i", f->desc->name, output_context->next_source);
						input_context->key_frame_requested = TRUE;
					}
				}
			}

			if (output_context->current_source != -1){
				input_context = &s->mInputContexts[output_context->current_source];
				if (input_context->state == RUNNING){
					video_filter_transfer(f, f->inputs[output_context->current_source], q, output_context, key_frame_start);
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

static int router_set_local_member_pin(MSFilter *f, void *data){
	MSRouterState *s=(MSRouterState *)f->data;
	MSVideoFilterPinControl *pc = (MSVideoFilterPinControl*)data;
	if (pc->pin >= 0 && pc->pin < f->desc->ninputs){
		s->mInputContexts[pc->pin].ignore_cseq = (unsigned char)pc->enabled;
		ms_message("MSVideoRouter: Pin #%i local member attribute: %i", pc->pin, pc->enabled);
		return 0;
	}
	ms_error("%s: invalid argument to MS_VIDEO_ROUTER_SET_AS_LOCAL_MEMBER", f->desc->name);
	return -1;
}

static int router_notify_pli(MSFilter *f, void *data){
	MSRouterState *s=(MSRouterState *)f->data;
	int pin = *(int*)data;
	int source_pin;
	int i;
	for(i=0;i<f->desc->noutputs;++i){
		if (s->mOutputContexts[i].needed_source == pin){
			/* Propagate the PLI to the current input source. */
			source_pin = s->mOutputContexts[i].current_source;
			if (source_pin != -1){
				ms_filter_notify(f, MS_VIDEO_ROUTER_SEND_PLI, &source_pin);
			}
		}
	}
	return 0;
}

static int router_notify_fir(MSFilter *f, void *data){
	MSRouterState *s=(MSRouterState *)f->data;
	int pin = *(int*)data;
	int source_pin;
	int i;
	for(i=0;i<f->desc->noutputs;++i){
		if (s->mOutputContexts[i].needed_source == pin){
			/* Propagate the FIR to the current input source. */
			source_pin = s->mOutputContexts[i].current_source;
			if (source_pin != -1){
				ms_filter_notify(f, MS_VIDEO_ROUTER_SEND_FIR, &source_pin);
			}
		}
	}
	return 0;
}

extern "C" {

static MSFilterMethod methods[]={
	{	MS_VIDEO_ROUTER_CONFIGURE_OUTPUT , router_configure_output },
	{	MS_VIDEO_ROUTER_UNCONFIGURE_OUTPUT , router_unconfigure_output },
	{	MS_VIDEO_ROUTER_SET_AS_LOCAL_MEMBER , router_set_local_member_pin },
	{	MS_VIDEO_ROUTER_NOTIFY_PLI, router_notify_pli },
	{	MS_VIDEO_ROUTER_NOTIFY_FIR, router_notify_fir },
	{	MS_FILTER_SET_INPUT_FMT, filter_set_fmt },
	{0,NULL}
};

#ifdef _MSC_VER

MSFilterDesc ms_video_router_desc={
	MS_VIDEO_ROUTER_ID,
	"MSVideoRouter",
	N_("A filter that router video streams from several inputs, used for video conferencing."),
	MS_FILTER_OTHER,
	NULL,
	ROUTER_MAX_INPUT_CHANNELS,
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
	.ninputs=ROUTER_MAX_INPUT_CHANNELS,
	.noutputs=ROUTER_MAX_OUTPUT_CHANNELS,
	.init=router_init,
	.preprocess=router_preprocess,
	.process=router_process,
	.postprocess=router_postprocess,
	.uninit=router_uninit,
	.methods=methods,
};

#endif
}// extern "C"

MS_FILTER_DESC_EXPORT(ms_video_router_desc)


