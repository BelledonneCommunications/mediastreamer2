/*
 * auiosnd.c -I/O unit Media plugin for Linphone-
 *
 *
 * Copyright (C) 2009  Belledonne Comunications, Grenoble, France
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Library General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#import <AudioToolbox/AudioToolbox.h>
#import <AVFoundation/AVAudioSession.h>
#import <Foundation/NSArray.h>
#import <Foundation/NSString.h>

#import "mediastreamer2/mssndcard.h"
#import "mediastreamer2/msfilter.h"
#import "mediastreamer2/msticker.h"

static const int flowControlInterval = 5000; // ms
static const int flowControlThreshold = 40; // ms

/*                          -------------------------
							| i                   o |
-- BUS 1 -- from mic -->	| n    REMOTE I/O     u | -- BUS 1 -- to app -->
							| p      AUDIO        t |
-- BUS 0 -- from app -->	| u       UNIT        p | -- BUS 0 -- to speaker -->
							| t                   u |
							|                     t |
							-------------------------
 */

static AudioUnitElement inputBus = 1;
static AudioUnitElement outputBus = 0;

static const char *audio_unit_format_error (OSStatus error) {
	switch (error) {
		case kAudioUnitErr_InvalidProperty: return "kAudioUnitErr_InvalidProperty";
		case kAudioUnitErr_InvalidParameter: return "kAudioUnitErr_InvalidParameter";
		case kAudioUnitErr_InvalidElement: return "kAudioUnitErr_InvalidElement";
		case kAudioUnitErr_NoConnection: return "kAudioUnitErr_NoConnection";
		case kAudioUnitErr_FailedInitialization: return "kAudioUnitErr_FailedInitialization";
		case kAudioUnitErr_TooManyFramesToProcess: return "kAudioUnitErr_TooManyFramesToProcess";
		case kAudioUnitErr_InvalidFile: return "kAudioUnitErr_InvalidFile";
		case kAudioUnitErr_UnknownFileType: return "kAudioUnitErr_UnknownFileType";
		case kAudioUnitErr_FileNotSpecified: return "kAudioUnitErr_FileNotSpecified";
		case kAudioUnitErr_FormatNotSupported: return "kAudioUnitErr_FormatNotSupported";
		case kAudioUnitErr_Uninitialized: return "kAudioUnitErr_Uninitialized";
		case kAudioUnitErr_InvalidScope: return "kAudioUnitErr_InvalidScope";
		case kAudioUnitErr_PropertyNotWritable: return "kAudioUnitErr_PropertyNotWritable";
		case kAudioUnitErr_CannotDoInCurrentContext: return "kAudioUnitErr_CannotDoInCurrentContext";
		case kAudioUnitErr_InvalidPropertyValue: return "kAudioUnitErr_InvalidPropertyValue";
		case kAudioUnitErr_PropertyNotInUse: return "kAudioUnitErr_PropertyNotInUse";
		case kAudioUnitErr_Initialized: return "kAudioUnitErr_Initialized";
		case kAudioUnitErr_InvalidOfflineRender: return "kAudioUnitErr_InvalidOfflineRender";
		case kAudioUnitErr_Unauthorized: return "kAudioUnitErr_Unauthorized";
		default: {
			ms_error ("Cannot start audioUnit because [%c%c%c%c]"
						  ,((char*)&error)[3]
						  ,((char*)&error)[2]
						  ,((char*)&error)[1]
						  ,((char*)&error)[0]);
			return "unknown error";
		}
	}

}

#define check_au_session_result(au,method) \
if (au!=AVAudioSessionErrorInsufficientPriority && au!=0) ms_error("AudioSession error for %s: ret=%i (%s:%d)",method, au, __FILE__, __LINE__ )

#define check_au_unit_result(au,method) \
if (au!=0) ms_error("AudioUnit error for %s: ret=%s (%li) (%s:%d)",method, audio_unit_format_error(au), (long)au, __FILE__, __LINE__ )

#define check_session_call(call)   do { OSStatus res = (call); check_au_session_result(res, #call); } while(0)
#define check_audiounit_call(call) do { OSStatus res = (call); check_au_unit_result(res, #call); } while(0)

static const char* AU_CARD_RECEIVER = "Audio Unit Receiver";
static const char* AU_CARD_NOVOICEPROC = "Audio Unit NoVoiceProc";
static const char* AU_CARD_FAST_IOUNIT = "Audio Unit Fast Receiver"; // Same as AU_CARD_RECEIVER but whiout audio session handling which are delegated to the application
static const char* AU_CARD_SPEAKER = "Audio Unit Speaker";
static const char* AU_CARD_TESTER = "Audio Unit Tester";

