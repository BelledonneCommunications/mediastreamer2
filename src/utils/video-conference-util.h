/*
 * Copyright (c) 2010-2021 Belledonne Communications SARL.
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

#ifndef VIDEO_CONFERENCE_UTIL_H
#define VIDEO_CONFERENCE_UTIL_H

#include "vp8rtpfmt.h"

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
	int needed_source; // video router specific
	int current_source;
	int next_source;
	int pin; // video router specific
}OutputContext;

typedef bool_t (*is_key_frame_func_t)(mblk_t *frame);


class MSVideoConferenceFilterState{
public:
	MSVideoConferenceFilterState() = default;
	virtual ~MSVideoConferenceFilterState() = default;
	is_key_frame_func_t mIsKeyFrame = is_key_frame_dummy;
};


void video_filter_transfer(MSFilter *f, MSQueue *input,  MSQueue *output, OutputContext *output_context, mblk_t *start);
void filter_channel_update_input(MSVideoConferenceFilterState *s, InputContext *input_context, int pin, MSQueue *q);
int filter_set_fmt(MSFilter *f, void *data);


extern "C" {
typedef struct _MSVideoFilterPinControl{
	int pin;
	int enabled;
}MSVideoFilterPinControl;
	
};

#endif /* VIDEO_CONFERENCE_UTIL_H */
