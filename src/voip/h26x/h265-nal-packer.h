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

#include "h265-utils.h"

#include "nal-packer.h"

namespace mediastreamer {

class H265NalPacker: public NalPacker {
public:
	H265NalPacker(size_t maxPayloadSize): NalPacker(new NaluAggregator(maxPayloadSize), new NaluSpliter(maxPayloadSize), maxPayloadSize) {}

private:
	class NaluAggregator: public NaluAggregatorInterface {
	public:
		using NaluAggregatorInterface::NaluAggregatorInterface;
		~NaluAggregator();

		mblk_t *feed(mblk_t *nalu) override;
		bool isAggregating() const override {return _ap != nullptr;}
		void reset() override;
		mblk_t *completeAggregation() override;

	private:
		void placeFirstNalu(mblk_t *nalu);
		void aggregate(mblk_t *nalu);

		size_t _size = 0;
		H265NaluHeader _apHeader;
		mblk_t *_ap = nullptr;
	};

	class NaluSpliter: public NaluSpliterInterface {
	public:
		using NaluSpliterInterface::NaluSpliterInterface;
		void feed(mblk_t *nalu) override;

	private:
		mblk_t *makeFu(const H265NaluHeader &naluHeader, const H265FuHeader &fuHeader, const uint8_t *payload, size_t length);
	};
};

}