static MSFilter *ms_au_read_new(MSSndCard *card);
static MSFilter *ms_au_write_new(MSSndCard *card);

typedef struct au_filter_read_data au_filter_read_data_t;
typedef struct au_filter_write_data au_filter_write_data_t;

typedef struct au_card {
	AudioUnit	io_unit;
	ms_mutex_t	mutex;
	unsigned int	rate;
	unsigned int	bits;
	unsigned int	nchannels;
	uint64_t last_failed_iounit_start_time;
	au_filter_read_data_t* read_data;
	au_filter_write_data_t* write_data;
	MSSndCard* ms_snd_card;
	bool_t is_ringer;
	bool_t is_fast;
	bool_t is_tester;
	bool_t io_unit_started;
	bool_t audio_session_configured;
	bool_t read_started;
	bool_t write_started;
	bool_t is_used;
} au_card_t;

typedef struct au_filter_base {
	au_card_t* card;
	int muted;
} au_filter_base_t;

struct au_filter_read_data{
	au_filter_base_t base;
	ms_mutex_t	mutex;
	queue_t		rq;
	AudioTimeStamp readTimeStamp;
	unsigned int n_lost_frame;
	MSTickerSynchronizer *ticker_synchronizer;
	uint64_t read_samples;
};

struct au_filter_write_data{
	au_filter_base_t base;
	ms_mutex_t mutex;
	MSFlowControlledBufferizer *bufferizer;
	unsigned int n_lost_frame;
};

static void stop_audio_unit (au_card_t* d);

/*
 mediastreamer2 function
 */

static void au_set_level(MSSndCard *card, MSSndCardMixerElem e, int percent) {}

static int au_get_level(MSSndCard *card, MSSndCardMixerElem e) {
	return 0;
}

static void au_set_source(MSSndCard *card, MSSndCardCapture source) {}

static OSStatus au_render_cb (
							  void                        *inRefCon,
							  AudioUnitRenderActionFlags  *ioActionFlags,
							  const AudioTimeStamp        *inTimeStamp,
							  UInt32                      inBusNumber,
							  UInt32                      inNumberFrames,
							  AudioBufferList             *ioData
							  );

static OSStatus au_read_cb (
							  void                        *inRefCon,
							  AudioUnitRenderActionFlags  *ioActionFlags,
							  const AudioTimeStamp        *inTimeStamp,
							  UInt32                      inBusNumber,
							  UInt32                      inNumberFrames,
							  AudioBufferList             *ioData
);

static void create_io_unit (AudioUnit* au, au_card_t *card) {
	AudioComponentDescription au_description;
	AudioComponent foundComponent;
	MSSndCard* sndcard = card->ms_snd_card;

	bool_t noVoiceProc = (strcasecmp(sndcard->name, AU_CARD_NOVOICEPROC) == 0);
	OSType subtype = noVoiceProc ? kAudioUnitSubType_RemoteIO : kAudioUnitSubType_VoiceProcessingIO;

	au_description.componentType          = kAudioUnitType_Output;
	au_description.componentSubType       = subtype;
	au_description.componentManufacturer  = kAudioUnitManufacturer_Apple;
	au_description.componentFlags         = 0;
	au_description.componentFlagsMask     = 0;

	foundComponent = AudioComponentFindNext (NULL,&au_description);

	check_audiounit_call( AudioComponentInstanceNew(foundComponent, au) );

	//Always configure readcb
	AURenderCallbackStruct renderCallbackStruct;
	renderCallbackStruct.inputProc       = au_read_cb;
	renderCallbackStruct.inputProcRefCon = card;
	check_audiounit_call(AudioUnitSetProperty (
								   card->io_unit,
								   kAudioOutputUnitProperty_SetInputCallback,
								   kAudioUnitScope_Input,
								   outputBus,
								   &renderCallbackStruct,
								   sizeof (renderCallbackStruct)
								   ));

	ms_message("AudioUnit created with type %s.", subtype==kAudioUnitSubType_RemoteIO ? "kAudioUnitSubType_RemoteIO" : "kAudioUnitSubType_VoiceProcessingIO" );
}

/* the interruption listener is not reliable, it can be overriden by other parts of the application */
/* as a result, we do nothing with it*/
static void au_interruption_listener (void *inClientData, UInt32 inInterruptionState) {}

