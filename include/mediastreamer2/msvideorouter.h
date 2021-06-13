
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

#ifndef msvideorouter_h
#define msvideorouter_h

#include "mediastreamer2/msfilter.h"
#include "mediastreamer2/msvideoswitcher.h"

typedef struct _MSVideoRouterPinData{
	int input;
	int output;
}MSVideoRouterPinData;
#define MS_VIDEO_ROUTER_CONFIGURE_OUTPUT MS_FILTER_METHOD(MS_VIDEO_ROUTER_ID,0,MSVideoRouterPinData)
#define MS_VIDEO_ROUTER_UNCONFIGURE_OUTPUT MS_FILTER_METHOD(MS_VIDEO_ROUTER_ID,1,int)

#define MS_VIDEO_ROUTER_SET_AS_LOCAL_MEMBER MS_FILTER_METHOD(MS_VIDEO_ROUTER_ID,2,MSVideoConferenceFilterPinControl)

#define MS_VIDEO_ROUTER_NOTIFY_PLI MS_FILTER_EVENT(MS_VIDEO_ROUTER_ID,2,int)
#define MS_VIDEO_ROUTER_NOTIFY_FIR MS_FILTER_EVENT(MS_VIDEO_ROUTER_ID,3,int)

/**Events raised by the router when it needs to receive a key frame in order to complete the switch to new input source*/
#define MS_VIDEO_ROUTER_SEND_FIR MS_FILTER_EVENT(MS_VIDEO_ROUTER_ID,0,int)
#define MS_VIDEO_ROUTER_SEND_PLI MS_FILTER_EVENT(MS_VIDEO_ROUTER_ID,1,int)

#endif
