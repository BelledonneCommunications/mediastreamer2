/*
 Mediastreamer2 videotoolbox-h264-utilities.h
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

#pragma once

#include "videotoolbox-utils.h"

namespace mediastreamer {

class VideoToolboxH264Utilities: public VideoToolboxUtilities {
public:
	CMVideoCodecType getCodecType() const override;
	CFStringRef getDefaultProfileLevel() const override;

private:
	void getParameterSet(const CMFormatDescriptionRef format, size_t offset, const uint8_t *&parameterSet, size_t &parameterSetSize, size_t &parameterSetsCount) const override;
	CMFormatDescriptionRef createFormatDescription(size_t parameterSetsCount, const uint8_t *parameterSets[], const size_t parameterSetSizes[]) const override;
};

}
