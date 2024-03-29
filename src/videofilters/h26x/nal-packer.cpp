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

#include "nal-packer.h"

using namespace std;

namespace mediastreamer {

void NalPacker::NaluAggregatorInterface::setMaxSize(size_t maxSize) {
	if (isAggregating()) {
		throw logic_error("changing payload size while aggregating NALus");
	}
	_maxSize = maxSize;
}

NalPacker::NalPacker(NaluAggregatorInterface *naluAggregator, NaluSpliterInterface *naluSpliter, size_t maxPayloadSize)
    : _naluSpliter(naluSpliter), _naluAggregator(naluAggregator) {
	setMaxPayloadSize(maxPayloadSize);
}

void NalPacker::setMaxPayloadSize(size_t size) {
	_maxSize = size;
	_naluSpliter->setMaxSize(size);
	_naluAggregator->setMaxSize(size);
	ms_message("H26xNalPacker: max payload size set to %zu bytes", size);
}

void NalPacker::pack(MSQueue *naluq, MSQueue *rtpq, uint32_t ts) {
	switch (_packMode) {
		case SingleNalUnitMode:
			packInSingleNalUnitMode(naluq, rtpq, ts);
			break;
		case NonInterleavedMode:
			packInNonInterleavedMode(naluq, rtpq, ts);
			break;
	}
}

void NalPacker::flush() {
	_naluAggregator->reset();
	ms_queue_flush(_naluSpliter->getPackets());
}

// Private methods
void NalPacker::packInSingleNalUnitMode(MSQueue *naluq, MSQueue *rtpq, uint32_t ts) {
	while (mblk_t *m = ms_queue_get(naluq)) {
		bool end = !!ms_queue_empty(naluq);
		size_t size = msgdsize(m);
		if (size > _maxSize) {
			ms_warning("This H264 packet does not fit into MTU: size=%u", static_cast<unsigned int>(size));
		}
		sendPacket(rtpq, ts, m, end);
	}
}

void NalPacker::packInNonInterleavedMode(MSQueue *naluq, MSQueue *rtpq, uint32_t ts) {
	while (mblk_t *m = ms_queue_get(naluq)) {
		bool end = !!ms_queue_empty(naluq);
		size_t sz = msgdsize(m);
		if (_aggregationEnabled) {
			if (_naluAggregator->isAggregating()) {
				mblk_t *stapPacket = _naluAggregator->feed(m);
				if (stapPacket) {
					sendPacket(rtpq, ts, stapPacket, false);
				} else continue;
			}
			if (sz < (_maxSize / 2)) {
				_naluAggregator->feed(m);
			} else {
				/*send as single NAL or FU-A*/
				if (sz > _maxSize) {
					ms_debug("Sending FU-A packets");
					fragNaluAndSend(rtpq, ts, m, end);
				} else {
					ms_debug("Sending Single NAL");
					sendPacket(rtpq, ts, m, end);
				}
			}
		} else {
			if (sz > _maxSize) {
				ms_debug("Sending FU-A packets");
				fragNaluAndSend(rtpq, ts, m, end);
			} else {
				ms_debug("Sending Single NAL");
				sendPacket(rtpq, ts, m, end);
			}
		}
	}
	if (_naluAggregator->isAggregating()) {
		ms_debug("Sending Single NAL (2)");
		sendPacket(rtpq, ts, _naluAggregator->completeAggregation(), true);
	}
}

void NalPacker::fragNaluAndSend(MSQueue *rtpq, uint32_t ts, mblk_t *nalu, bool_t marker) {
	_naluSpliter->feed(nalu);
	MSQueue *nalus = _naluSpliter->getPackets();
	while (mblk_t *m = ms_queue_get(nalus)) {
		sendPacket(rtpq, ts, m, ms_queue_empty(nalus) ? marker : false);
	}
}

void NalPacker::sendPacket(MSQueue *rtpq, uint32_t ts, mblk_t *m, bool_t marker) {
	mblk_set_timestamp_info(m, ts);
	mblk_set_marker_info(m, marker);
	mblk_set_cseq(m, _refCSeq++);
	ms_queue_put(rtpq, m);
}

} // namespace mediastreamer
