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

#include "mediastreamer2/msjava.h"
#include "mediastreamer2/mscommon.h"

static JavaVM *ms2_vm=NULL;

#ifndef _WIN32
#include <pthread.h>

static pthread_key_t jnienv_key;

/*
 * Do not forget that any log within this routine may cause re-attach of the thread to the jvm because the logs can callback the java application
 * (see LinphoneCoreFactory.setLogHandler() ).
**/
void _android_key_cleanup(void *data){
	JNIEnv *env = (JNIEnv*) data;

	if (env != NULL) {
		//ms_message("Thread end, detaching jvm from current thread");
		(*ms2_vm)->DetachCurrentThread(ms2_vm);
		pthread_setspecific(jnienv_key,NULL);
	}
}
#endif

void ms_set_jvm_from_env(JNIEnv *env){
    (*env)->GetJavaVM(env, &ms2_vm);
#ifndef _WIN32
	pthread_key_create(&jnienv_key,_android_key_cleanup);
#endif
}

void ms_set_jvm(JavaVM *vm){
	ms2_vm=vm;
#ifndef _WIN32
	pthread_key_create(&jnienv_key,_android_key_cleanup);
#endif
}

JavaVM *ms_get_jvm(void){
	return ms2_vm;
}

JNIEnv *ms_get_jni_env(void){
	JNIEnv *env=NULL;
	if (ms2_vm==NULL){
		ms_fatal("Calling ms_get_jni_env() while no jvm has been set using ms_set_jvm().");
	}else{
#ifndef _WIN32
		env=(JNIEnv*)pthread_getspecific(jnienv_key);
		if (env==NULL){
			if ((*ms2_vm)->AttachCurrentThread(ms2_vm,&env,NULL)!=0){
				ms_fatal("AttachCurrentThread() failed !");
				return NULL;
			}
			pthread_setspecific(jnienv_key,env);
		}
#else
		ms_fatal("ms_get_jni_env() not implemented on windows.");
#endif
	}
	return env;
}

#ifdef __ANDROID__

static void handle_jni_exception(JNIEnv *env) {
	if ((*env)->ExceptionCheck(env)) {
		(*env)->ExceptionDescribe(env);
		(*env)->ExceptionClear(env);
		ms_error("JNI exception happened");
	}
}

int ms_get_android_sdk_version(void) {
	static int sdk_version = 0;
	if (sdk_version==0){
		/* Get Android SDK version. */
		JNIEnv *env = ms_get_jni_env();
		jclass version_class = (*env)->FindClass(env, "android/os/Build$VERSION");
		jfieldID fid = (*env)->GetStaticFieldID(env, version_class, "SDK_INT", "I");
		sdk_version = (*env)->GetStaticIntField(env, version_class, fid);
		ms_message("SDK version [%i] detected", sdk_version);
		(*env)->DeleteLocalRef(env, version_class);
	}
	return sdk_version;
}

static const char* GetStringUTFChars(JNIEnv* env, jstring string) {
	const char *cstring = string ? (*env)->GetStringUTFChars(env, string, NULL) : NULL;
	return cstring;
}

static void ReleaseStringUTFChars(JNIEnv* env, jstring string, const char *cstring) {
	if (string) (*env)->ReleaseStringUTFChars(env, string, cstring);
}

char * ms_get_android_libraries_path(void) {
	JNIEnv *env = ms_get_jni_env();
	char *libs_directory = NULL;
	jclass mediastreamerAndroidContextClass = (*env)->FindClass(env, "org/linphone/mediastream/MediastreamerAndroidContext");

	if (mediastreamerAndroidContextClass != NULL) {
		jmethodID getNativeLibDir = (*env)->GetStaticMethodID(env, mediastreamerAndroidContextClass, "getNativeLibrariesDirectory", "()Ljava/lang/String;");
		if (getNativeLibDir != NULL) {
			jobject nativeLibDir = (*env)->CallStaticObjectMethod(env, mediastreamerAndroidContextClass, getNativeLibDir);
			const char *libDir = GetStringUTFChars(env, nativeLibDir);
			libs_directory = ms_strdup(libDir);
			ms_message("Found native libraries path [%s]", libs_directory);
			ReleaseStringUTFChars(env, nativeLibDir, libDir);
		}
		(*env)->DeleteLocalRef(env, mediastreamerAndroidContextClass);
	}

	return libs_directory;
}

bctbx_list_t *ms_get_android_plugins_list(void) {
	bctbx_list_t *plugins = NULL;
	JNIEnv *env = ms_get_jni_env();

	jclass buildConfig = (*env)->FindClass(env, "org/linphone/core/BuildConfig");
	handle_jni_exception(env);

	if (buildConfig != NULL) {
		jfieldID pluginsField = (*env)->GetStaticFieldID(env, buildConfig, "PLUGINS_ARRAY", "[Ljava/lang/String;");
		handle_jni_exception(env);

		if (pluginsField != NULL) {
			jobjectArray pluginsArray = (*env)->GetStaticObjectField(env, buildConfig, pluginsField);
			int pluginsArrayCount = (*env)->GetArrayLength(env, pluginsArray);
			for (int i = 0; i < pluginsArrayCount; i++) {
				jobject pluginString = (*env)->GetObjectArrayElement(env, pluginsArray, i);
				const char *plugin = GetStringUTFChars(env, pluginString);
				if (plugin) {
					ms_message("Found Android plugin %s", plugin);
					plugins = bctbx_list_append(plugins, ms_strdup(plugin));
					ReleaseStringUTFChars(env, pluginString, plugin);
				}
			}
		} else {
			ms_error("Couldn't find field PLUGINS_ARRAY in org.linphone.core.BuildConfig");
		}
		(*env)->DeleteLocalRef(env, buildConfig);
	} else {
		ms_error("Couldn't find class org.linphone.core.BuildConfig");
	}

	return plugins;
}

JNIEXPORT void JNICALL Java_org_linphone_mediastream_Log_d(JNIEnv* env, jobject thiz, jstring jmsg) {
	const char* msg = jmsg ? (*env)->GetStringUTFChars(env, jmsg, NULL) : NULL;
	ms_debug("%s", msg);
	if (msg) (*env)->ReleaseStringUTFChars(env, jmsg, msg);
}

JNIEXPORT void JNICALL Java_org_linphone_mediastream_Log_i(JNIEnv* env, jobject thiz, jstring jmsg) {
	const char* msg = jmsg ? (*env)->GetStringUTFChars(env, jmsg, NULL) : NULL;
	ms_message("%s", msg);
	if (msg) (*env)->ReleaseStringUTFChars(env, jmsg, msg);
}

JNIEXPORT void JNICALL Java_org_linphone_mediastream_Log_w(JNIEnv* env, jobject thiz, jstring jmsg) {
	const char* msg = jmsg ? (*env)->GetStringUTFChars(env, jmsg, NULL) : NULL;
	ms_warning("%s", msg);
	if (msg) (*env)->ReleaseStringUTFChars(env, jmsg, msg);
}

JNIEXPORT void JNICALL Java_org_linphone_mediastream_Log_e(JNIEnv* env, jobject thiz, jstring jmsg) {
	const char* msg = jmsg ? (*env)->GetStringUTFChars(env, jmsg, NULL) : NULL;
	ms_error("%s", msg);
	if (msg) (*env)->ReleaseStringUTFChars(env, jmsg, msg);
}

#endif