static void au_init(MSSndCard *card){
	ms_debug("au_init");
	au_card_t *d=ms_new0(au_card_t,1);

	if (strcmp(card->name,AU_CARD_SPEAKER)==0) {
		d->is_ringer=TRUE;
	} else if (strcmp(card->name,AU_CARD_FAST_IOUNIT)==0) {
		d->is_fast=TRUE;
	} else if( strcmp(card->name,AU_CARD_TESTER)==0){
		d->is_tester=TRUE;
	}
	d->bits=16;
	d->rate=AVAudioSession.sharedInstance.sampleRate; /*not set*/
	d->nchannels=1;
	d->ms_snd_card=card;
	d->is_used = TRUE;
	card->preferred_sample_rate=44100;
	card->capabilities|=MS_SND_CARD_CAP_BUILTIN_ECHO_CANCELLER|MS_SND_CARD_CAP_IS_SLOW;
	ms_mutex_init(&d->mutex,NULL);
	card->data=d;
}

static void au_uninit(MSSndCard *card){
	au_card_t *d=(au_card_t*)card->data;
	stop_audio_unit(d);
	ms_mutex_destroy(&d->mutex);
	ms_free(d);
	card->data = NULL;
}

static void check_unused(au_card_t *card){
	if (card->read_data || card->write_data)
		return;

	if (card->is_tester || !card->is_used)
		stop_audio_unit(card);
}

static void au_usage_hint(MSSndCard *card, bool_t used){
	au_card_t *d = (au_card_t*)card->data;
	if (!d)
		return;

	d->is_used = used;
	check_unused(d);
}

static void au_detect(MSSndCardManager *m);
static MSSndCard *au_duplicate(MSSndCard *obj);

MSSndCardDesc au_card_desc={
.driver_type="AU",
.detect=au_detect,
.init=au_init,
.set_level=au_set_level,
.get_level=au_get_level,
.set_capture=au_set_source,
.set_control=NULL,
.get_control=NULL,
.create_reader=ms_au_read_new,
.create_writer=ms_au_write_new,
.uninit=au_uninit,
.duplicate=au_duplicate,
.usage_hint=au_usage_hint
};

static MSSndCard *au_duplicate(MSSndCard *obj){
	MSSndCard *card=ms_snd_card_new_with_name(&au_card_desc,obj->name);
	return card;
}

static MSSndCard *au_card_new(const char* name){
	MSSndCard *card=ms_snd_card_new_with_name(&au_card_desc,name);
	return card;
}

static void au_detect(MSSndCardManager *m){
	ms_debug("au_detect");
	MSSndCard *card=au_card_new(AU_CARD_RECEIVER);
	ms_snd_card_manager_add_card(m,card);
	card=au_card_new(AU_CARD_FAST_IOUNIT);
	ms_snd_card_manager_add_card(m,card);
	card = au_card_new(AU_CARD_NOVOICEPROC);
	ms_snd_card_manager_add_card(m,card);
	card = au_card_new(AU_CARD_TESTER);
	ms_snd_card_manager_add_card(m,card);
}

/********************write cb only used for write operation******************/
static OSStatus au_read_cb (
							  void                        *inRefCon,
							  AudioUnitRenderActionFlags  *ioActionFlags,
							  const AudioTimeStamp        *inTimeStamp,
							  UInt32                      inBusNumber,
							  UInt32                      inNumberFrames,
							  AudioBufferList             *ioData
)
{
	au_card_t* card = (au_card_t*)inRefCon;
	ms_mutex_lock(&card->mutex);
	if (!card->read_data) {
		//just return from now;
		ms_mutex_unlock(&card->mutex);
		return 0;
	}
	au_filter_read_data_t *d=card->read_data;
	if (d->readTimeStamp.mSampleTime <0) {
		d->readTimeStamp=*inTimeStamp;
	}
	OSStatus err=0;
	mblk_t * rm=NULL;
	AudioBufferList readAudioBufferList;
	readAudioBufferList.mBuffers[0].mDataByteSize=inNumberFrames*d->base.card->bits/8;
	readAudioBufferList.mNumberBuffers=1;
	readAudioBufferList.mBuffers[0].mNumberChannels=d->base.card->nchannels;

	if (d->base.card->read_started) {
		rm=allocb(readAudioBufferList.mBuffers[0].mDataByteSize,0);
		readAudioBufferList.mBuffers[0].mData=rm->b_wptr;
		err = AudioUnitRender(d->base.card->io_unit, ioActionFlags, &d->readTimeStamp, inBusNumber,inNumberFrames, &readAudioBufferList);
		if (err == 0) {
			rm->b_wptr += readAudioBufferList.mBuffers[0].mDataByteSize;
			ms_mutex_lock(&d->mutex);
			putq(&d->rq,rm);
			ms_mutex_unlock(&d->mutex);
			d->readTimeStamp.mSampleTime+=readAudioBufferList.mBuffers[0].mDataByteSize/(d->base.card->bits/2);
		} else {
			check_au_unit_result(err, "AudioUnitRender");
			freeb(rm);
		}
	}
	ms_mutex_unlock(&card->mutex);
	return err;
}

