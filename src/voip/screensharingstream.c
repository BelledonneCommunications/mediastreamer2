/*
mediastreamer2 library - modular sound and video processing and streaming
Copyright (C) 2006  Simon MORLAT (simon.morlat@linphone.org)

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

#ifdef HAVE_FREERDP_SHADOW
#include <freerdp/freerdp.h>
#include <freerdp/server/shadow.h>
#endif

#ifdef HAVE_FREERDP_CLIENT
#include <freerdp/freerdp.h>
#include <freerdp/client.h>
#endif

#include <sys/types.h>
#include <stdio.h>
#ifndef _WIN32
	#include <sys/socket.h>
	#include <netdb.h>
#endif

//TODO define WSA stun_udp.h

static void screensharing_stream_free(ScreenStream *stream) {
#ifdef HAVE_FREERDP_SHADOW
	if(stream->server!=NULL)
		shadow_server_uninit(stream->server);
#endif
#ifdef HAVE_FREERDP_CLIENT
	if(stream->client!=NULL)
		freerdp_client_context_free(stream->client);
#endif
	media_stream_free(&stream->ms);
	ms_free(stream);
}

ScreenStream *screensharing_stream_new(int loc_tcp_port, bool_t ipv6) {
	return screensharing_stream_new2(ipv6 ? "::" : "0.0.0.0", loc_tcp_port);
}

ScreenStream *screensharing_stream_new2(const char* ip, int loc_tcp_port) {
	ScreenStream *stream = (ScreenStream *)ms_new0(ScreenStream, 1);
	stream->ms.type = MSScreensharing;
	strcpy(stream->addr_ip,ip);
	stream->is_server=FALSE;
	stream->status=-1;
	stream->server=NULL;
	stream->client=NULL;
	stream->tcp_port=loc_tcp_port;
	stream->ms.owns_sessions=TRUE;
	return stream;
}

ScreenStream* screensharing_stream_start_client(ScreenStream *stream) {
#ifdef DONTCOMPIL
#ifdef HAVE_FREERDP_CLIENT
	RDP_CLIENT_ENTRY_POINTS clientEntryPoints;
	rdpContext* client;
	RdpClientEntry(&clientEntryPoints);

	client = freerdp_client_context_new(&clientEntryPoints);
	if (!client)
		return stream;

	stream->client=client;
	client->settings->ServerPort=stream->tcp_port;
	client->settings->Authentication=FALSE;
	client->settings->MouseAttached=FALSE;
	client->settings->ServerHostname="localhost";

	freerdp_client_start(client);
#endif
#endif
	return stream;
}

ScreenStream* screensharing_stream_start_server(ScreenStream *stream) {
#ifdef HAVE_FREERDP_SHADOW
	rdpShadowServer* server;
	int *status = &(stream->status);

	shadow_subsystem_set_entry_builtin(NULL);

	server = shadow_server_new();
	
	if(!server)
		return stream;
	stream->server=server;

	server->authentication=FALSE;
	
	server->port=(DWORD)stream->tcp_port;

	if ((*status=shadow_server_init(server)) < 0)
		goto fail_server_init;

	if ((*status=shadow_server_start(server)) < 0)
		goto fail_server_start;

	return stream;
	
fail_server_start:
	shadow_server_uninit(server);
fail_server_init:
	shadow_server_free(server);
#endif
	return stream;
}

ScreenStream* screensharing_stream_start(ScreenStream *stream) {
	if(stream->is_server)
		return screensharing_stream_start_server(stream);
	return screensharing_stream_start_client(stream);
}

void screensharing_stream_stop(ScreenStream *stream) {
#ifdef HAVE_FREERDP_SHADOW
	if(stream->server!=NULL)
		shadow_server_stop(stream->server);
#endif
#ifdef HAVE_FREERDP_CLIENT
	if(stream->client!=NULL)
		freerdp_client_stop(stream->client);
#endif
	screensharing_stream_free(stream);
}

void screensharing_stream_iterate(ScreenStream *stream) {
	media_stream_iterate(&stream->ms);
}