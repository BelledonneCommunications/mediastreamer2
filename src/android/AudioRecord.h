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

#ifndef AUDIORECORD_H_
#define AUDIORECORD_H_

#include <stdint.h>

#include "audio.h"
#include "AudioSystem.h"

#include "loader.h"

namespace fake_android {

class AudioRecordImpl;
// ----------------------------------------------------------------------------

class AudioRecord : public RefBase
{
	friend class AudioRecordImpl;
public:

    static const int DEFAULT_SAMPLE_RATE = 8000;

    /* Events used by AudioRecord callback function (callback_t).
     *
     * to keep in sync with frameworks/base/media/java/android/media/AudioRecord.java
     */
    enum event_type {
        EVENT_MORE_DATA = 0,        // Request to reqd more data from PCM buffer.
        EVENT_OVERRUN = 1,          // PCM buffer overrun occured.
        EVENT_MARKER = 2,           // Record head is at the specified marker position
                                    // (See setMarkerPosition()).
        EVENT_NEW_POS = 3,          // Record head is at a new position
                                    // (See setPositionUpdatePeriod()).
    };

    /* Create Buffer on the stack and pass it to obtainBuffer()
     * and releaseBuffer().
     */

    class OldBuffer
    {
    public:
        enum {
            MUTE    = 0x00000001
        };
        uint32_t    flags;
        int         channelCount;
        audio_format_t format;
        size_t      frameCount;
        size_t      size;
        union {
            void*       raw;
            short*      i16;
            int8_t*     i8;
        };
    };
	
	class Buffer //Android 4.3
    {
    public:
        size_t      frameCount;   // number of sample frames corresponding to size;
                                  // on input it is the number of frames desired,
                                  // on output is the number of frames actually filled

        size_t      size;         // input/output in byte units
        union {
            void*       raw;
            short*      i16;    // signed 16-bit
            int8_t*     i8;     // unsigned 8-bit, offset by 0x80
        };
    };
	
	static void readBuffer(const void *p_info, Buffer *buffer);

    /* These are static methods to control the system-wide AudioFlinger
     * only privileged processes can have access to them
     */

//    static status_t setMasterMute(bool mute);

    /* As a convenience, if a callback is supplied, a handler thread
     * is automatically created with the appropriate priority. This thread
     * invokes the callback when a new buffer becomes ready or an overrun condition occurs.
     * Parameters:
     *
     * event:   type of event notified (see enum AudioRecord::event_type).
     * user:    Pointer to context for use by the callback receiver.
     * info:    Pointer to optional parameter according to event type:
     *          - EVENT_MORE_DATA: pointer to AudioRecord::Buffer struct. The callback must not read
     *          more bytes than indicated by 'size' field and update 'size' if less bytes are
     *          read.
     *          - EVENT_OVERRUN: unused.
     *          - EVENT_MARKER: pointer to an uin32_t containing the marker position in frames.
     *          - EVENT_NEW_POS: pointer to an uin32_t containing the new position in frames.
     */

    typedef void (*callback_t)(int event, void* user, void *info);

    /* Returns the minimum frame count required for the successful creation of
     * an AudioRecord object.
     * Returned status (from utils/Errors.h) can be:
     *  - NO_ERROR: successful operation
     *  - NO_INIT: audio server or audio hardware not initialized
     *  - BAD_VALUE: unsupported configuration
     */

     static status_t getMinFrameCount(int* frameCount,
                                      uint32_t sampleRate,
                                      audio_format_t format,
                                      int channelCount);

    /* Constructs an uninitialized AudioRecord. No connection with
     * AudioFlinger takes place.
     */
                        AudioRecord();

    /* Creates an AudioRecord track and registers it with AudioFlinger.
     * Once created, the track needs to be started before it can be used.
     * Unspecified values are set to the audio hardware's current
     * values.
     *
     * Parameters:
     *
     * inputSource:        Select the audio input to record to (e.g. AUDIO_SOURCE_DEFAULT).
     * sampleRate:         Track sampling rate in Hz.
     * format:             Audio format (e.g AUDIO_FORMAT_PCM_16_BIT for signed
     *                     16 bits per sample).
     * channelMask:        Channel mask: see audio_channels_t.
     * frameCount:         Total size of track PCM buffer in frames. This defines the
     *                     latency of the track.
     * flags:              A bitmask of acoustic values from enum record_flags.  It enables
     *                     AGC, NS, and IIR.
     * cbf:                Callback function. If not null, this function is called periodically
     *                     to provide new PCM data.
     * notificationFrames: The callback function is called each time notificationFrames PCM
     *                     frames are ready in record track output buffer.
     * user                Context for use by the callback receiver.
     */