static OSStatus au_write_cb (
							  void                        *inRefCon,
							  AudioUnitRenderActionFlags  *ioActionFlags,
							  const AudioTimeStamp        *inTimeStamp,
							  UInt32                      inBusNumber,
							  UInt32                      inNumberFrames,
							  AudioBufferList             *ioData
							 ) {
	ms_debug("render cb");
	au_card_t* card = (au_card_t*)inRefCon;
	ms_mutex_lock(&card->mutex);
	if( !card->write_data ){
		ms_mutex_unlock(&card->mutex);
		return -1;
	}
	ioData->mBuffers[0].mDataByteSize=inNumberFrames*card->bits/8;
	ioData->mNumberBuffers=1;

	au_filter_write_data_t *d=card->write_data;

	if (d!=NULL){
		unsigned int size;
		ms_mutex_lock(&d->mutex);
		size = inNumberFrames*d->base.card->bits/8;
		if (ms_flow_controlled_bufferizer_get_avail(d->bufferizer) >= size) {
			ms_flow_controlled_bufferizer_read(d->bufferizer, ioData->mBuffers[0].mData, size);
		} else {
			//writing silence;
			memset(ioData->mBuffers[0].mData, 0,ioData->mBuffers[0].mDataByteSize);
			ms_debug("nothing to write, pushing silences,  framezize is %u bytes mDataByteSize %u"
					 ,inNumberFrames*card->bits/8
					 ,(unsigned int)ioData->mBuffers[0].mDataByteSize);
		}
		ms_mutex_unlock(&d->mutex);
	}
	ms_mutex_unlock(&card->mutex);
	return 0;
}

/****************config**************/
static void configure_audio_session (au_card_t* d,uint64_t time) {
	NSError *err = nil;;
	//UInt32 audioCategorySize=sizeof(audioCategory);
	AVAudioSession *audioSession = [AVAudioSession sharedInstance];
	bool_t changed;

	if (!d->is_fast){

		if (d->audio_session_configured){
			/*check that category wasn't changed*/
			NSString *audioCategory = audioSession.category;

			changed=(audioCategory!=AVAudioSessionCategoryAmbient && d->is_ringer)
			||(audioCategory!=AVAudioSessionCategoryPlayAndRecord && !d->is_ringer);
		}

		if (!d->audio_session_configured || changed) {
			if (d->is_ringer && kCFCoreFoundationVersionNumber > kCFCoreFoundationVersionNumber10_6 /*I.E is >=OS4*/) {
				ms_message("Configuring audio session for playback");
    		[audioSession setCategory:AVAudioSessionCategoryAmbient
                        		error:&err];
    		if(err)
					ms_error("Unable to change audio session category because : %s", [err localizedDescription].UTF8String);
				err = nil;
    		[audioSession setMode:AVAudioSessionModeDefault error:&err];
    		if(err)
					ms_error("Unable to change audio session mode because : %s", [err localizedDescription].UTF8String);
				err = nil;
			} else {
				ms_message("Configuring audio session for playback/record");
    		[audioSession setCategory:AVAudioSessionCategoryPlayAndRecord
    												 mode:AVAudioSessionModeVoiceChat
                      		options:AVAudioSessionCategoryOptionAllowBluetooth | AVAudioSessionCategoryOptionAllowBluetoothA2DP
                        		error:&err];
				if(err)
					ms_error("Unable to change audio session because : %s", [err localizedDescription].UTF8String);
				err = nil;
			}

			[audioSession setActive:TRUE error:&err];
			if(err)
				ms_error("Unable to activate audio session because : %s", [err localizedDescription].UTF8String);
			err = nil;
		} else
			ms_message("Audio session already correctly configured.");
		d->audio_session_configured=TRUE;
	} else {
		ms_message("Fast iounit mode, audio session configuration must be done at application level.");
	}

}

