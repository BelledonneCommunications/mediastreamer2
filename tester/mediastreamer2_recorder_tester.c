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

#include "mediastreamer2_tester.h"
#include "mediastreamer2/msmediarecorder.h"
#include "mediastreamer2/mediastream.h"

static MSFactory* _factory = NULL;

typedef enum {
	RECORDER_TEST_NONE = 0,
	RECORDER_TEST_UNSUPPORTED_FORMAT = 1,
    RECORDER_TEST_H264 = 2,
} RecorderTestFlags;


const char *get_filename_ext(const char *filename) {
    const char *dot = strrchr(filename, '.');
    if(!dot || dot == filename) return "";
    return dot + 1;
}

static int tester_before_all(void) {
	_factory = ms_factory_new_with_voip();
	return 0;
}

static int tester_after_all(void) {
	ms_factory_destroy(_factory);
	return 0;
}
static void record_file(const char *filepath, RecorderTestFlags flags) {
	bool_t succeed;
	MSMediaRecorder *file_recorder = NULL;
	MSSndCard *snd_card = ms_snd_card_manager_get_default_capture_card(ms_factory_get_snd_card_manager(_factory));
	MSWebCam *web_cam = ms_web_cam_manager_get_default_cam(ms_factory_get_web_cam_manager(_factory));
	const char *display_name = ms_factory_get_default_video_renderer(_factory);

	BC_ASSERT_PTR_NOT_NULL(snd_card);
	BC_ASSERT_PTR_NOT_NULL(web_cam);
	//Put switch to decide what file format and codec to use
    MSFileFormat file_format = MS_FILE_FORMAT_UNKNOWN;
    char *codec = "";
    const char *file_ext = get_filename_ext(filepath);
    if (strcmp(file_ext, "wav") == 0) {
        file_format = MS_FILE_FORMAT_WAVE;
    } else if (strcmp(file_ext, "mkv") == 0) {
        file_format = MS_FILE_FORMAT_MATROSKA;
        if(flags & RECORDER_TEST_H264) {
            codec = "h264";
        } else {
            codec = "vp8";
        }
    }
	file_recorder = ms_media_recorder_new(_factory, snd_card, web_cam, display_name, 0, file_format, codec);
	BC_ASSERT_PTR_NOT_NULL(file_recorder);
	if(file_recorder == NULL) return;

	BC_ASSERT_EQUAL(ms_media_recorder_get_state(file_recorder), MSRecorderClosed, int, "%d");

	succeed = ms_media_recorder_open(file_recorder, filepath, 0);
	if(flags & RECORDER_TEST_UNSUPPORTED_FORMAT) {
		BC_ASSERT_FALSE(succeed);
		BC_ASSERT_EQUAL(ms_media_recorder_get_state(file_recorder), MSRecorderClosed, int, "%d");
	} else {
		BC_ASSERT_TRUE(succeed);
		BC_ASSERT_EQUAL(ms_media_recorder_get_state(file_recorder), MSRecorderPaused, int, "%d");
	}
	if(!succeed) {
		ms_media_recorder_free(file_recorder);
		return;
	}

	succeed = ms_media_recorder_start(file_recorder);
	BC_ASSERT_TRUE(succeed);
	BC_ASSERT_EQUAL(ms_media_recorder_get_state(file_recorder), MSRecorderRunning, int, "%d");

    ms_sleep(5);

	ms_media_recorder_close(file_recorder);
	BC_ASSERT_EQUAL(ms_media_recorder_get_state(file_recorder), MSRecorderClosed, int, "%d");
    //ms_media_recorder_remove_file(file_recorder, filepath);
	ms_media_recorder_free(file_recorder);
}

static void record_wav(void) {
    char* file = bc_tester_res("records/test_record_wav.wav");
    RecorderTestFlags flags = ms_media_recorder_matroska_supported() ? RECORDER_TEST_NONE : RECORDER_TEST_UNSUPPORTED_FORMAT;
    record_file(file, flags);
}

static void record_mkv_vp8(void) {
    char* file = bc_tester_res("records/test_record_mkv_vp8.mkv");
    RecorderTestFlags flags = ms_media_recorder_matroska_supported() ? RECORDER_TEST_NONE : RECORDER_TEST_UNSUPPORTED_FORMAT;
    record_file(file, flags);
}

static void record_mkv_h264(void) {
    char* file = bc_tester_res("records/test_record_mkv_h264.mkv");
    RecorderTestFlags flags = ms_media_recorder_matroska_supported() ? RECORDER_TEST_NONE : RECORDER_TEST_UNSUPPORTED_FORMAT;
    record_file(file, flags | RECORDER_TEST_H264);
}

static test_t tests[] = {
	TEST_NO_TAG("Record .wav"                        , record_wav),
    TEST_NO_TAG("Record .mkv vp8"                    , record_mkv_vp8),
    TEST_NO_TAG("Record .mkv h264"                   , record_mkv_h264),
};

test_suite_t recorder_test_suite = {
	"Recorder",
	tester_before_all,
	tester_after_all,
	NULL,
	NULL,
	sizeof(tests)/sizeof(test_t),
	tests
};
