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

#ifdef VIDEO_ENABLED
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
	for(i=0;i<mMixer->desc->ninputs-1;++i){
		if (mInputs[i] == -1) {
			mInputs[i] =0;
			return i;
		}
	}
	ms_fatal("No more free input pin in video router filter");
	return -1;
}

int VideoConferenceAllToAll::findSinkPin(std::string participant) {
	for (const bctbx_list_t *elem = mEndpoints; elem != nullptr; elem = elem->next){
		VideoEndpoint *ep_it = (VideoEndpoint *)elem->data;
		if (ep_it->mName.compare(participant) == 0) {
			ms_message("Found sink pin %d for %s", ep_it->mPin, participant.c_str());
			return ep_it->mPin;
		}
	}
	ms_error("Can not find sink pin for %s",participant.c_str());
	return -1;
}

int VideoConferenceAllToAll::findSourcePin(std::string participant) {
	for (const bctbx_list_t *elem = getMembers(); elem != nullptr; elem = elem->next){
		VideoEndpoint *ep_it = (VideoEndpoint *)elem->data;
		if (ep_it->mName.compare(participant) == 0) {
			ms_message("Found source pin %d for %s", ep_it->mPin, participant.c_str());
			return ep_it->mPin;
		}
	}
	ms_error("Can not find source pin for %s",participant.c_str());
	return -1;
}

static void configureEndpoint(VideoEndpoint *ep){
	VideoConferenceAllToAll *conf = (VideoConferenceAllToAll *)ep->mConference;
	conf->connectEndpoint(ep);
}

static void unlinkEndpoint(VideoEndpoint *ep){
	if (ep->connected && !ep->switched) {
		unplumb_from_conf(ep);
		ep->connected = false;
		ms_message("[all to all] unlink endpoint %s with output pin %d source pin %d", ep->mName.c_str(), ep->mOutPin, ep->mSource);
		ep->mSource = -1;
	}
}

static void unconfigureEndpoint(VideoEndpoint *ep, int pin){
	if (ep->mSource == pin) {
		ep->mSource = -1;
		VideoConferenceAllToAll *conf = (VideoConferenceAllToAll *)ep->mConference;
		conf->unconfigureOutput(ep->mOutPin);
	}
}

static MSVideoEndpoint *create_video_placeholder_member(VideoConferenceAllToAll *obj) {
	// create an endpoint for static image
	VideoStream *stream = video_stream_new(obj->getMixer()->factory, 65004, 65005, FALSE);
	media_stream_set_direction(&stream->ms, MediaStreamSendOnly);
	MSMediaStreamIO io = MS_MEDIA_STREAM_IO_INITIALIZER;
	io.input.type = MSResourceCamera;
	io.output.type = MSResourceVoid;
	MSWebCam *nowebcam = ms_web_cam_manager_get_cam(obj->getMixer()->factory->wbcmanager, "StaticImage: Static picture");
	io.input.camera = nowebcam;
	RtpProfile *prof = rtp_profile_new("dummy video");
	PayloadType *pt = payload_type_clone(&payload_type_vp8);
	pt->clock_rate = 90000;
	rtp_profile_set_payload(prof, 95, pt);
	video_stream_start_from_io(stream, prof, "127.0.0.1",65004, "127.0.0.1", 65005, 95, &io);
	return ms_video_endpoint_get_from_stream(stream, FALSE);
}

void VideoConferenceAllToAll::addVideoPlaceholderMember() {
	mVideoPlaceholderMember = (VideoEndpoint *)(create_video_placeholder_member(this));

	mVideoPlaceholderMember->mConference = (MSVideoConference *)this;
	mVideoPlaceholderMember->mPin = mMixer->desc->ninputs-1;
	mVideoPlaceholderMember->switched = TRUE;
	ms_message("[one to all] add video placeholder %p to pin %i", mVideoPlaceholderMember, mVideoPlaceholderMember->mPin);
	plumb_to_conf(mVideoPlaceholderMember);
	video_stream_set_encoder_control_callback(mVideoPlaceholderMember->mSt, ms_video_conference_process_encoder_control, mVideoPlaceholderMember);
}

