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

#include <strings.h>
#include <bctoolbox/tester.h>
#include "mediastreamer2/msfilter.h"
#include "mediastreamer2/msticker.h"
#include "mediastreamer2/msfactory.h"
#include "mediastreamer2/msvideo.h"
#include "mediastreamer-config.h"

typedef struct {
	bool_t first_image_decoded;
	int decoding_errors_count;
	int pli_requests_count;
} _DecodingStats;

typedef enum {
	_TestParamsNone          = 0,
	_TestParamsDecodable     = 1<<0,
	_TestParamsCorruptions   = 1<<1,
	_TestParamsFreezeOnError = 1<<2
} _TestParams;

static MSFactory *_factory = NULL;

static int tester_before_all(void) {
	_factory = ms_factory_new_with_voip();
	return 0;
}

static int tester_after_all(void) {
	ms_factory_destroy(_factory);
	return 0;
}

static void player_events_cb(void *userdata, MSFilter *player, unsigned int id, void *arg) {
	if(id == MS_PLAYER_EOF) {
		*(bool_t *)userdata = TRUE;
	}
}

static void decoder_events_cb(void *userdata, MSFilter *decoder, unsigned int id, void *arg) {
	_DecodingStats *stats = (_DecodingStats *)userdata;
	switch (id) {
		case MS_VIDEO_DECODER_FIRST_IMAGE_DECODED:
			stats->first_image_decoded = TRUE;
			break;
		case MS_VIDEO_DECODER_DECODING_ERRORS:
			stats->decoding_errors_count++;
			break;
		case MS_VIDEO_DECODER_SEND_PLI:
			stats->pli_requests_count++;
			break;
		default:
			break;
	}
}

static bool_t _ms_decoder_require_freeze_on_error(MSFilter *decoder) {
	bool_t freeze_on_error_enabled;
	if (ms_filter_call_method(decoder, MS_VIDEO_DECODER_FREEZE_ON_ERROR_ENABLED, &freeze_on_error_enabled) != 0) return FALSE;
	if (!freeze_on_error_enabled) {
		freeze_on_error_enabled = TRUE;
		if (ms_filter_call_method(decoder, MS_VIDEO_DECODER_FREEZE_ON_ERROR, &freeze_on_error_enabled) != 0) return FALSE;
	}
	return TRUE;
}

static void play_scenario(MSFilterDesc *decoder_desc, const char *pcap_scenario_file, MSFilter *output, _TestParams params) {
	MSFilter *player = ms_factory_create_filter(_factory, MS_FILE_PLAYER_ID);
	MSFilter *decoder = ms_factory_create_filter_from_desc(_factory, decoder_desc);
	MSTicker *ticker = ms_ticker_new();
	bool_t eof = FALSE;
	_DecodingStats decoder_stats = {0};
	int sample_rate = 90000;
	const int sleep_time = 100*1000;
	int err;
	
	BC_ASSERT_PTR_NOT_NULL(decoder);
	if(decoder == NULL) goto end;
	if ((params & _TestParamsFreezeOnError) && !_ms_decoder_require_freeze_on_error(decoder)) {
		ms_warning("%s filter does not support 'freeze on error' mode. Aborting test", decoder->desc->name);
		goto end;
	}
	
	ms_filter_call_method(player, MS_FILTER_SET_SAMPLE_RATE, &sample_rate);
	err = ms_filter_call_method(player, MS_PLAYER_OPEN, (void *)pcap_scenario_file);
	BC_ASSERT_EQUAL(err, 0, int, "%d");
	if (err != 0) goto end;
	
	ms_filter_add_notify_callback(player, player_events_cb, &eof, TRUE);
	ms_filter_add_notify_callback(decoder, decoder_events_cb, &decoder_stats, TRUE);
	
	ms_filter_link(player, 0, decoder, 0);
	ms_filter_link(decoder, 0, output, 0);
	ms_ticker_attach(ticker, player);
	
	ms_filter_call_method_noarg(player, MS_PLAYER_START);
	while(!eof) {
		ms_usleep(sleep_time);
	}
	ms_usleep(sleep_time);
	
	ms_ticker_detach(ticker, player);
	ms_filter_unlink(player, 0, decoder, 0);
	ms_filter_unlink(decoder, 0, output, 0);
	
	if (params & _TestParamsDecodable) {
		BC_ASSERT(decoder_stats.first_image_decoded);
	} else {
		BC_ASSERT(!decoder_stats.first_image_decoded);
	}
	
	if (params & _TestParamsCorruptions) {
		int errors_count = decoder_stats.decoding_errors_count + decoder_stats.pli_requests_count;
		BC_ASSERT_NOT_EQUAL(errors_count, 0, int, "%d");
	} else {
		BC_ASSERT_EQUAL(decoder_stats.decoding_errors_count, 0, int, "%d");
		BC_ASSERT_EQUAL(decoder_stats.pli_requests_count, 0, int, "%d");
	}
	
end:
	if(player) ms_filter_destroy(player);
	if(decoder) ms_filter_destroy(decoder);
	if(ticker) ms_ticker_destroy(ticker);
}

