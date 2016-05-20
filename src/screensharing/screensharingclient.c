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

bool_t screensharing_client_test_server(ScreenStream *stream) {
#ifdef HAVE_XFREERDP_CLIENT
	int cpt=0;
	int socket_server;
	struct sockaddr_in serverSockAddr;
	struct hostent *serverHostEnt = NULL;
	long hostAddr;
	int test;

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

	if ((socket_server = socket(AF_INET,SOCK_STREAM,0)) < 0)
		return FALSE;

	while((test=connect(socket_server,
		(struct sockaddr *)&serverSockAddr,
		sizeof(serverSockAddr))) < 0 && cpt < 10) {
		//TODO To improve
		cpt++;
	}

	close(socket_server);
	return (cpt < 10);
#else
	return FALSE;
#endif
}

void screensharing_client_iterate(ScreenStream* stream) {
	switch(stream->state){
		case MSScreenSharingConnecting:
			//TODO Timeout
			if (screensharing_client_test_server(stream))
				screensharing_client_start(stream);
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
	ms_message("Screensharing client connecting on: %s",stream->addr_ip);

	ZeroMemory(&clientEntryPoints, sizeof(RDP_CLIENT_ENTRY_POINTS));
	clientEntryPoints.Size = sizeof(RDP_CLIENT_ENTRY_POINTS);
	clientEntryPoints.Version = RDP_CLIENT_INTERFACE_VERSION;

	RdpClientEntry(&clientEntryPoints);

	client = freerdp_client_context_new(&clientEntryPoints);
	if (!client)
		return stream;

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

	//TODO timeout system
	if (freerdp_client_start(client) < 0)
		freerdp_client_context_free(client);
	else stream->state = MSScreenSharingStreamRunning;
#endif
	return stream;
}

void screensharing_client_stop(ScreenStream *stream) {
#ifdef HAVE_XFREERDP_CLIENT
	if(stream->client != NULL)
		freerdp_client_stop(stream->client);
#endif
}
