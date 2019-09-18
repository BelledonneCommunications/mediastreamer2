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

/** 
 * \file payloadtype.h
 * \brief Definition of payload types
 *
**/

#ifndef PAYLOADTYPE_H
#define PAYLOADTYPE_H
#include <ortp/port.h>

#ifdef __cplusplus
extern "C"{
#endif

/* flags for PayloadType::flags */

#define	PAYLOAD_TYPE_ALLOCATED (1)
	/* private flags for future use by ortp */
#define	PAYLOAD_TYPE_PRIV1 (1<<1)
#define	PAYLOAD_TYPE_PRIV2 (1<<2)
#define	PAYLOAD_TYPE_PRIV3 (1<<3)
	/* user flags, can be used by the application on top of oRTP */
#define	PAYLOAD_TYPE_USER_FLAG_0 (1<<4)
#define	PAYLOAD_TYPE_USER_FLAG_1 (1<<5)
#define	PAYLOAD_TYPE_USER_FLAG_2 (1<<6)
	/* ask for more if you need*/

#define PAYLOAD_AUDIO_CONTINUOUS 0
#define PAYLOAD_AUDIO_PACKETIZED 1
#define PAYLOAD_VIDEO 2
#define PAYLOAD_TEXT 4
#define PAYLOAD_OTHER 3  /* ?? */

struct _PayloadType
{
	int type; /**< one of PAYLOAD_* macros*/
	int clock_rate; /**< rtp clock rate*/
	char bits_per_sample;	/* in case of continuous audio data */
	char *zero_pattern;
	int pattern_length;
	/* other useful information for the application*/
	int normal_bitrate;	/*in bit/s */
	char *mime_type; /**<actually the submime, ex: pcm, pcma, gsm*/
	int channels; /**< number of channels of audio */
	char *recv_fmtp; /* various format parameters for the incoming stream */
	char *send_fmtp; /* various format parameters for the outgoing stream */
	int flags;
	void *user_data;
};

#ifndef PayloadType_defined
#define PayloadType_defined
typedef struct _PayloadType PayloadType;
#endif

#define payload_type_set_flag(pt,flag) (pt)->flags|=((int)flag)
#define payload_type_unset_flag(pt,flag) (pt)->flags&=(~(int)flag)
#define payload_type_get_flags(pt)	(pt)->flags


ORTP_PUBLIC PayloadType *payload_type_new(void);
ORTP_PUBLIC PayloadType *payload_type_clone(PayloadType *payload);
ORTP_PUBLIC char *payload_type_get_rtpmap(PayloadType *pt);
ORTP_PUBLIC void payload_type_destroy(PayloadType *pt);
ORTP_PUBLIC void payload_type_set_recv_fmtp(PayloadType *pt, const char *fmtp);
ORTP_PUBLIC void payload_type_set_send_fmtp(PayloadType *pt, const char *fmtp);
ORTP_PUBLIC void payload_type_append_recv_fmtp(PayloadType *pt, const char *fmtp);
ORTP_PUBLIC void payload_type_append_send_fmtp(PayloadType *pt, const char *fmtp);

#define payload_type_get_bitrate(pt)	((pt)->normal_bitrate)
#define payload_type_get_rate(pt)		((pt)->clock_rate)
#define payload_type_get_mime(pt)		((pt)->mime_type)

ORTP_PUBLIC bool_t fmtp_get_value(const char *fmtp, const char *param_name, char *result, size_t result_len);

#define payload_type_set_user_data(pt,p)	(pt)->user_data=(p)
#define payload_type_get_user_data(pt)		((pt)->user_data)


/* some payload types */
/* audio */
VAR_DECLSPEC PayloadType payload_type_pcmu8000;
VAR_DECLSPEC PayloadType payload_type_pcma8000;
VAR_DECLSPEC PayloadType payload_type_pcm8000;
VAR_DECLSPEC PayloadType payload_type_l16_mono;
VAR_DECLSPEC PayloadType payload_type_l16_stereo;
VAR_DECLSPEC PayloadType payload_type_lpc1016;
VAR_DECLSPEC PayloadType payload_type_g729;
VAR_DECLSPEC PayloadType payload_type_g7231;
VAR_DECLSPEC PayloadType payload_type_g7221;
VAR_DECLSPEC PayloadType payload_type_g726_40;
VAR_DECLSPEC PayloadType payload_type_g726_32;
VAR_DECLSPEC PayloadType payload_type_g726_24;
VAR_DECLSPEC PayloadType payload_type_g726_16;
VAR_DECLSPEC PayloadType payload_type_aal2_g726_40;
VAR_DECLSPEC PayloadType payload_type_aal2_g726_32;
VAR_DECLSPEC PayloadType payload_type_aal2_g726_24;
VAR_DECLSPEC PayloadType payload_type_aal2_g726_16;
VAR_DECLSPEC PayloadType payload_type_gsm;
VAR_DECLSPEC PayloadType payload_type_lpc;
VAR_DECLSPEC PayloadType payload_type_lpc1015;
VAR_DECLSPEC PayloadType payload_type_speex_nb;
VAR_DECLSPEC PayloadType payload_type_speex_wb;
VAR_DECLSPEC PayloadType payload_type_speex_uwb;
VAR_DECLSPEC PayloadType payload_type_ilbc;
VAR_DECLSPEC PayloadType payload_type_amr;
VAR_DECLSPEC PayloadType payload_type_amrwb;
VAR_DECLSPEC PayloadType payload_type_truespeech;
VAR_DECLSPEC PayloadType payload_type_evrc0;
VAR_DECLSPEC PayloadType payload_type_evrcb0;
VAR_DECLSPEC PayloadType payload_type_silk_nb;	
VAR_DECLSPEC PayloadType payload_type_silk_mb;
VAR_DECLSPEC PayloadType payload_type_silk_wb;
VAR_DECLSPEC PayloadType payload_type_silk_swb;

	/* video */
VAR_DECLSPEC PayloadType payload_type_mpv;
VAR_DECLSPEC PayloadType payload_type_h261;
VAR_DECLSPEC PayloadType payload_type_h263;
VAR_DECLSPEC PayloadType payload_type_h263_1998;
VAR_DECLSPEC PayloadType payload_type_h263_2000;
VAR_DECLSPEC PayloadType payload_type_mp4v;
VAR_DECLSPEC PayloadType payload_type_theora;
VAR_DECLSPEC PayloadType payload_type_h264;
VAR_DECLSPEC PayloadType payload_type_x_snow;
VAR_DECLSPEC PayloadType payload_type_jpeg;
VAR_DECLSPEC PayloadType payload_type_vp8;

VAR_DECLSPEC PayloadType payload_type_g722;

/* text */
VAR_DECLSPEC PayloadType payload_type_t140;
VAR_DECLSPEC PayloadType payload_type_t140_red;

/* non standard file transfer over UDP */
VAR_DECLSPEC PayloadType payload_type_x_udpftp;

/* telephone-event */
VAR_DECLSPEC PayloadType payload_type_telephone_event;

#ifdef __cplusplus
}
#endif

#endif