     // FIXME consider removing this alias and replacing it by audio_in_acoustics_t
     //       or removing the parameter entirely if it is unused
     enum record_flags {
         RECORD_AGC_ENABLE = AUDIO_IN_ACOUSTICS_AGC_ENABLE,
         RECORD_NS_ENABLE  = AUDIO_IN_ACOUSTICS_NS_ENABLE,
         RECORD_IIR_ENABLE = AUDIO_IN_ACOUSTICS_TX_IIR_ENABLE,
     };
	 
	 enum transfer_type {
        TRANSFER_DEFAULT,   // not specified explicitly; determine from other parameters
        TRANSFER_CALLBACK,  // callback EVENT_MORE_DATA
        TRANSFER_OBTAIN,    // FIXME deprecated: call obtainBuffer() and releaseBuffer()
        TRANSFER_SYNC,      // synchronous read()
    };


                        AudioRecord(audio_source_t inputSource,
                                    uint32_t sampleRate = 0,
                                    audio_format_t format = AUDIO_FORMAT_DEFAULT,
                                    uint32_t channelMask = AUDIO_CHANNEL_IN_MONO,
                                    int frameCount      = 0,
                                    callback_t cbf = NULL,
                                    void* user = NULL,
                                    int notificationFrames = 0,
                                    int sessionId = 0, 
									transfer_type transferType = TRANSFER_DEFAULT,
                                    audio_input_flags_t flags = AUDIO_INPUT_FLAG_NONE);


    /* Terminates the AudioRecord and unregisters it from AudioFlinger.
     * Also destroys all resources assotiated with the AudioRecord.
     */
                        ~AudioRecord();


    /* Initialize an uninitialized AudioRecord.
     * Returned status (from utils/Errors.h) can be:
     *  - NO_ERROR: successful intialization
     *  - INVALID_OPERATION: AudioRecord is already intitialized or record device is already in use
     *  - BAD_VALUE: invalid parameter (channels, format, sampleRate...)
     *  - NO_INIT: audio server or audio hardware not initialized
     *  - PERMISSION_DENIED: recording is not allowed for the requesting process
     * */
            status_t    set(audio_source_t inputSource = AUDIO_SOURCE_DEFAULT,
                            uint32_t sampleRate = 0,
                            audio_format_t format = AUDIO_FORMAT_DEFAULT,
                            uint32_t channelMask = AUDIO_CHANNEL_IN_MONO,
                            int frameCount      = 0,
                            record_flags flags  = (record_flags) 0,
                            callback_t cbf = NULL,
                            void* user = NULL,
                            int notificationFrames = 0,
                            bool threadCanCallJava = false,
                            int sessionId = 0);


    /* Result of constructing the AudioRecord. This must be checked
     * before using any AudioRecord API (except for set()), using
     * an uninitialized AudioRecord produces undefined results.
     * See set() method above for possible return codes.
     */
            status_t    initCheck() const;

    /* Returns this track's latency in milliseconds.
     * This includes the latency due to AudioRecord buffer size
     * and audio hardware driver.
     */
            uint32_t     latency() const;

   /* getters, see constructor */

            audio_format_t format() const;
            int         channelCount() const;
            int         channels() const;
            uint32_t    frameCount() const;
            size_t      frameSize() const;
            audio_source_t inputSource() const;


    /* After it's created the track is not active. Call start() to
     * make it active. If set, the callback will start being called.
     * if event is not AudioSystem::SYNC_EVENT_NONE, the capture start will be delayed until
     * the specified event occurs on the specified trigger session.
     */
            status_t    start(AudioSystem::sync_event_t event = AudioSystem::SYNC_EVENT_NONE,
                              int triggerSession = 0);

    /* Stop a track. If set, the callback will cease being called and
     * obtainBuffer returns STOPPED. Note that obtainBuffer() still works
     * and will fill up buffers until the pool is exhausted.
     */
            status_t    stop();
            bool        stopped() const;

    /* get sample rate for this record track
     */
            uint32_t    getSampleRate() const;

    /* Sets marker position. When record reaches the number of frames specified,
     * a callback with event type EVENT_MARKER is called. Calling setMarkerPosition
     * with marker == 0 cancels marker notification callback.
     * If the AudioRecord has been opened with no callback function associated,
     * the operation will fail.
     *
     * Parameters:
     *
     * marker:   marker position expressed in frames.
     *
     * Returned status (from utils/Errors.h) can be:
     *  - NO_ERROR: successful operation
     *  - INVALID_OPERATION: the AudioRecord has no callback installed.
     */
            status_t    setMarkerPosition(uint32_t marker);
            status_t    getMarkerPosition(uint32_t *marker) const;


