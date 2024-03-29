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

#include <bctoolbox/defs.h>

#include "decoding-filter-wrapper.h"

using namespace std;

namespace mediastreamer {

int DecodingFilterWrapper::onAddFmtpCall(MSFilter *f, void *arg) {
	try {
		const char *fmtp = static_cast<const char *>(arg);
		static_cast<DecoderFilter *>(f->data)->addFmtp(fmtp);
		return 0;
	} catch (const DecoderFilter::MethodCallFailed &) {
		return -1;
	}
}

int DecodingFilterWrapper::onResetFirstImageCall(MSFilter *f, BCTBX_UNUSED(void *arg)) {
	try {
		static_cast<DecoderFilter *>(f->data)->resetFirstImage();
		return 0;
	} catch (const DecoderFilter::MethodCallFailed &) {
		return -1;
	}
}

int DecodingFilterWrapper::onGetVideoSizeCall(MSFilter *f, void *arg) {
	try {
		MSVideoSize *vsize = static_cast<MSVideoSize *>(arg);
		*vsize = static_cast<DecoderFilter *>(f->data)->getVideoSize();
		return 0;
	} catch (const DecoderFilter::MethodCallFailed &) {
		return -1;
	}
}

int DecodingFilterWrapper::onGetFpsCall(MSFilter *f, void *arg) {
	try {
		float *fps = static_cast<float *>(arg);
		*fps = static_cast<DecoderFilter *>(f->data)->getFps();
		return 0;
	} catch (const DecoderFilter::MethodCallFailed &) {
		return -1;
	}
}

int DecodingFilterWrapper::onGetOutFmtCall(MSFilter *f, void *arg) {
	try {
		MSPinFormat *pinFormat = static_cast<MSPinFormat *>(arg);
		pinFormat->fmt = static_cast<DecoderFilter *>(f->data)->getOutputFmt();
		return 0;
	} catch (const DecoderFilter::MethodCallFailed &) {
		return -1;
	}
}

int DecodingFilterWrapper::onEnableAvpfCall(MSFilter *f, void *arg) {
	try {
		static_cast<DecoderFilter *>(f->data)->enableAvpf(*static_cast<bool *>(arg));
		return 0;
	} catch (const DecoderFilter::MethodCallFailed &) {
		return -1;
	}
}

int DecodingFilterWrapper::onEnableFreezeOnErrorCall(MSFilter *f, void *arg) {
	try {
		static_cast<DecoderFilter *>(f->data)->enableFreezeOnError(*static_cast<bool *>(arg));
		return 0;
	} catch (const DecoderFilter::MethodCallFailed &) {
		return -1;
	}
}

int DecodingFilterWrapper::onFreezeOnErrorEnabledCall(MSFilter *f, void *arg) {
	try {
		bool_t *foeEnabled = static_cast<bool_t *>(arg);
		*foeEnabled = static_cast<DecoderFilter *>(f->data)->freezeOnErrorEnabled();
		return 0;
	} catch (const DecoderFilter::MethodCallFailed &) {
		return -1;
	}
}

} // namespace mediastreamer
