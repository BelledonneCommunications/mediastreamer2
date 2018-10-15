/*
mediastreamer2 library - modular sound and video processing and streaming
Copyright (C) 2010-2018  Simon MORLAT (simon.morlat@linphone.org)

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
*/

#include "mediastreamer2/msfilter.h"
#include "mediastreamer2/mschanadapter.h"
#include "mediastreamer2/msticker.h"

/*
 This filter transforms stereo buffers to mono and vice versa.
*/

typedef struct AdapterState{
	int inputchans;
	int outputchans;
	int sample_rate;
	MSBufferizer input_buffer1;
	MSBufferizer input_buffer2;
}AdapterState;

static void adapter_init(MSFilter *f){
	AdapterState *s = ms_new0(AdapterState, 1);
	ms_bufferizer_init(&s->input_buffer1);
	ms_bufferizer_init(&s->input_buffer2);
	s->inputchans = 1;
	s->outputchans = 1;
	s->sample_rate = 8000;
	f->data = s;
}

static void adapter_uninit(MSFilter *f){
	AdapterState *s = (AdapterState*)f->data;
	ms_bufferizer_uninit(&s->input_buffer1);
	ms_bufferizer_uninit(&s->input_buffer2);
	ms_free(s);
}

static void adapter_process(MSFilter *f){
	AdapterState *s = (AdapterState*)f->data;

	// Two mono input t stereo output
	if (s->inputchans == 2 && s->outputchans == 1) {
		size_t min_buffer_size, buffer_size1, buffer_size2;
		uint8_t *buffer1, *buffer2;

		ms_bufferizer_put_from_queue(&s->input_buffer1, f->inputs[0]);
		ms_bufferizer_put_from_queue(&s->input_buffer2, f->inputs[1]);

		min_buffer_size = (f->ticker->interval * s->sample_rate) / 1000;

		buffer_size1 = ms_bufferizer_get_avail(&s->input_buffer1);
		buffer_size2 = ms_bufferizer_get_avail(&s->input_buffer2);

		while (buffer_size1 >= min_buffer_size || buffer_size2 >= min_buffer_size) {
			mblk_t *om;
			buffer1 = (uint8_t*)alloca(min_buffer_size);
			buffer2 = (uint8_t*)alloca(min_buffer_size);

			if (buffer_size1 < min_buffer_size) memset(buffer1, 0, min_buffer_size);
			if (buffer_size2 < min_buffer_size) memset(buffer2, 0, min_buffer_size);

			ms_bufferizer_read(&s->input_buffer1, buffer1, min_buffer_size);
			ms_bufferizer_read(&s->input_buffer2, buffer2, min_buffer_size);

			om = allocb(min_buffer_size * 2, 0);

			for (unsigned int i = 0 ; i < min_buffer_size / sizeof(int16_t) ; i++ , om->b_wptr += 4) {
				((int16_t*)om->b_wptr)[0] = ((int16_t*)buffer1)[i];
				((int16_t*)om->b_wptr)[1] = ((int16_t*)buffer2)[i];
			}

			ms_queue_put(f->outputs[0], om);

			buffer_size1 = ms_bufferizer_get_avail(&s->input_buffer1);
			buffer_size2 = ms_bufferizer_get_avail(&s->input_buffer2);
		}
	} else {
		mblk_t *im, *om;
		size_t msgsize;

		while ((im = ms_queue_get(f->inputs[0])) != NULL) {
			if (s->inputchans == s->outputchans) {
				ms_queue_put(f->outputs[0], im);
			} else if (s->outputchans == 2) {
				msgsize = msgdsize(im) * 2;
				om = allocb(msgsize, 0);
				for (;im->b_rptr < im->b_wptr ; im->b_rptr += 2 ,om->b_wptr += 4){
					((int16_t*)om->b_wptr)[0] = *(int16_t*)im->b_rptr;
					((int16_t*)om->b_wptr)[1] = *(int16_t*)im->b_rptr;
				}
				ms_queue_put(f->outputs[0], om);
				freemsg(im);
			}
		}
	}
}

static int adapter_set_sample_rate(MSFilter *f, void *data) {
	AdapterState *s = (AdapterState*)f->data;
	s->sample_rate = *(int*)data;
	return 0;
}

static int adapter_get_sample_rate(MSFilter *f, void *data) {
	AdapterState *s = (AdapterState*)f->data;
	*(int*)data = s->sample_rate;
	return 0;
}

static int adapter_set_nchannels(MSFilter *f, void *data) {
	AdapterState *s = (AdapterState*)f->data;
	s->inputchans = *(int*)data;
	return 0;
}

static int adapter_get_nchannels(MSFilter *f, void *data) {
	AdapterState *s = (AdapterState*)f->data;
	*(int*)data = s->inputchans;
	return 0;
}

static int adapter_set_out_nchannels(MSFilter *f, void *data) {
	AdapterState *s = (AdapterState*)f->data;
	s->outputchans = *(int*)data;
	return 0;
}

static int adapter_get_out_nchannels(MSFilter *f, void *data) {
	AdapterState *s = (AdapterState*)f->data;
	*(int*)data = s->outputchans;
	return 0;
}

static MSFilterMethod methods[] = {
	{	MS_FILTER_SET_SAMPLE_RATE, adapter_set_sample_rate},
	{	MS_FILTER_GET_SAMPLE_RATE, adapter_get_sample_rate},
	{	MS_FILTER_SET_NCHANNELS , adapter_set_nchannels },
	{	MS_FILTER_GET_NCHANNELS, adapter_get_nchannels },
	{	MS_CHANNEL_ADAPTER_SET_OUTPUT_NCHANNELS, adapter_set_out_nchannels },
	{	MS_CHANNEL_ADAPTER_GET_OUTPUT_NCHANNELS, adapter_get_out_nchannels },
	{	0,	NULL }
};

MSFilterDesc ms_channel_adapter_desc={
	MS_CHANNEL_ADAPTER_ID,
	"MSChannelAdapter",
	N_("A filter that converts from mono to stereo and vice versa."),
	MS_FILTER_OTHER,
	NULL,
	2,
	1,
	adapter_init,
	NULL,
	adapter_process,
	NULL,
	adapter_uninit,
	methods
};

MS_FILTER_DESC_EXPORT(ms_channel_adapter_desc)
