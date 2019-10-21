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
#include "g711.h"

typedef struct _AlawEncData{
	MSBufferizer *bz;
	int ptime;
	int maxptime;
	uint32_t ts;
} AlawEncData;

static AlawEncData * alaw_enc_data_new(void){
	AlawEncData *obj=(AlawEncData *)ms_new0(AlawEncData,1);
	obj->bz=ms_bufferizer_new();
	obj->ptime=0;
	obj->maxptime=MIN(MS_DEFAULT_MAX_PTIME,140);
	obj->ts=0;
	return obj;
}

static void alaw_enc_data_destroy(AlawEncData *obj){
	ms_bufferizer_destroy(obj->bz);
	ms_free(obj);
}

static void alaw_enc_init(MSFilter *obj){
	obj->data=alaw_enc_data_new();
}

static void alaw_enc_uninit(MSFilter *obj){
	alaw_enc_data_destroy((AlawEncData*)obj->data);
}

static void alaw_enc_process(MSFilter *obj){
	AlawEncData *dt=(AlawEncData*)obj->data;
	MSBufferizer *bz=dt->bz;
	uint8_t buffer[2240];
	int frame_per_packet=2;
	size_t size_of_pcm=320;

	mblk_t *m;
	
	if (dt->ptime>=10)
	{
		frame_per_packet = dt->ptime/10;
	}

	if (frame_per_packet<=0)
		frame_per_packet=1;
	if (frame_per_packet>14) /* 7*20 == 140 ms max */
		frame_per_packet=14;

	size_of_pcm = 160*frame_per_packet; /* ex: for 20ms -> 160*2==320 */

	while((m=ms_queue_get(obj->inputs[0]))!=NULL){
		ms_bufferizer_put(bz,m);
	}
	while (ms_bufferizer_read(bz,buffer,size_of_pcm)==size_of_pcm){
		mblk_t *o=allocb(size_of_pcm/2,0);
		size_t i;
		for (i=0;i<size_of_pcm/2;i++){
			*o->b_wptr=Snack_Lin2Alaw(((int16_t*)buffer)[i]);
			o->b_wptr++;
		}
		ms_bufferizer_fill_current_metas(bz, o);
		mblk_set_timestamp_info(o,dt->ts);
		dt->ts+=(uint32_t)(size_of_pcm/2);
		ms_queue_put(obj->outputs[0],o);
	}
}

static int enc_add_fmtp(MSFilter *f, void *arg){
	const char *fmtp=(const char *)arg;
	AlawEncData *s=(AlawEncData*)f->data;
	char tmp[30];
	if ( fmtp_get_value ( fmtp,"maxptime",tmp,sizeof ( tmp ) ) ) {
		s->maxptime=MIN(atoi(tmp),MS_DEFAULT_MAX_PTIME);
	}
	if (fmtp_get_value(fmtp,"ptime",tmp,sizeof(tmp))){
		ms_message("%s configured with ptime=%s",f->desc->name, tmp);
		s->ptime=MIN(atoi(tmp),s->maxptime) ;
		if (s->ptime == s->maxptime)
			ms_message("%s ptime set to maxptime=%i", f->desc->name, s->maxptime);
	}
	return 0;
}

static int enc_add_attr(MSFilter *f, void *arg){
	const char *fmtp=(const char *)arg;
	AlawEncData *s=(AlawEncData*)f->data;
	if (strstr(fmtp,"ptime:10")!=NULL){
		s->ptime=10;
	}else if (strstr(fmtp,"ptime:20")!=NULL){
		s->ptime=20;
	}else if (strstr(fmtp,"ptime:30")!=NULL){
		s->ptime=30;
	}else if (strstr(fmtp,"ptime:40")!=NULL){
		s->ptime=40;
	}else if (strstr(fmtp,"ptime:50")!=NULL){
		s->ptime=50;
	}else if (strstr(fmtp,"ptime:60")!=NULL){
		s->ptime=60;
	}else if (strstr(fmtp,"ptime:70")!=NULL){
		s->ptime=70;
	}else if (strstr(fmtp,"ptime:80")!=NULL){
		s->ptime=80;
	}else if (strstr(fmtp,"ptime:90")!=NULL){
		s->ptime=90;
	}else if (strstr(fmtp,"ptime:100")!=NULL){
		s->ptime=100;
	}else if (strstr(fmtp,"ptime:110")!=NULL){
		s->ptime=110;
	}else if (strstr(fmtp,"ptime:120")!=NULL){
		s->ptime=120;
	}else if (strstr(fmtp,"ptime:130")!=NULL){
		s->ptime=130;
	}else if (strstr(fmtp,"ptime:140")!=NULL){
		s->ptime=140;
	}
	return 0;
}