static bool_t  start_audio_unit (au_filter_base_t* d,uint64_t time) {
	au_card_t* card=d->card;
	AVAudioSession *audioSession = [AVAudioSession sharedInstance];
	if (card->io_unit == NULL) {
		create_io_unit(&card->io_unit, card);
		if (card->io_unit == NULL) ms_fatal("io_unit is NULL");
	}
	if (!card->io_unit_started && (card->last_failed_iounit_start_time == 0 || (time - card->last_failed_iounit_start_time)>100)) {

		check_audiounit_call(AudioUnitInitialize(card->io_unit));
		ms_message("io unit initialized");

		Float64 delay;
		UInt32 delaySize = sizeof(delay);
		check_audiounit_call(AudioUnitGetProperty(card->io_unit
									  ,kAudioUnitProperty_Latency
									  , kAudioUnitScope_Global
									  , 0
									  , &delay
									  , &delaySize));

		UInt32 quality;
		UInt32 qualitySize = sizeof(quality);
		check_audiounit_call(AudioUnitGetProperty(card->io_unit
									  ,kAudioUnitProperty_RenderQuality
									  , kAudioUnitScope_Global
									  , 0
									  , &quality
									  , &qualitySize));
		ms_message("I/O unit latency [%f], quality [%u]",delay,(unsigned)quality);
		Float32 hwoutputlatency = audioSession.outputLatency;

		Float32 hwinputlatency = audioSession.inputLatency;

		Float32 hwiobuf = audioSession.IOBufferDuration;

		Float64 hwsamplerate = audioSession.sampleRate;

		OSStatus auresult;
		check_audiounit_call( (auresult = AudioOutputUnitStart(card->io_unit)) );
		card->io_unit_started = (auresult ==0);
		if (!card->io_unit_started) {
			ms_message("AudioUnit could not be started, current hw output latency [%f] input [%f] iobuf[%f] hw sample rate [%f]",hwoutputlatency,hwinputlatency,hwiobuf,hwsamplerate);
			d->card->last_failed_iounit_start_time=time;
		} else {
			ms_message("AudioUnit started, current hw output latency [%f] input [%f] iobuf[%f] hw sample rate [%f]",hwoutputlatency,hwinputlatency,hwiobuf,hwsamplerate);
			d->card->last_failed_iounit_start_time=0;
		}
	}
	return card->io_unit_started;

}

static void destroy_audio_unit (au_card_t* d) {
	if (d->io_unit) {
		AudioComponentInstanceDispose (d->io_unit);
		d->io_unit=NULL;
		if (!d->is_fast) {
			NSError *err = nil;;
			[[AVAudioSession sharedInstance] setActive:FALSE withOptions:AVAudioSessionSetActiveOptionNotifyOthersOnDeactivation error:&err];
			if(err) ms_error("Unable to activate audio session because : %s", [err localizedDescription].UTF8String);
			err = nil;
		}
		ms_message("AudioUnit destroyed");
	}
}

static void stop_audio_unit (au_card_t* d) {
	if (d->io_unit && d->io_unit_started) {
		check_audiounit_call( AudioOutputUnitStop(d->io_unit) );
		ms_message("AudioUnit stopped");
		d->io_unit_started=FALSE;
		d->audio_session_configured=FALSE;

	}
	if (d->io_unit) {
		check_audiounit_call( AudioUnitUninitialize(d->io_unit) );
		destroy_audio_unit(d);
	}
}

/***********************************read function********************/

static void au_read_preprocess(MSFilter *f){
	au_filter_read_data_t *d= (au_filter_read_data_t*)f->data;
	au_card_t* card=d->base.card;
	AVAudioSession *audioSession = [AVAudioSession sharedInstance];
	NSError *err = nil;
	configure_audio_session(card, f->ticker->time);

	if (!card->io_unit) create_io_unit(&card->io_unit, card);
	d->ticker_synchronizer = ms_ticker_synchronizer_new();
	ms_ticker_set_synchronizer(f->ticker, d->ticker_synchronizer);

	if (card->io_unit_started) {
		ms_message("Audio Unit already started");
		return;
	}

	/*format are always set in the write preprocess*/
	Float32 preferredBufferSize;
	switch (card->rate) {
		case 11025:
		case 22050:
			preferredBufferSize= .020;
			break;
		default:
			preferredBufferSize= .015;
	}
	[audioSession setPreferredIOBufferDuration:(NSTimeInterval)preferredBufferSize
                               error:&err];
	if(err) ms_error("Unable to change IO buffer duration because : %s", [err localizedDescription].UTF8String);
	err = nil;
}

