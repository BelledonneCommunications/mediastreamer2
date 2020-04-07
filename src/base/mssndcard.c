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

#ifdef HAVE_CONFIG_H
#include "mediastreamer-config.h"
#endif

#include "mediastreamer2/mssndcard.h"


static bool_t bypass_sndcard_detection = FALSE;


MSSndCardManager * ms_snd_card_manager_new(void){
	MSSndCardManager *obj=(MSSndCardManager *)ms_new0(MSSndCardManager,1);
	obj->factory = NULL;
	obj->cards=NULL;
	obj->descs=NULL;
	return obj;
}

void ms_snd_card_manager_destroy(MSSndCardManager* scm){
	if (scm!=NULL && !bypass_sndcard_detection){
		bctbx_list_t *elem;
		for(elem=scm->descs;elem!=NULL;elem=elem->next){
			MSSndCardDesc *desc = (MSSndCardDesc*)elem->data;
			if (desc->unload!=NULL)
				desc->unload(scm);
		}
		bctbx_list_for_each(scm->cards,(void (*)(void*))ms_snd_card_unref);
		bctbx_list_free(scm->cards);
		bctbx_list_free(scm->descs);
	}
	ms_free(scm);
}

MSFactory * ms_snd_card_get_factory(MSSndCard * c){
	return c->sndcardmanager->factory;
}

MSSndCard * ms_snd_card_manager_get_card(MSSndCardManager *m, const char *id){
	bctbx_list_t *elem;
	for (elem=m->cards;elem!=NULL;elem=elem->next){
		MSSndCard *card=(MSSndCard*)elem->data;
		if (id==NULL) return card;
		if (strcmp(ms_snd_card_get_string_id(card),id)==0)	return card;
	}
	if (id!=NULL) ms_warning("no card with id %s",id);
	return NULL;
}

static MSSndCard *get_card_with_cap(MSSndCardManager *m, const char *id, unsigned int caps){
	bctbx_list_t *elem;
	for (elem=m->cards;elem!=NULL;elem=elem->next){
		MSSndCard *card=(MSSndCard*)elem->data;
		if ((id== NULL || strcmp(ms_snd_card_get_string_id(card),id)==0) && (card->capabilities & caps) == caps)	return card;
	}
	return NULL;
}

bctbx_list_t * ms_snd_card_manager_get_all_cards_with_name(MSSndCardManager *m, const char *name){
	bctbx_list_t *cards = NULL;
	bctbx_list_t *elem;
	for (elem=m->cards;elem!=NULL;elem=elem->next){
		MSSndCard *card=(MSSndCard*)elem->data;
		if (strcmp(ms_snd_card_get_name(card),name)==0) {
			cards=bctbx_list_append(cards, ms_snd_card_ref(card));
		}
	}
	return cards;
}

MSSndCard * ms_snd_card_manager_get_playback_card(MSSndCardManager *m, const char *id){
	MSSndCard *ret;
	ret = get_card_with_cap(m, id, MS_SND_CARD_CAP_PLAYBACK);
	if (ret) return ret;
	if (id != NULL) ms_warning("No playback card with id %s",id);
	return NULL;
}

MSSndCard * ms_snd_card_manager_get_capture_card(MSSndCardManager *m, const char *id){
	MSSndCard *ret;
	ret = get_card_with_cap(m, id, MS_SND_CARD_CAP_CAPTURE);
	if (ret) return ret;
	if (id != NULL) ms_warning("No capture card with id %s",id);
	return NULL;
}

MSSndCard * ms_snd_card_manager_get_default_card(MSSndCardManager *m){
	/*return the first card that has the capture+playback capability */
	return get_card_with_cap(m, NULL, MS_SND_CARD_CAP_PLAYBACK | MS_SND_CARD_CAP_CAPTURE);
}

MSSndCard * ms_snd_card_manager_get_default_capture_card(MSSndCardManager *m){
	return get_card_with_cap(m, NULL, MS_SND_CARD_CAP_CAPTURE);
}

MSSndCard * ms_snd_card_manager_get_default_playback_card(MSSndCardManager *m){
	return get_card_with_cap(m, NULL, MS_SND_CARD_CAP_PLAYBACK);
}

const bctbx_list_t * ms_snd_card_manager_get_list(MSSndCardManager *m){
	return m->cards;
}

void ms_snd_card_set_manager(MSSndCardManager*m, MSSndCard *c){
	if (c->sndcardmanager == NULL) c->sndcardmanager = m;
}

