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

#pragma once

#include <list>
#include <memory>

#include <VideoToolbox/VTDecompressionSession.h>

#include "mediastreamer2/mscommon.h"
#include "mediastreamer2/msvideo.h"

#include "h26x-utils.h"
#include "videotoolbox-utils.h"

#include "h26x-decoder.h"

namespace mediastreamer {

class VideoToolboxDecoder: public H26xDecoder {
public:
	VideoToolboxDecoder(const std::string &mime);
	~VideoToolboxDecoder();

	bool feed(MSQueue *encodedFrame, uint64_t timestamp) override;
	Status fetch(mblk_t *&frame) override;

	void waitForKeyFrame() override {_freeze = true;}

private:
	class InvalidSessionError : public std::runtime_error {
	public:
		InvalidSessionError() : std::runtime_error(toString(kVTInvalidSessionErr)) {}
	};

	class Frame {
	public:
		Frame(mblk_t *data = nullptr): _data(data) {}
		Frame(const Frame &src): _data(src._data ? dupmsg(src._data) : nullptr) {}
		~Frame() {if (_data) freemsg(_data);}
		mblk_t *getData() const {return _data ? dupmsg(_data) : nullptr;}

	private:
		mblk_t *_data = nullptr;
	};

	void createDecoder();
	void destroyDecoder();
	void decodeFrame(MSQueue *encodedFrame, uint64_t timestamp);
	void formatDescFromSpsPps();

	static void outputCb(void *decompressionOutputRefCon, void *sourceFrameRefCon, OSStatus status,
						 VTDecodeInfoFlags infoFlags, CVImageBufferRef imageBuffer,
						 CMTime presentationTimeStamp, CMTime presentationDuration);

	VTDecompressionSessionRef _session = nullptr;
	CMFormatDescriptionRef _formatDesc = nullptr;
	MSYuvBufAllocator *_pixbufAllocator = nullptr;
	std::list<Frame> _queue;
	std::mutex _mutex;
	std::unique_ptr<H26xParameterSetsStore> _psStore;
	std::unique_ptr<H26xNaluHeader> _naluHeader;
	bool _freeze = true;
	bool _destroying = false;
	static const size_t _naluSizeLength = 4;
};

}
