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

#include "video-conference.h"
#include "mediastreamer2/msconference.h"


using namespace ms2;
namespace ms2 {

static int find_free_input_pin(MSFilter *mixer) {
	int i;
	for(i=0;i<mixer->desc->ninputs;++i){
		if (mixer->inputs[i]==NULL){
			return i;
		}
	}
	ms_fatal("No more free input pin in video router filter");
	return -1;
}

static int find_free_output_pin(MSFilter *mixer) {
	int i;
	for(i=0;i<mixer->desc->noutputs;++i){
		if (mixer->outputs[i]==NULL){
			return i;
		}
	}
	ms_fatal("No more free output pin in video router filter");
	return -1;
}

VideoConferenceAllToAll::VideoConferenceAllToAll(MSFactory *f, const MSVideoConferenceParams *params) {
	const MSFmtDescriptor *fmt;
	MSVideoSize vsize = {0};

	mTicker=ms_ticker_new();
	ms_ticker_set_name(mTicker,"Video conference(all to all) MSTicker");
	ms_ticker_set_priority(mTicker,__ms_get_default_prio(FALSE));
	mMixer = ms_factory_create_filter(f, MS_VIDEO_ROUTER_ID);
	fmt = ms_factory_get_video_format(f, params->codec_mime_type ? params->codec_mime_type : "VP8" ,vsize,0,NULL);
	ms_filter_call_method(mMixer, MS_FILTER_SET_INPUT_FMT, (void*)fmt);

	ms_filter_add_notify_callback(mMixer,on_filter_event,this,TRUE);
	mCfparams=*params;
}

int VideoConferenceAllToAll::findInputPin(std::string participant) {
	for (const bctbx_list_t *elem = getMembers(); elem != nullptr; elem = elem->next){
		VideoEndpoint *ep_it = (VideoEndpoint *)elem->data;
		if (ep_it->mName.compare(participant) == 0){
			return ep_it->mPin;
		}
	}
	ms_error("input not found");
	return -1;
}

void VideoConferenceAllToAll::addMember(VideoEndpoint *ep) {
	/* now connect to the filter */
	ep->mConference = (MSVideoConference *)this;
	if (mMembers != NULL || mEndpoints != NULL) {
		ms_ticker_detach(mTicker,mMixer);
	}
	if (ep->mSt->dir != MediaStreamSendOnly) {
		ep->mPin = find_free_input_pin(getMixer());
	} else {
		ep->mPin = findInputPin(ep->mName);
	}
	if (ep->mSt->dir != MediaStreamRecvOnly) {
		ep->mOutPin = find_free_output_pin(getMixer());
	}
	plumb_to_conf(ep);
	video_stream_set_encoder_control_callback(ep->mSt, ms_video_conference_process_encoder_control, ep);
	ms_ticker_attach(mTicker,mMixer);
	if (ep->mSt->dir != MediaStreamSendOnly) {
		mMembers=bctbx_list_append(mMembers,ep);
	} else {
		mEndpoints = bctbx_list_append(mEndpoints, ep);
	}
	configureOutput(ep);
}

void VideoConferenceAllToAll::removeMember(VideoEndpoint *ep) {
	if (ep->mSt->dir != MediaStreamSendOnly) {
		mMembers=bctbx_list_remove(mMembers,ep);
	} else {
		mEndpoints=bctbx_list_remove(mEndpoints,ep);
	}
	video_stream_set_encoder_control_callback(ep->mSt, NULL, NULL);
	ms_ticker_detach(mTicker,mMixer);
	unplumb_from_conf(ep);
	ep->mConference=NULL;
	
	if (mMembers!=NULL || mEndpoints != NULL) {
		ms_ticker_attach(mTicker,mMixer);
	}
}

void VideoConferenceAllToAll::configureOutput(VideoEndpoint *ep) {
	MSVideoRouterPinData pd;
	pd.input = ep->mPin;
	pd.output = ep->mOutPin;
	ms_filter_call_method(mMixer, MS_VIDEO_ROUTER_CONFIGURE_OUTPUT, &pd);
}

void VideoConferenceAllToAll::unconfigureOutput(int pin) {
	ms_filter_call_method(mMixer, MS_VIDEO_ROUTER_UNCONFIGURE_OUTPUT, &pin);
}

void VideoConferenceAllToAll::setLocalMember(MSVideoConferenceFilterPinControl pc) {
	ms_filter_call_method(mMixer, MS_VIDEO_ROUTER_SET_AS_LOCAL_MEMBER, &pc);
}

void VideoConferenceAllToAll::notifyFir(int pin) {
	ms_filter_call_method(mMixer, MS_VIDEO_ROUTER_NOTIFY_FIR, &pin);
}

void VideoConferenceAllToAll::notifySli(int pin) {
	ms_filter_call_method(mMixer, MS_VIDEO_ROUTER_NOTIFY_PLI, &pin);
}

VideoConferenceAllToAll::~VideoConferenceAllToAll() {
	ms_ticker_destroy(mTicker);
	ms_filter_destroy(mMixer);
}

}// namespace ms2
