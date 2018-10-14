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
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
*/

#ifndef msvolume_h
#define msvolume_h

#include <mediastreamer2/msfilter.h>

/**
 * The Volume MSFilter can do:
 * 	- measurements of the input signal power, returned in dbm0 or linear scale
 * 	- apply a gain to the input signal and output this amplified signal to its output.
 * By default gain is 1, in which case the filter does not modify the signal (and even does not
 * copy the buffers, just post them on its output queue.
**/


/*returns a volume meter in db0 (max=0 db0)*/
#define MS_VOLUME_GET		MS_FILTER_METHOD(MS_VOLUME_ID,0,float)
/*returns a volume in linear scale between 0 and 1 */
#define MS_VOLUME_GET_LINEAR		MS_FILTER_METHOD(MS_VOLUME_ID,1,float)
/* set a gain */
#define MS_VOLUME_SET_GAIN		MS_FILTER_METHOD(MS_VOLUME_ID,2,float)

#define MS_VOLUME_SET_PEER		MS_FILTER_METHOD(MS_VOLUME_ID,4, MSFilter )

#define MS_VOLUME_SET_EA_THRESHOLD	MS_FILTER_METHOD(MS_VOLUME_ID,5,float)

#define MS_VOLUME_SET_EA_SPEED		MS_FILTER_METHOD(MS_VOLUME_ID,6,float)

#define MS_VOLUME_SET_EA_FORCE		MS_FILTER_METHOD(MS_VOLUME_ID,7,float)

#define MS_VOLUME_ENABLE_AGC		MS_FILTER_METHOD(MS_VOLUME_ID,8,int)

#define MS_VOLUME_ENABLE_NOISE_GATE	MS_FILTER_METHOD(MS_VOLUME_ID,9,int)

#define MS_VOLUME_SET_NOISE_GATE_THRESHOLD	MS_FILTER_METHOD(MS_VOLUME_ID,10,float)

#define MS_VOLUME_SET_EA_SUSTAIN	MS_FILTER_METHOD(MS_VOLUME_ID,11,int)

#define MS_VOLUME_SET_NOISE_GATE_FLOORGAIN MS_FILTER_METHOD(MS_VOLUME_ID,12,float)

/* set a gain in db */
#define MS_VOLUME_SET_DB_GAIN		MS_FILTER_METHOD(MS_VOLUME_ID,13,float)

/* get a linear gain */
#define MS_VOLUME_GET_GAIN		MS_FILTER_METHOD(MS_VOLUME_ID,14,float)

/* get the gain in db*/
#define MS_VOLUME_GET_GAIN_DB		MS_FILTER_METHOD(MS_VOLUME_ID,15,float)

#define MS_VOLUME_REMOVE_DC	MS_FILTER_METHOD(MS_VOLUME_ID,16,int)

#define MS_VOLUME_SET_EA_TRANSMIT_THRESHOLD	MS_FILTER_METHOD(MS_VOLUME_ID,17,float)

/**
 * Obtain the minimum volume, in db, over the last X seconds period completed (X=30 seconds by default)
**/
#define MS_VOLUME_GET_MIN	MS_FILTER_METHOD(MS_VOLUME_ID,18,float)

/**
 * Obtain the maximum volume, in db, over the last X seconds period completed (X=1 second by default)
**/
#define MS_VOLUME_GET_MAX	MS_FILTER_METHOD(MS_VOLUME_ID,19,float)

#define MS_VOLUME_DB_LOWEST		(-120)	/*arbitrary value returned when linear volume is 0*/

extern MSFilterDesc ms_volume_desc;

#endif
