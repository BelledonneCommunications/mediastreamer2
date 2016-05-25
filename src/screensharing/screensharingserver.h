/*
mediastreamer2 library - modular sound and video processing and streaming
Copyright (C) 2016  Belledonne Communications SARL, Grenoble France.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
*/

#ifndef msscreensharingserver_hh
#define msscreensharingserver_hh

#include "mediastreamer2/mediastream.h"

#ifdef __cplusplus
extern "C" {
#endif
	
extern void clock_start(MSTimeSpec *start);

extern bool_t clock_elapsed(const MSTimeSpec *start, int value_ms);

void screensharing_server_iterate(ScreenStream *stream);

void screensharing_server_free(ScreenStream *stream);

ScreenStream* screensharing_server_start(ScreenStream *stream);

void screensharing_server_stop(ScreenStream *stream);

#ifdef __cplusplus
}
#endif

#endif
