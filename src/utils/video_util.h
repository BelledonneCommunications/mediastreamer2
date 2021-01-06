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


 #ifndef VIDEO_UTIL_H
 #define VIDEO_UTIL_H

#include "vp8rtpfmt.h"

#define ROUTER_MAX_CHANNELS 5
#define ROUTER_MAX_INPUT_CHANNELS ROUTER_MAX_CHANNELS+1 // 1 input for static image
#define ROUTER_MAX_OUTPUT_CHANNELS ROUTER_MAX_CHANNELS*(ROUTER_MAX_CHANNELS-1)

#define SWITCHER_MAX_CHANNELS 20
#define SWITCHER_MAX_INPUT_CHANNELS SWITCHER_MAX_CHANNELS+1 //one more pin for static image

bool_t is_vp8_key_frame(mblk_t *m);
bool_t is_h264_key_frame(mblk_t *frame);
bool_t is_key_frame_dummy(mblk_t *frame);

typedef enum _IOState{
 STOPPED,
 RUNNING
}IOState;

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
 int needed_source; // for video router
 int current_source;
 int next_source;
}OutputContext;

typedef bool_t (*is_key_frame_func_t)(mblk_t *frame);


typedef struct _RouterState{
 InputContext input_contexts[ROUTER_MAX_INPUT_CHANNELS];
 OutputContext output_contexts[ROUTER_MAX_OUTPUT_CHANNELS];
 is_key_frame_func_t is_key_frame;
 int inputs_num;
}RouterState;

typedef struct _SwitcherState{
	InputContext input_contexts[SWITCHER_MAX_INPUT_CHANNELS];
	OutputContext output_contexts[SWITCHER_MAX_CHANNELS];
	int focus_pin;
	is_key_frame_func_t is_key_frame;
}SwitcherState;

typedef struct _MSVideoFilterPinControl{
	int pin;
	int enabled;
}MSVideoFilterPinControl;

void video_filter_channel_update_input(InputContext *input_context, int pin, MSQueue *q);
void video_filter_transfer(MSFilter *f, MSQueue *input,  MSQueue *output, OutputContext *output_context, mblk_t *start);
void router_channel_update_input(RouterState *s, int pin, MSQueue *q);
void switcher_channel_update_input(SwitcherState *s, int pin, MSQueue *q);

#endif /* VIDEO_UTIL_H */