static int get_sample_rate(MSFilter *f, void *arg) {
	*((int *)arg) = 8000;
	return 0;
}

static int dec_have_plc(MSFilter *f, void *arg) {
	*((int *)arg) = 0;
	return 0;
}

static int get_channels(MSFilter *f, void *arg) {
	*((int *)arg) = 1;
	return 0;
}

static int get_ptime(MSFilter *f, void * arg){
	AlawEncData *s=(AlawEncData*)f->data;
	*((int *)arg) = s->ptime;
	return 0;
}

static MSFilterMethod enc_methods[]={
	{	MS_FILTER_ADD_ATTR			,	enc_add_attr},
	{	MS_FILTER_ADD_FMTP			,	enc_add_fmtp},
	{	MS_FILTER_GET_NCHANNELS		,	get_channels},
	{	MS_FILTER_GET_SAMPLE_RATE	,	get_sample_rate},
	{	MS_AUDIO_ENCODER_GET_PTIME	,	get_ptime},
	{	0							,	NULL		}
};

#ifdef _MSC_VER

MSFilterDesc ms_alaw_enc_desc={
	MS_ALAW_ENC_ID,
	"MSAlawEnc",
	N_("ITU-G.711 alaw encoder"),
	MS_FILTER_ENCODER,
	"pcma",
	1,
	1,
	alaw_enc_init,
	NULL,
    alaw_enc_process,
	NULL,
    alaw_enc_uninit,
	enc_methods
};

#else

MSFilterDesc ms_alaw_enc_desc={
	.id=MS_ALAW_ENC_ID,
	.name="MSAlawEnc",
	.text=N_("ITU-G.711 alaw encoder"),
	.category=MS_FILTER_ENCODER,
	.enc_fmt="pcma",
	.ninputs=1,
	.noutputs=1,
	.init=alaw_enc_init,
	.process=alaw_enc_process,
	.uninit=alaw_enc_uninit,
	.methods=enc_methods
};

#endif

static void alaw_dec_process(MSFilter *obj){
	mblk_t *m;
	while((m=ms_queue_get(obj->inputs[0]))!=NULL){
		mblk_t *o;
		msgpullup(m,-1);
		o=allocb((m->b_wptr-m->b_rptr)*2,0);
		mblk_meta_copy(m, o);
		for(;m->b_rptr<m->b_wptr;m->b_rptr++,o->b_wptr+=2){
			*((int16_t*)(o->b_wptr))=Snack_Alaw2Lin(*m->b_rptr);
		}
		freemsg(m);
		ms_queue_put(obj->outputs[0],o);
	}
}

static MSFilterMethod dec_methods[]={
	{	MS_FILTER_GET_NCHANNELS		,	get_channels},
	{	MS_FILTER_GET_SAMPLE_RATE	,	get_sample_rate},
	{	MS_DECODER_HAVE_PLC			,	dec_have_plc},
	{	0							,	NULL		}
};

#ifdef _MSC_VER

MSFilterDesc ms_alaw_dec_desc={
	MS_ALAW_DEC_ID,
	"MSAlawDec",
	N_("ITU-G.711 alaw decoder"),
	MS_FILTER_DECODER,
	"pcma",
	1,
	1,
	NULL,
    NULL,
    alaw_dec_process,
    NULL,
    NULL,
    dec_methods
};

#else

MSFilterDesc ms_alaw_dec_desc={
	.id=MS_ALAW_DEC_ID,
	.name="MSAlawDec",
	.text=N_("ITU-G.711 alaw decoder"),
	.category=MS_FILTER_DECODER,
	.enc_fmt="pcma",
	.ninputs=1,
	.noutputs=1,
	.process=alaw_dec_process,
	.methods=dec_methods
};

#endif

MS_FILTER_DESC_EXPORT(ms_alaw_dec_desc)
MS_FILTER_DESC_EXPORT(ms_alaw_enc_desc)
