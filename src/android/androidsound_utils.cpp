/*
 * Copyright (c) 2010-2020 Belledonne Communications SARL.
 *
 * androidsound_utils.cpp - Android Audio Utils.
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

#include <mediastreamer2/mssndcard.h>
#include <mediastreamer2/android_utils.h>

int get_preferred_sample_rate() {
	int DevicePreferredSampleRate = -1;
	JNIEnv *env = ms_get_jni_env();
	jclass mediastreamerAndroidContextClass = env->FindClass("org/linphone/mediastream/MediastreamerAndroidContext");
	if (mediastreamerAndroidContextClass != NULL) {
		jmethodID getSampleRate = env->GetStaticMethodID(mediastreamerAndroidContextClass, "getDeviceFavoriteSampleRate", "()I");
		if (getSampleRate != NULL) {
			jint ret = env->CallStaticIntMethod(mediastreamerAndroidContextClass, getSampleRate);
			DevicePreferredSampleRate = (int)ret;
			ms_message("[Android Audio Utils] Using %i for sample rate value", DevicePreferredSampleRate);
		}
		env->DeleteLocalRef(mediastreamerAndroidContextClass);
	}

	if (DevicePreferredSampleRate == -1) {
		ms_error("[Android Audio Utils] Failed to retrive sample rate - keeping value %0d", DevicePreferredSampleRate);
	}

	return DevicePreferredSampleRate;
}

jobject get_all_devices(JNIEnv *env, const char * dir) {
	jclass mediastreamerAndroidContextClass = env->FindClass("org/linphone/mediastream/MediastreamerAndroidContext");

	jobject audioDevices = NULL;

	if (mediastreamerAndroidContextClass != NULL) {
		jmethodID getAudioDevicesId = env->GetStaticMethodID(mediastreamerAndroidContextClass, "getAudioDevices", "(Ljava/lang/String;)[Landroid/media/AudioDeviceInfo;");
		if (getAudioDevicesId != NULL) {
			jstring device_dir = env->NewStringUTF(dir);
			jobject deviceInfoObj = env->CallStaticObjectMethod(mediastreamerAndroidContextClass, getAudioDevicesId, device_dir);

			audioDevices = env->NewGlobalRef(deviceInfoObj);
			if (audioDevices == NULL) {
				ms_error("[Android Audio Utils] Failed to convert local ref to audio devices to global ref");
			}

			env->DeleteLocalRef(deviceInfoObj);
		}
		env->DeleteLocalRef(mediastreamerAndroidContextClass);
	}

	return audioDevices;
}

unsigned int get_device_id(JNIEnv *env, jobject deviceInfo) {

	unsigned int id = 0;

	jclass audioDeviceInfoClass = env->FindClass("android/media/AudioDeviceInfo");
	if (audioDeviceInfoClass != NULL) {
		jmethodID getIDID = env->GetMethodID(audioDeviceInfoClass, "getId", "()I");
		if (getIDID != NULL) {
			id = (int) env->CallIntMethod(deviceInfo, getIDID);
		}
		env->DeleteLocalRef(audioDeviceInfoClass);
	}

	return id;

}

int getJVIntField(JNIEnv *env, const char * className, const char * fieldName) {

	int value = -1;
	jclass JVClass = env->FindClass(className);
	if (JVClass != NULL) {
		jfieldID fieldID = env->GetStaticFieldID(JVClass, fieldName, "I");
		if (fieldID != NULL) {
			value = (int) env->GetStaticIntField(JVClass, fieldID);
		}
		env->DeleteLocalRef(JVClass);
	}

	return value;
}

DeviceType get_device_type(JNIEnv *env, jobject deviceInfo) {

	int typeID = -1;

	const char * audioDeviceInfoClassName = "android/media/AudioDeviceInfo";

	jclass audioDeviceInfoClass = env->FindClass(audioDeviceInfoClassName);
	if (audioDeviceInfoClass != NULL) {
		jmethodID getTypeID = env->GetMethodID(audioDeviceInfoClass, "getType", "()I");
		if (getTypeID != NULL) {
			typeID = (int) env->CallIntMethod(deviceInfo, getTypeID);
		}
		env->DeleteLocalRef(audioDeviceInfoClass);
	}

	DeviceType deviceType = DeviceType::UNKNOWN;
	if ((typeID == getJVIntField(env, audioDeviceInfoClassName, "TYPE_BLUETOOTH_SCO")) || (typeID == getJVIntField(env, audioDeviceInfoClassName, "TYPE_BLUETOOTH_A2DP"))) {
		deviceType = DeviceType::BLUETOOTH;
	} else if (typeID == getJVIntField(env, audioDeviceInfoClassName, "TYPE_BUILTIN_EARPIECE")) {
		deviceType = DeviceType::EARPIECE;
	} else if (typeID == getJVIntField(env, audioDeviceInfoClassName, "TYPE_BUILTIN_SPEAKER")) {
		deviceType = DeviceType::SPEAKER;
	} else if (typeID == getJVIntField(env, audioDeviceInfoClassName, "TYPE_BUILTIN_MIC")) {
		deviceType = DeviceType::MICROPHONE;
	} else if ((typeID == getJVIntField(env, audioDeviceInfoClassName, "TYPE_USB_HEADSET")) || (typeID == getJVIntField(env, audioDeviceInfoClassName, "TYPE_WIRED_HEADSET"))) {
		deviceType = DeviceType::HEADSET;
	} else if (typeID == getJVIntField(env, audioDeviceInfoClassName, "TYPE_WIRED_HEADPHONES")) {
		deviceType = DeviceType::HEADPHONES;
	} else if (typeID == getJVIntField(env, audioDeviceInfoClassName, "TYPE_USB_DEVICE")) {
		deviceType = DeviceType::GENERIC_USB;
	} else if (typeID == getJVIntField(env, audioDeviceInfoClassName, "TYPE_AUX_LINE")) {
		deviceType = DeviceType::AUX_LINE;
	} else if (typeID == getJVIntField(env, audioDeviceInfoClassName, "TYPE_TELEPHONY")) {
		deviceType = DeviceType::TELEPHONY;
	} else {
		deviceType = DeviceType::UNKNOWN;
		ms_error("[Android Audio Utils] Unknown device type for type ID %0d", typeID);
	}

	return deviceType;

}

unsigned int get_device_capabilities(JNIEnv *env, jobject deviceInfo) {

	unsigned int cap = MS_SND_CARD_CAP_DISABLED;

	jclass audioDeviceInfoClass = env->FindClass("android/media/AudioDeviceInfo");
	if (audioDeviceInfoClass != NULL) {
		jmethodID isSinkID = env->GetMethodID(audioDeviceInfoClass, "isSink", "()Z");
		if (isSinkID != NULL) {
			jboolean isSink = env->CallBooleanMethod(deviceInfo, isSinkID);
			if (isSink) {
				cap |= MS_SND_CARD_CAP_PLAYBACK;
			}
		}

		jmethodID isSourceID = env->GetMethodID(audioDeviceInfoClass, "isSource", "()Z");
		if (isSourceID != NULL) {
			jboolean isSource = env->CallBooleanMethod(deviceInfo, isSourceID);
			if (isSource) {
				cap |= MS_SND_CARD_CAP_CAPTURE;
			}
		}
		env->DeleteLocalRef(audioDeviceInfoClass);
	}

	return cap;
}

char * get_device_product_name(JNIEnv *env, jobject deviceInfo) {

	char * productName = NULL;

	jclass audioDeviceInfoClass = env->FindClass("android/media/AudioDeviceInfo");
	if (audioDeviceInfoClass != NULL) {
		jmethodID getProductNameID = env->GetMethodID(audioDeviceInfoClass, "getProductName", "()Ljava/lang/CharSequence;");
		if (getProductNameID != NULL) {
			jobject productNameChar = env->CallObjectMethod(deviceInfo, getProductNameID);
			if (productNameChar != NULL) {
				jclass charSequenceClass = env->FindClass("java/lang/CharSequence");
				if (charSequenceClass != NULL) {
					jmethodID toStringID = env->GetMethodID(charSequenceClass, "toString", "()Ljava/lang/String;");
					if (toStringID != NULL) {
						jstring productNameString = static_cast<jstring>(env->CallObjectMethod(productNameChar, toStringID));
						productName = (char *) env->GetStringUTFChars(productNameString, nullptr);

						env->ReleaseStringUTFChars(productNameString, productName);
						env->DeleteLocalRef(productNameString);
					}
				}
			}
		}
		env->DeleteLocalRef(audioDeviceInfoClass);
	}

	return productName;
}
