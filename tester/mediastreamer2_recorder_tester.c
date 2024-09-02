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

#include "mediastreamer2/mediastream.h"
#include "mediastreamer2/msmediarecorder.h"
#include "mediastreamer2_tester.h"
#include "mediastreamer2_tester_private.h"

static MSFactory *_factory = NULL;

static int tester_before_all(void) {
	_factory = ms_tester_factory_new();
	return 0;
}

static int tester_after_all(void) {
	ms_factory_destroy(_factory);
	return 0;
}

static void record_file(const char *filepath, MSFileFormat file_format, const char *codec, int expect_success) {
	bool_t succeed;
	MSMediaRecorder *file_recorder = NULL;
	MSSndCard *snd_card = ms_snd_card_manager_get_default_capture_card(ms_factory_get_snd_card_manager(_factory));
#ifdef VIDEO_ENABLED
	MSWebCam *web_cam = mediastreamer2_tester_get_mire(_factory);
	BC_ASSERT_PTR_NOT_NULL(web_cam);
#else
	MSWebCam *web_cam = NULL;
#endif
	const char *display_name = ms_factory_get_default_video_renderer(_factory);

	BC_ASSERT_PTR_NOT_NULL(snd_card);
	// Put switch to decide what file format and codec to use

	file_recorder = ms_media_recorder_new(_factory, snd_card, web_cam, display_name, 0, file_format, codec);
	BC_ASSERT_PTR_NOT_NULL(file_recorder);
	if (file_recorder == NULL) return;

	BC_ASSERT_EQUAL(ms_media_recorder_get_state(file_recorder), MSRecorderClosed, int, "%d");

	succeed = ms_media_recorder_open(file_recorder, filepath);
	if (!expect_success) {
		BC_ASSERT_FALSE(succeed);
		BC_ASSERT_EQUAL(ms_media_recorder_get_state(file_recorder), MSRecorderClosed, int, "%d");
	} else {
		BC_ASSERT_TRUE(succeed);
		BC_ASSERT_EQUAL(ms_media_recorder_get_state(file_recorder), MSRecorderPaused, int, "%d");
	}
	if (!succeed) {
		ms_media_recorder_free(file_recorder);
		return;
	}

	succeed = ms_media_recorder_start(file_recorder);
	BC_ASSERT_TRUE(succeed);
	BC_ASSERT_EQUAL(ms_media_recorder_get_state(file_recorder), MSRecorderRunning, int, "%d");

	ms_sleep(5);

	ms_media_recorder_close(file_recorder);
	BC_ASSERT_EQUAL(ms_media_recorder_get_state(file_recorder), MSRecorderClosed, int, "%d");
	ms_media_recorder_remove_file(file_recorder, filepath);
	ms_media_recorder_free(file_recorder);
}

static void record_wav(void) {
	char *random_filename = ms_tester_get_random_filename("test_record_wav-", ".wav");
	char *file = bc_tester_file(random_filename);
	bctbx_free(random_filename);
	record_file(file, MS_FILE_FORMAT_WAVE, "pcmu", TRUE);
	bctbx_free(file);
}

static void record_mkv_vp8(void) {
#ifdef VIDEO_ENABLED
	char *random_filename = ms_tester_get_random_filename("test_record_mkv_vp8-", ".mkv");
	char *file = bc_tester_file(random_filename);
	bctbx_free(random_filename);
	record_file(file, MS_FILE_FORMAT_MATROSKA, "vp8", ms_media_recorder_matroska_supported());
	bctbx_free(file);
#endif
}

static void record_mkv_h264(void) {
#ifdef VIDEO_ENABLED
	if (ms_factory_codec_supported(_factory, "h264")) {
		char *random_filename = ms_tester_get_random_filename("test_record_mkv_h264-", ".mkv");
		char *file = bc_tester_file(random_filename);
		bctbx_free(random_filename);
		record_file(file, MS_FILE_FORMAT_MATROSKA, "h264", ms_media_recorder_matroska_supported());
		bctbx_free(file);
	} else {
		ms_error("H264 codec is not supported! Skip test");
	}
#endif
}

static void record_smff_av1(void) {
#ifdef VIDEO_ENABLED
	if (ms_factory_codec_supported(_factory, "av1")) {
		char *random_filename = ms_tester_get_random_filename("test_record_av1-", ".smff");
		char *file = bc_tester_file(random_filename);
		bctbx_free(random_filename);
		record_file(file, MS_FILE_FORMAT_SMFF, "av1", TRUE);
		bctbx_free(file);
	} else {
		ms_error("AV1 codec is not supported! Skip test");
	}
#endif
}

static test_t tests[] = {TEST_NO_TAG("Record .wav", record_wav), TEST_NO_TAG("Record .mkv vp8", record_mkv_vp8),
                         TEST_NO_TAG("Record .mkv h264", record_mkv_h264),
                         TEST_NO_TAG("Record .smff av1", record_smff_av1)};

test_suite_t recorder_test_suite = {
    "Recorder", tester_before_all, tester_after_all, NULL, NULL, sizeof(tests) / sizeof(test_t), tests, 0};
