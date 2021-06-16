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

	std::fill_n(mOutputs, ROUTER_MAX_OUTPUT_CHANNELS, -1);
	std::fill_n(mInputs, ROUTER_MAX_INPUT_CHANNELS, -1);
}

int VideoConferenceAllToAll::findFreeOutputPin() {
	int i;
	for(i=0;i<mMixer->desc->noutputs;++i){
		if (mOutputs[i] == -1) {
			mOutputs[i] = 0;
			return i;
		}
	}
	ms_fatal("No more free output pin in video router filter");
	return -1;
}

int VideoConferenceAllToAll::findFreeInputPin() {
	int i;
	for(i=0;i<mMixer->desc->ninputs;++i){
		if (mInputs[i] == -1) {
			mInputs[i] =0;
			return i;
		}
	}
	ms_fatal("No more free input pin in video router filter");
	return -1;
}

int VideoConferenceAllToAll::findSourcePin(std::string participant) {
	for (const bctbx_list_t *elem = getMembers(); elem != nullptr; elem = elem->next){
		VideoEndpoint *ep_it = (VideoEndpoint *)elem->data;
		if (ep_it->mName.compare(participant) == 0) {
			ms_message("Find source pin %d for %s", ep_it->mPin, participant.c_str());
			return ep_it->mPin;
		}
	}
	ms_message("Can not find source pin for %s",participant.c_str());
	return -1;
}

static void configureEndpoint(VideoEndpoint *ep){
	VideoConferenceAllToAll *conf = (VideoConferenceAllToAll *)ep->mConference;
	conf->connectEndpoint(ep);
}

static void unlinkEndpoint(VideoEndpoint *ep){
	unplumb_from_conf(ep);
	ep->connected = false;
	ep->mSource = -1;
	ms_message("[all to all] unlink endpoint %s with output pin %d input pin %d", ep->mName.c_str(), ep->mOutPin, ep->mPin);
}

void VideoConferenceAllToAll::addMember(VideoEndpoint *ep) {
	/* now connect to the filter */
	ep->mConference = (MSVideoConference *)this;
	if (media_stream_get_direction(&ep->mSt->ms) == MediaStreamSendRecv) {

	} else if (media_stream_get_direction(&ep->mSt->ms) == MediaStreamSendOnly) {
		ep->mOutPin = findFreeOutputPin();
		ms_message("[all to all] add endpoint %s with output pin %d", ep->mName.c_str(), ep->mOutPin);
		connectEndpoint(ep);
		mEndpoints = bctbx_list_append(mEndpoints, ep);
	} else {
		ep->mPin = findFreeInputPin();
		ms_message("[all to all] add member %s with input pin %d", ep->mName.c_str(), ep->mPin);
		mMembers=bctbx_list_append(mMembers,ep);
		bctbx_list_for_each(mEndpoints, (void (*)(void*))configureEndpoint);
   }
}

void VideoConferenceAllToAll::removeMember(VideoEndpoint *ep) {
	if (media_stream_get_direction(&ep->mSt->ms) != MediaStreamSendOnly) {
		ms_message("[all to all] remove member %s with input pin %d", ep->mName.c_str(), ep->mPin);
		mMembers=bctbx_list_remove(mMembers,ep);
		mInputs[ep->mPin] = -1;
	} else {
		ms_message("[all to all] remove endpoint %s with output pin %d", ep->mName.c_str(), ep->mOutPin);
		mEndpoints=bctbx_list_remove(mEndpoints,ep);
		mOutputs[ep->mOutPin] = -1;
	}
	
	if (!ep->connected) {
		ep->mConference=NULL;
		return;
	}

	video_stream_set_encoder_control_callback(ep->mSt, NULL, NULL);
	ms_ticker_detach(mTicker,mMixer);

	if (mMembers == NULL) {
		// unlink all outputs
		bctbx_list_for_each(mEndpoints, (void (*)(void*))unlinkEndpoint);
	}

	unplumb_from_conf(ep);
	ep->mConference=NULL;

	if (mEndpoints == NULL) {
		// unlink all inputs
		bctbx_list_for_each(mMembers, (void (*)(void*))unlinkEndpoint);
	}

	if (mMembers!=NULL && mEndpoints != NULL) {
		ms_ticker_attach(mTicker,mMixer);
	}
}

void VideoConferenceAllToAll::connectEndpoint(VideoEndpoint *ep) {
	if (ep->connected) return;
	ep->mSource = findSourcePin(ep->mName);
	if (ep->mSource > -1) {
		if (mEndpoints != NULL) {
			ms_ticker_detach(mTicker,mMixer);
		}
		VideoEndpoint *member = getMemberAtPin(ep->mSource);
		if (!member->connected) {
			ms_message("[all to all] connect member %s with input pin %d", member->mName.c_str(), member->mPin);
			plumb_to_conf(member);
			member->connected = true;
			video_stream_set_encoder_control_callback(member->mSt, ms_video_conference_process_encoder_control, ep);
		}
		plumb_to_conf(ep);
		ep->connected = true;
		ms_ticker_attach(mTicker,mMixer);
		configureOutput(ep);
	}
}

void VideoConferenceAllToAll::configureOutput(VideoEndpoint *ep) {
	MSVideoRouterPinData pd;
	pd.input = ep->mSource;
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
