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

#include "mediastreamer2/msconference.h"
#include "mediastreamer2/msvideoswitcher.h"
#include "private.h"

static void ms_video_conference_update_bitrate_request(MSVideoConference *obj);

struct _MSVideoConference{
	MSVideoConferenceParams params;
	MSAudioConference *audioconf;
	MSTicker *ticker;
	MSFilter *mixer;
	bctbx_list_t *members;
	int bitrate;
};

struct _MSVideoEndpoint{
	VideoStream *st;
	void *user_data;
	MSCPoint out_cut_point;
	MSCPoint out_cut_point_prev;
	MSCPoint in_cut_point;
	MSCPoint in_cut_point_prev;
	MSCPoint mixer_in;
	MSCPoint mixer_out;
	MSVideoConference *conference;
	int pin;
	int is_remote;
	int last_tmmbr_received; /*Value in bits/s */
};

static MSVideoEndpoint *get_endpoint_at_pin(MSVideoConference *obj, int pin){
	const MSList *it;
	for (it=obj->members;it!=NULL;it=it->next){
		MSVideoEndpoint *ep=(MSVideoEndpoint*)it->data;
		if (ep->pin==pin) return ep;
	}
	return NULL;
}

static void on_switcher_event(void *data, MSFilter *f, unsigned int event_id, void *event_data){
	MSVideoConference *obj=(MSVideoConference*)data;
	int pin=*(int*)event_data;
	MSVideoEndpoint *ep=get_endpoint_at_pin(obj,pin);
	if (ep){
		if (event_id==MS_VIDEO_SWITCHER_NEEDS_KEYFRAME){
			ms_message("Switcher needs a key frame for [%s] endpoint created from VideoStream [%p]",
				   ep->is_remote ? "remote" : "local",
				   ep->st);
			if (ep->is_remote){
				video_stream_send_fir(ep->st);
			}else{
				video_stream_send_vfu(ep->st);
			}
		}
	}else{
		ms_error("Switcher generated an event for an unknown pin [%i]",pin);
	}
}


MSVideoConference * ms_video_conference_new(MSFactory *f, const MSVideoConferenceParams *params){
	MSVideoConference *obj=ms_new0(MSVideoConference,1);
	const MSFmtDescriptor *fmt;
	MSVideoSize vsize = {0};
	
	obj->ticker=ms_ticker_new();
	ms_ticker_set_name(obj->ticker,"Video conference MSTicker");
	ms_ticker_set_priority(obj->ticker,__ms_get_default_prio(FALSE));
	obj->mixer = ms_factory_create_filter(f, MS_VIDEO_SWITCHER_ID);
	fmt = ms_factory_get_video_format(f, params->codec_mime_type ? params->codec_mime_type : "VP8" ,vsize,0,NULL);
	ms_filter_call_method(obj->mixer, MS_FILTER_SET_INPUT_FMT, (void*)fmt);
	ms_filter_add_notify_callback(obj->mixer,on_switcher_event,obj,TRUE);
	obj->params=*params;
	return obj;
}

static MSCPoint just_before(MSFilter *f){
	MSQueue *q;
	MSCPoint pnull={0};
	if ((q=f->inputs[0])!=NULL){
		return q->prev;
	}
	ms_fatal("No filter before %s",f->desc->name);
	return pnull;
}

static MSCPoint just_after(MSFilter *f){
	MSQueue *q;
	MSCPoint pnull={0};
	if ((q=f->outputs[0])!=NULL){
		return q->next;
	}
	ms_fatal("No filter after %s",f->desc->name);
	return pnull;
}


static void ms_video_endpoint_tmmbr_received(const OrtpEventData *evd, void *user_pointer){
	MSVideoEndpoint *ep = (MSVideoEndpoint*)user_pointer;
	
	switch (rtcp_RTPFB_get_type(evd->packet)) {
		case RTCP_RTPFB_TMMBR: {
			int tmmbr_mxtbr = (int)rtcp_RTPFB_tmmbr_get_max_bitrate(evd->packet);

			ms_message("MSVideoConference [%p]: received a TMMBR for bitrate %i kbits/s on pin %i."
						, ep->conference, (int)(tmmbr_mxtbr/1000), ep->pin);
			ep->last_tmmbr_received = tmmbr_mxtbr;
			ms_video_conference_update_bitrate_request(ep->conference);
		}
		break;
		default:
		break;
	}
}


