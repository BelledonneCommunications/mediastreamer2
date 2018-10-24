/*
 Mediastreamer2 h26x-utils.h
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

#include <cstdint>
#include <map>
#include <memory>
#include <unordered_map>
#include <vector>

#include "mediastreamer2/msqueue.h"

#include "nal-packer.h"
#include "nal-unpacker.h"

namespace mediastreamer {


class H26xNaluType {
public:
	H26xNaluType() = default;
	H26xNaluType(uint8_t value);
	virtual ~H26xNaluType() = default;

	operator uint8_t() const {return _value;}

	virtual bool isVcl() const = 0;
	virtual bool isParameterSet() const = 0;
	virtual bool isKeyFramePart() const = 0;

protected:
	uint8_t _value = 0;
};

class H26xNaluHeader {
public:
	H26xNaluHeader() = default;
	virtual ~H26xNaluHeader() = default;

	void setFBit(bool val) {_fBit = val;}
	bool getFBit() const {return _fBit;}

	virtual const H26xNaluType &getAbsType() const = 0;

	virtual void parse(const uint8_t *header) = 0;
	virtual mblk_t *forge() const = 0;

protected:
	bool _fBit = false;
};

class MS2_PUBLIC H26xUtils {
public:
	H26xUtils() = delete;

	static void naluStreamToNalus(const std::vector<uint8_t> &byteStream, MSQueue *out);
	static void naluStreamToNalus(const uint8_t *byteStream, size_t size, MSQueue *out);

	static void byteStreamToNalus(const std::vector<uint8_t> &byteStream, MSQueue *out);
	static void byteStreamToNalus(const uint8_t *byteStream, size_t size, MSQueue *out);

	static void nalusToByteStream(MSQueue *nalus, std::vector<uint8_t> &byteStream);
};


class H26xParameterSetsInserter {
public:
	virtual ~H26xParameterSetsInserter() = default;
	virtual void process(MSQueue *in, MSQueue *out) = 0;
	virtual void flush() = 0;

protected:
	static void replaceParameterSet(mblk_t *&ps, mblk_t *newPs);
};

class H26xParameterSetsStore {
public:
	virtual ~H26xParameterSetsStore();

	bool hasNewParameters() const {return _newParameters;}
	void acknowlege() {_newParameters = false;}

	void extractAllPs(MSQueue *frame);
	bool psGatheringCompleted() const;
	void fetchAllPs(MSQueue *outq) const;

protected:
	H26xParameterSetsStore(const std::string &mime, const std::initializer_list<int> &psCodes);
	void addPs(int naluType, mblk_t *nalu);

	std::map<int, mblk_t *> _ps;
	std::unique_ptr<H26xNaluHeader> _naluHeader;
	bool _newParameters = false;
};

class H26xToolFactory {
public:
	H26xToolFactory() = default;
	virtual ~H26xToolFactory() = default;

	MS2_PUBLIC static const H26xToolFactory &get(const std::string &mime);

	virtual H26xNaluHeader *createNaluHeader() const = 0;
	virtual NalPacker *createNalPacker(MSFactory *factory) const = 0;
	virtual NalUnpacker *createNalUnpacker() const = 0;
	virtual H26xParameterSetsInserter *createParameterSetsInserter() const = 0;
	virtual H26xParameterSetsStore *createParameterSetsStore() const = 0;

private:
	static std::unordered_map<std::string, std::unique_ptr<H26xToolFactory>> _instances;
};

}
