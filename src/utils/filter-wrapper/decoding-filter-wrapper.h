/*
 Mediastreamer2 decoding-filter-wrapper.h
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

#include "mediastreamer2/msfilter.h"

#include "filter-interface/decoder-filter.h"
#include "filter-wrapper-base.h"

namespace mediastreamer {

class DecodingFilterWrapper {
public:
	static int onAddFmtpCall(MSFilter *f, void *arg);
	static int onResetFirstImageCall(MSFilter *f, void *arg);
	static int onGetVideoSizeCall(MSFilter *f, void *arg);
	static int onGetFpsCall(MSFilter *f, void *arg);
	static int onGetOutFmtCall(MSFilter *f, void *arg);
	static int onEnableAvpfCall(MSFilter *f, void *arg);
	static int onEnableFreezeOnErrorCall(MSFilter *f, void *arg);
	static int onFreezeOnErrorEnabledCall(MSFilter *f, void *arg);
};

}

#define MS_DECODING_FILTER_WRAPPER_METHODS_DECLARATION(base_name) \
static MSFilterMethod  MS_FILTER_WRAPPER_METHODS_NAME(base_name)[] = { \
	{	MS_FILTER_ADD_FMTP                                 , DecodingFilterWrapper::onAddFmtpCall              }, \
	{	MS_VIDEO_DECODER_RESET_FIRST_IMAGE_NOTIFICATION    , DecodingFilterWrapper::onResetFirstImageCall      }, \
	{	MS_FILTER_GET_VIDEO_SIZE                           , DecodingFilterWrapper::onGetVideoSizeCall         }, \
	{	MS_FILTER_GET_FPS                                  , DecodingFilterWrapper::onGetFpsCall               }, \
	{	MS_FILTER_GET_OUTPUT_FMT                           , DecodingFilterWrapper::onGetOutFmtCall            }, \
	{ 	MS_VIDEO_DECODER_ENABLE_AVPF                       , DecodingFilterWrapper::onEnableAvpfCall           }, \
	{	MS_VIDEO_DECODER_FREEZE_ON_ERROR                   , DecodingFilterWrapper::onEnableFreezeOnErrorCall  }, \
	{	MS_VIDEO_DECODER_FREEZE_ON_ERROR_ENABLED           , DecodingFilterWrapper::onFreezeOnErrorEnabledCall }, \
	{	0                                                  , nullptr                                           } \
}

#define MS_DECODING_FILTER_WRAPPER_DESCRIPTION_DECLARATION(base_name, id, text, enc_fmt, flags) \
	MS_FILTER_WRAPPER_FILTER_DESCRIPTION_BASE(base_name, id, text, MS_FILTER_DECODER, enc_fmt, 1, 1, flags)