static void play_scenario_for_all_decoders(const char *mime, const char *pcap_scenario_file, _TestParams params) {
	MSFilter *output = ms_factory_create_filter(_factory, MS_VOID_SINK_ID);
	bctbx_list_t *it;
	for(it=_factory->desc_list; it!=NULL; it=it->next) {
		MSFilterDesc *desc = (MSFilterDesc *)it->data;
		if(desc->category == MS_FILTER_DECODER && strcasecmp(desc->enc_fmt, mime) == 0) {
			ms_message("\n");
			ms_message("Playing scenario with '%s' decoder", desc->name);
			play_scenario(desc, pcap_scenario_file, output, params);
		}
	}
	ms_filter_destroy(output);
}

#define scenario_test(scenario_name, params) \
static void scenario_name(void) { \
	char *scenario_pcap_file = ms_strdup_printf("%s/%s", \
			bc_tester_get_resource_dir_prefix(), \
			"scenarios/" #scenario_name ".pcap"); \
	play_scenario_for_all_decoders("h264", scenario_pcap_file, params); \
	ms_free(scenario_pcap_file); \
}

scenario_test(h264_missing_pps_in_second_i_frame, _TestParamsDecodable | _TestParamsCorruptions)
scenario_test(h264_one_nalu_per_frame, _TestParamsDecodable | _TestParamsCorruptions)
scenario_test(h264_one_nalu_per_frame_with_corrupted_idr, _TestParamsCorruptions | _TestParamsFreezeOnError)

#ifdef HAVE_MATROSKA
static void play_scenario_with_mkv_recorder(const char *pcap_scenario_file, const MSFmtDescriptor *fmt) {
	MSFilter *player = ms_factory_create_filter(_factory, MS_FILE_PLAYER_ID);
	MSFilter *recorder = ms_factory_create_filter(_factory, MS_MKV_RECORDER_ID);
	MSTicker *ticker = ms_ticker_new();
	MSPinFormat pinfmt = {0, fmt};
	char *output_file = bctbx_strdup_printf("%s/output.mkv", bc_tester_get_writable_dir_prefix());
	bool_t eof = FALSE;
	
	unlink(output_file);
	
	ms_filter_link(player, 0, recorder, 0);
	ms_filter_add_notify_callback(player, player_events_cb, &eof, TRUE);
	ms_filter_call_method(player, MS_PLAYER_OPEN, (char *)pcap_scenario_file);
	
	ms_filter_call_method(recorder, MS_FILTER_SET_INPUT_FMT, &pinfmt);
	ms_filter_call_method(recorder, MS_RECORDER_OPEN, output_file);
	ms_ticker_attach(ticker, player);
	
	ms_filter_call_method_noarg(recorder, MS_RECORDER_START);
	ms_filter_call_method_noarg(player, MS_PLAYER_START);
	
	while (!eof) {
		ms_usleep(100000);
	}
	
	ms_ticker_detach(ticker, player);
	ms_filter_unlink(player, 0, recorder, 0);
	ms_filter_destroy(player);
	ms_filter_destroy(recorder);
	ms_ticker_destroy(ticker);
	
	unlink(output_file);
	bctbx_free(output_file);
}

void h264_one_nalu_per_frame_with_mkv_recorder(void) {
	const MSFmtDescriptor *fmt = ms_factory_get_video_format(_factory, "h264", MS_VIDEO_SIZE_VGA, 15, NULL);
	char *scenario_pcap_file = bctbx_strdup_printf("%s/scenarios/h264_one_nalu_per_frame.pcap",bc_tester_get_resource_dir_prefix());
	play_scenario_with_mkv_recorder(scenario_pcap_file, fmt);
	bctbx_free(scenario_pcap_file);
}
#endif

static test_t tests[] = {
	{ "H264: missing PPS in second i-frame scenario"         , h264_missing_pps_in_second_i_frame         },
	{ "H264: one NALu per frame scenario"                    , h264_one_nalu_per_frame                    },
	{ "H264: one NALu per frame with corrupted IDR scenario" , h264_one_nalu_per_frame_with_corrupted_idr },
#ifdef HAVE_MATROSKA
	{ "H264: one NALu per frame scenario (MKV recorder)"     , h264_one_nalu_per_frame_with_mkv_recorder  }
#endif
};

test_suite_t codec_impl_test_suite = {
	"CodecImplTesters",
	tester_before_all,
	tester_after_all,
	NULL,
	NULL,
	sizeof(tests) / sizeof(tests[0]),
	tests
};