static void au_read_postprocess(MSFilter *f){
	au_filter_read_data_t *d= (au_filter_read_data_t*)f->data;
	ms_mutex_lock(&d->mutex);
	flushq(&d->rq,0);
	ms_ticker_set_synchronizer(f->ticker, NULL);
	ms_ticker_synchronizer_destroy(d->ticker_synchronizer);
	ms_mutex_unlock(&d->mutex);
}

static void au_read_process(MSFilter *f){
	au_filter_read_data_t *d=(au_filter_read_data_t*)f->data;
	mblk_t *m;
	
	if (!(d->base.card->read_started=d->base.card->io_unit_started)) {
		//make sure audio unit is started
		start_audio_unit((au_filter_base_t*)d,f->ticker->time);
	}
	ms_mutex_lock(&d->mutex);
	while((m = getq(&d->rq)) != NULL){
		d->read_samples += (msgdsize(m) / 2) / d->base.card->nchannels;
		ms_queue_put(f->outputs[0],m);
	}
	ms_mutex_unlock(&d->mutex);

	ms_ticker_synchronizer_update(d->ticker_synchronizer, d->read_samples, d->base.card->rate);
}

/***********************************write function********************/

static void au_write_preprocess(MSFilter *f){
	ms_debug("au_write_preprocess");
	OSStatus auresult;
	NSError *err = nil;
	Float32 bufferSizeInSec = 0.02f;
	au_filter_write_data_t *d= (au_filter_write_data_t*)f->data;
	au_card_t* card=d->base.card;
	AVAudioSession *audioSession = [AVAudioSession sharedInstance];

	if (card->io_unit_started) {
		ms_message("AudioUnit already started");
		return;
	}
	configure_audio_session(card, f->ticker->time);

	if (!card->io_unit)
		create_io_unit(&card->io_unit, card);

	[audioSession setPreferredIOBufferDuration:(NSTimeInterval)bufferSizeInSec error:&err];
	if (err)
		ms_error("Unable to change IO buffer duration because : %s", err.localizedDescription.UTF8String);
	err = nil;

	AudioStreamBasicDescription audioFormat;
	/*card sampling rate is fixed at that time*/
	audioFormat.mSampleRate			= card->rate;
	audioFormat.mFormatID			= kAudioFormatLinearPCM;
	audioFormat.mFormatFlags		= kAudioFormatFlagIsSignedInteger | kAudioFormatFlagIsPacked;
	audioFormat.mFramesPerPacket	= 1;
	audioFormat.mChannelsPerFrame	= card->nchannels;
	audioFormat.mBitsPerChannel		= card->bits;
	audioFormat.mBytesPerPacket		= card->bits / 8;
	audioFormat.mBytesPerFrame		= card->nchannels * card->bits / 8;

	UInt32 doNotSetProperty    = 0;
	UInt32 doSetProperty    = 1;

	//enable speaker output
	auresult =AudioUnitSetProperty (
									card->io_unit,
									kAudioOutputUnitProperty_EnableIO,
									kAudioUnitScope_Output ,
									outputBus,
									&doSetProperty,
									sizeof (doSetProperty)
									);
	check_au_unit_result(auresult,"kAudioOutputUnitProperty_EnableIO,kAudioUnitScope_Output");

	/*enable mic for scheduling render call back, why ?*/
	auresult=AudioUnitSetProperty (/*enable mic input*/
								   card->io_unit,
								   kAudioOutputUnitProperty_EnableIO,
								   kAudioUnitScope_Input ,
								   inputBus,
								   &doSetProperty,
								   sizeof (doSetProperty)
								   );

	check_au_unit_result(auresult,"kAudioOutputUnitProperty_EnableIO,kAudioUnitScope_Input");
	auresult=AudioUnitSetProperty (
								   card->io_unit,
								   kAudioUnitProperty_StreamFormat,
								   kAudioUnitScope_Output,
								   inputBus,
								   &audioFormat,
								   sizeof (audioFormat)
								   );
	check_au_unit_result(auresult,"kAudioUnitProperty_StreamFormat,kAudioUnitScope_Output");
	/*end of: enable mic for scheduling render call back, why ?*/

	//setup stream format
	auresult=AudioUnitSetProperty (
								   card->io_unit,
								   kAudioUnitProperty_StreamFormat,
								   kAudioUnitScope_Input,
								   outputBus,
								   &audioFormat,
								   sizeof (audioFormat)
								   );
	check_au_unit_result(auresult,"kAudioUnitProperty_StreamFormat,kAudioUnitScope_Input");

	//disable unit buffer allocation
	auresult=AudioUnitSetProperty (
								   card->io_unit,
								   kAudioUnitProperty_ShouldAllocateBuffer,
								   kAudioUnitScope_Output,
								   outputBus,
								   &doNotSetProperty,
								   sizeof (doNotSetProperty)
								   );
	check_au_unit_result(auresult,"kAudioUnitProperty_ShouldAllocateBuffer,kAudioUnitScope_Output");
	AURenderCallbackStruct renderCallbackStruct;
	renderCallbackStruct.inputProc       = au_write_cb;
	renderCallbackStruct.inputProcRefCon = card;

	auresult=AudioUnitSetProperty (
								   card->io_unit,
								   kAudioUnitProperty_SetRenderCallback,
								   kAudioUnitScope_Input,
								   outputBus,
								   &renderCallbackStruct,
								   sizeof (renderCallbackStruct)
								   );
	check_au_unit_result(auresult,"kAudioUnitProperty_SetRenderCallback,kAudioUnitScope_Input");
}