    /* Sets position update period. Every time the number of frames specified has been recorded,
     * a callback with event type EVENT_NEW_POS is called.
     * Calling setPositionUpdatePeriod with updatePeriod == 0 cancels new position notification
     * callback.
     * If the AudioRecord has been opened with no callback function associated,
     * the operation will fail.
     *
     * Parameters:
     *
     * updatePeriod:  position update notification period expressed in frames.
     *
     * Returned status (from utils/Errors.h) can be:
     *  - NO_ERROR: successful operation
     *  - INVALID_OPERATION: the AudioRecord has no callback installed.
     */
            status_t    setPositionUpdatePeriod(uint32_t updatePeriod);
            status_t    getPositionUpdatePeriod(uint32_t *updatePeriod) const;


    /* Gets record head position. The position is the  total number of frames
     * recorded since record start.
     *
     * Parameters:
     *
     *  position:  Address where to return record head position within AudioRecord buffer.
     *
     * Returned status (from utils/Errors.h) can be:
     *  - NO_ERROR: successful operation
     *  - BAD_VALUE:  position is NULL
     */
            status_t    getPosition(uint32_t *position) const;

    /* returns a handle on the audio input used by this AudioRecord.
     *
     * Parameters:
     *  none.
     *
     * Returned value:
     *  handle on audio hardware input
     */
            audio_io_handle_t    getInput() const;

    /* returns the audio session ID associated to this AudioRecord.
     *
     * Parameters:
     *  none.
     *
     * Returned value:
     *  AudioRecord session ID.
     */
            int    getSessionId() const;

    /* obtains a buffer of "frameCount" frames. The buffer must be
     * filled entirely. If the track is stopped, obtainBuffer() returns
     * STOPPED instead of NO_ERROR as long as there are buffers available,
     * at which point NO_MORE_BUFFERS is returned.
     * Buffers will be returned until the pool (buffercount())
     * is exhausted, at which point obtainBuffer() will either block
     * or return WOULD_BLOCK depending on the value of the "blocking"
     * parameter.
     */

        enum {
            NO_MORE_BUFFERS = 0x80000001,
            STOPPED = 1
        };

            status_t    obtainBuffer(Buffer* audioBuffer, int32_t waitCount);
            void        releaseBuffer(Buffer* audioBuffer);


    /* As a convenience we provide a read() interface to the audio buffer.
     * This is implemented on top of obtainBuffer/releaseBuffer.
     */
            ssize_t     read(void* buffer, size_t size);

    /* Return the amount of input frames lost in the audio driver since the last call of this
     * function.  Audio driver is expected to reset the value to 0 and restart counting upon
     * returning the current value by this function call.  Such loss typically occurs when the
     * user space process is blocked longer than the capacity of audio driver buffers.
     * Unit: the number of input audio frames
     */
            unsigned int  getInputFramesLost() const;
protected:
	/* ms2 addition:*/
	virtual void *getRealThis()const;
	virtual bool isRefCounted()const;
	virtual void destroy()const;
private:
	uint8_t *mThis;
	AudioRecordImpl *mImpl;
	int mSessionId;
};

class AudioRecordImpl{
public:
	static bool init(Library *lib);
	static AudioRecordImpl *get(){
		return sImpl;
	}
	Function11<void,
		void*,
		audio_source_t,
		uint32_t,
		audio_format_t,
		uint32_t,
		int,
		AudioRecord::record_flags,
		AudioRecord::callback_t,
		void*,
		int,
		int> mCtorBeforeAPI17;
	Function12<void,
		void*,
		audio_source_t,
		uint32_t,
		audio_format_t,
		uint32_t,
		int,
		AudioRecord::callback_t,
		void*,
		int,
		int, 
		int,
		int> mCtor;
	Function1<void,void*> mDtor;
	Function1<void,void*> mDefaultCtor;
	Function1<status_t,const void *> mInitCheck;
	Function1<status_t,void *> mStop;
	Function3<status_t, void *, AudioSystem::sync_event_t , int> mStart;
	Function4<status_t, int*, uint32_t, int, int> mGetMinFrameCount;
	Function1<int,const void *> mGetSessionId;
	//Function1<audio_io_handle_t,void*> mGetInput;
	ptrdiff_t mRefBaseOffset;
	int mApiVersion;
	bool mUseRefcount;
	static const size_t sObjSize=1024;
private:
	AudioRecordImpl(Library *lib);
	static AudioRecordImpl *sImpl;
};


}; // namespace android

#endif /*AUDIORECORD_H_*/
