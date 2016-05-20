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
#include "screensharingserver.h"

#ifdef HAVE_FREERDP_SHADOW
#include <freerdp/freerdp.h>
#include <freerdp/server/shadow.h>
#endif

#include <sys/types.h>

#ifndef _WIN32
	#include <sys/socket.h>
	#include <netdb.h>
#endif

void screensharing_server_iterate(ScreenStream* stream) {
	switch(stream->state){
		case MSScreenSharingListening:
			screensharing_server_start(stream);
			break;
		case MSScreenSharingStreamRunning:
			//TODO handle error
			break;
		case MSScreenSharingInactive:
		case MSScreenSharingWaiting:
		default:
			break;
	}
}

void screensharing_server_free(ScreenStream *stream) {
#ifdef HAVE_FREERDP_SHADOW
	if(stream->server != NULL)
		shadow_server_uninit(stream->server);
#endif
}

ScreenStream* screensharing_server_start(ScreenStream *stream) {
#ifdef HAVE_FREERDP_SHADOW
	rdpShadowServer* server;
	int *status = &(stream->status);
	ms_message("Screensharing server starting on port: %d",stream->tcp_port);

	shadow_subsystem_set_entry_builtin(NULL);

	server = shadow_server_new();

	if(!server)
		return stream;
	stream->server = server;

	server->authentication = FALSE;

	server->port = (DWORD)stream->tcp_port;

	if ((*status=shadow_server_init(server)) < 0)
		goto fail_server_init;

	if ((*status=shadow_server_start(server)) < 0)
		goto fail_server_start;

	stream->state = MSScreenSharingStreamRunning;
	return stream;

//TODO error handling
fail_server_start:
	shadow_server_uninit(server);
fail_server_init:
	shadow_server_free(server);
#endif
	return stream;
}

void screensharing_server_stop(ScreenStream *stream) {
#ifdef HAVE_FREERDP_SHADOW
	if(stream->server != NULL)
		shadow_server_stop(stream->server);
#endif
}