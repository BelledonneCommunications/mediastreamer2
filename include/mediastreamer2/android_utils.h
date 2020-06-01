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

#ifndef android_utils_h
#define android_utils_h

#include <mediastreamer2/mssndcard.h>
#include <mediastreamer2/msfilter.h>
#include <jni.h>

/**
 * Retrieve preferred buffer size from Mediastreamer Android Context.
**/
MS2_PUBLIC int get_preferred_buffer_size();

/**
 * Retrieve preferred sample rate from Mediastreamer Android Context.
**/
MS2_PUBLIC int get_preferred_sample_rate();

/**
 * Retrieve all devices in a given direction.
 * Valid values for string dir are "all" "output" "input"
**/
MS2_PUBLIC jobjectArray get_all_devices(JNIEnv *env, const char * dir);

/**
 * Retrieve device ID for the device deviceInfo.
**/
MS2_PUBLIC unsigned int get_device_id(JNIEnv *env, jobject deviceInfo);

/**
 * Retrieve the value of a JV class field fieldName.
**/
MS2_PUBLIC int getJVIntField(JNIEnv *env, const char * className, const char * fieldName);

/**
 * Retrieve device type for the device deviceInfo.
**/
MS2_PUBLIC MSSndCardDeviceType get_device_type(JNIEnv *env, jobject deviceInfo);

/**
 * Retrieve device capabilities (recorder or playback) for the device deviceInfo.
**/
MS2_PUBLIC unsigned int get_device_capabilities(JNIEnv *env, jobject deviceInfo);

/**
 * Retrieve product name for the device deviceInfo.
**/
MS2_PUBLIC char * get_device_product_name(JNIEnv *env, jobject deviceInfo);

/**
 * Retrieve SDK version the app is running on.
**/
MS2_PUBLIC int get_sdk_version(JNIEnv *env);

/**
 * Make upcalls to change device from mediastreamer.
**/
MS2_PUBLIC void change_device(JNIEnv *env, MSSndCardDeviceType type);

/**
 * Make upcalls to enable/disable bluetooth devices from mediastreamer.
**/

MS2_PUBLIC void set_bt_enable(JNIEnv *env, const bool enable);

#ifdef __cplusplus
extern "C"{
#endif

/**
 * Creates an AcousticEchoCanceler java object for the given session ID so the sound card uses the device's hardware echo canceller if available.
 * Currently only AndroidSound, AndroidSoundDepr and AAudio sound cards support it.
**/
MS2_PUBLIC jobject ms_android_enable_hardware_echo_canceller(JNIEnv *env, int sessionId);

/**
 * Deletes the AcousticEchoCanceler created by ms_android_enable_hardware_echo_canceller.
**/
MS2_PUBLIC void ms_android_delete_hardware_echo_canceller(JNIEnv *env, jobject aec);

#ifdef __cplusplus
}
#endif

#endif // android_utils_h

