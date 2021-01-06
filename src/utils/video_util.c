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


#include "video_util.h"
#include "mediastreamer2/msticker.h"


bool_t is_vp8_key_frame(mblk_t *m) {
 uint8_t *p;

 if (m->b_cont){
   /* When data comes directly from the VP8 encoder, the VP8 payload is the second of the mblk_t chain.*/
   return !(m->b_cont->b_rptr[0] & 1);
 }
 p = vp8rtpfmt_skip_payload_descriptor(m);

 if (!p) {
   ms_warning("MSVideoUtil: invalid vp8 payload descriptor.");
   return FALSE;
 }
 return !(p[0] & 1);
}

bool_t is_h264_key_frame(mblk_t *frame) {
 /*TODO*/
 return FALSE;
}

bool_t is_key_frame_dummy(mblk_t *frame) {
 return TRUE;
}



void video_filter_channel_update_input(InputContext *input_context, int pin, MSQueue *q){
 	// todo
}

void router_channel_update_input(RouterState *s, int pin, MSQueue *q){
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
 			ms_warning("video_util: Sequence discontinuity detected on pin %i, key-frame requested", pin);
 			input_context->state = STOPPED;
 			input_context->key_frame_requested = TRUE;
 		}
 		if (input_context->key_frame_requested){
 			if (!input_context->seq_set || input_context->cur_ts != new_ts){
 				/* Possibly a beginning of frame ! */
 				if (s->is_key_frame(m)){
 					ms_message("video_util: key frame detected on pin %i", pin);
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

void switcher_channel_update_input(SwitcherState *s, int pin, MSQueue *q){
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
 			ms_warning("video_util: Sequence discontinuity detected on pin %i, key-frame requested", pin);
 			input_context->state = STOPPED;
 			input_context->key_frame_requested = TRUE;
 		}
 		if (input_context->key_frame_requested){
 			if (!input_context->seq_set || input_context->cur_ts != new_ts){
 				/* Possibly a beginning of frame ! */
 				if (s->is_key_frame(m)){
 					ms_message("video_util: key frame detected on pin %i", pin);
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

void video_filter_transfer(MSFilter *f, MSQueue *input,  MSQueue *output, OutputContext *output_context, mblk_t *start){
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
