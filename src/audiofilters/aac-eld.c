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

#include <mediastreamer2/msfilter.h>
#include <mediastreamer2/mscodecutils.h>
#include <mediastreamer2/msticker.h>
#include <AudioToolbox/AudioToolbox.h>

/* frame are encoded/decoded for a constant sample number of 512(but an RTP packet may contain several frames */
#define SIGNAL_FRAME_SIZE (512*sizeof(SInt16))
/* RFC3640 3.3.6: aac-hbr mode have a 4 bytes header(we use only one frame per packet) */
#define AU_HDR_SZ 4

struct EncState {
	uint32_t timeStamp; /* timeStamp is actually expressed in number of sample processed, needed for encoder only, inserted in the encoded message block */
	int ptime; /* wait until we have at least this amount of data in input before processing it */
	int maxptime; /* at first call to set ptime, set this value to max of 50, given parameter in order to avoid automatic ptime changing reaching high values unless we enforce it at init */
	uint32_t samplingRate; /* sampling rate of signal to be encoded */
	uint32_t bitRate; /* bit rate of encode signal */
	size_t nbytes; /* amount of data in a processedTime window usually sampling rate*time*number of byte per sample*number of channels, this one is set to a integer number of 512 samples frame */
	int nchannels; /* number of channels, default 1(mono) */
	MSBufferizer *bufferizer; /* buffer to store data from input queue before processing them */
	uint8_t *inputBuffer; /* buffer to store nbytes of input data and send them to the encoder */
	/** AAC ELD related properties */
	AudioStreamBasicDescription  sourceFormat; /* description of input audio format */
	AudioStreamBasicDescription  destinationFormat; /* description of output audio format */
	AudioConverterRef            audioConverter;
	UInt32 maxOutputPacketSize; /* maximum size of the output packet */
	UInt32 bytesToEncode;
    UInt32 encoderType; /* AAC_ELD or AAC_ELD_SBR */
};

/* Encoder */

typedef struct {
    UInt32 encoderType;
    UInt32 sampling;
    UInt32 min_bitrate;
    UInt32 max_bitrate;
 } aac_rates_t;

/* The iOS AAC encoder supports different sampling rates and bitrates.
   These also depend on the type of encoder (with/without SBR).
 */
static aac_rates_t aac_rates[] = {
    { kAudioFormatMPEG4AAC_ELD, 16000, 12000, 48000 },
    { kAudioFormatMPEG4AAC_ELD, 22050, 16000, 72000 },
    { kAudioFormatMPEG4AAC_ELD, 32000, 24000, 96000 },
    { kAudioFormatMPEG4AAC_ELD, 44100, 32000, 128000 },
    { kAudioFormatMPEG4AAC_ELD, 48000, 32000, 128000 },

    { kAudioFormatMPEG4AAC_ELD_SBR, 16000, 16000, 32000 },
    { kAudioFormatMPEG4AAC_ELD_SBR, 22050, 16000, 48000 },
    { kAudioFormatMPEG4AAC_ELD_SBR, 32000, 28000, 64000 },
    { kAudioFormatMPEG4AAC_ELD_SBR, 44100, 28000, 64000 },
    { kAudioFormatMPEG4AAC_ELD_SBR, 48000, 28000, 64000 },
};
static int aac_rates_size = sizeof(aac_rates)/sizeof(*aac_rates);

/* initial values, known to be working for ELD and ELD-SBR */
#define AAC_DEFAULT_SR 22050
#define AAC_DEFAULT_BR 24000


/* UTILS */

/*  defines needed to compute network bit rate, copied from misc.c */
#define UDP_HDR_SZ 8
#define RTP_HDR_SZ 12
#define IP4_HDR_SZ 20
#define OVERHEAD_BYTES (IP4_HDR_SZ + RTP_HDR_SZ + UDP_HDR_SZ)


static int  codec_to_network_bitrate(struct EncState* s, int codecBitrate ){
    float packetsPerSecond = 1000.0/s->ptime;
    float bytesPerPacket = codecBitrate / (packetsPerSecond * 8);
    int networkBitrate = ( bytesPerPacket + OVERHEAD_BYTES ) * packetsPerSecond * 8;
    return networkBitrate;
}

static int  network_to_codec_bitrate(struct EncState* s, int networkBitrate ){
    float packetsPerSec = 1000/s->ptime;
    float bytesPerPacket = ((float)networkBitrate / (packetsPerSec * 8 /* bits per byte */));
    int codecBitrate=(int) ( (bytesPerPacket - OVERHEAD_BYTES) * packetsPerSec * 8 );

    return codecBitrate;
}



/* ENCODER */

