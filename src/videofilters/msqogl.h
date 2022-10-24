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

#ifndef _MS_QOGL_H
#define _MS_QOGL_H

#include <QQuickFramebufferObject>
#include <QOpenGLFunctions>

#include "mediastreamer2/msfilter.h"
#include "mediastreamer2/msogl_functions.h"

#include "opengles_display.h"

#include <mutex>


class BufferRenderer;
class QQuickWindow;

struct _FilterData {
	BufferRenderer * renderer;// Store the current Renderer. Do not manage memory as it is done by Qt
	OpenGlFunctions functions;
	
	struct opengles_display *display;
	MSVideoDisplayMode mode;
	
	MSVideoSize video_size; // Not used at this moment.
	
	bool_t show_video;
	bool_t mirroring;
	bool_t update_mirroring;
	bool_t update_context;
	bool_t is_sdk_linked;	// The filter Data can be deleted when both qt and sdk are unlinked
	bool_t is_qt_linked;
	
	mblk_t * prev_inm;
	MSFilter *parent;// Used to call render with the Filter in order to use lock mecanisms
	std::mutex * free_lock;// Avoid to use MSFilter lock when freeing data
};
typedef struct _FilterData FilterData;

class BufferRenderer : public QQuickFramebufferObject::Renderer {
public:
	BufferRenderer ();
	virtual ~BufferRenderer ();
	
	int mWidth, mHeight;// Frame sizes, requested by Qt
	FilterData * mParent;// Allow the renderer to call filter functions
	
protected:
	QOpenGLFramebufferObject *createFramebufferObject (const QSize &size) override;
	void render () override;
	void synchronize (QQuickFramebufferObject *item) override;

private:
	QQuickWindow *mWindow = nullptr;
};


#endif //_MS_QOGL_H
