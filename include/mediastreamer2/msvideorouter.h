/*
 * videoswitcher.h - Video conference filter using switching (no composition), interface definition.
 *
 *
 * Copyright (C) 2014  Belledonne Communications, Grenoble, France
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */


#ifndef msvideorouter_h
#define msvideorouter_h

#include "mediastreamer2/msfilter.h"
#include "video_util.h"

#define MS_VIDEO_ROUTER_CONFIGURE_OUTPUT MS_FILTER_METHOD(MS_VIDEO_ROUTER_ID,0,int)
#define MS_VIDEO_ROUTER_UNCONFIGURE_OUTPUT MS_FILTER_METHOD(MS_VIDEO_ROUTER_ID,1,int)

#define MS_VIDEO_ROUTER_SET_AS_LOCAL_MEMBER MS_FILTER_METHOD(MS_VIDEO_ROUTER_ID,2,MSVideoFilterPinControl)

#define MS_VIDEO_ROUTER_NOTIFY_PLI MS_FILTER_EVENT(MS_VIDEO_ROUTER_ID,3,int)
#define MS_VIDEO_ROUTER_NOTIFY_FIR MS_FILTER_EVENT(MS_VIDEO_ROUTER_ID,4,int)

/**Events raised by the switcher when it needs to receive a key frame in order to complete the switch to new input source*/
#define MS_VIDEO_ROUTER_SEND_FIR MS_FILTER_EVENT(MS_VIDEO_ROUTER_ID,0,int) // ?
#define MS_VIDEO_ROUTER_SEND_PLI MS_FILTER_EVENT(MS_VIDEO_ROUTER_ID,1,int)// ?

#endif