static void enc_update_bitrate( struct EncState* s, uint32_t new_bitrate);
/* called at init and any modification of ptime: compute the input data length */
static void enc_update ( struct EncState *s ) {
	s->nbytes= ( sizeof ( SInt16 ) *s->nchannels*s->samplingRate*s->ptime ) /1000; /* input is 16 bits LPCM: 2 bytes per sample, ptime is in ms so /1000 */
	/* the nbytes must be a multiple of SIGNAL_FRAME_SIZE(512 sample frame) */
	/* nbytes % SIGNAL_FRAME_SIZE must be 0, min SIGNAL_FRAME_SIZE */
	/* this gives for sampling rate at 22050Hz available values of ptime: 23.2ms and multiples up to 92.8ms */
	/* at 44100Hz: 11.6ms and multiples up to 92.8ms */
	/* given an input ptime, select the one immediatly inferior in the list of available possibilities */
	if ( s->nbytes > SIGNAL_FRAME_SIZE*s->nchannels ) {
		s->nbytes -= ( s->nbytes % SIGNAL_FRAME_SIZE*s->nchannels );
	} else {
		s->nbytes = SIGNAL_FRAME_SIZE*s->nchannels;
	}
}

/* init the encoder: create an encoder State structure, initialise it and attach it to the MSFilter structure data property */
static void enc_init ( MSFilter *f ) {
	/* instanciate the encoder status */
	struct EncState *s= ( struct EncState* ) ms_new ( struct EncState,1 );
    s->timeStamp    = 0;
    s->bufferizer   = ms_bufferizer_new();
    s->ptime        = 10;
    s->maxptime     = -1;
    s->samplingRate = AAC_DEFAULT_SR;
    s->bitRate      = AAC_DEFAULT_BR;
    s->nchannels    = 1;
    s->inputBuffer  = NULL;
    s->encoderType  = kAudioFormatMPEG4AAC_ELD_SBR;
	memset ( & ( s->sourceFormat ), 0, sizeof ( AudioStreamBasicDescription ) );
	memset ( & ( s->destinationFormat ), 0, sizeof ( AudioStreamBasicDescription ) );

	s->inputBuffer = ( uint8_t * ) malloc ( SIGNAL_FRAME_SIZE*sizeof ( uint8_t )*2/*in case of stereo*/ ); /* allocate input buffer */

	/* attach it to the MSFilter structure */
	f->data=s;
}

/* pre process: at this point the sampling rate, ptime, number of channels have been correctly setted, so we can initialize the encoder */
static void enc_preprocess ( MSFilter *f ) {
	struct EncState *s= ( struct EncState* ) f->data;

	/* update the nbytes value */
	enc_update ( s );

	/*** initialize the encoder ***/
	/* initialise the input audio stream basic description : LPCM */
    s->sourceFormat.mSampleRate            = s->samplingRate;
    s->sourceFormat.mFormatID              = kAudioFormatLinearPCM;
    s->sourceFormat.mFormatFlags           = kAudioFormatFlagIsSignedInteger | kAudioFormatFlagIsPacked;
    s->sourceFormat.mBytesPerPacket        = s->nchannels * sizeof ( SInt16 );
    s->sourceFormat.mFramesPerPacket       = 1;
    s->sourceFormat.mBytesPerFrame         = s->nchannels * sizeof ( SInt16 );
    s->sourceFormat.mChannelsPerFrame      = s->nchannels;
    s->sourceFormat.mBitsPerChannel        = 8*sizeof ( SInt16 );

    /* Encoder setup */
    s->destinationFormat.mFormatID         = s->encoderType;
    s->destinationFormat.mChannelsPerFrame = s->nchannels;
    s->destinationFormat.mSampleRate       = s->samplingRate;

	/* have coreAudio fill the rest of the audio stream description */
	UInt32 dataSize = sizeof ( s->destinationFormat );
	OSStatus status = AudioFormatGetProperty ( kAudioFormatProperty_FormatInfo, 0, NULL, &dataSize , & ( s->destinationFormat ) ) ;

	/* Create a new audio converter */
	status = AudioConverterNew ( & ( s->sourceFormat ), & ( s->destinationFormat ), & ( s->audioConverter ) ) ;
	ms_debug("AudioConverter: %p, status: %x", s->audioConverter, (unsigned int)status );

    // set bitrate
    enc_update_bitrate(s, s->bitRate);

	/* Get the maximum output size of output buffer */
	UInt32 maxOutputSizePerPacket = 0;
	dataSize = sizeof ( maxOutputSizePerPacket );
	status = AudioConverterGetProperty ( s->audioConverter,
								kAudioConverterPropertyMaximumOutputPacketSize,
								&dataSize,
								&maxOutputSizePerPacket );
	s->maxOutputPacketSize = maxOutputSizePerPacket;
	ms_debug("get kAudioConverterPropertyMaximumOutputPacketSize to %d -> %x", (unsigned int)maxOutputSizePerPacket, (unsigned int)status);

	ms_debug("AAC encoder set to: SR: %d mBitsPerChannel: %d mChannelsPerFrame: %d maxOutputPacketSize: %d bitrate: %d",
		s->samplingRate,
		(unsigned int)s->sourceFormat.mBitsPerChannel,
		s->nchannels,
		(unsigned int)s->maxOutputPacketSize,
		s->bitRate);
}