void VideoConferenceAllToAll::addMember(VideoEndpoint *ep) {
	/* now connect to the filter */
	ep->mConference = (MSVideoConference *)this;
	if (ep->mIsRemote && media_stream_get_direction(&ep->mSt->ms) == MediaStreamSendOnly) {
		if (!ep->mName.empty() && findSinkPin(ep->mName) == -1) { // todo
			ep->mOutPin = findFreeOutputPin();
			ms_message("[all to all] add endpoint %s with output pin %d", ep->mName.c_str(), ep->mOutPin);
			connectEndpoint(ep);
			mEndpoints = bctbx_list_append(mEndpoints, ep);
		}
	} else if (media_stream_get_direction(&ep->mSt->ms) == MediaStreamSendRecv) {
		if (mVideoPlaceholderMember == NULL) {
			addVideoPlaceholderMember();
		} else {
			ms_ticker_detach(mTicker,mMixer);
		}
		ep->mPin = findFreeInputPin();
		ep->mOutPin = findFreeOutputPin();
		ep->switched = TRUE;
		ms_message("[one to all] add member %s (%p) to pin input %d output %d", ep->mName.c_str(), mVideoPlaceholderMember, ep->mPin, ep->mOutPin);
		plumb_to_conf(ep);
		ep->connected = true;
		video_stream_set_encoder_control_callback(ep->mSt, ms_video_conference_process_encoder_control, ep);
		ms_ticker_attach(mTicker,mMixer);
		mMembers=bctbx_list_append(mMembers,ep);

		configureOutput(ep);
	} else {
		if (findSourcePin(ep->mName) == -1) { // todo
			ep->mPin = findFreeInputPin();
			ms_message("[all to all] add remote[%d] member %s with input pin %d", ep->mIsRemote, ep->mName.c_str(), ep->mPin);
			mMembers=bctbx_list_append(mMembers,ep);
			bctbx_list_for_each(mEndpoints, (void (*)(void*))configureEndpoint);
		}
	}
}

static int find_first_remote_endpoint(const VideoEndpoint *ep, const void *dummy)
{
	return !(ep->switched || (ep->mIsRemote && ep->connected));
}

static int find_connected_endpoint(const VideoEndpoint *ep, const void *dummy)
{
	return !ep->connected;
}

void VideoConferenceAllToAll::removeMember(VideoEndpoint *ep) {
	if (bctbx_list_find(mMembers,ep) != NULL) {
		ms_message("[all to all] remove member %s with input pin %d", ep->mName.c_str(), ep->mPin);
		mMembers=bctbx_list_remove(mMembers,ep);
		mInputs[ep->mPin] = -1;
		if (ep->mOutPin > -1) mOutputs[ep->mOutPin] = -1;
		bctbx_list_for_each2(mEndpoints, (void (*)(void*,void*))unconfigureEndpoint, &ep->mPin);
	} else if (bctbx_list_find(mEndpoints,ep) != NULL) {
		ms_message("[all to all] remove endpoint %s with output pin %d", ep->mName.c_str(), ep->mOutPin);
		mEndpoints=bctbx_list_remove(mEndpoints,ep);
		mOutputs[ep->mOutPin] = -1;
	} else {
		return;
	}
	
	if (!ep->connected) {
		ep->mConference=NULL;
		return;
	}

	video_stream_set_encoder_control_callback(ep->mSt, NULL, NULL);
	ms_ticker_detach(mTicker,mMixer);

	if (mMembers == NULL) {
		if (mVideoPlaceholderMember) {
			ms_message("[one to all] remove video placeholder member %p at pin %0d", mVideoPlaceholderMember, mVideoPlaceholderMember->mPin);
			video_stream_set_encoder_control_callback(mVideoPlaceholderMember->mSt, NULL, NULL);
			unplumb_from_conf(mVideoPlaceholderMember);
			video_stream_free(mVideoPlaceholderMember->mSt);
			delete mVideoPlaceholderMember;
			mVideoPlaceholderMember =  NULL;
		}
		// unlink all outputs
		bctbx_list_for_each(mEndpoints, (void (*)(void*))unlinkEndpoint);
	}

	unplumb_from_conf(ep);
	ep->mConference=NULL;

	bctbx_list_t *elem = bctbx_list_find_custom(mEndpoints, (bctbx_compare_func)find_connected_endpoint, NULL);
	if (elem == NULL) {
		// unlink all non switched inputs when no output connected
		bctbx_list_for_each(mMembers, (void (*)(void*))unlinkEndpoint);
	}

	const bool isRemoteFound = (bctbx_list_find_custom(mMembers, (bctbx_compare_func)find_first_remote_endpoint, NULL) != 0);
	// Attach only if at least one remote participant is still in the member list
	if (isRemoteFound && (elem != NULL)) {
		ms_ticker_attach(mTicker,mMixer);
	}
}

void VideoConferenceAllToAll::connectEndpoint(VideoEndpoint *ep) {
	if (ep->connected && ep->mSource > -1) return;
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
		}
		plumb_to_conf(ep);
		video_stream_set_encoder_control_callback(ep->mSt, ms_video_conference_process_encoder_control, ep);
		ep->connected = true;
		ms_message("[all to all] connect endpoint output pin %d with source pin %d", ep->mOutPin, ep->mSource);
		ms_ticker_attach(mTicker,mMixer);
		configureOutput(ep);
	}
}

void VideoConferenceAllToAll::configureOutput(VideoEndpoint *ep) {
	MSVideoRouterPinData pd;
	pd.input = ep->mSource;
	pd.output = ep->mOutPin;
	pd.switched = ep->switched;
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

void VideoConferenceAllToAll::setFocus(VideoEndpoint *ep) {
	ms_filter_call_method(mMixer, MS_VIDEO_ROUTER_SET_FOCUS, &ep->mPin);
}

VideoConferenceAllToAll::~VideoConferenceAllToAll() {
	ms_ticker_destroy(mTicker);
	ms_filter_destroy(mMixer);
}

}// namespace ms2
#endif
