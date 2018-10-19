/*
 *  androidvideo_utils.h
 *
 *  mediastreamer2 library - modular sound and video processing and streaming
 *
 *  Copyright (C) 2010-2018  Belledonne Communications, Grenoble, France
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#ifndef androidvideo_utils_include
#define androidvideo_utils_include

#include <string>

#include <camera/NdkCameraDevice.h>
#include <media/NdkImageReader.h>

#include "mediastreamer2/mscommon.h"

namespace AndroidVideo {
	static camera_status_t checkReturnCameraStatus(camera_status_t status, const std::string &caller) {
		switch (status) {
			case ACAMERA_OK:
				return ACAMERA_OK;
			default:
			case ACAMERA_ERROR_UNKNOWN :
				ms_error("%s: camera_status_t: Unknown error.", caller.c_str());
				break;
			case ACAMERA_ERROR_INVALID_PARAMETER :
				ms_error("%s: camera_status_t: Invalid paramter error.", caller.c_str());
				break;
			case ACAMERA_ERROR_CAMERA_DISCONNECTED :
				ms_error("%s: camera_status_t: Error, camera disconnected.", caller.c_str());
				break;
			case ACAMERA_ERROR_NOT_ENOUGH_MEMORY :
				ms_error("%s: camera_status_t: Error, not enough memory.", caller.c_str());
				break;
			case ACAMERA_ERROR_METADATA_NOT_FOUND :
				ms_error("%s: camera_status_t: Error, metadata not found.", caller.c_str());
				break;
			case ACAMERA_ERROR_CAMERA_DEVICE :
				ms_error("%s: camera_status_t: Error with camera device.", caller.c_str());
				break;
			case ACAMERA_ERROR_CAMERA_SERVICE :
				ms_error("%s: camera_status_t: Error with camera service.", caller.c_str());
				break;
			case ACAMERA_ERROR_SESSION_CLOSED :
				ms_error("%s: camera_status_t: Error, session closed.", caller.c_str());
				break;
			case ACAMERA_ERROR_INVALID_OPERATION :
				ms_error("%s: camera_status_t: Error, invalid operation.", caller.c_str());
				break;
			case ACAMERA_ERROR_STREAM_CONFIGURE_FAIL :
				ms_error("%s: camera_status_t: Error, stream configuration failed.", caller.c_str());
				break;
			case ACAMERA_ERROR_CAMERA_IN_USE :
				ms_error("%s: camera_status_t: Error, camera in use.", caller.c_str());
				break;
			case ACAMERA_ERROR_MAX_CAMERA_IN_USE :
				ms_error("%s: camera_status_t: Error, maximum number of cameras in use reached.", caller.c_str());
				break;
			case ACAMERA_ERROR_CAMERA_DISABLED :
				ms_error("%s: camera_status_t: Error, camera disabled.", caller.c_str());
				break;
			case ACAMERA_ERROR_PERMISSION_DENIED :
				ms_error("%s: camera_status_t: Error, camera permission denied.", caller.c_str());
				break;
		}

		return ACAMERA_ERROR_BASE;
	}

	static media_status_t checkReturnMediaStatus(media_status_t status, const std::string &caller) {
		switch (status) {
			case AMEDIA_OK:
				return AMEDIA_OK;
			case AMEDIA_ERROR_MALFORMED :
				ms_error("%s: media_status_t: Error, media malformed.", caller.c_str());
				break;
			case AMEDIA_ERROR_UNSUPPORTED :
				ms_error("%s: media_status_t: Error, media unsupported.", caller.c_str());
				break;
			case AMEDIA_ERROR_INVALID_OBJECT :
				ms_error("%s: media_status_t: Error, invalid object.", caller.c_str());
				break;
			case AMEDIA_ERROR_INVALID_PARAMETER :
				ms_error("%s: media_status_t: Error, invalid parameter.", caller.c_str());
				break;
			default:
			case AMEDIA_DRM_NOT_PROVISIONED :
			case AMEDIA_DRM_RESOURCE_BUSY :
			case AMEDIA_DRM_DEVICE_REVOKED :
			case AMEDIA_DRM_SHORT_BUFFER :
			case AMEDIA_DRM_SESSION_NOT_OPENED :
			case AMEDIA_DRM_TAMPER_DETECTED :
			case AMEDIA_DRM_VERIFY_FAILED :
			case AMEDIA_DRM_NEED_KEY :
			case AMEDIA_DRM_LICENSE_EXPIRED :
			case AMEDIA_ERROR_UNKNOWN :
				ms_error("%s: media_status_t: Unknown error: %d.", caller.c_str(), status);
				break;
		}

		return AMEDIA_ERROR_BASE;
	}
}

#endif //androidvideo_utils_include