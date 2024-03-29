/*
 * Copyright (c) 2010-2022 Belledonne Communications SARL.
 *
 * This file is part of mediastreamer2
 * (see https://gitlab.linphone.org/BC/public/mediastreamer2).
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as
 * published by the Free Software Foundation, either version 3 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef mseventqueue_h
#define mseventqueue_h

#include <mediastreamer2/mscommon.h>
#include <mediastreamer2/msfactory.h>

typedef struct _MSEventQueue MSEventQueue;

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Creates an event queue to receive notifications from MSFilters.
 *
 * The queue can be installed to be global with ms_set_global_event_queue().
 * The application can then schedule the callbacks for the events
 * received by the queue by calling ms_event_queue_pump()
 **/
MS2_PUBLIC MSEventQueue *ms_event_queue_new(void);

/**
 * Install a global event queue.
 *
 * All filters currently scheduled by MSTickers will send events (notifications)
 * to the event queue.
 *
 **/
MS2_PUBLIC MS2_DEPRECATED void ms_set_global_event_queue(MSEventQueue *q);

/**
 * Run callbacks associated to the events received.
 * The user can register a notify callback per filter using
 * ms_filter_set_notify_callback() in order to be informed
 * of various events generated by a MSFilter.
 **/
MS2_PUBLIC void ms_event_queue_pump(MSEventQueue *q);

/**
 * Discard all pending events.
 **/
MS2_PUBLIC void ms_event_queue_skip(MSEventQueue *q);

/**
 * Destroys an event queue.
 **/
// MS2_PUBLIC MS2_DEPRECATED void ms_event_queue_destroy(MSEventQueue *q);
MS2_PUBLIC void ms_event_queue_destroy(MSEventQueue *q);

#ifdef __cplusplus
}
#endif

#endif
