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

#ifndef ms_devices_h
#define ms_devices_h

#include <mediastreamer2/mscommon.h>

#define DEVICE_HAS_BUILTIN_AEC (1)
#define DEVICE_HAS_BUILTIN_AEC_CRAPPY                                                                                  \
	(1 << 1) /* set when the device is claiming to have AEC but we should not trust it */
#define DEVICE_USE_ANDROID_MIC                                                                                         \
	(1 << 2) /* set when the device needs to capture using MIC instead of Voice communication (I.E kindle fire) */
#define DEVICE_HAS_BUILTIN_OPENSLES_AEC                                                                                \
	(1 << 3) /* set when the device automatically enables it's AEC when using OpenSLES */
#define DEVICE_HAS_CRAPPY_ANDROID_FASTTRACK                                                                            \
	(1 << 4) /* set when the AUDIO_OUTPUT_FLAG_FAST flag of android AudioTrack doesn't work*/
#define DEVICE_HAS_CRAPPY_ANDROID_FASTRECORD                                                                           \
	(1 << 5) /* set when the AUDIO_INPUT_FLAG_FAST flag of android AudioRecord doesn't work*/
#define DEVICE_HAS_UNSTANDARD_LIBMEDIA                                                                                 \
	(1 << 6) /* set when the libmedia backend shall not be used because of proprietary modifications made into it by   \
	            the manufacturer*/
#define DEVICE_HAS_CRAPPY_OPENGL (1 << 7)   /* set when the opengl is crappy and our opengl surfaceview will crash */
#define DEVICE_HAS_CRAPPY_OPENSLES (1 << 8) /* set when the opensles latency is crappy*/
#define DEVICE_USE_ANDROID_CAMCORDER                                                                                   \
	(1 << 9) /* set when the device needs to capture using CAMCORDER instead of Voice communication (I.E kindle fire)  \
	          */
#define DEVICE_MCH264ENC_NO_PIX_FMT_CONV                                                                               \
	(1 << 10) /* set in order to avoid pixel format convervion before MediaCodec H264 encoder input */
#define DEVICE_MCH265_LIMIT_DEQUEUE_OF_OUTPUT_BUFFERS                                                                  \
	(1 << 11)                              /* set in order to avoid calling dequeueOutputBuffers() too often. */
#define DEVICE_HAS_CRAPPY_AAUDIO (1 << 12) /* set to prevent aaudio plugin to create a soundcard */

struct SoundDeviceAudioHacks {
	const char *mic_equalizer;
	float mic_gain;
	const char *spk_equalizer;
	float spk_gain;
};

typedef struct SoundDeviceAudioHacks SoundDeviceAudioHacks;

struct SoundDeviceDescription {
	const char *manufacturer;
	const char *model;
	const char *platform;
	unsigned int flags;
	int delay;
	int recommended_rate;
	SoundDeviceAudioHacks *hacks;
};

typedef struct SoundDeviceDescription SoundDeviceDescription;

struct MSDevicesInfo {
	MSList *sound_devices_descriptions;
};

typedef struct MSDevicesInfo MSDevicesInfo;

#ifdef __cplusplus
extern "C" {
#endif

MSDevicesInfo *ms_devices_info_new(void);

void ms_devices_info_free(MSDevicesInfo *devices_info);

int ms2_android_get_sdk_version(void);

MS2_PUBLIC void ms_devices_info_add(MSDevicesInfo *devices_info,
                                    const char *manufacturer,
                                    const char *model,
                                    const char *platform,
                                    unsigned int flags,
                                    int delay,
                                    int recommended_rate);

MS2_PUBLIC SoundDeviceDescription *ms_devices_info_lookup_device(MSDevicesInfo *devices_info,
                                                                 const char *manufacturer,
                                                                 const char *model,
                                                                 const char *platform);

MS2_PUBLIC SoundDeviceDescription *ms_devices_info_get_sound_device_description(MSDevicesInfo *devices_info);

MS2_PUBLIC bool_t ms_sound_devices_description_equals(const SoundDeviceDescription *sdd1,
                                                      const SoundDeviceDescription *sdd2);

#ifdef __cplusplus
}
#endif

#endif
