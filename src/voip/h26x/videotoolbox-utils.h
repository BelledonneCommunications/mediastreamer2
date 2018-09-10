/*
 Mediastreamer2 videotoolbox-utils.h
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

#include <exception>
#include <string>

#include <VideoToolbox/VideoToolbox.h>

#include "mediastreamer2/msqueue.h"

#include "h26x-utils.h"

namespace mediastreamer {

std::string toString(OSStatus status);

class AppleOSError: public std::exception {
public:
	AppleOSError(OSStatus status): _desc(toString(status)) {}
	const char *what() const noexcept override {return _desc.c_str();}

private:
	std::string _desc;
};

class VideoToolboxUtilities {
public:
	virtual ~VideoToolboxUtilities() = default;

	virtual CMVideoCodecType getCodecType() const = 0;
	virtual CFStringRef getDefaultProfileLevel() const = 0;

	void getParameterSets(const CMFormatDescriptionRef format, MSQueue *outPs) const;
	CMFormatDescriptionRef createFormatDescription(const H26xParameterSetsStore &psStore) const;

	static VideoToolboxUtilities *create(const std::string &mime);

protected:
	virtual void getParameterSet(const CMFormatDescriptionRef format, size_t offset, const uint8_t *&parameterSet, size_t &parameterSetSize, size_t &parameterSetsCount) const = 0;
	virtual CMFormatDescriptionRef createFormatDescription(size_t parameterSetsCount, const uint8_t *parameterSets[], const size_t parameterSetSizes[]) const = 0;
};

} // namespace mediastreamer
