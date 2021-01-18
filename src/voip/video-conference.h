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

#ifndef MS_VIDEO_CONFERENCE_H
#define MS_VIDEO_CONFERENCE_H

#include "mediastreamer2/msconference.h"
#include "mediastreamer2/msvideoswitcher.h"
#include "mediastreamer2/msvideorouter.h"
#include "private.h"


namespace ms2 {

static MSVideoEndpoint *ms_video_endpoint_new(void);
static void cut_video_stream_graph(MSVideoEndpoint *ep, bool_t is_remote);
static void redo_video_stream_graph(MSVideoEndpoint *ep);
static void ms_video_endpoint_destroy(MSVideoEndpoint *ep);

class VideoConferenceGeneric {
  public:
	VideoConferenceGeneric()=default;
	virtual ~VideoConferenceGeneric()=default;
	virtual MSVideoEndpoint *getEndpointAtPin(int pin) const;
	virtual void updateBitrateRequest();
	virtual void addVideoPlaceholderMember();
	virtual void addMember(MSVideoEndpoint *ep);
	virtual void removeMember(MSVideoEndpoint *ep);
	virtual void applyNewBitrateRequest();
	virtual int getSize() const;
	virtual const bctbx_list_t* getMembers() const;
	virtual MSVideoEndpoint *getVideoPlaceholderMember() const;
	virtual MSFilter *getMixer() const;
	virtual void setLocalMember(MSVideoFilterPinControl pc) = 0;
	virtual void setFocus(MSVideoEndpoint *ep) = 0;

	virtual void notifyFir(int pin) = 0;
	virtual void notifySli(int pin) = 0;
	virtual void configureOutput(int pin) = 0;
	virtual void unConfigureOutput(int pin) = 0;

  protected:
	MSVideoConferenceParams mCfparams;
	MSTicker *mTicker = NULL;
	MSFilter *mMixer = NULL;
	bctbx_list_t *mMembers = NULL;
	int mBitrate = 0;
	MSVideoEndpoint *mVideoPlaceholderMember = NULL;
};

class VideoConferenceOneToAll: public VideoConferenceGeneric {
  public:
	VideoConferenceOneToAll(MSFactory *f, const MSVideoConferenceParams *params);
	~VideoConferenceOneToAll();

	void setLocalMember(MSVideoFilterPinControl pc) override;
	void setFocus(MSVideoEndpoint *ep) override;
	void notifyFir(int pin) override;
	void notifySli(int pin) override;
	void configureOutput(int pin) override {};
	void unConfigureOutput(int pin) override {};
};

class VideoConferenceAllToAll: public VideoConferenceGeneric {
  public:
	VideoConferenceAllToAll(MSFactory *f, const MSVideoConferenceParams *params);
	~VideoConferenceAllToAll();

	void setLocalMember(MSVideoFilterPinControl pc) override;
	void setFocus(MSVideoEndpoint *ep) override {};
	void notifyFir(int pin) override;
	void notifySli(int pin) override;
	void configureOutput(int pin) override;
	void unConfigureOutput(int pin) override;
};

} // namespace ms2
#endif /* MS_VIDEO_CONFERENCE_H */
