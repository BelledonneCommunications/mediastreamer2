/*
 Mediastreamer2 videotoolbox-h265-utilities.cpp
 Copyright (C) 2018 Belledonne Communications SARL

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

#include <dlfcn.h>

#include "videotoolbox-h265-utilities.h"

using namespace std;

namespace mediastreamer {

CFStringRef VideoToolboxH265Utilities::_kVTProfileLevel_HEVC_Main_AutoLevel = nullptr;
VideoToolboxH265Utilities::GetParameterSetNativeFunc VideoToolboxH265Utilities::_CMVideoFormatDescriptionGetHEVCParameterSetAtIndex = nullptr;
VideoToolboxH265Utilities::CreateFormatDescriptionNativeFunc VideoToolboxH265Utilities::_CMVideoFormatDescriptionCreateFromHEVCParameterSets = nullptr;

VideoToolboxH265Utilities::VideoToolboxH265Utilities() {
	if (_kVTProfileLevel_HEVC_Main_AutoLevel == nullptr) {
		_kVTProfileLevel_HEVC_Main_AutoLevel = *reinterpret_cast<CFStringRef *>(dlsym(RTLD_DEFAULT, "kVTProfileLevel_HEVC_Main_AutoLevel"));
	}
	if (_CMVideoFormatDescriptionGetHEVCParameterSetAtIndex == nullptr) {
		void *symPtr = dlsym(RTLD_DEFAULT, "CMVideoFormatDescriptionGetHEVCParameterSetAtIndex");
		_CMVideoFormatDescriptionGetHEVCParameterSetAtIndex = reinterpret_cast<GetParameterSetNativeFunc>(symPtr);
	}
	if (_CMVideoFormatDescriptionCreateFromHEVCParameterSets == nullptr) {
		void *symPtr = dlsym(RTLD_DEFAULT, "CMVideoFormatDescriptionCreateFromHEVCParameterSets");
		_CMVideoFormatDescriptionCreateFromHEVCParameterSets = reinterpret_cast<CreateFormatDescriptionNativeFunc>(symPtr);
	}
}

CMVideoCodecType VideoToolboxH265Utilities::getCodecType() const {
	return kCMVideoCodecType_HEVC;
}

CFStringRef VideoToolboxH265Utilities::getDefaultProfileLevel() const {
	return _kVTProfileLevel_HEVC_Main_AutoLevel;
}

void VideoToolboxH265Utilities::getParameterSet(const CMFormatDescriptionRef format, size_t offset, const uint8_t *&parameterSet, size_t &parameterSetSize, size_t &parameterSetsCount) const {
	OSStatus status = _CMVideoFormatDescriptionGetHEVCParameterSetAtIndex(format, offset, &parameterSet, &parameterSetSize, &parameterSetsCount, nullptr);
	if (status != noErr) throw AppleOSError(status);
}

CMFormatDescriptionRef VideoToolboxH265Utilities::createFormatDescription(size_t paramterSetsCount, const uint8_t *parameterSets[], const size_t parameterSetSizes[]) const {
	CMFormatDescriptionRef formatDesc;
	OSStatus status = _CMVideoFormatDescriptionCreateFromHEVCParameterSets(nullptr, paramterSetsCount, parameterSets, parameterSetSizes, 4, nullptr, &formatDesc);
	if (status != noErr) {
		throw AppleOSError(status);
	}
	return formatDesc;
}

}