static const char *cap_to_string(unsigned int cap){
	if ((cap & MS_SND_CARD_CAP_CAPTURE) && (cap & MS_SND_CARD_CAP_PLAYBACK)) return "capture, playback";
	if (cap & MS_SND_CARD_CAP_CAPTURE) return "capture";
	if (cap & MS_SND_CARD_CAP_PLAYBACK) return "playback";
	return "none";
}

void ms_snd_card_manager_add_card(MSSndCardManager *m, MSSndCard *c){
	ms_snd_card_set_manager(m,c);
	ms_message("Card '%s' added with capabilities [%s]",ms_snd_card_get_string_id(c), cap_to_string(c->capabilities));
	m->cards=bctbx_list_append(m->cards, ms_snd_card_ref(c));
}

void ms_snd_card_manager_prepend_card(MSSndCardManager *m, MSSndCard *c){
	ms_snd_card_set_manager(m,c);
	ms_message("Card '%s' prepended with capabilities [%s]",ms_snd_card_get_string_id(c), cap_to_string(c->capabilities));
	m->cards=bctbx_list_prepend(m->cards, ms_snd_card_ref(c));
}

void ms_snd_card_manager_prepend_cards(MSSndCardManager *m, bctbx_list_t *l) {
	bctbx_list_t *elem;
	bctbx_list_t *lcopy = bctbx_list_copy(l);
	if (m->cards != NULL) m->cards = bctbx_list_concat(lcopy, m->cards);
	else m->cards = lcopy;
	for (elem = l; elem != NULL; elem = elem->next) {
		MSSndCard *card = (MSSndCard *)elem->data;
		ms_snd_card_set_manager(m, card);
		ms_message("Card '%s' added", ms_snd_card_get_string_id(card));
	}
}

void ms_snd_card_manager_bypass_soundcard_detection(bool_t value) {
	bypass_sndcard_detection = value;
}

static void card_detect(MSSndCardManager *m, MSSndCardDesc *desc){
	if (bypass_sndcard_detection) return;

	if (desc->detect!=NULL)
		desc->detect(m);
}

void ms_snd_card_manager_register_desc(MSSndCardManager *m, MSSndCardDesc *desc){
	if (bctbx_list_find(m->descs, desc) == NULL) {
		m->descs = bctbx_list_append(m->descs, desc);
		card_detect(m, desc);
	}
}

void ms_snd_card_manager_unregister_desc(MSSndCardManager *m, MSSndCardDesc *desc){
	if (bctbx_list_find(m->descs, desc) != NULL) {
		m->descs = bctbx_list_remove(m->descs, desc);
	}
}

void ms_snd_card_manager_reload(MSSndCardManager *m){
	bctbx_list_t *elem;
	bctbx_list_for_each(m->cards, (void (*)(void*))ms_snd_card_unref);
	bctbx_list_free(m->cards);
	m->cards = NULL;
	for (elem = m->descs; elem != NULL; elem = elem->next) {
		card_detect(m, (MSSndCardDesc*)elem->data);
	}
}

MSSndCard* ms_snd_card_dup(MSSndCard *card){
	MSSndCard *obj=NULL;
	if (card->desc->duplicate!=NULL)
		obj=card->desc->duplicate(card);
	return obj;
}

MSSndCard * ms_snd_card_new(MSSndCardDesc *desc){
	return ms_snd_card_new_with_name(desc,NULL);
}

MSSndCard * ms_snd_card_new_with_name(MSSndCardDesc *desc,const char* name) {
	MSSndCard *obj=(MSSndCard *)ms_new0(MSSndCard,1);
	obj->sndcardmanager = NULL;
	obj->desc=desc;
	obj->name=name?ms_strdup(name):NULL;
	obj->data=NULL;
	obj->id=NULL;
	obj->internal_id=-1;
	obj->device_type=MS_SND_CARD_DEVICE_TYPE_UNKNOWN;
	obj->capabilities=MS_SND_CARD_CAP_CAPTURE|MS_SND_CARD_CAP_PLAYBACK;
	obj->streamType=MS_SND_CARD_STREAM_VOICE;
	if (desc->init!=NULL)
		desc->init(obj);
	return obj;
}

MSSndCardDeviceType ms_snd_card_get_device_type(const MSSndCard *obj){
	return obj->device_type;
}

