/*
mediastreamer2 library - modular sound and video processing and streaming
Copyright (C) 2016  Simon MORLAT (simon.morlat@linphone.org)

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

#include "mediastreamer2/mediastream.h"
#include "private.h"
#include "screensharingclient.h"
#include "screensharingserver.h"

#ifdef HAVE_FREERDP_SHADOW
#include <freerdp/freerdp.h>
#include <freerdp/server/shadow.h>
#endif

#ifdef HAVE_FREERDP_CLIENT
#include <freerdp/freerdp.h>
#include <freerdp/client.h>
#endif

#include <sys/types.h>

#ifndef _WIN32
	#include <sys/socket.h>
	#include <netdb.h>
#endif

static void screensharing_stream_free(ScreenStream *stream) {
	if(stream->server != NULL)
		screensharing_server_free(stream);
	if(stream->client != NULL)
		screensharing_client_free(stream);
	media_stream_free(&stream->ms);
	ms_free(stream);
}

void clock_start(MSTimeSpec *start){
	ms_get_cur_time(start);
}

bool_t clock_elapsed(const MSTimeSpec *start, int value_ms){
	MSTimeSpec current;
	ms_get_cur_time(&current);
	if ((((current.tv_sec-start->tv_sec)*1000LL) + ((current.tv_nsec-start->tv_nsec)/1000000LL))>=value_ms)
		return TRUE;
	return FALSE;
}

ScreenStream *screensharing_stream_new(int loc_tcp_port, bool_t ipv6) {
	return screensharing_stream_new2(ipv6 ? "::" : "0.0.0.0", loc_tcp_port);
}

ScreenStream *screensharing_stream_new2(const char* ip, int loc_tcp_port) {
	ScreenStream *stream = (ScreenStream *)ms_new0(ScreenStream, 1);
	stream->ms.type = MSScreensharing;
	strcpy(stream->addr_ip,ip);
	stream->is_server = FALSE;
	stream->status = -1;
	stream->server = NULL;
	stream->client = NULL;
	stream->timer = NULL;
	stream->sockAddr = NULL;
	stream->time_out = 3000; // Option ?
	stream->tcp_port = loc_tcp_port;
	stream->ms.owns_sessions = TRUE;
	return stream;
}

void screensharing_stream_stop(ScreenStream *stream) {
	if(stream->server != NULL)
		screensharing_server_stop(stream);
	if(stream->client != NULL)
		screensharing_client_stop(stream);
	screensharing_stream_free(stream);
}

void screensharing_stream_iterate(ScreenStream *stream) {
	if(stream->is_server)
		screensharing_server_iterate(stream);
	else
		screensharing_client_iterate(stream);
	media_stream_iterate(&stream->ms);
}

bool_t screensharing_client_supported() {
#ifdef HAVE_FREERDP_CLIENT
	return TRUE;
#else
	return FALSE;
#endif
}

bool_t screensharing_server_supported() {
#ifdef HAVE_FREERDP_SHADOW
	return TRUE;
#else
	return FALSE;
#endif
}