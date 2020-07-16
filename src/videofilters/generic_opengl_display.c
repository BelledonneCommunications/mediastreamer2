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

#include "mediastreamer2/msfilter.h"
#include "mediastreamer2/msogl_functions.h"
#include "mediastreamer2/msogl.h"
#include "mediastreamer2/msvideo.h"

#include "opengles_display.h"

// =============================================================================

struct _ContextInfo {
	GLuint width;
	GLuint height;

	OpenGlFunctions *functions;
};

typedef struct _ContextInfo ContextInfo;

struct _FilterData {
	ContextInfo context_info;

	struct opengles_display *display;

	MSVideoSize video_size; // Not used at this moment.

	bool_t show_video;
	bool_t mirroring;
	bool_t update_mirroring;
	bool_t update_context;

	mblk_t * prev_inm;
};

typedef struct _FilterData FilterData;

// =============================================================================
// Process.
// =============================================================================

static void ogl_init (MSFilter *f) {
	FilterData *data = ms_new0(FilterData, 1);
	data->display = ogl_display_new();
	data->show_video = TRUE;
	data->mirroring = TRUE;
	data->update_mirroring = FALSE;
	data->prev_inm = NULL;

	f->data = data;
}

static void ogl_uninit (MSFilter *f) {
	FilterData *data = f->data;
	ogl_display_free(data->display);
	ms_free(data);
}
static void ogl_process (MSFilter *f) {
	FilterData *data;
	ContextInfo *context_info;
	MSPicture src;
	mblk_t *inm;

	ms_filter_lock(f);

	data = f->data;
	context_info = &data->context_info;

	// No context given or video disabled.
	if (!context_info->width || !context_info->height || !context_info->functions || !data->show_video)
		goto end;

	if (
		f->inputs[0] != NULL &&
		((inm = ms_queue_peek_last(f->inputs[0])) != NULL) &&
		ms_yuv_buf_init_from_mblk(&src, inm) == 0
	) {
		data->video_size.width = src.w;
		data->video_size.height = src.h;

		//if (data->mirroring && !mblk_get_precious_flag(inm))
		//	ms_yuv_buf_mirror(&src);

		ogl_display_set_yuv_to_display(data->display, inm);

		// Apply mirroring flag if the frame changed compared to last time process was executed or at the 1st iteration
		if (((data->prev_inm != inm) || (data->prev_inm == NULL)) && (data->update_mirroring)) {
			ogl_display_enable_mirroring_to_display(data->display, data->mirroring);
			data->update_mirroring = FALSE;
		}
		data->prev_inm = inm;
	}

end:
	ms_filter_unlock(f);

	if (f->inputs[0] != NULL)
		ms_queue_flush(f->inputs[0]);

	if (f->inputs[1] != NULL)
		ms_queue_flush(f->inputs[1]);
}

// =============================================================================
// Methods.
// =============================================================================

static int ogl_set_video_size (MSFilter *f, void *arg) {
	ms_filter_lock(f);
	((FilterData *)f->data)->video_size = *(MSVideoSize *)arg;
	ms_filter_unlock(f);

	return 0;
}

static int ogl_set_native_window_id (MSFilter *f, void *arg) {
	FilterData *data;
	ContextInfo *context_info;

	ms_filter_lock(f);

	data = f->data;
	context_info = *((ContextInfo **)arg);

	if (context_info && (unsigned long)context_info != MS_FILTER_VIDEO_NONE) {
		ms_message(
			"set native window id: %p (width: %d, height: %d)\n",
			(void*)context_info, (int)context_info->width, (int)context_info->height
		);
		data->context_info = *context_info;
	} else {
		ms_message("reset native window id");
		memset(&data->context_info, 0, sizeof data->context_info);
	}

	data->update_context = TRUE;

	ms_filter_unlock(f);

	return 0;
}

static int ogl_get_native_window_id (MSFilter *f, void *arg) {
	(void)f;
	(void)arg;

	return 0;
}

static int ogl_show_video (MSFilter *f, void *arg) {
	ms_filter_lock(f);
	((FilterData *)f->data)->show_video = *(bool_t *)arg;
	ms_filter_unlock(f);

	return 0;
}

static int ogl_zoom (MSFilter *f, void *arg) {
	ms_filter_lock(f);
	ogl_display_zoom(((FilterData *)f->data)->display, arg);
	ms_filter_unlock(f);

	return 0;
}

static int ogl_enable_mirroring (MSFilter *f, void *arg) {
	FilterData *data = (FilterData *)f->data;
	ms_filter_lock(f);
	data->mirroring = *(bool_t *)arg;
	// This is a request to update the mirroring flag and it will be honored as soon as a new frame comes in
	data->update_mirroring = TRUE;
	ms_filter_unlock(f);

	return 0;
}

static int ogl_call_render (MSFilter *f, void *arg) {
	FilterData *data;
	const ContextInfo *context_info;

	(void)arg;

	ms_filter_lock(f);

	data = f->data;
	context_info = &data->context_info;

	if (context_info->width && context_info->height && context_info->functions && data->show_video) {
		if (data->update_context) {
			ogl_display_init(data->display, context_info->functions, context_info->width, context_info->height);
			data->update_context = FALSE;
		}

		ogl_display_render(data->display, 0);
	}

	ms_filter_unlock(f);

	return 0;
}

// =============================================================================
// Register filter.
// =============================================================================

static MSFilterMethod methods[] = {
	{ MS_FILTER_SET_VIDEO_SIZE, ogl_set_video_size },
	{ MS_VIDEO_DISPLAY_SET_NATIVE_WINDOW_ID, ogl_set_native_window_id },
	{ MS_VIDEO_DISPLAY_GET_NATIVE_WINDOW_ID, ogl_get_native_window_id },
	{ MS_VIDEO_DISPLAY_SHOW_VIDEO, ogl_show_video },
	{ MS_VIDEO_DISPLAY_ZOOM, ogl_zoom },
	{ MS_VIDEO_DISPLAY_ENABLE_MIRRORING, ogl_enable_mirroring },
	{ MS_OGL_RENDER, ogl_call_render },
	{ 0, NULL }
};

MSFilterDesc ms_ogl_desc = {
	.id = MS_OGL_ID,
	.name = "MSOGL",
	.text = "A generic opengl video display",
	.category = MS_FILTER_OTHER,
	.ninputs = 2,
	.noutputs = 0,
	.init = ogl_init,
	.process = ogl_process,
	.uninit = ogl_uninit,
	.methods = methods
};

MS_FILTER_DESC_EXPORT(ms_ogl_desc)