static OSStatus encoderCallback ( AudioConverterRef inAudioConverter, UInt32 *ioNumberDataPackets, AudioBufferList *ioData,
								  AudioStreamPacketDescription **outDataPacketDescription,void *inUserData ) {
	/* Get the current encoder state from the inUserData parameter */
	struct EncState *s= ( struct EncState* ) inUserData;


	/* Compute the maximum number of output packets -- useless as we shall always have nbytes at 512 bytes */
	UInt32 maxPackets = s->nbytes / s->sourceFormat.mBytesPerPacket;
	if ( *ioNumberDataPackets > maxPackets ) {
		/* If requested number of packets is bigger, adjust */
		*ioNumberDataPackets = maxPackets;
	}

	/* Check to make sure we have only one audio buffer */
	if ( ioData->mNumberBuffers != 1 ) {
		return 1;
	}

	/* Set the data to be encoded */
	ioData->mBuffers[0].mDataByteSize   = s->nbytes;
	ioData->mBuffers[0].mData           = s->inputBuffer;
	ioData->mBuffers[0].mNumberChannels = s->nchannels;

	if ( outDataPacketDescription ) {
		*outDataPacketDescription = NULL;
	}

	return noErr;
}


/* process: get data from MSFilter queue and put it into the EncState buffer, then process it until there is less than nbytes in the input buffer */
static void enc_process ( MSFilter *f ) {
	struct EncState *s= ( struct EncState* ) f->data;

	/* lock the MSFilter properties in order to delay any change of properties(mostly ptimes which impact nbytes) */
	ms_filter_lock ( f );

	/* get data from MSFilter queue and put it in the EncState buffer */
	ms_bufferizer_put_from_queue ( s->bufferizer,f->inputs[0] );

	UInt16 frameNumber = s->nbytes/(SIGNAL_FRAME_SIZE*s->nchannels); /* number of frame in the packet */
	ms_debug("enc_process -- frameNumber: %d nbytes: %d", frameNumber, s->nbytes);

	/* until we find at least the requested amount of input data in the input buffer, process it */
	while ( ms_bufferizer_get_avail ( s->bufferizer ) >=s->nbytes ) {
		mblk_t *outputMessage=allocb ( (s->maxOutputPacketSize+2)*frameNumber+2,0 ); /* create an output message of requested size(max ouput size * number of frame in the packet + 2 bytes per frame for au header and 2 bytes for au header length) */
		UInt16 messageLength = 2 + 2*frameNumber; /* store in bytes the complete message length to increment the write pointer of output message at the end of the encoding, initialise with au header length */

		/* insert the header accoring to RC3640 3.3.6: 2 bytes of AU-Header section and one AU-HEader of 2 bytes */
		/* au header length is frame number*16(2 bytes per frame) */
		outputMessage->b_wptr[0] = (UInt8)(frameNumber>>4);
		outputMessage->b_wptr[1] = (UInt8)(frameNumber<<4);

		/*** encoding ***/
		/* create an audio stream packet description to feed the encoder in order to get the description of encoded data */
		AudioStreamPacketDescription outPacketDesc[1];

		/* encode a 512 sample frame (nbytes is 512*sizeof(SInt16) or a greater multiple */
		UInt32 bufferIndex =0;
		unsigned char *outputBuffer = outputMessage->b_wptr+2+2*frameNumber; /* set the pointer to the output buffer just after the header */

		for ( bufferIndex=0; bufferIndex<s->nbytes; bufferIndex+=SIGNAL_FRAME_SIZE*s->nchannels ) {

            /* Create the output buffer list */
			AudioBufferList outBufferList;
			outBufferList.mNumberBuffers = 1;
			outBufferList.mBuffers[0].mNumberChannels = s->nchannels;
			outBufferList.mBuffers[0].mDataByteSize   = s->maxOutputPacketSize;
			outBufferList.mBuffers[0].mData           = outputBuffer;

			/* get the input data from bufferizer to the input buffer */
			ms_bufferizer_read ( s->bufferizer, s->inputBuffer, SIGNAL_FRAME_SIZE*s->nchannels );

			/* start the encoding process, iOS will call back when ready the callback function provided as second arg to get the input data */
			UInt32 numOutputDataPackets = 1;
			OSStatus status = AudioConverterFillComplexBuffer (
				s->audioConverter, // the instance of the converter
				encoderCallback, // callback to call when conversion is done
				s, // user data to pass to the callback
				&numOutputDataPackets, // sizeof the outBufferList in number of packets
				&outBufferList, // buffer where the AAC packet is to be written
				outPacketDesc ); // buffer where the description of the output packet will be placed



			if ( status != noErr ) {
				ms_message ( "AAC-ELD unable to encode, exit status : %ld", (long)status );
            	ms_filter_unlock(f);
				return;
			}

			/* get the length of output data and increment the outputMessage write pointer, ouput data is now in the output message */
			UInt16 encodedFrameLength = ( UInt16 ) ( outPacketDesc[0].mDataByteSize );

			/* header for this frame : 2 bytes: length of encoded frame (3 LSB set to zero, length shifted 3 places left according to RFC3640 3.3.6 */
			outputMessage->b_wptr[2+2*bufferIndex/SIGNAL_FRAME_SIZE] = (UInt8)(((encodedFrameLength<<3)&0xFF00)>>8);
			outputMessage->b_wptr[2+2*bufferIndex/SIGNAL_FRAME_SIZE+1] = (UInt8)((encodedFrameLength<<3)&0x00FF);

			messageLength += encodedFrameLength;
			outputBuffer += encodedFrameLength;
		}
		outputMessage->b_wptr += messageLength;

		/* set timeStamp in the output Message */
		mblk_set_timestamp_info ( outputMessage,s->timeStamp );
		s->timeStamp += s->nbytes/ ( sizeof ( SInt16 ) *s->nchannels ); /* increment timeStamp by the number of sample processed (divided by the number of channels) */

		/* insert the output message into the output queue of MSFilter */
		ms_queue_put ( f->outputs[0],outputMessage );
	}

	/* release the lock */
	ms_filter_unlock ( f );
}

