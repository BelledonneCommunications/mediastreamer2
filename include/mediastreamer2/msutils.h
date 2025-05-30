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

#ifndef msutils_h
#define msutils_h

#include "mediastreamer2/mscommon.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*MSAudioDiffProgressNotify)(void *user_data, int percentage);

typedef struct _MSAudioDiffParams {
	int max_shift_percent; /*percentage of overlap between the two signals, used to restrict the cross correlation
	                          around t=0 in range [1 ; 100].*/
	int chunk_size_ms;     /*chunk size in milliseconds, if chunked cross correlation is to be used. Use 0 otherwise.*/
} MSAudioDiffParams;

/**
 * Utility that compares two PCM 16 bits audio files and returns a similarity factor between 0 and 1.
 * @param ref_file path to a wav file contaning the reference audio segment
 * @param matched_file path to a wav file contaning the audio segment where the reference file is to be matched.
 * @param ret the similarity factor, set in return
 * @param max_shift_percent percentage of overlap between the two signals, used to restrict the cross correlation around
 *t=0 in range [1 ; 100].
 * @param func a callback called to show progress of the operation
 * @param user_data a user data passed to the callback when invoked.
 * @return -1 on error, 0 if succesful.
 **/
MS2_PUBLIC int ms_audio_diff(const char *ref_file,
                             const char *matched_file,
                             double *ret,
                             const MSAudioDiffParams *params,
                             MSAudioDiffProgressNotify func,
                             void *user_data);

/**
 * Utility that compares two PCM 16 bits audio files and returns a similarity factor between 0 and 1 computed on the
 *speech parts, and the energy measured on the silence parts of the matched file. The audio segments are first aligned
 *on the position of the maximal cross correlation computed between start_time_short_ms and stop_time_short_ms. Then the
 *silence and the speech parts are identified by filtering the reference audio from the time start_time_ms to the end of
 *the file. The similarity score and the energy are measured on the speech and silence parts respectively.
 * @param ref_file path to a wav file contaning the reference audio segment.
 * @param matched_file path to a wav file contaning the audio segment where the reference file is to be matched.
 * @param ret the similarity factor, set in return.
 * @param energy the energy measurement in the silence of the matched audio segment, set in return.
 * @param max_shift_percent percentage of overlap between the two signals, used to restrict the cross correlation around
 *t=0 in range [1 ; 100], in order to align the audio segments.
 * @param func a callback called to show progress of the operation.
 * @param user_data a user data passed to the callback when invoked.
 * @param start_time_short_ms beginning of the audio segment used to find the alignement, in ms.
 * @param stop_time_short_ms end of the audio segment used to find the alignement, in ms.
 * @param start_time_ms beginning of the audio segment used to compare the reference with the matched filed, after
 *alignement, in ms.
 * @return -1 on error, 0 if succesful.
 **/
MS2_PUBLIC int ms_audio_compare_silence_and_speech(const char *ref_file,
                                                   const char *matched_file,
                                                   double *ret,
                                                   double *energy,
                                                   const MSAudioDiffParams *params,
                                                   MSAudioDiffProgressNotify func,
                                                   void *user_data,
                                                   const int start_time_short_ms,
                                                   const int stop_time_short_ms,
                                                   const int start_time_ms);

/**
 * Utility that computes and returns the energy of a PCM 16 bits audio file.
 * @param file path to a wav file containing the audio segment.
 * @param energy the energy, set in return.
 * @return -1 on error, 0 if succesful.
 **/
MS2_PUBLIC int ms_audio_energy(const char *ref_file, double *energy);

#ifdef __cplusplus
}
#endif

#endif