const char * ms_snd_card_device_type_to_string(const MSSndCardDeviceType type){
	switch(type) {
		case MS_SND_CARD_DEVICE_TYPE_TELEPHONY:
			return "Telephony";
		case MS_SND_CARD_DEVICE_TYPE_AUX_LINE:
			return "Aux line";
		case MS_SND_CARD_DEVICE_TYPE_GENERIC_USB:
			return "USB device";
		case MS_SND_CARD_DEVICE_TYPE_HEADSET:
			return "Headset";
		case MS_SND_CARD_DEVICE_TYPE_MICROPHONE:
			return "Microphone";
		case MS_SND_CARD_DEVICE_TYPE_EARPIECE:
			return "Earpiece";
		case MS_SND_CARD_DEVICE_TYPE_HEADPHONES:
			return "Headphones";
		case MS_SND_CARD_DEVICE_TYPE_SPEAKER:
			return "Speaker";
		case MS_SND_CARD_DEVICE_TYPE_BLUETOOTH:
			return "Bluetooth";
		case MS_SND_CARD_DEVICE_TYPE_BLUETOOTH_A2DP:
			return "Bluetooth A2DP";
		case MS_SND_CARD_DEVICE_TYPE_UNKNOWN:
			return "Unknown";
		default:
			return "bad type";
	}

	return "bad type";
}

const char *ms_snd_card_get_driver_type(const MSSndCard *obj){
	return obj->desc->driver_type;
}

const char *ms_snd_card_get_name(const MSSndCard *obj){
	return obj->name;
}

unsigned int ms_snd_card_get_capabilities(const MSSndCard *obj){
	return obj->capabilities;
}

MS2_PUBLIC int ms_snd_card_get_minimal_latency(MSSndCard *obj){
	return obj->latency;
}

const char *ms_snd_card_get_string_id(MSSndCard *obj){
	if (obj->id==NULL) {
		bool_t addExtraData = ((obj->device_type==MS_SND_CARD_DEVICE_TYPE_BLUETOOTH) && (strcmp(obj->desc->driver_type, "openSLES") != 0));
		if (addExtraData == TRUE) {
			obj->id=ms_strdup_printf("%s %s %s: %s",obj->desc->driver_type,ms_snd_card_device_type_to_string(obj->device_type),cap_to_string(obj->capabilities),obj->name);
		} else {
			obj->id=ms_strdup_printf("%s %s: %s",obj->desc->driver_type,ms_snd_card_device_type_to_string(obj->device_type),obj->name);
		}
	}
	return obj->id;
}

void ms_snd_card_set_internal_id(MSSndCard *obj, int id){
	obj->internal_id = id;
}

int ms_snd_card_get_internal_id(MSSndCard *obj){
	return obj->internal_id;
}

void ms_snd_card_set_level(MSSndCard *obj, MSSndCardMixerElem e, int percent){
	if (obj->desc->set_level!=NULL)
		obj->desc->set_level(obj,e,percent);
	else ms_warning("ms_snd_card_set_level: unimplemented by %s wrapper",obj->desc->driver_type);
}

int ms_snd_card_get_level(MSSndCard *obj, MSSndCardMixerElem e){
	if (obj->desc->get_level!=NULL)
		return obj->desc->get_level(obj,e);
	else {
		ms_warning("ms_snd_card_get_level: unimplemented by %s wrapper",obj->desc->driver_type);
		return -1;
	}
}

void ms_snd_card_set_capture(MSSndCard *obj, MSSndCardCapture c){
	if (obj->desc->set_capture!=NULL)
		obj->desc->set_capture(obj,c);
	else ms_warning("ms_snd_card_set_capture: unimplemented by %s wrapper",obj->desc->driver_type);
}

int ms_snd_card_set_control(MSSndCard *obj, MSSndCardControlElem e, int val)
{
	if (obj->desc->set_control!=NULL)
		return obj->desc->set_control(obj,e,val);
	else {
		ms_warning("ms_snd_card_set_control: unimplemented by %s wrapper",obj->desc->driver_type);
		return -1;
	}
}

int ms_snd_card_get_control(MSSndCard *obj, MSSndCardControlElem e)
{
	if (obj->desc->get_control!=NULL)
		return obj->desc->get_control(obj,e);
	else {
		ms_warning("ms_snd_card_get_control: unimplemented by %s wrapper",obj->desc->driver_type);
		return -1;
	}
}

struct _MSFilter * ms_snd_card_create_reader(MSSndCard *obj){
	if (obj->desc->create_reader!=NULL)
		return obj->desc->create_reader(obj);
	else ms_warning("ms_snd_card_create_reader: unimplemented by %s wrapper",obj->desc->driver_type);
	return NULL;
}

