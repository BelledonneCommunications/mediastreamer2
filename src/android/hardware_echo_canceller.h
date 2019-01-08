/*
 * hardware_echo_canceller.h -Utility methods to manage hardware echo canceller on Android
 *
 * Copyright (C) 2014  Belledonne Communications, Grenoble, France
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#ifndef hardware_echo_canceller_h
#define hardware_echo_canceller_h

#include <mediastreamer2/msfilter.h>
#include <jni.h>

#ifdef __cplusplus
extern "C"{
#endif

jobject enable_hardware_echo_canceller(JNIEnv *env, int sessionId);
void delete_hardware_echo_canceller(JNIEnv *env, jobject aec);

#ifdef __cplusplus
}
#endif

#endif

