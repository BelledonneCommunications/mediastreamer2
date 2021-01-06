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
   	//VideoConferenceGeneric()=default;
    virtual ~VideoConferenceGeneric()=default;
    virtual MSVideoEndpoint *getEndpointAtPin(int pin) const;
    virtual void updateBitrateRequest() = 0;
    virtual void addVideoPlaceholderMember() = 0;
    virtual void addMember(MSVideoEndpoint *ep) = 0;
    virtual void removeMember(MSVideoEndpoint *ep) = 0;
    //virtual void setFocus(MSVideoEndpoint *ep) = 0;
    virtual void applyNewBitrateRequest() = 0;
    virtual int getSize() const;
    virtual const bctbx_list_t* getMembers() const;
    virtual MSVideoEndpoint *getVideoPlaceholderMember() const;
    virtual MSFilter *getMixer() const;


    virtual void setLocalMember(MSVideoFilterPinControl pc) = 0;

  protected:

    MSVideoConferenceParams mCfparams;
    MSTicker *mTicker;
    MSFilter *mMixer;
    bctbx_list_t *mMembers;
    int mBitrate;
    MSVideoEndpoint *mVideoPlaceholderMember;
};

class VideoConferenceOneToAll: public VideoConferenceGeneric {
  public:
    VideoConferenceOneToAll(MSFactory *f, const MSVideoConferenceParams *params);
    ~VideoConferenceOneToAll() ;

    void addVideoPlaceholderMember() override;
    void addMember(MSVideoEndpoint *ep) override;
    void removeMember(MSVideoEndpoint *ep) override;
    void setFocus(MSVideoEndpoint *ep) ;
    void applyNewBitrateRequest() override;
    void updateBitrateRequest() override;



    void setLocalMember(MSVideoFilterPinControl pc)override;



};

class VideoConferenceAllToAll: public VideoConferenceGeneric {
public:
  VideoConferenceAllToAll(MSFactory *f, const MSVideoConferenceParams *params);
  ~VideoConferenceAllToAll() ;
  void addMember(MSVideoEndpoint *ep) override;
  void addVideoPlaceholderMember() override;
  void removeMember(MSVideoEndpoint *ep) override;


  void setLocalMember(MSVideoFilterPinControl pc)override;

};

} // namespace ms2
#endif /* MS_VIDEO_CONFERENCE_H */