static void au_write_postprocess(MSFilter *f){
	ms_debug("au_write_postprocess");
	au_filter_write_data_t *d= (au_filter_write_data_t*)f->data;
	ms_mutex_lock(&d->mutex);
	ms_flow_controlled_bufferizer_flush(d->bufferizer);
	ms_mutex_unlock(&d->mutex);
}

static void au_write_process(MSFilter *f){
	ms_debug("au_write_process");
	au_filter_write_data_t *d=(au_filter_write_data_t*)f->data;

	if (!(d->base.card->write_started=d->base.card->io_unit_started)) {
		//make sure audio unit is started
		start_audio_unit((au_filter_base_t*)d,f->ticker->time);
	}

	if (d->base.muted){
		ms_queue_flush(f->inputs[0]);
		return;
	}
	ms_mutex_lock(&d->mutex);
	ms_flow_controlled_bufferizer_put_from_queue(d->bufferizer, f->inputs[0]);
	ms_mutex_unlock(&d->mutex);
}

static int read_set_rate(MSFilter *f, void *arg){
	int proposed_rate = *((int*)arg);
	ms_debug("set_rate %d",proposed_rate);
	au_filter_read_data_t *d=(au_filter_read_data_t*)f->data;
	d->base.card->rate = AVAudioSession.sharedInstance.sampleRate;
	if ((unsigned int)proposed_rate != d->base.card->rate){
		return -1;//only support 1 rate
	} else {
		return 0;
	}
}

static int write_set_rate(MSFilter *f, void *arg){
	int proposed_rate = *((int*)arg);
	ms_debug("set_rate %d",proposed_rate);
	au_filter_write_data_t *d=(au_filter_write_data_t*)f->data;
	d->base.card->rate = AVAudioSession.sharedInstance.sampleRate;
	ms_flow_controlled_bufferizer_set_samplerate(d->bufferizer, d->base.card->rate);
	if ((unsigned int)proposed_rate != d->base.card->rate){
		return -1;//only support 1 rate
	} else {
		return 0;
	}
}

static int get_rate(MSFilter *f, void *data){
	au_filter_base_t *d=(au_filter_base_t*)f->data;
	*(int*)data=d->card->rate;
	return 0;
}

static int read_set_nchannels(MSFilter *f, void *arg){
	ms_debug("set_nchannels %d", *((int*)arg));
	au_filter_base_t *d=(au_filter_base_t*)f->data;
	d->card->nchannels=*(int*)arg;
	return 0;
}

static int write_set_nchannels(MSFilter *f, void *arg){
	ms_debug("set_nchannels %d", *((int*)arg));
	au_filter_write_data_t *d=(au_filter_write_data_t*)f->data;
	d->base.card->nchannels=*(int*)arg;
	ms_flow_controlled_bufferizer_set_nchannels(d->bufferizer, d->base.card->nchannels);
	return 0;
}

static int get_nchannels(MSFilter *f, void *data) {
	au_filter_base_t *d=(au_filter_base_t *)f->data;
	*(int *)data = d->card->nchannels;
	return 0;
}

