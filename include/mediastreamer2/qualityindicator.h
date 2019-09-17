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

#ifndef ms2_qualityindicator_h
#define ms2_qualityindicator_h

#include "mediastreamer2/mscommon.h"
#include <ortp/ortp.h>

typedef struct _MSQualityIndicator MSQualityIndicator;

#ifdef __cplusplus
extern "C"{
#endif

/**
 * Creates a quality indicator object.
 * @param session the RtpSession being monitored.
**/ 
MS2_PUBLIC MSQualityIndicator *ms_quality_indicator_new(RtpSession *session);

/**
 * Set a label, just used for logging.
**/
MS2_PUBLIC void ms_quality_indicator_set_label(MSQualityIndicator *obj, const char *label);

/**
 * Updates quality indicator based on a received RTCP packet.
**/
MS2_PUBLIC void ms_quality_indicator_update_from_feedback(MSQualityIndicator *qi, mblk_t *rtcp);

/**
 * Updates quality indicator based on the local statistics directly computed by the RtpSession used when creating the indicator.
 * This function must be called typically every second.
**/
MS2_PUBLIC void ms_quality_indicator_update_local(MSQualityIndicator *qi);

/**
 * Return the real time rating of the session. Its value is between 0 (worse) and 5.0 (best).
**/
MS2_PUBLIC float ms_quality_indicator_get_rating(const MSQualityIndicator *qi);

/**
 * Returns the average rating of the session, that is the rating for all the duration of the session.
**/
MS2_PUBLIC float ms_quality_indicator_get_average_rating(const MSQualityIndicator *qi);

/**
 * Return the real time rating of the listening quality of the session. Its value is between 0.0 (worse) and 5.0 (best).
**/
MS2_PUBLIC float ms_quality_indicator_get_lq_rating(const MSQualityIndicator *qi);

/**
 * Returns the average rating of the listening quality of the session, that is the rating of the listening quality for all the duration of the session.
**/
MS2_PUBLIC float ms_quality_indicator_get_average_lq_rating(const MSQualityIndicator *qi);

/**
 * Returns the local loss rate, as computed internally by ms_quality_indicator_update_local().
 * The value is expressed as a percentage.
 * This method is for advanced usage.
**/
MS2_PUBLIC float ms_quality_indicator_get_local_loss_rate(const MSQualityIndicator *qi);

/**
 * Returns the local late rate, as computed internally by ms_quality_indicator_update_local().
 * The value is expressed as a percentage.
 * This method is for advanced usage.
**/
MS2_PUBLIC float ms_quality_indicator_get_local_late_rate(const MSQualityIndicator *qi);

/**
 * Destroys the quality indicator object.
**/
MS2_PUBLIC void ms_quality_indicator_destroy(MSQualityIndicator *qi);



#ifdef __cplusplus
}
#endif


#endif

