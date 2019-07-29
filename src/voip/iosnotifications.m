/*
 * iosnotifications.m
 *
 *
 * Copyright (C) 2009  Belledonne Comunications, Grenoble, France
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Library General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#import <UserNotifications/UserNotifications.h>
#import <AVFoundation/AVAudioSession.h>
#include "mediastreamer2/mscommon.h"
#include "mediastreamer2/mssndcard.h"
#include "mediastreamer2/iosnotifications.h"

static id<NSObject> localAudioSessionObserver;

void reset_card(MSSndCard *obj){
	if (obj && (ms_snd_card_get_capabilities(obj) & MS_SND_CARD_CAP_IS_SLOW)) {
		ms_snd_card_set_usage_hint(obj, FALSE);
	}
}

void addAudioSessionObserver(MSSndCardManager* scm) {
	ms_message("[IOS]Audio Session interruption notification added.");
	localAudioSessionObserver=[NSNotificationCenter.defaultCenter addObserverForName:AVAudioSessionInterruptionNotification
						object:nil
						 queue:nil
					usingBlock:^(NSNotification* notification) {
						int interruptionType = [notification.userInfo[AVAudioSessionInterruptionTypeKey] intValue];
						if (interruptionType == AVAudioSessionInterruptionTypeBegan) {
							ms_message("[IOS]Sound interruption detected!");
							bctbx_list_for_each(scm->cards,(void (*)(void*))reset_card);
					}
	       }];
}

void removeAudioSessionObserver(void) {
	ms_message("[IOS]Audio Session interruption notification removed.");
	[NSNotificationCenter.defaultCenter removeObserver:localAudioSessionObserver
	                                     name:AVAudioSessionInterruptionNotification
																			 object:nil];
}
