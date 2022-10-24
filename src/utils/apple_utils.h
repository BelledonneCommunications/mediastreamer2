/*
 * Copyright (c) 2010-2022 Belledonne Communications SARL.
 *
 * This file is part of mediastreamer2 
 * (see https://gitlab.linphone.org/BC/public/mediastreamer2).
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as
 * published by the Free Software Foundation, either version 3 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef apple_utils_h
#define apple_utils_h

#import <CoreFoundation/CoreFoundation.h>

#define MS2_FRAMEWORK "org.linphone.mediastreamer2"
#define DISPATCH_SYNC_MAIN(blockName) {\
	if ([NSThread isMainThread]) {blockName();}\
	else {dispatch_sync(dispatch_get_main_queue(), blockName);}\
}

char *getPluginsDir(void);

char *toSafeCStr(CFStringRef str, CFStringEncoding encoding);

char *getBundleResourceDirPath(const char *framework, const char *resource);

#endif
