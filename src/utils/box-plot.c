/*
mediastreamer2 library - modular sound and video processing and streaming
Copyright (C) 2010  Belledonne Communications SARL
Author: Simon Morlat <simon.morlat@linphone.org>

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
*/

#include <bctoolbox/port.h>

#include "mediastreamer2/box-plot.h"

#undef min
#undef max
#define min(x, y) (x < y) ? x : y;
#define max(x, y) (x < y) ? y : x;


void ms_box_plot_reset(MSBoxPlot *bp) {
	memset(bp, 0, sizeof(MSBoxPlot));
}

void ms_box_plot_add_value(MSBoxPlot *bp, int64_t value) {
	if (bp->count == 0) {
		bp->min = bp->max = value;
		bp->mean = (double)value;
	} else {
		bp->min = min(bp->min, value);
		bp->max = max(bp->max, value);
		bp->mean = ((bp->mean * bp->count) + value) / (bp->count + 1);
	}
	bp->count++;
}

char *ms_box_plot_to_string(const MSBoxPlot *bp, const char *unit) {
	if (unit == NULL) unit = "";
	return bctbx_strdup_printf("{ min=%lld%s, mean=%.1f%s, max=%lld%s }", (long long int)bp->min, unit, bp->mean, unit, (long long int)bp->max, unit);
}


void ms_u_box_plot_reset(MSUBoxPlot *bp) {
	memset(bp, 0, sizeof(MSUBoxPlot));
}

void ms_u_box_plot_add_value(MSUBoxPlot *bp, uint64_t value) {
	if (bp->count == 0) {
		bp->min = bp->max = value;
		bp->mean = (double)value;
	} else {
		bp->min = min(bp->min, value);
		bp->max = max(bp->max, value);
		bp->mean = ((bp->mean * bp->count) + value) / (bp->count + 1);
	}
	bp->count++;
}

char *ms_u_box_plot_to_string(const MSUBoxPlot *bp, const char *unit) {
	if (unit == NULL) unit = "";
	return bctbx_strdup_printf("{ min=%llu%s, mean=%.1f%s, max=%llu%s }", (long long unsigned)bp->min, unit, bp->mean, unit, (long long unsigned)bp->max, unit);
}
