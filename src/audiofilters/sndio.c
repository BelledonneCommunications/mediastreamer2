/*
 * Copyright (c) 2021 Landry Breuil
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

#include "mediastreamer2/mssndcard.h"
#include "mediastreamer2/msfilter.h"

#include <sndio.h>

#include <errno.h>
#include <assert.h>
#include <fcntl.h>
#include <sys/time.h>
#include <sys/ioctl.h>
#include <unistd.h>

MSFilter *ms_sndio_read_new(MSSndCard *card);
MSFilter *ms_sndio_write_new(MSSndCard *card);


typedef struct SndioData{
	struct sio_hdl *hdl;
	ms_thread_t thread;
	ms_mutex_t mutex;
	queue_t rq;
	MSBufferizer * bufferizer;
	int rate;
	int rchan;
	int pchan;
	bool_t read_started;
	bool_t write_started;
} SndioData;

static void sndio_set_level(MSSndCard *card, MSSndCardMixerElem e, int percent)
{
	ms_message("sndio_set_level(%d)", percent);
	SndioData *d=(SndioData*)card->data;
	/* set sndio volume from percent */
	if (!sio_setvol(d->hdl, percent * SIO_MAXVOL / 100))
		ms_message("sio_setvol failed");
}

static void sndio_onvol(void * data, unsigned val)
{
	*(int*)data = val * 100 / SIO_MAXVOL;
	ms_message("onvol: sndio val=%u, returning %u", val, *(int*)data);
}

static int sndio_get_level(MSSndCard *card, MSSndCardMixerElem e)
{
	ms_message("sndio_get_level");
	int level = SIO_MAXVOL;
	SndioData *d=(SndioData*)card->data;
	/* return sndio volume as a percent between 0 and SIO_MAXVOL (127) */
	if (!sio_onvol(d->hdl, sndio_onvol, &level))
		ms_message("no volume knob ?");
	return level;

}

static void sndio_set_source(MSSndCard *card, MSSndCardCapture source)
{
	ms_message("sndio_get_source");
}


static void sndio_init(MSSndCard *card){
	ms_message("sndio_init");
	SndioData *d=ms_new0(SndioData,1);
	d->hdl = sio_open(SIO_DEVANY, SIO_PLAY | SIO_REC, 0);
	d->read_started=FALSE;
	d->write_started=FALSE;
	qinit(&d->rq);
	d->bufferizer=ms_bufferizer_new();
	ms_mutex_init(&d->mutex,NULL);
	card->data=d;
}

static void sndio_uninit(MSSndCard *card){
	ms_message("sndio_uninit");
	SndioData *d=(SndioData*)card->data;
	sio_close(d->hdl);
	ms_bufferizer_destroy(d->bufferizer);
	flushq(&d->rq,0);
	ms_mutex_destroy(&d->mutex);
	ms_free(d);
}

static void sndio_unload(MSSndCardManager *m)
{
	ms_message("sndio_unload");
}

static void sndio_detect(MSSndCardManager *m);

MSSndCardDesc sndio_card_desc={
	.driver_type="SNDIO",
	.detect=sndio_detect,
	.init=sndio_init,
	.set_level=sndio_set_level,
	.get_level=sndio_get_level,
	.set_capture=sndio_set_source,
	.create_reader=ms_sndio_read_new,
	.create_writer=ms_sndio_write_new,
	.uninit=sndio_uninit,
	.unload=sndio_unload
};

static void sndio_detect(MSSndCardManager *m){
	ms_message("sndio_detect");
	MSSndCard *card = ms_snd_card_new(&sndio_card_desc);
	card->name = ms_strdup("OpenBSD backend");
	card->device_type = MS_SND_CARD_DEVICE_TYPE_TELEPHONY;
	card->capabilities = MS_SND_CARD_CAP_PLAYBACK|MS_SND_CARD_CAP_CAPTURE;
	ms_snd_card_manager_prepend_card(m, card);
}