struct _MSFilter * ms_snd_card_create_writer(MSSndCard *obj){
	if (obj->desc->create_writer!=NULL)
		return obj->desc->create_writer(obj);
	else ms_warning("ms_snd_card_create_writer: unimplemented by %s wrapper",obj->desc->driver_type);
	return NULL;
}

void ms_snd_card_set_usage_hint(MSSndCard *obj, bool_t is_going_to_be_used){
	if (obj->desc->usage_hint!=NULL)
		obj->desc->usage_hint(obj, is_going_to_be_used);
}

void ms_snd_card_notify_audio_session_activated(MSSndCard *obj, bool_t activated) {
	if (obj->desc->audio_session_activated != NULL)
		obj->desc->audio_session_activated(obj,activated);
}

void ms_snd_card_app_notifies_activation(MSSndCard *obj, bool_t yesno) {
	if (obj->desc->callkit_enabled != NULL)
		obj->desc->callkit_enabled(obj, yesno);
}

void ms_snd_card_destroy(MSSndCard *obj){
	ms_snd_card_unref(obj);
}

int ms_snd_card_get_preferred_sample_rate(const MSSndCard *obj) {
	return obj->preferred_sample_rate;
}

int ms_snd_card_set_preferred_sample_rate(MSSndCard *obj,int rate) {
	obj->preferred_sample_rate=rate;
	return 0;
}

void ms_snd_card_set_stream_type(MSSndCard *obj, MSSndCardStreamType type) {
	obj->streamType = type;
}

MSSndCardStreamType ms_snd_card_get_stream_type(MSSndCard *obj) {
	return obj->streamType;
}

#ifdef __linux
#ifndef __ALSA_ENABLED__
MSSndCard * ms_alsa_card_new_custom(const char *pcmdev, const char *mixdev){
	ms_warning("Alsa support not available in this build of mediastreamer2");
	return NULL;
}

void ms_alsa_card_set_forced_sample_rate(int samplerate){
	ms_warning("Alsa support not available in this build of mediastreamer2");
}
#endif
#endif

#ifdef _MSC_VER
#pragma warning(disable : 4996)
#else
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif

MSSndCardManager* ms_snd_card_manager_get(void) {
	return ms_factory_get_snd_card_manager(ms_factory_get_fallback());
}

MSSndCard* ms_snd_card_ref(MSSndCard *sndCard) {
	sndCard->ref_count++;
	return sndCard;
}

void ms_snd_card_unref(MSSndCard *sndCard) {
	sndCard->ref_count--;
	if (sndCard->ref_count <= 0) {
		if (sndCard->desc->uninit) sndCard->desc->uninit(sndCard);
		if (sndCard->name != NULL) ms_free(sndCard->name);
		if (sndCard->id != NULL) ms_free(sndCard->id);
		ms_free(sndCard);
	}
}

bool_t ms_snd_card_is_card_duplicate(MSSndCardManager *m, MSSndCard * card, bool_t checkCapabilities) {
	bctbx_list_t * cards = ms_snd_card_manager_get_all_cards_with_name(m, card->name);

	// Distinguish card by playback and capture.
	// Ignore other capatibilirties such as built-in echo cancelling
	unsigned int caps_mask = (MS_SND_CARD_CAP_PLAYBACK | MS_SND_CARD_CAP_CAPTURE);
	unsigned int card_caps = ms_snd_card_get_capabilities(card) & caps_mask;

	// In modern devices, some devices are duplicated to improve performance.
	// Only one of them will be added to the card manager. In order to simplify the logic, the device added to the manager will depend on the order devices are in the list.
	// For any given combination of <name>,<driver_type>,<device_type> only the first one inn the list will be added to the card manager
	// If a card with same driver type and device_type has already been added to the sound card manager
	bctbx_list_t * elem;
	bool_t same_type_card_found = FALSE;
	for (elem=cards;elem!=NULL;elem=elem->next){
		MSSndCard *c=(MSSndCard*)elem->data;
		unsigned int elem_caps = ms_snd_card_get_capabilities(c) & caps_mask;
		if ((c->device_type == card->device_type) && (strcmp(c->desc->driver_type, card->desc->driver_type)==0) && ((checkCapabilities == FALSE) || (card_caps == elem_caps))) {
			same_type_card_found = TRUE;
		}
	}

	// Free memory and unref sound cards
	bctbx_list_for_each(cards, (void (*)(void*))ms_snd_card_unref);
	bctbx_list_free(cards);

	return same_type_card_found;
}
