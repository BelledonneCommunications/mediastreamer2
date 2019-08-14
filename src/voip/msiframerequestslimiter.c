/*
 * Copyright (c) 2010-2019 Belledonne Communications SARL.
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

#include "mediastreamer2/mscodecutils.h"

void ms_iframe_requests_limiter_init(MSIFrameRequestsLimiterCtx *obj, int min_iframe_interval) {
	memset(obj, 0, sizeof(MSIFrameRequestsLimiterCtx));
	obj->last_sent_iframe_time = (uint64_t)-1;
	obj->min_iframe_interval = min_iframe_interval;
}

void ms_iframe_requests_limiter_request_iframe(MSIFrameRequestsLimiterCtx *obj) {
	obj->iframe_required = TRUE;
}

bool_t ms_iframe_requests_limiter_iframe_requested(const MSIFrameRequestsLimiterCtx *obj, uint64_t curtime) {
	return obj->iframe_required && ( obj->last_sent_iframe_time == (uint64_t)-1 || (int)(curtime - obj->last_sent_iframe_time) > obj->min_iframe_interval);
}

void ms_iframe_requests_limiter_notify_iframe_sent(MSIFrameRequestsLimiterCtx *obj, uint64_t curtime) {
	obj->iframe_required = FALSE;
	obj->last_sent_iframe_time = curtime;
}