static void enc_postprocess ( MSFilter *f ) {
}

static void enc_uninit ( MSFilter *f ) {
	struct EncState *s= ( struct EncState* ) f->data;
	ms_bufferizer_destroy ( s->bufferizer );
	free ( s->inputBuffer );
	AudioConverterDispose(s->audioConverter);
	ms_free ( s );
	f->data = 0;
}


static int set_ptime ( struct EncState *s, int value ) {
	/* at first call to this function, set the maxptime to MAX (50, value) but not higher than 100 */
	if (s->maxptime<0) {
		s->maxptime = MIN(100,MAX(value, 50));
	}

	if ( value>0 && value<=s->maxptime ) {
		s->ptime=value;
		ms_message ( "AAC-ELD encoder using ptime=%i",value );
		enc_update ( s );
		return 0;
	} else {
		return -1; /* to tell the automatic ptime adjustment to stop trying changing ptime */
	}
}

static int enc_add_attr ( MSFilter *f, void *arg ) {
	const char *fmtp= ( const char* ) arg;
	struct EncState *s= ( struct EncState* ) f->data;
	int retval=0;
	if ( strstr ( fmtp,"ptime:" ) ) {
		ms_filter_lock ( f );
		retval = set_ptime ( s,atoi ( fmtp+6 ) );
		ms_filter_unlock ( f );
	}
	return retval;
};


static int enc_add_fmtp ( MSFilter *f, void *arg ) {
	const char *fmtp= ( const char* ) arg;
	struct EncState *s= ( struct EncState* ) f->data;
	char tmp[16]= {0};
	int retval=0;

	if ( fmtp_get_value ( fmtp,"ptime",tmp,sizeof ( tmp ) ) ) {
		ms_filter_lock ( f );
		retval = set_ptime ( s,atoi ( tmp ) );
		ms_filter_unlock ( f );
	}
	if( fmtp_get_value(fmtp, "SBR-enabled", tmp, sizeof(tmp)) ){
		int enabled = atoi(tmp);
		if( enabled != 0){
            ms_warning("AAC encoder will send SBR");
			s->encoderType = kAudioFormatMPEG4AAC_ELD_SBR;
		}
	}
	return retval;
}

static int enc_set_sr ( MSFilter *f, void *arg ) {
	struct EncState *s= ( struct EncState* ) f->data;
    int wantedSampleRate = ( ( int* ) arg ) [0];
	bool_t found = FALSE;
	int i;

	for( i=0; i<aac_rates_size;i++){
		if( s->samplingRate == aac_rates[i].sampling ){
            s->samplingRate = wantedSampleRate;
			found = TRUE;
            break;
		}
	}

	if(!found) {
		ms_debug("AAC-ELD codec, try to set unsupported sampling rate %d Hz, fallback to 22050Hz", wantedSampleRate);
        s->samplingRate = AAC_DEFAULT_SR;
	}
	ms_message("AAC-ELD encoder sample rate set to %d (asked %d)", s->samplingRate, *(int*)arg);

	return 0;
}

static int enc_get_sr ( MSFilter *f, void *arg ) {
	struct EncState *s= ( struct EncState* ) f->data;
	( ( int* ) arg ) [0]=s->samplingRate;
	ms_debug("AAC encoder sample rate is %d", s->samplingRate);
	return 0;
}

static int enc_get_br ( MSFilter *f, void *arg ) {
    struct EncState *s= ( struct EncState* ) f->data;
     ( ( int* ) arg ) [0]= codec_to_network_bitrate(s, s->bitRate);
    return 0;
}

static int enc_set_br ( MSFilter *f, void *arg ) {
	struct EncState *s= ( struct EncState* ) f->data;

	int networkBitrate = ((int*)arg)[0];
    int codecBitrate = network_to_codec_bitrate(s, networkBitrate);
    ms_message("AAC wanted network bitrate: %d -> codec bitrate: %d", networkBitrate, codecBitrate);

    if( f->ticker != NULL ) {
        // we are running: set the bitrate to the encoder
        enc_update_bitrate(s, codecBitrate);
    } else {
        s->bitRate = codecBitrate;
    }


    return 0;
}