static void * sndio_thread(void *p){
	ms_message("sndio_thread");
	MSSndCard *card=(MSSndCard*)p;
	SndioData *d=(SndioData*)card->data;
	uint8_t *wbuf = NULL, *rbuf = NULL;
	mblk_t *rm = NULL;
	size_t n;
	size_t todo;
	size_t rblksz, wblksz;
	unsigned char *data;
	struct sio_par par;
	int prime;

	sio_initpar(&par);

	/* sndio doesn't support a-low and u-low */
	par.bits = 16;
	par.bps  = SIO_BPS(par.bits);
	par.sig  = 1;
	par.le   = SIO_LE_NATIVE;

	par.rchan = d->rchan;
	par.pchan = d->pchan;
	par.rate = d->rate;
	ms_message("sndio: calling setpar with rchan=%d, pchan=%d, rate=%d", d->rchan, d->pchan, d->rate);

	if (!sio_setpar(d->hdl, &par)) {
		ms_message("sio_setpar call failed");
		return NULL;
	}

	if (!sio_getpar(d->hdl, &par)) {
		ms_message("sio_getpar call failed");
		return NULL;
	}
	d->rchan = par.rchan;
	d->pchan = par.pchan;
	d->rate = par.rate;
	ms_message("sndio: after setpar/getpar, rchan=%d, pchan=%d, rate=%d, round=%d, bufsz=%d, appbufsz=%d", d->rchan, d->pchan, d->rate, par.round, par.bufsz, par.appbufsz);

	wblksz = par.round * par.bps * par.pchan;
	rblksz = par.round * par.bps * par.rchan;
	wbuf = (uint8_t *)malloc(wblksz);
	rbuf = (uint8_t *)malloc(rblksz);

	if (!sio_start(d->hdl)) {
		ms_message("sndio: could not start");
		return NULL;
	}

	ms_message("sndio_thread: started, wblksz=%zu, rblksz=%zu", wblksz, rblksz);

	/*
	 * device starts as soon as play buffer is full (bufsz sample
	 * written). Calling sio_read() before that (i.e. while device
	 * is still stopped) will deadlock.
	 */
	prime = par.bufsz / par.round;

	while (d->read_started || d->write_started) {
		ms_message("sndio_thread: in mainloop, read_started=%d, write_started=%d", d->read_started, d->write_started);

		/*
		 * read and queue a block, but only if
		 * device has started (bufsz written)
		 */
		if (prime == 0) {
			if (d->read_started) {
				rm = allocb(rblksz, 0);
				data = rm->b_wptr;
			} else
				data = rbuf;

			todo = rblksz;
			while (todo > 0) {
				ms_message("sndio_thread: todo=%zu", todo);
				n = sio_read(d->hdl, data, todo);
				if (n == 0) {
					ms_message("sndio_thread: sio_read error");
					goto exit_thread;
				}
				ms_message("sndio_thread: read %zu blocks from sndio", n);
				data += n;
				todo -= n;
			}
			if (d->read_started) {
				rm->b_wptr += rblksz;
				ms_message("sndio_thread: read started, queuing block (queue has %d items)", d->rq.q_mcount);
				ms_mutex_lock(&d->mutex);
				putq(&d->rq, rm);
				ms_mutex_unlock(&d->mutex);
				rm = NULL;
			} else
				ms_message("sndio_thread: read not started, dropping block");
		} else {
			ms_message("sndio_thread: prime = %d, skipped block", prime);
			prime--;
		}

		if (d->write_started) {
			n = ms_bufferizer_read(d->bufferizer, wbuf, wblksz);
			if (n > 0) {
				ms_message("sndio_thread: got %zu blocks from bufferizer", n);
			} else
				ms_message("sndio_thread: bufferizer empty ?");
		} else {
			n = 0;
			ms_message("sndio_thread: write not started, zeroing wbuf");
		}

		/* fill with silence remaining buffer */
		memset(wbuf + n, 0, wblksz - n);

		n = sio_write(d->hdl, wbuf, wblksz);
		ms_message("sndio_thread: wrote %zu blocks to sndio", n);
	}

exit_thread:
	ms_message("sndio_thread: stopping");
	if (!sio_stop(d->hdl)) {
		ms_message("sndio: could not stop");
		return NULL;
	}

	if (rm != NULL) {
		freeb(rm);
		rm = NULL;
	}

	free(wbuf);
	free(rbuf);

	return NULL;
}

