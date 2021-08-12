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

static int find_free_pin(MSFilter *mixer) {
	int i;
	for(i=0;i<mixer->desc->ninputs-1;++i){
		if (mixer->inputs[i]==NULL){
			return i;
		}
	}
	ms_fatal("No more free pin in video mixer filter");
	return -1;
}

//--------------------------------------------------------------------------
static MSVideoEndpoint *create_video_placeholder_member(VideoConferenceOneToAll *obj) {
	// create an endpoint for static image
	VideoStream *stream = video_stream_new(obj->getMixer()->factory, 65004, 65005, FALSE);
	media_stream_set_direction(&stream->ms, MediaStreamSendOnly);
	MSMediaStreamIO io = MS_MEDIA_STREAM_IO_INITIALIZER;
	io.input.type = MSResourceCamera;
	io.output.type = MSResourceVoid;
	MSWebCam *nowebcam = ms_web_cam_manager_get_cam(obj->getMixer()->factory->wbcmanager, "StaticImage: Static picture");
	io.input.camera = nowebcam;
	video_stream_start_from_io(stream, obj->getPlaceholderProfile(), "127.0.0.1",65004, "127.0.0.1", 65005, 95, &io);
	return ms_video_endpoint_get_from_stream(stream, FALSE);
}
//--------------------------------------------------------------------------

RtpProfile *VideoConferenceOneToAll::getPlaceholderProfile() const {
	return placeholderProfile;
}

void VideoConferenceOneToAll::addVideoPlaceholderMember() {
	mVideoPlaceholderMember = (VideoEndpoint *)(create_video_placeholder_member(this));

	ms_message("[one to all] add video placeholder to pin %i", mMixer->desc->ninputs-1);
	mVideoPlaceholderMember->mConference = (MSVideoConference *)this;
	mVideoPlaceholderMember->mPin = mMixer->desc->ninputs-1;
	plumb_to_conf(mVideoPlaceholderMember);
	video_stream_set_encoder_control_callback(mVideoPlaceholderMember->mSt, ms_video_conference_process_encoder_control, mVideoPlaceholderMember);
}

VideoConferenceOneToAll::VideoConferenceOneToAll(MSFactory *f, const MSVideoConferenceParams *params) {
	const MSFmtDescriptor *fmt;
	MSVideoSize vsize = {0};

	mTicker=ms_ticker_new();
	ms_ticker_set_name(mTicker,"Video conference MSTicker");
	ms_ticker_set_priority(mTicker,__ms_get_default_prio(FALSE));
	mMixer = ms_factory_create_filter(f, MS_VIDEO_SWITCHER_ID);
	fmt = ms_factory_get_video_format(f, params->codec_mime_type ? params->codec_mime_type : "VP8" ,vsize,0,NULL);
	ms_filter_call_method(mMixer, MS_FILTER_SET_INPUT_FMT, (void*)fmt);
	ms_filter_add_notify_callback(mMixer,on_filter_event,this,TRUE);
	mCfparams=*params;
	placeholderProfile = rtp_profile_new("dummy video");
	PayloadType *pt = payload_type_clone(&payload_type_vp8);
	pt->clock_rate = 90000;
	rtp_profile_set_payload(placeholderProfile, 95, pt);
}

void VideoConferenceOneToAll::addMember(VideoEndpoint *ep) {
	/* now connect to the filter */
	ep->mConference = (MSVideoConference *)this;
	if (mMembers == NULL) {
		addVideoPlaceholderMember();
	} else {
		ms_ticker_detach(mTicker,mMixer);
	}
	setPin(ep);
	ms_message("[one to all] add member to pin %i", ep->mPin);
	plumb_to_conf(ep);
	video_stream_set_encoder_control_callback(ep->mSt, ms_video_conference_process_encoder_control, ep);
	ms_ticker_attach(mTicker,mMixer);
	mMembers=bctbx_list_append(mMembers,ep);
	
	configureOutput(ep);
}

void VideoConferenceOneToAll::removeMember(VideoEndpoint *ep) {
	if (bctbx_list_find(mMembers,ep) == NULL) {
		return;
	}
	video_stream_set_encoder_control_callback(ep->mSt, NULL, NULL);
	ms_ticker_detach(mTicker,mMixer);
	unplumb_from_conf(ep);
	ep->mConference=NULL;
	mMembers=bctbx_list_remove(mMembers,ep);
	if (mMembers!=NULL) {
		ms_message("[one to all] remove member at pin %i", ep->mPin);
		ms_ticker_attach(mTicker,mMixer);
	} else {
		ms_message("[one to all] remove video placeholder member %p", mVideoPlaceholderMember);
		video_stream_set_encoder_control_callback(mVideoPlaceholderMember->mSt, NULL, NULL);
		unplumb_from_conf(mVideoPlaceholderMember);

		if (mVideoPlaceholderMember) {
			video_stream_free(mVideoPlaceholderMember->mSt);
			delete mVideoPlaceholderMember;
		}
		mVideoPlaceholderMember =  NULL;
	}
}

void VideoConferenceOneToAll::setPin(VideoEndpoint *ep) {
	ep->mPin = find_free_pin(getMixer());
	ep->mOutPin = ep->mPin;
}

void VideoConferenceOneToAll::setFocus(VideoEndpoint *ep) {
	ms_filter_call_method(mMixer, MS_VIDEO_SWITCHER_SET_FOCUS, &ep->mPin);
}

void VideoConferenceOneToAll::notifyFir(int pin) {
	ms_filter_call_method(mMixer, MS_VIDEO_SWITCHER_NOTIFY_FIR, &pin);
}

void VideoConferenceOneToAll::notifySli(int pin) {
	ms_filter_call_method(mMixer, MS_VIDEO_SWITCHER_NOTIFY_PLI, &pin);
}

void VideoConferenceOneToAll::setLocalMember(MSVideoConferenceFilterPinControl pc) {
	ms_filter_call_method(mMixer, MS_VIDEO_SWITCHER_SET_AS_LOCAL_MEMBER, &pc);
}

VideoConferenceOneToAll::~VideoConferenceOneToAll() {
	if (placeholderProfile) {
		rtp_profile_destroy(placeholderProfile);
	}
}


} // namespace ms2
#endif