static void enc_update_bitrate( struct EncState* s, uint32_t new_bitrate){
    int i;
    bool_t found = FALSE;

    if( s->audioConverter == NULL ){
        ms_error("No AAC audio encoder: cannot update bitrate");
        return;
    }

    // get the current encoder output bitrate
    UInt32 currentBitRate = 0;
    UInt32 dataSize = sizeof ( currentBitRate );
    OSStatus status = AudioConverterGetProperty ( s->audioConverter,
                                                 kAudioConverterEncodeBitRate,
                                                 &dataSize,
                                                 &currentBitRate );
    ms_debug("AAC current encoder bitrate: %d", (unsigned int)currentBitRate);


    for( i=0; i<aac_rates_size;i++){
        if( s->samplingRate == aac_rates[i].sampling && s->encoderType == aac_rates[i].encoderType ){
            aac_rates_t* rate = &aac_rates[i];
            // clamp bitrate to supported bitrate
            s->bitRate = MIN( MAX(new_bitrate, rate->min_bitrate), rate->max_bitrate);
            found = TRUE;
            break;
        }
    }

    if( !found ){
        ms_warning("AAC could not set bitrate to %d, keeping %d", new_bitrate, s->bitRate);
    } else {
        /* Set the output bitrate */
        UInt32 outputBitrate = s->bitRate;
        dataSize = sizeof ( outputBitrate );
        status = AudioConverterSetProperty ( s->audioConverter, kAudioConverterEncodeBitRate, dataSize, &outputBitrate );
        ms_debug("set kAudioConverterEncodeBitRate to %d -> %x", (unsigned int)outputBitrate, (unsigned int)status);

        if( status != noErr ){
            ms_warning("Could not set bitrate for AAC encoder (%x), reverting to default encoder bitRate: %d", ( unsigned int)status, (int)currentBitRate);
            dataSize = sizeof ( outputBitrate );
            status = AudioConverterSetProperty ( s->audioConverter, kAudioConverterEncodeBitRate, dataSize, &currentBitRate );
            ms_debug("set kAudioConverterEncodeBitRate to %d -> %x", (unsigned int)currentBitRate, (unsigned int)status);
        } else {
            ms_message("AAC target bitrate changed from %d to %d", (unsigned int)currentBitRate, s->bitRate);
        }
    }

}

static int enc_set_nchannels(MSFilter *f, void *arg) {
	struct EncState *s = (struct EncState *)f->data;
	s->nchannels = *(int *)arg;
	return 0;
}

static int enc_get_nchannels(MSFilter *f, void *arg) {
	struct EncState *s = (struct EncState *)f->data;
	*(int *)arg = s->nchannels;
	return 0;
}

static int enc_set_ptime(MSFilter *f, void *arg) {
	struct EncState *s = (struct EncState *)f->data;
	int retval=0;

	ms_filter_lock ( f );
	retval = set_ptime ( s, *(int *)arg );
	ms_filter_unlock ( f );

	return retval;
}

/* attach encoder methods to MSFilter IDs */
static MSFilterMethod enc_methods[]= {
	{	MS_FILTER_ADD_ATTR		,	enc_add_attr},
	{	MS_FILTER_ADD_FMTP		,	enc_add_fmtp},
	{	MS_FILTER_SET_SAMPLE_RATE	,	enc_set_sr	},
	{	MS_FILTER_GET_SAMPLE_RATE	,	enc_get_sr	},
	{	MS_FILTER_SET_BITRATE		,	enc_set_br	},
	{	MS_FILTER_GET_BITRATE		,	enc_get_br	},
	{	MS_FILTER_SET_NCHANNELS		,	enc_set_nchannels},
	{	MS_FILTER_GET_NCHANNELS		,	enc_get_nchannels},
	{	MS_AUDIO_ENCODER_SET_PTIME	,	enc_set_ptime	},
	{0, NULL}
};


MSFilterDesc ms_aac_eld_enc_desc= {
	MS_AAC_ELD_ENC_ID, /**< this ID must be declared in allfilter.h */
	"MSAACELDEnc", /**< filter name */
	"AAC-ELD encoder", /**< filter Description */
	MS_FILTER_ENCODER, /**< filter category */
	"mpeg4-generic", /**< MIME type for AAC-ELD */
	1, /**< number of inputs */
	1, /**< number of outputs */
	enc_init, /**< Filter's init function*/
	enc_preprocess, /**< Filter's preprocess function, called one time before starting to process*/
	enc_process, /**< Filter's process function, called every tick by the MSTicker to do the filter's job*/
	enc_postprocess, /**< Filter's postprocess function, called once after processing (the filter is no longer called in process() after)*/
	enc_uninit, /**< Filter's uninit function, used to deallocate internal structures*/
	enc_methods /**<Filter's method table*/
};


/* Decoder */

struct DecState {
	uint32_t samplingRate; /* sampling rate of decoded signal */
	uint8_t audioConverterProperty[512]; /* config string retrieved from SDP message  useless?*/
	int audioConverterProperty_size; /* previous property size */
	int nchannels; /* number of channels, default 1(mono) */
	//int nbytes; /* size of ouput buffer for the decoder */
	MSConcealerTsContext *concealer; /* concealment management */
	/** AAC ELD related properties */
	AudioStreamBasicDescription  sourceFormat;
	AudioStreamBasicDescription  destinationFormat;
	AudioConverterRef            audioConverter;
	UInt32                       bytesToDecode;
	void                        *decodeBuffer;
	AudioStreamPacketDescription packetDesc[1];
	UInt32                       maxOutputPacketSize;
};

