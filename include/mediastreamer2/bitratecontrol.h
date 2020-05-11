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

#ifndef ms2_ratecontrol
#define ms2_ratecontrol

#include "mediastreamer2/msfilter.h"
#include <ortp/ortp.h>

#ifdef __cplusplus
extern "C" {
#endif

struct _MediaStream;

typedef struct _MSBandwidthControllerStats{
	float estimated_download_bandwidth; /*in bits/seconds*/
	float controlled_stream_bandwidth;
	bool_t in_congestion;
}MSBandwidthControllerStats;

struct _MSBandwidthController{
	bctbx_list_t *streams; /*list of MediaStream objects*/
	struct _MediaStream *controlled_stream; /*the most bandwidth consuming stream, which is the one flow controlled*/
	MSBandwidthControllerStats stats;
	float remote_video_bandwidth_available_estimated;
	float maximum_bw_usage;
	float currently_requested_stream_bandwidth; // According to congestion control and bandwith estimator only.
	bool_t congestion_detected;
};
/**
 * The MSBandwidthController is a object managing several streams (audio, video) and monitoring congestion of inbound streams.
 * If a congestion is detected, it will send RTCP TMMBR packet to the remote sender in order to stop the congestion and adapt
 * the incoming bitrate of received streams to the available bandwidth.
 * It superseeds the MSBitrateController, MSQosAnalyzer, MSBitrateDriver objects, which were using different but less robust techniques
 * to detect congestion and adapt bandwidth usage.
**/
typedef struct _MSBandwidthController MSBandwidthController;


MS2_PUBLIC MSBandwidthController *ms_bandwidth_controller_new(void);

/**
 * Set an explicit download bandwidth target usage expressed in bits/seconds, used for video conferencing cases.
 * This target bandwidth is considered as a maximum that shall not be exceeded.
 * The congestion control and bandwidth estimations are used normally to find the correct bandwidth usage below this target usage.
 */
MS2_PUBLIC void  ms_bandwidth_controller_set_maximum_bandwidth_usage(MSBandwidthController *obj, int bitrate);

MS2_PUBLIC void ms_bandwidth_controller_add_stream(MSBandwidthController *obj, struct _MediaStream *stream);

MS2_PUBLIC void ms_bandwidth_controller_remove_stream(MSBandwidthController *obj, struct _MediaStream *stream);

MS2_PUBLIC const MSBandwidthControllerStats * ms_bandwidth_controller_get_stats(MSBandwidthController *obj);

MS2_PUBLIC void ms_bandwidth_controller_reset_state(MSBandwidthController *obj);

MS2_PUBLIC void ms_bandwidth_controller_destroy(MSBandwidthController *obj);
	

/**
 * Audio Bitrate controller object.
 * @deprecated
**/
typedef struct _MSAudioBitrateController MSAudioBitrateController;

enum _MSRateControlActionType{
	MSRateControlActionDoNothing,
	MSRateControlActionDecreaseBitrate,
	MSRateControlActionDecreasePacketRate,
	MSRateControlActionIncreaseQuality,
};
typedef enum _MSRateControlActionType MSRateControlActionType;
const char *ms_rate_control_action_type_name(MSRateControlActionType t);


typedef struct _MSRateControlAction{
	MSRateControlActionType type;
	int value;
}MSRateControlAction;

typedef struct _MSBitrateDriver MSBitrateDriver;
typedef struct _MSBitrateDriverDesc MSBitrateDriverDesc;

struct _MSBitrateDriverDesc{
	int (*execute_action)(MSBitrateDriver *obj, const MSRateControlAction *action);
	void (*uninit)(MSBitrateDriver *obj);
};

/**
 * The MSBitrateDriver has the responsibility to execute rate control actions.
 * This is an abstract interface.
 * @deprecated
**/
struct _MSBitrateDriver{
	MSBitrateDriverDesc *desc;
	int refcnt;
};

MS2_PUBLIC int ms_bitrate_driver_execute_action(MSBitrateDriver *obj, const MSRateControlAction *action);
MS2_PUBLIC MSBitrateDriver * ms_bitrate_driver_ref(MSBitrateDriver *obj);
MS2_PUBLIC void ms_bitrate_driver_unref(MSBitrateDriver *obj);

MS2_PUBLIC MSBitrateDriver *ms_audio_bitrate_driver_new(RtpSession *session, MSFilter *encoder);
MS2_PUBLIC MSBitrateDriver *ms_av_bitrate_driver_new(RtpSession *asession, MSFilter *aenc, RtpSession *vsession, MSFilter *venc);
MS2_PUBLIC MSBitrateDriver *ms_bandwidth_bitrate_driver_new(RtpSession *asession, MSFilter *aenc, RtpSession *vsession, MSFilter *venc);

typedef struct _MSQosAnalyzer MSQosAnalyzer;
typedef struct _MSQosAnalyzerDesc MSQosAnalyzerDesc;

struct _MSQosAnalyzerDesc{
	bool_t (*process_rtcp)(MSQosAnalyzer *obj, mblk_t *rtcp);
	void (*suggest_action)(MSQosAnalyzer *obj, MSRateControlAction *action);
	bool_t (*has_improved)(MSQosAnalyzer *obj);
	void (*update)(MSQosAnalyzer *);
	void (*uninit)(MSQosAnalyzer *);
};

enum _MSQosAnalyzerAlgorithm {
	MSQosAnalyzerAlgorithmSimple,
	MSQosAnalyzerAlgorithmStateful
};
typedef enum _MSQosAnalyzerAlgorithm MSQosAnalyzerAlgorithm;
MS2_PUBLIC const char* ms_qos_analyzer_algorithm_to_string(MSQosAnalyzerAlgorithm alg);
MS2_PUBLIC MSQosAnalyzerAlgorithm ms_qos_analyzer_algorithm_from_string(const char* alg);

/**
 * A MSQosAnalyzer is responsible to analyze RTCP feedback and suggest
 * actions on bitrate or packet rate accordingly.
 * This is an abstract interface.
 * @deprecated
**/
struct _MSQosAnalyzer{
	MSQosAnalyzerDesc *desc;
	OrtpLossRateEstimator *lre;
	char *label;
	/**
	* Each time the algorithm suggest an action, this callback is called with the userpointer
	* @param userpointer on_action_suggested_user_pointer pointer given
	* @param argc number of arguments on the third argument array
	* @param argv array containing various algorithm dependent information
	**/
	void (*on_action_suggested)(void* userpointer, int argc, const char** argv);
	/** User pointer given at #on_action_suggested callback **/
	void *on_action_suggested_user_pointer;
	int refcnt;
	MSQosAnalyzerAlgorithm type;
};


MS2_PUBLIC MSQosAnalyzer * ms_qos_analyzer_ref(MSQosAnalyzer *obj);
MS2_PUBLIC void ms_qos_analyzer_unref(MSQosAnalyzer *obj);
MS2_PUBLIC void ms_qos_analyser_set_label(MSQosAnalyzer *obj, const char *label);
MS2_PUBLIC void ms_qos_analyzer_suggest_action(MSQosAnalyzer *obj, MSRateControlAction *action);
MS2_PUBLIC bool_t ms_qos_analyzer_has_improved(MSQosAnalyzer *obj);
MS2_PUBLIC bool_t ms_qos_analyzer_process_rtcp(MSQosAnalyzer *obj, mblk_t *rtcp);
MS2_PUBLIC void ms_qos_analyzer_update(MSQosAnalyzer *obj);
MS2_PUBLIC const char* ms_qos_analyzer_get_name(MSQosAnalyzer *obj);
MS2_PUBLIC void ms_qos_analyzer_set_on_action_suggested(MSQosAnalyzer *obj, void (*on_action_suggested)(void*,int,const char**),void* u);

/**
 * The simple qos analyzer is an implementation of MSQosAnalyzer that performs analysis for single stream.
**/
MS2_PUBLIC MSQosAnalyzer * ms_simple_qos_analyzer_new(RtpSession *session);

MS2_PUBLIC MSQosAnalyzer * ms_stateful_qos_analyzer_new(RtpSession *session);
/**
 * The audio/video qos analyzer is an implementation of MSQosAnalyzer that performs analysis of two audio and video streams.
**/
/*MSQosAnalyzer * ms_av_qos_analyzer_new(RtpSession *asession, RtpSession *vsession);*/

/**
 * The MSBitrateController the overall behavior and state machine of the adaptive rate control system.
 * It requires a MSQosAnalyzer to obtain analyse of the quality of service, and a MSBitrateDriver
 * to run the actions on media streams, like decreasing or increasing bitrate.
**/
typedef struct _MSBitrateController MSBitrateController;

/**
 * Instanciates MSBitrateController
 * @param qosanalyzer a Qos analyzer object
 * @param driver a bitrate driver object.
 * The newly created bitrate controller owns references to the analyzer and the driver.
**/
MS2_PUBLIC MSBitrateController *ms_bitrate_controller_new(MSQosAnalyzer *qosanalyzer, MSBitrateDriver *driver);

/**
 * Asks the bitrate controller to process a newly received RTCP packet.
 * @param MSBitrateController the bitrate controller object.
 * @param rtcp an RTCP packet received for the media session(s) being managed by the controller.
 * If the RTCP packet contains useful feedback regarding quality of the media streams received by the far end,
 * then the bitrate controller may take decision and execute actions on the local media streams to adapt the
 * output bitrate.
**/
MS2_PUBLIC void ms_bitrate_controller_process_rtcp(MSBitrateController *obj, mblk_t *rtcp);

MS2_PUBLIC void ms_bitrate_controller_update(MSBitrateController *obj);

/**
 * Return the QoS analyzer associated to the bitrate controller
**/
MS2_PUBLIC MSQosAnalyzer * ms_bitrate_controller_get_qos_analyzer(MSBitrateController *obj);

/**
 * Destroys the bitrate controller
 *
 * If no other entity holds references to the underlyings MSQosAnalyzer and MSBitrateDriver object,
 * then they will be destroyed too.
**/
MS2_PUBLIC void ms_bitrate_controller_destroy(MSBitrateController *obj);

/**
 * Convenience function to create a bitrate controller managing a single audio stream.
 * @param session the RtpSession object for the media stream
 * @param encoder the MSFilter object responsible for encoding the audio data.
 * @param flags unused.
 * This function actually calls internally:
 * <br>
 * \code
 * ms_bitrate_controller_new(ms_simple_qos_analyzer_new(session),ms_audio_bitrate_driver_new(encoder));
 * \endcode
**/
MS2_PUBLIC MSBitrateController *ms_audio_bitrate_controller_new(RtpSession *session, MSFilter *encoder, unsigned int flags);

/**
 * Convenience fonction to create a bitrate controller managing a video and an audio stream.
 * @param vsession the video RtpSession
 * @param venc the video encoder
 * @param asession the audio RtpSession
 * @param aenc the audio encoder
 * This function actually calls internally:
 * <br>
 * \code
 * ms_bitrate_controller_new(ms_av_qos_analyzer_new(asession,vsession),ms_av_bitrate_driver_new(aenc,venc));
 * \endcode
**/
MS2_PUBLIC MSBitrateController *ms_av_bitrate_controller_new(RtpSession *asession, MSFilter *aenc, RtpSession *vsession, MSFilter *venc);

MS2_PUBLIC MSBitrateController *ms_bandwidth_bitrate_controller_new(RtpSession *asession, MSFilter *aenc, RtpSession *vsession, MSFilter *venc);
#ifdef __cplusplus
}
#endif

#endif