static mblk_t *sndio_get(MSSndCard *card){
	SndioData *d=(SndioData*)card->data;
	if (d->rq.q_mcount > 0)
		ms_message("sndio_get (queue has %d items)", d->rq.q_mcount);
	mblk_t *m;
	ms_mutex_lock(&d->mutex);
	m=getq(&d->rq);
	ms_mutex_unlock(&d->mutex);
	return m;
}

static void sndio_put(MSSndCard *card, mblk_t *m){
	ms_message("sndio_put");
	SndioData *d=(SndioData*)card->data;
	ms_mutex_lock(&d->mutex);
	ms_bufferizer_put(d->bufferizer,m);
	ms_mutex_unlock(&d->mutex);
}


static void sndio_read_preprocess(MSFilter *f){
	ms_message("sndio_read_preprocess");
	MSSndCard *card=(MSSndCard*)f->data;
	SndioData *d=(SndioData*)card->data;
	if (d->read_started==FALSE && d->write_started==FALSE){
		d->read_started=TRUE;
		ms_thread_create(&d->thread,NULL,sndio_thread,card);
	}else d->read_started=TRUE;
}

static void sndio_read_postprocess(MSFilter *f){
	ms_message("sndio_read_postprocess");
	MSSndCard *card=(MSSndCard*)f->data;
	SndioData *d=(SndioData*)card->data;
	d->read_started=FALSE;
	if (d->write_started==FALSE){
		ms_message("sndio_read_postprocess: waiting for thread exit");
		ms_thread_join(d->thread,NULL);
	}
}

static void sndio_read_process(MSFilter *f){
	//ms_message("sndio_read_process");
	MSSndCard *card=(MSSndCard*)f->data;
	/* defined in ortp/str_utils.h */
	mblk_t *m;
	while((m=sndio_get(card))!=NULL){
		ms_queue_put(f->outputs[0],m);
	}
}

static void sndio_write_preprocess(MSFilter *f){
	ms_message("sndio_write_preprocess");
	MSSndCard *card=(MSSndCard*)f->data;
	SndioData *d=(SndioData*)card->data;
	if (d->read_started==FALSE && d->write_started==FALSE){
		d->write_started=TRUE;
		ms_thread_create(&d->thread,NULL,sndio_thread,card);
	}else{
		d->write_started=TRUE;
	}
}

static void sndio_write_postprocess(MSFilter *f){
	ms_message("sndio_write_postprocess");
	MSSndCard *card=(MSSndCard*)f->data;
	SndioData *d=(SndioData*)card->data;
	d->write_started=FALSE;
	if (d->read_started==FALSE){
		ms_message("sndio_write_postprocess: waiting for thread exit");
		ms_thread_join(d->thread,NULL);
	}
}

static void sndio_write_process(MSFilter *f){
	ms_message("sndio_write_process");
	MSSndCard *card=(MSSndCard*)f->data;
	mblk_t *m;
	while((m=ms_queue_get(f->inputs[0]))!=NULL){
		sndio_put(card,m);
	}
}

static int sndio_set_rate(MSFilter *f, void *arg){
	MSSndCard *card=(MSSndCard*)f->data;
	SndioData *d=(SndioData*)card->data;
	d->rate=*((int*)arg);
	ms_message("sndio_set_rate(%d)", d->rate);
	return 0;
}

static int sndio_get_rate(MSFilter *f, void *arg){
	MSSndCard *card=(MSSndCard*)f->data;
	SndioData *d=(SndioData*)card->data;
	ms_message("sndio_get_rate() returning %d", d->rate);
	/* set arg with rate */
	*((int*)arg)=d->rate;
	return 0;
}