static void dec_init ( MSFilter *f ) {
	/* instanciate the decoder State structure */
	struct DecState *s= ( struct DecState * ) ms_new ( struct DecState,1 );
	s->samplingRate=44100;
	s->nchannels=1; /* default mono */
	/* attach it to the MSFilter */
	f->data=s;

	/* initialise source and destination format for AAC-ELD decoder */
	memset ( & ( s->sourceFormat ), 0, sizeof ( AudioStreamBasicDescription ) );
	memset ( & ( s->destinationFormat ), 0, sizeof ( AudioStreamBasicDescription ) );
}

static void dec_preprocess ( MSFilter *f ) {
	struct DecState *s= ( struct DecState* ) f->data;
	/*** initialise the AAC-ELD decoder ***/
	/* initialise the ouput audio stream basic description : LPCM */
	s->destinationFormat.mSampleRate = s->samplingRate;
	s->destinationFormat.mFormatID = kAudioFormatLinearPCM;
	s->destinationFormat.mFormatFlags = kAudioFormatFlagIsSignedInteger | kAudioFormatFlagIsPacked;
	s->destinationFormat.mBytesPerPacket = s->nchannels * sizeof ( SInt16 );
	s->destinationFormat.mFramesPerPacket = 1;
	s->destinationFormat.mBytesPerFrame = s->nchannels * sizeof ( SInt16 );
	s->destinationFormat.mChannelsPerFrame = s->nchannels;
	s->destinationFormat.mBitsPerChannel = 8*sizeof ( SInt16 );

	/* from AAC-ELD, having the same sampling rate, but possibly a different channel configuration */
	s->sourceFormat.mFormatID         = /*kAudioFormatMPEG4AAC*/ kAudioFormatMPEG4AAC_ELD; // ELD can handle SBR as well
	s->sourceFormat.mChannelsPerFrame = s->nchannels;
	s->sourceFormat.mSampleRate       = s->samplingRate;

	/* Get the rest of the format info */
	UInt32 dataSize = sizeof ( s->sourceFormat );
	AudioFormatGetProperty ( kAudioFormatProperty_FormatInfo,
							 0,
							 NULL,
							 &dataSize,
							 & ( s->sourceFormat ) );

	/* Create a new AudioConverter instance for the conversion AAC-ELD -> LPCM */
	AudioConverterNew ( & ( s->sourceFormat ),
						& ( s->destinationFormat ),
						& ( s->audioConverter ) );

	if ( !s->audioConverter ) {
		ms_message ( "fail to initialise decoder" );
	}
	/* Check for variable output packet size -- useless?*/
	UInt32 maxOutputSizePerPacket = 0;
	dataSize = sizeof ( maxOutputSizePerPacket );
	AudioConverterGetProperty ( s->audioConverter,
								kAudioConverterPropertyMaximumOutputPacketSize,
								&dataSize,
								&maxOutputSizePerPacket );
	s->maxOutputPacketSize = maxOutputSizePerPacket;
/* don't know yet how to convert config string in sens of rfc3640 to kAudioConverterDecompressionMagicCookie. hints can be found from https://www.iis.fraunhofer.de/en/ff/amm/prod/kommunikation/komm/aaceld.html application bulletin.

	OSStatus status = AudioConverterSetProperty (  s->audioConverter
												 , kAudioConverterDecompressionMagicCookie
												 , s->audioConverterProperty_size
												 , s->audioConverterProperty);
	if (status != noErr )	 {
		ms_error ("Cannot set acc decoder property kAudioConverterDecompressionMagicCookie because [%c%c%c%c]"
				  ,((char*)&status)[3]
				  ,((char*)&status)[2]
				  ,((char*)&status)[1]
				  ,((char*)&status)[0]);
		
	}
*/
	/* initialise concealment context */
	s->concealer = ms_concealer_ts_context_new(UINT32_MAX);
}


/* decoder Callback function: feed the decoder with input data */
static OSStatus decoderCallback ( AudioConverterRef inAudioConverter,
								  UInt32 *ioNumberDataPackets,
								  AudioBufferList *ioData,
								  AudioStreamPacketDescription **outDataPacketDescription,
								  void *inUserData ) {
	/* Get the current decoder state from the inUserData parameter */
	struct DecState *s= ( struct DecState* ) inUserData;

	ioData->mBuffers[0].mData           = s->decodeBuffer;
	ioData->mBuffers[0].mDataByteSize   = s->bytesToDecode;
	ioData->mBuffers[0].mNumberChannels = s->nchannels;


	/* And set the packet description */
	if ( outDataPacketDescription ) {
		s->packetDesc[0].mStartOffset            = 0;
		s->packetDesc[0].mVariableFramesInPacket = 0;
		s->packetDesc[0].mDataByteSize           = s->bytesToDecode;

		( *outDataPacketDescription ) = s->packetDesc;
	}

	if ( s->bytesToDecode == 0 ) {
		// We are currently out of data but want to keep on processing
		// See Apple Technical Q&A QA1317
		*ioNumberDataPackets=0;
		return 'eof ';
	}

	s->bytesToDecode = 0;

	return noErr;
}


