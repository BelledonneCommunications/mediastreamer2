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
#ifndef msfileplayer_h
#define msfileplayer_h

#include <mediastreamer2/msfilter.h>


/*methods*/
#define MS_FILE_PLAYER_OPEN	MS_FILTER_METHOD(MS_FILE_PLAYER_ID,0,const char*)
#define MS_FILE_PLAYER_START	MS_FILTER_METHOD_NO_ARG(MS_FILE_PLAYER_ID,1)
#define MS_FILE_PLAYER_STOP	MS_FILTER_METHOD_NO_ARG(MS_FILE_PLAYER_ID,2)
#define MS_FILE_PLAYER_CLOSE	MS_FILTER_METHOD_NO_ARG(MS_FILE_PLAYER_ID,3)
/* set loop mode: 
	-1: no looping, 
	0: loop at end of file, 
	x>0, loop after x miliseconds after eof
*/
#define MS_FILE_PLAYER_LOOP	MS_FILTER_METHOD(MS_FILE_PLAYER_ID,4,int)
#define MS_FILE_PLAYER_DONE	MS_FILTER_METHOD(MS_FILE_PLAYER_ID,5,int)
#define MS_FILE_PLAYER_BIG_BUFFER	MS_FILTER_METHOD(MS_FILE_PLAYER_ID,6,int)

/*events*/
#define MS_FILE_PLAYER_EOF	MS_FILTER_EVENT_NO_ARG(MS_FILE_PLAYER_ID,0)

#endif