static void cut_video_stream_graph(MSVideoEndpoint *ep, bool_t is_remote){
	VideoStream *st=ep->st;

	/*stop the video graph*/
	ms_ticker_detach(st->ms.sessions.ticker, st->source);
	ms_ticker_detach(st->ms.sessions.ticker, st->ms.rtprecv);
	ep->is_remote=is_remote;
	ep->in_cut_point_prev.pin=0;
	if (is_remote){
		/*we need to cut just after the rtp recveiver*/
		ep->in_cut_point_prev.filter=st->ms.rtprecv;
	}else{
		/*we need to cut just after the encoder*/
		ep->in_cut_point_prev.filter=st->ms.encoder;
	}
	ep->in_cut_point=just_after(ep->in_cut_point_prev.filter);
	ms_filter_unlink(ep->in_cut_point_prev.filter,ep->in_cut_point_prev.pin,ep->in_cut_point.filter, ep->in_cut_point.pin);

	ep->out_cut_point.pin=0;
	if (is_remote){
		ep->out_cut_point.filter=st->ms.rtpsend;
	}else{
		ep->out_cut_point.filter=st->ms.decoder;
	}
	ep->out_cut_point_prev=just_before(ep->out_cut_point.filter);
	ms_filter_unlink(ep->out_cut_point_prev.filter,ep->out_cut_point_prev.pin,ep->out_cut_point.filter,ep->out_cut_point.pin);

	ep->mixer_in=ep->in_cut_point_prev;
	ep->mixer_out=ep->out_cut_point;
	
	/* Replaces own's MediaStream tmmbr handler by the video conference implementation.*/
	media_stream_remove_tmmbr_handler((MediaStream*)ep->st, media_stream_tmmbr_received, ep->st);
	media_stream_add_tmmbr_handler((MediaStream*)ep->st, ms_video_endpoint_tmmbr_received, ep);
}


static void redo_video_stream_graph(MSVideoEndpoint *ep){
	VideoStream *st=ep->st;
	
	media_stream_remove_tmmbr_handler((MediaStream*)ep->st, ms_video_endpoint_tmmbr_received, ep);
	media_stream_add_tmmbr_handler((MediaStream*)ep->st, media_stream_tmmbr_received, ep->st);
	ms_filter_link(ep->in_cut_point_prev.filter,ep->in_cut_point_prev.pin,ep->in_cut_point.filter,ep->in_cut_point.pin);
	ms_filter_link(ep->out_cut_point_prev.filter,ep->out_cut_point_prev.pin,ep->out_cut_point.filter,ep->out_cut_point.pin);
	ms_ticker_attach_multiple(st->ms.sessions.ticker, st->source, st->ms.rtprecv);
	
}

static int find_free_pin(MSFilter *mixer){
	int i;
	for(i=0;i<mixer->desc->ninputs;++i){
		if (mixer->inputs[i]==NULL){
			return i;
		}
	}
	ms_fatal("No more free pin in video mixer filter");
	return -1;
}

static void plumb_to_conf(MSVideoEndpoint *ep){
	MSVideoConference *conf=ep->conference;
	MSVideoSwitcherPinControl pc;
	
	ep->pin=find_free_pin(conf->mixer);
	
	if (ep->mixer_in.filter){
		ms_filter_link(ep->mixer_in.filter,ep->mixer_in.pin,conf->mixer,ep->pin);
	}
	if (ep->mixer_out.filter){
		ms_filter_link(conf->mixer,ep->pin,ep->mixer_out.filter,ep->mixer_out.pin);
	}
	pc.pin = ep->pin;
	pc.enabled = !ep->is_remote;
	ms_filter_call_method(conf->mixer, MS_VIDEO_SWITCHER_SET_AS_LOCAL_MEMBER, &pc);
}

void ms_video_conference_add_member(MSVideoConference *obj, MSVideoEndpoint *ep){
	/* now connect to the mixer */
	ep->conference=obj;
	if (obj->members!=NULL) ms_ticker_detach(obj->ticker,obj->mixer);
	plumb_to_conf(ep);
	ms_ticker_attach(obj->ticker,obj->mixer);
	obj->members=bctbx_list_append(obj->members,ep);
}