static void dec_process ( MSFilter *f ) {
	struct DecState *s= ( struct DecState* ) f->data;
	/* get the input message from queue */
	mblk_t *inputMessage;

	UInt32 numOutputDataPackets;
	while ( ( inputMessage=ms_queue_get ( f->inputs[0] ) ) ) {
		/* process the input message */
		/* parse the haeder to get the number of frames in the message au-headers-length is length of headers in bits, for each frame, 16 bits header */
		UInt16 frameNumber = ((((UInt16)(inputMessage->b_rptr[0]))<<8) + ((UInt16)(inputMessage->b_rptr[1])))/16;
		UInt16 headerOffset = 2*frameNumber + 2; /* actual frame start at this offset in input message */
		size_t outputMessageSize = frameNumber*4 /*to support up to 2048 audio samples*/ *SIGNAL_FRAME_SIZE * s->nchannels;
		mblk_t *outputMessage = allocb ( outputMessageSize, 0 ); /* fixed size output frame, allocate the requested amount in the output message */

		int frameIndex;
		s->decodeBuffer=inputMessage->b_rptr + headerOffset; /* initialise the read pointer to the beginning of the first frame */
		for (frameIndex=0; frameIndex<frameNumber; frameIndex++) {

			/* get the frame length from the header */
			UInt16 frameLength = ((((UInt16)(inputMessage->b_rptr[2+2*frameIndex]))<<8) + ((UInt16)(inputMessage->b_rptr[2+2*frameIndex+1])))>>3;

			s->bytesToDecode = frameLength;

			/* Create the output buffer list */
			AudioBufferList outBufferList;
			outBufferList.mNumberBuffers = 1;
			outBufferList.mBuffers[0].mNumberChannels = s->nchannels;
			outBufferList.mBuffers[0].mDataByteSize   = outputMessageSize;
			outBufferList.mBuffers[0].mData           = outputMessage->b_wptr;

			OSStatus status = noErr;
			numOutputDataPackets=outBufferList.mBuffers[0].mDataByteSize/s->maxOutputPacketSize;
			AudioStreamPacketDescription outputPacketDesc[numOutputDataPackets];
			/* Start the decoding process */
			status = AudioConverterFillComplexBuffer ( s->audioConverter,
													  decoderCallback,
													  s,
													  &numOutputDataPackets,
													  &outBufferList,
													  outputPacketDesc );
			if (status != noErr && numOutputDataPackets == 0)	 {
				ms_error ("Cannot decode AAC sample because [%c%c%c%c]"
						  ,((char*)&status)[3]
						  ,((char*)&status)[2]
						  ,((char*)&status)[1]
						  ,((char*)&status)[0]);

			}
			outputMessage->b_wptr += numOutputDataPackets*s->maxOutputPacketSize; // increment write pointer of output message by the amount of data written by the decoder
			outputMessageSize = outputMessageSize - numOutputDataPackets*s->maxOutputPacketSize; /*for next frame*/
			s->decodeBuffer = (UInt8*)s->decodeBuffer + frameLength; /* increase the read pointer to the begining of next frame (if any, otherwise, it won't be used) */
		}
		/* insert the output message with all decoded frame in the queue and free the input message */
		ms_queue_put ( f->outputs[0],outputMessage ); /* insert the decoded message in the output queue for MSFilter */
		/* signal to concealment context the reception of data */
		ms_concealer_ts_context_inc_sample_ts(s->concealer,  f->ticker->time*s->samplingRate/1000, numOutputDataPackets, 1);
		freemsg ( inputMessage );
	}

	/* is concealment needed? */
	if(ms_concealer_ts_context_is_concealement_required(s->concealer, f->ticker->time*s->samplingRate/1000)) {
		mblk_t *outputMessage = allocb (SIGNAL_FRAME_SIZE*s->nchannels, 0);

		/* send a zero frame to the decoder (2 bytes at 0) */
		s->bytesToDecode = 2;
		UInt16 zerobytes = 0;
		s->decodeBuffer = &zerobytes;
		numOutputDataPackets = SIGNAL_FRAME_SIZE*s->nchannels / s->maxOutputPacketSize;

		/* Create the output buffer list */
		AudioBufferList outBufferList;
		outBufferList.mNumberBuffers = 1;
		outBufferList.mBuffers[0].mNumberChannels = s->nchannels;
		outBufferList.mBuffers[0].mDataByteSize   = SIGNAL_FRAME_SIZE*s->nchannels;
		outBufferList.mBuffers[0].mData           = outputMessage->b_wptr;

		OSStatus status = noErr;
		AudioStreamPacketDescription outputPacketDesc[numOutputDataPackets];

		/* Start the decoding process */
		status = AudioConverterFillComplexBuffer ( s->audioConverter,
												  decoderCallback,
												  s,
												  &numOutputDataPackets,
												  &outBufferList,
												  outputPacketDesc );

		outputMessage->b_wptr += numOutputDataPackets*s->nchannels;
		mblk_set_plc_flag(outputMessage, 1);
		ms_queue_put (f->outputs[0], outputMessage);
		ms_concealer_ts_context_inc_sample_ts(s->concealer, f->ticker->time*s->samplingRate/1000, SIGNAL_FRAME_SIZE/sizeof(SInt16), 0);
	}
}

