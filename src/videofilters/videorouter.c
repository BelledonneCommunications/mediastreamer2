/*
 * Copyright (c) 2020 Belledonne Communications SARL.
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
#include "video_util.h"


static void router_init(MSFilter *f){
  RouterState *s=ms_new0(RouterState,1);
 	s->is_key_frame=is_key_frame_dummy;
  s->inputs_num = 0;
  int i;
  for(i=0;i<ROUTER_MAX_OUTPUT_CHANNELS;++i){
    s->output_contexts[i].needed_source = -1;
  }
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
 int k = 1;
 for(i=0;i<f->desc->noutputs && k<s->inputs_num;++i,++k){
   if (s->output_contexts[i].needed_source == -1){
     s->output_contexts[i].needed_source = pin;
     s->output_contexts[i].next_source = pin;
    }
  }
  ms_filter_unlock(f);
  ms_message("%s: router configure output for pin %i", f->desc->name, pin);
  return 0;
}

static int router_unconfigure_output(MSFilter *f, void *data){
 RouterState *s=(RouterState *)f->data;
 int pin=*(int*)data;
 ms_filter_lock(f);
 int i;
 for(i=0;i<f->desc->noutputs;++i){
   if (s->output_contexts[i].needed_source == pin){
      s->output_contexts[i].needed_source = -1;
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
 				input_context = &s->input_contexts[output_context->next_source];
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
 				input_context = &s->input_contexts[output_context->current_source];
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

static int router_set_local_member_pin(MSFilter *f, void *data){
 	RouterState *s=(RouterState *)f->data;
 	MSVideoFilterPinControl *pc = (MSVideoFilterPinControl*)data;
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
 	int source_pin;
  int i;
  for(i=0;i<f->desc->noutputs;++i){
    if (s->output_contexts[i].needed_source == pin){
      /* Propagate the PLI to the current input source. */
     	source_pin = s->output_contexts[i].current_source;
     	if (source_pin != -1){
     		ms_filter_notify(f, MS_VIDEO_ROUTER_SEND_PLI, &source_pin);
     	}
    }
  }
 	return 0;
}

static int router_notify_fir(MSFilter *f, void *data){
 	RouterState *s=(RouterState *)f->data;
 	int pin = *(int*)data;
 	int source_pin;
  int i;
  for(i=0;i<f->desc->noutputs;++i){
    if (s->output_contexts[i].needed_source == pin){
      /* Propagate the FIR to the current input source. */
     	source_pin = s->output_contexts[i].current_source;
     	if (source_pin != -1){
     		ms_filter_notify(f, MS_VIDEO_ROUTER_SEND_FIR, &source_pin);
     	}
    }
  }
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

 MS_FILTER_DESC_EXPORT(ms_video_router_desc)
