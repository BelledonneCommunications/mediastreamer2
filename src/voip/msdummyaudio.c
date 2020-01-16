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

#include <mediastreamer2/mssndcard.h>

static MSFilter* dummy_audio_snd_card_create_reader(MSSndCard *sndcard) {
	MSFactory *factory = ms_snd_card_get_factory(sndcard);
	MSFilter *f = ms_factory_create_filter_from_desc(factory, ms_factory_lookup_filter_by_name(factory, "MSVoidSource"));
	return f;
}

static MSFilter* dummy_audio_snd_card_create_writer(MSSndCard *sndcard) {
	MSFactory *factory = ms_snd_card_get_factory(sndcard);
	MSFilter *f = ms_factory_create_filter_from_desc(factory, ms_factory_lookup_filter_by_name(factory, "MSVoidSink"));
	return f;
}

static void dummy_sound_snd_card_detect(MSSndCardManager *m);

MSSndCardDesc dummy_audio_snd_card_desc={
	"DummyAudio",
	dummy_sound_snd_card_detect,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	dummy_audio_snd_card_create_reader,
	dummy_audio_snd_card_create_writer
};

MSSndCard* ms_dummy_audio_snd_card_new(void) {
	MSSndCard* sndcard;
	sndcard = ms_snd_card_new(&dummy_audio_snd_card_desc);
	sndcard->data = NULL;
	sndcard->name = ms_strdup("dummy audio sound card");
	sndcard->capabilities = MS_SND_CARD_CAP_PLAYBACK | MS_SND_CARD_CAP_CAPTURE;
	sndcard->latency = 0;
	return sndcard;
}

static void dummy_sound_snd_card_detect(MSSndCardManager *m) {
	ms_snd_card_manager_add_card(m, ms_dummy_audio_snd_card_new());
}

MS_FILTER_DESC_EXPORT(dummy_audio_snd_card_desc)