static void dec_postprocess ( MSFilter *f ) {

}

static void dec_uninit ( MSFilter *f ) {
	struct DecState *s= ( struct DecState* ) f->data;
	ms_concealer_ts_context_destroy(s->concealer);
	AudioConverterDispose(s->audioConverter);
	ms_free ( s );
	f->data = 0;
}

static int dec_add_fmtp ( MSFilter *f, void *data ) {
	const char *fmtp= ( const char* ) data;
	struct DecState *s= ( struct DecState* ) f->data;
	char config[512];
	if ( fmtp_get_value ( fmtp,"config",config,sizeof ( config ) ) ) {
		//convert hexa decimal config string into a bitstream
		int i,j,max=strlen ( config );
		char octet[3];
		octet[2]=0;
		for ( i=0,j=0; i<max; i+=2,j++ ) {
			octet[0]=config[i];
			octet[1]=config[i+1];
			s->audioConverterProperty[j]= ( uint8_t ) strtol ( octet,NULL,16 );
		}
		s->audioConverterProperty_size=j;
		ms_message ( "Got mpeg4 config string: %s",config );
	}
	return 0;
}

static int dec_set_sr ( MSFilter *f, void *arg ) {
	struct DecState *s= ( struct DecState* ) f->data;
	bool_t found = FALSE;
	int i;

	s->samplingRate = ( ( uint32_t* ) arg ) [0];
	for( i=0; i<aac_rates_size;i++){
		if( s->samplingRate == aac_rates[i].sampling ){
			found = TRUE;
            break;
		}
	}

	if(!found) {
		// unlike the encoder, we fall back to 44100 here so that we can still do lower in any case
		ms_message("AAC-ELD decoder, try to set unsupported sampling rate %d Hz, fallback to 44100Hz", s->samplingRate);
		s->samplingRate = 44100;
	}
	ms_debug("AAC-ELD decoder sample rate set to %d (asked %d)", s->samplingRate, *(int*)arg);
	return 0;
}

static int dec_get_sr ( MSFilter *f, void *arg ) {
	struct DecState *s= ( struct DecState* ) f->data;
	( ( int* ) arg ) [0]=s->samplingRate;
	return 0;
}

/* decoder support PLC, allow Mediastreamer2 to know about it */
static int dec_have_plc ( MSFilter *f, void *arg ) {
	* ( ( int * ) arg ) = 1;
	return 0;
}
static int dec_set_nchannels(MSFilter *f, void *arg){
	ms_debug("dec_set_nchannels %d", *((int*)arg));
	struct DecState *s=( struct DecState* )f->data;
	s->nchannels=*(int*)arg;
	return 0;
}

static int dec_get_nchannels(MSFilter *f, void *data) {
	struct DecState *s=( struct DecState* )f->data;
	*(int *)data = s->nchannels;
	return 0;
}
/* attach decoder methods to MSFilter IDs */
static MSFilterMethod dec_methods[]= {
	{	MS_FILTER_SET_SAMPLE_RATE	,	dec_set_sr	},
	{	MS_FILTER_GET_SAMPLE_RATE	,	dec_get_sr	},
	{	MS_FILTER_ADD_FMTP			,	dec_add_fmtp	},
	{ 	MS_DECODER_HAVE_PLC			, 	dec_have_plc	},
	{	MS_FILTER_SET_NCHANNELS		,	dec_set_nchannels},
	{	MS_FILTER_GET_NCHANNELS		,	dec_get_nchannels},
	{	0				,	NULL		}
};

MSFilterDesc ms_aac_eld_dec_desc= {
	MS_AAC_ELD_DEC_ID, /**< this ID must be declared in allfilter.h */
	"MSAACELDDec", /**< filter name */
	"AAC-ELD decoder", /**< filter Description */
	MS_FILTER_DECODER, /**< filter category */
	"mpeg4-generic", /**< MIME type for AAC-ELD */
	1, /**< number of inputs */
	1, /**< number of outputs */
	dec_init, /**< Filter's init function*/
	dec_preprocess, /**< Filter's preprocess function, called one time before starting to process*/
	dec_process, /**< Filter's process function, called every tick by the MSTicker to do the filter's job*/
	dec_postprocess, /**< Filter's postprocess function, called once after processing (the filter is no longer called in process() after)*/
	dec_uninit, /**< Filter's uninit function, used to deallocate internal structures*/
	dec_methods /**<Filter's method table*/
};


MS_FILTER_DESC_EXPORT ( ms_aac_eld_dec_desc )
MS_FILTER_DESC_EXPORT ( ms_aac_eld_enc_desc )