static int set_muted(MSFilter *f, void *data){
	au_filter_base_t *d=(au_filter_base_t*)f->data;
	d->muted = *(int*)data;
	return 0;
}

static MSFilterMethod au_read_methods[]={
	{	MS_FILTER_SET_SAMPLE_RATE	, read_set_rate	},
	{	MS_FILTER_GET_SAMPLE_RATE	, get_rate	},
	{	MS_FILTER_SET_NCHANNELS		, read_set_nchannels	},
	{	MS_FILTER_GET_NCHANNELS		, get_nchannels	},
	{	0				, NULL		}
};

static MSFilterMethod au_write_methods[]={
	{	MS_FILTER_SET_SAMPLE_RATE	, write_set_rate	},
	{	MS_FILTER_GET_SAMPLE_RATE	, get_rate	},
	{	MS_FILTER_SET_NCHANNELS		, write_set_nchannels	},
	{	MS_FILTER_GET_NCHANNELS		, get_nchannels	},
	{	MS_AUDIO_PLAYBACK_MUTE	 	, set_muted	},
	{	0				, NULL		}
};

static void au_read_uninit(MSFilter *f) {
	au_filter_read_data_t *d=(au_filter_read_data_t*)f->data;
	au_card_t* card=d->base.card;

	ms_mutex_lock(&card->mutex);
	card->read_data=NULL;
	ms_mutex_unlock(&card->mutex);

	check_unused(card);

	ms_mutex_destroy(&d->mutex);

	flushq(&d->rq,0);
	ms_free(d);
}

static void au_write_uninit(MSFilter *f) {
	au_filter_write_data_t *d=(au_filter_write_data_t*)f->data;
	au_card_t* card=d->base.card;

	ms_mutex_lock(&card->mutex);
	card->write_data=NULL;
	ms_mutex_unlock(&card->mutex);

	check_unused(card);

	ms_mutex_destroy(&d->mutex);
	ms_flow_controlled_bufferizer_destroy(d->bufferizer);
	ms_free(d);
}

MSFilterDesc au_read_desc={
.id=MS_IOUNIT_READ_ID,
.name="MSAURead",
.text=N_("Sound capture filter for iOS Audio Unit Service"),
.category=MS_FILTER_OTHER,
.ninputs=0,
.noutputs=1,
.preprocess=au_read_preprocess,
.process=au_read_process,
.postprocess=au_read_postprocess,
.uninit=au_read_uninit,
.methods=au_read_methods
};


MSFilterDesc au_write_desc={
.id=MS_IOUNIT_WRITE_ID,
.name="MSAUWrite",
.text=N_("Sound playback filter for iOS Audio Unit Service"),
.category=MS_FILTER_OTHER,
.ninputs=1,
.noutputs=0,
.preprocess=au_write_preprocess,
.process=au_write_process,
.postprocess=au_write_postprocess,
.uninit=au_write_uninit,
.methods=au_write_methods
};

static MSFilter *ms_au_read_new(MSSndCard *mscard){
	ms_debug("ms_au_read_new");
	au_card_t* card=(au_card_t*)(mscard->data);
	MSFilter *f=ms_factory_create_filter_from_desc(ms_snd_card_get_factory(mscard), &au_read_desc);
	au_filter_read_data_t *d=ms_new0(au_filter_read_data_t,1);
	qinit(&d->rq);
	d->readTimeStamp.mSampleTime=-1;
	ms_mutex_init(&d->mutex,NULL);
	d->base.card=card;
	card->read_data=d;
	f->data=d;
	return f;
}

static MSFilter *ms_au_write_new(MSSndCard *mscard){
	ms_debug("ms_au_write_new");
	au_card_t* card=(au_card_t*)(mscard->data);
	MSFilter *f=ms_factory_create_filter_from_desc(ms_snd_card_get_factory(mscard), &au_write_desc);
	au_filter_write_data_t *d=ms_new0(au_filter_write_data_t,1);
	d->bufferizer= ms_flow_controlled_bufferizer_new(f, card->rate, card->nchannels);
	ms_flow_controlled_bufferizer_set_max_size_ms(d->bufferizer, flowControlThreshold);
	ms_flow_controlled_bufferizer_set_flow_control_interval_ms(d->bufferizer, flowControlInterval);
	ms_mutex_init(&d->mutex,NULL);
	d->base.card=card;
	card->write_data=d;
	f->data=d;
	return f;
}

MS_FILTER_DESC_EXPORT(au_read_desc)
MS_FILTER_DESC_EXPORT(au_write_desc)