static int sndio_set_play_nchannels(MSFilter *f, void *arg){
	MSSndCard *card=(MSSndCard*)f->data;
	SndioData *d=(SndioData*)card->data;
	/* set sndio pchan from arg */
	d->pchan=*((int*)arg);
	ms_message("sndio_set_play_nchannels(%d)", d->pchan);
	return 0;
}

static int sndio_get_play_nchannels(MSFilter *f, void *arg){
	MSSndCard *card=(MSSndCard*)f->data;
	SndioData *d=(SndioData*)card->data;
	ms_message("sndio_get_play_nchannels() returning %d", d->pchan);
	/* set arg with sndio pchan, should actually call sio_getpar ? */
	*((int*)arg)=d->pchan;
	return 0;
}

static int sndio_set_record_nchannels(MSFilter *f, void *arg){
	MSSndCard *card=(MSSndCard*)f->data;
	SndioData *d=(SndioData*)card->data;
	/* set sndio rchan from arg */
	d->rchan=*((int*)arg);
	ms_message("sndio_set_record_nchannels(%d)", d->rchan);
	return 0;
}

static int sndio_get_record_nchannels(MSFilter *f, void *arg){
	MSSndCard *card=(MSSndCard*)f->data;
	SndioData *d=(SndioData*)card->data;
	ms_message("sndio_get_record_nchannels() returning %d", d->rchan);
	/* set arg with sndio rchan */
	*((int*)arg)=d->rchan;
	return 0;
}

static MSFilterMethod sndio_read_methods[]={
	{	MS_FILTER_SET_SAMPLE_RATE	, sndio_set_rate },
	{	MS_FILTER_GET_SAMPLE_RATE	, sndio_get_rate },
	{	MS_FILTER_SET_NCHANNELS		, sndio_set_record_nchannels },
	{	MS_FILTER_GET_NCHANNELS		, sndio_get_record_nchannels },
	{	0				, NULL		}
};

static MSFilterMethod sndio_write_methods[]={
	{	MS_FILTER_SET_SAMPLE_RATE	, sndio_set_rate },
	{	MS_FILTER_GET_SAMPLE_RATE	, sndio_get_rate },
	{	MS_FILTER_SET_NCHANNELS		, sndio_set_play_nchannels },
	{	MS_FILTER_GET_NCHANNELS		, sndio_get_play_nchannels },
	{	0				, NULL		}
};
MSFilterDesc sndio_read_desc={
	.id=MS_SNDIO_READ_ID,
	.name="MSSndioRead",
	.text="Sound capture filter for SNDIO drivers",
	.category=MS_FILTER_OTHER,
	.ninputs=0,
	.noutputs=1,
	.preprocess=sndio_read_preprocess,
	.process=sndio_read_process,
	.postprocess=sndio_read_postprocess,
	.methods=sndio_read_methods
};


MSFilterDesc sndio_write_desc={
	.id=MS_SNDIO_WRITE_ID,
	.name="MSSndioWrite",
	.text="Sound playback filter for SNDIO drivers",
	.category=MS_FILTER_OTHER,
	.ninputs=1,
	.noutputs=0,
	.preprocess=sndio_write_preprocess,
	.process=sndio_write_process,
	.postprocess=sndio_write_postprocess,
	.methods=sndio_write_methods
};

MSFilter *ms_sndio_read_new(MSSndCard *card){
	ms_message("sndio_read_new");
	MSFilter *f=ms_factory_create_filter_from_desc(ms_snd_card_get_factory(card),&sndio_read_desc);
	f->data=card;
	return f;
}


MSFilter *ms_sndio_write_new(MSSndCard *card){
	ms_message("sndio_write_new");
	MSFilter *f=ms_factory_create_filter_from_desc(ms_snd_card_get_factory(card),&sndio_write_desc);
	f->data=card;
	return f;
}

MS_FILTER_DESC_EXPORT(sndio_read_desc)
MS_FILTER_DESC_EXPORT(sndio_write_desc)
