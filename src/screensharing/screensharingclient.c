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

#ifdef HAVE_XFREERDP_CLIENT
#include <freerdp/freerdp.h>
#include <freerdp/client.h>
#include <freerdp/client/cmdline.h>
#endif
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <sys/types.h>
#include <sys/socket.h>
#ifndef _WIN32
	#include <netdb.h>
#endif

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

bool_t screensharing_client_test_server(ScreenStream *stream) {
#ifdef HAVE_XFREERDP_CLIENT

	int sock_buf;
	struct sockaddr_in serverSockAddr;
	struct hostent *serverHostEnt = NULL;
	long hostAddr;
	int test = 0;

	//if(stream->socket_server == -1) {
		ZeroMemory(&serverSockAddr,sizeof(serverSockAddr));
		hostAddr = inet_addr(stream->addr_ip);

		if ( (long)hostAddr != (long)-1)
			bcopy(&hostAddr,&serverSockAddr.sin_addr,sizeof(hostAddr));
		else {
			serverHostEnt = gethostbyname(stream->addr_ip);

			if (serverHostEnt == NULL)
				return FALSE;

			bcopy(serverHostEnt->h_addr,&serverSockAddr.sin_addr,serverHostEnt->h_length);
		}

		serverSockAddr.sin_port = htons(stream->tcp_port);
		serverSockAddr.sin_family = AF_INET;

		if ((sock_buf = socket(AF_INET,SOCK_STREAM,0)) < 0)
			return FALSE;
		
		stream->socket_server = sock_buf;
	//}

	test=connect(stream->socket_server,(struct sockaddr *)&serverSockAddr,sizeof(serverSockAddr));

	close(stream->socket_server);

	return (test != -1);
#else
	return FALSE;
#endif
}

void screensharing_client_iterate(ScreenStream* stream) {
	switch(stream->state){
		case MSScreenSharingConnecting:
			ms_message("Screensharing Client: Test server connection");
			/*if (stream->timer == NULL) {
				stream->timer = malloc(sizeof(MSTimeSpec));
				clock_start(stream->timer);
			}
			if (!clock_elapsed(stream->timer, 10000)) {*/
				if (screensharing_client_test_server(stream))
					screensharing_client_start(stream);
			//} else
			//	stream->state = MSScreenSharingInactive;
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

void screensharing_client_free(ScreenStream *stream) {
#ifdef HAVE_XFREERDP_CLIENT
	ms_message("Screensharing Client: Free client");
	if(stream->client != NULL)
		freerdp_client_context_free(stream->client);
#endif
}

#ifdef HAVE_XFREERDP_CLIENT
extern int RdpClientEntry(RDP_CLIENT_ENTRY_POINTS* pEntryPoints);
#endif

ScreenStream* screensharing_client_start(ScreenStream *stream) {
#ifdef HAVE_XFREERDP_CLIENT
	RDP_CLIENT_ENTRY_POINTS clientEntryPoints;
	rdpContext* client;
	ms_message("Screensharing Client: Connecting on = %s; Port = %d",stream->addr_ip,stream->tcp_port);

	ZeroMemory(&clientEntryPoints, sizeof(RDP_CLIENT_ENTRY_POINTS));
	clientEntryPoints.Size = sizeof(RDP_CLIENT_ENTRY_POINTS);
	clientEntryPoints.Version = RDP_CLIENT_INTERFACE_VERSION;

	RdpClientEntry(&clientEntryPoints);

	client = freerdp_client_context_new(&clientEntryPoints);
	if (!client) {
		ms_message("Screensharing Client: Fail new client");
		return stream;
	}
	
	ms_message("Screensharing Client: New client");

	stream->client = client;

	//TODO OPTIONS ?
	client->settings->ServerHostname = malloc(sizeof(char)*sizeof(stream->addr_ip));
	strncpy(client->settings->ServerHostname,stream->addr_ip,sizeof(stream->addr_ip));
	client->settings->ServerPort = stream->tcp_port;
	client->settings->TlsSecurity = FALSE;
	client->settings->NlaSecurity = TRUE;
	client->settings->RdpSecurity = TRUE;
	client->settings->ExtSecurity = FALSE;
	client->settings->Authentication = FALSE;
	client->settings->AudioPlayback = FALSE;
	client->settings->RemoteConsoleAudio = FALSE;

	//TODO timeout system
	if (freerdp_client_start(client) < 0) {
		ms_message("Screensharing Client: Fail to start");
		freerdp_client_context_free(client);
	}
	else stream->state = MSScreenSharingStreamRunning;

	ms_message("Screensharing Client: State = %d",stream->state);
#endif
	return stream;
}

void screensharing_client_stop(ScreenStream *stream) {
#ifdef HAVE_XFREERDP_CLIENT
	ms_message("Screensharing Client: Stop client");
	if(stream->client != NULL)
		freerdp_client_stop(stream->client);
#endif
}