static void unplumb_from_conf(MSVideoEndpoint *ep){
	MSVideoConference *conf=ep->conference;
	
	if (ep->mixer_in.filter){
		ms_filter_unlink(ep->mixer_in.filter,ep->mixer_in.pin,conf->mixer,ep->pin);
	}
	if (ep->mixer_out.filter){
		ms_filter_unlink(conf->mixer,ep->pin,ep->mixer_out.filter,ep->mixer_out.pin);
	}
}

void ms_video_conference_remove_member(MSVideoConference *obj, MSVideoEndpoint *ep){
	ms_ticker_detach(obj->ticker,obj->mixer);
	unplumb_from_conf(ep);
	ep->conference=NULL;
	obj->members=bctbx_list_remove(obj->members,ep);
	if (obj->members!=NULL) ms_ticker_attach(obj->ticker,obj->mixer);
}

void ms_video_conference_set_focus(MSVideoConference *obj, MSVideoEndpoint *ep){
	ms_filter_call_method(obj->mixer, MS_VIDEO_SWITCHER_SET_FOCUS, &ep->pin);
}

static void ms_video_conference_apply_new_bitrate_request(MSVideoConference *obj){
	const bctbx_list_t *elem;
	for (elem = obj->members; elem != NULL; elem = elem->next){
		MSVideoEndpoint *ep = (MSVideoEndpoint*) elem->data;
		if (ep->is_remote){
			rtp_session_send_rtcp_fb_tmmbr(ep->st->ms.sessions.rtp_session, (uint64_t)obj->bitrate);
		}else{
			media_stream_process_tmmbr((MediaStream*)ep->st, obj->bitrate);
		}
	}
}

static void ms_video_conference_update_bitrate_request(MSVideoConference *obj){
	const bctbx_list_t *elem;
	int min_of_tmmbr = -1;
	for (elem = obj->members; elem != NULL; elem = elem->next){
		MSVideoEndpoint *ep = (MSVideoEndpoint*) elem->data;
		if (ep->last_tmmbr_received != 0){
			if (min_of_tmmbr == -1){
				min_of_tmmbr = ep->last_tmmbr_received;
			}else{
				if (ep->last_tmmbr_received < min_of_tmmbr){
					min_of_tmmbr = ep->last_tmmbr_received;
				}
			}
		}
	}
	if (min_of_tmmbr != -1){
		if (obj->bitrate != min_of_tmmbr){
			obj->bitrate = min_of_tmmbr;
			ms_message("MSVideoConference [%p]: new bitrate requested: %i kbits/s.", obj, obj->bitrate/1000);
			ms_video_conference_apply_new_bitrate_request(obj);
		}
	}
}

void ms_video_conference_set_audio_conference(MSVideoConference *obj, MSAudioConference *audioconf){
	if (obj->audioconf){
		ms_audio_conference_set_video_conference(obj->audioconf, NULL);
	}
	obj->audioconf = audioconf;
	if (audioconf) ms_audio_conference_set_video_conference(audioconf, obj);
}

int ms_video_conference_get_size(MSVideoConference *obj){
	return bctbx_list_size(obj->members);
}

const bctbx_list_t* ms_video_conference_get_members(const MSVideoConference *obj){
	return obj->members;
}


void ms_video_conference_destroy(MSVideoConference *obj){
	ms_ticker_destroy(obj->ticker);
	ms_filter_destroy(obj->mixer);
	ms_free(obj);
}

MSVideoEndpoint *ms_video_endpoint_new(void){
	MSVideoEndpoint *ep=ms_new0(MSVideoEndpoint,1);
	return ep;
}

void ms_video_endpoint_set_user_data(MSVideoEndpoint *ep, void *user_data){
	ep->user_data = user_data;
}


void * ms_video_endpoint_get_user_data(const MSVideoEndpoint *ep){
	return ep->user_data;
}


MSVideoEndpoint * ms_video_endpoint_get_from_stream(VideoStream *st, bool_t is_remote){
	MSVideoEndpoint *ep=ms_video_endpoint_new();
	ep->st=st;
	cut_video_stream_graph(ep,is_remote);
	return ep;
}

void ms_video_endpoint_destroy(MSVideoEndpoint *ep){
	ms_free(ep);
}

void ms_video_endpoint_release_from_stream(MSVideoEndpoint *obj){
	redo_video_stream_graph(obj);
	ms_video_endpoint_destroy(obj);
}
