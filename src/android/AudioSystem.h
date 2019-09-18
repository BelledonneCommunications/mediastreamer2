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

#ifndef ANDROID_AUDIOSYSTEM_H_
#define ANDROID_AUDIOSYSTEM_H_


#include "audio.h"
#include "loader.h"
#include "String8.h"
//#include <system/audio_policy.h>

/* XXX: Should be include by all the users instead */
//#include <media/AudioParameter.h>

namespace fake_android {
	
	


typedef void (*audio_error_callback)(status_t err);

class IAudioPolicyService;

class AudioSystem
{
public:

    /* These are static methods to control the system-wide AudioFlinger
     * only privileged processes can have access to them
     */

    // mute/unmute microphone
    static status_t muteMicrophone(bool state);
    static status_t isMicrophoneMuted(bool *state);

    // set/get master volume
    static status_t setMasterVolume(float value);
    static status_t getMasterVolume(float* volume);

    // mute/unmute audio outputs
    static status_t setMasterMute(bool mute);
    static status_t getMasterMute(bool* mute);

    // set/get stream volume on specified output
    static status_t setStreamVolume(audio_stream_type_t stream, float value,
                                    audio_io_handle_t output);
    static status_t getStreamVolume(audio_stream_type_t stream, float* volume,
                                    audio_io_handle_t output);

    // mute/unmute stream
    static status_t setStreamMute(audio_stream_type_t stream, bool mute);
    static status_t getStreamMute(audio_stream_type_t stream, bool* mute);

    // set audio mode in audio hardware
    static status_t setMode(audio_mode_t mode);

    // returns true in *state if tracks are active on the specified stream or has been active
    // in the past inPastMs milliseconds
    static status_t isStreamActive(audio_stream_type_t stream, bool *state, uint32_t inPastMs = 0);

    // set/get audio hardware parameters. The function accepts a list of parameters
    // key value pairs in the form: key1=value1;key2=value2;...
    // Some keys are reserved for standard parameters (See AudioParameter class).
    static status_t setParameters(audio_io_handle_t ioHandle, const String8& keyValuePairs);
    static String8  getParameters(audio_io_handle_t ioHandle, const String8& keys);

    static void setErrorCallback(audio_error_callback cb);


    static float linearToLog(int volume);
    static int logToLinear(float volume);

    static status_t getOutputSamplingRate(int* samplingRate, audio_stream_type_t stream = AUDIO_STREAM_DEFAULT);
    static status_t getOutputFrameCount(int* frameCount, audio_stream_type_t stream = AUDIO_STREAM_DEFAULT);
    static status_t getOutputLatency(uint32_t* latency, audio_stream_type_t stream = AUDIO_STREAM_DEFAULT);
    static status_t getSamplingRate(audio_io_handle_t output,
                                          audio_stream_type_t streamType,
                                          int* samplingRate);
    // returns the number of frames per audio HAL write buffer. Corresponds to
    // audio_stream->get_buffer_size()/audio_stream_frame_size()
    static status_t getFrameCount(audio_io_handle_t output,
                                  audio_stream_type_t stream,
                                  int* frameCount);
    // returns the audio output stream latency in ms. Corresponds to
    // audio_stream_out->get_latency()
    static status_t getLatency(audio_io_handle_t output,
                               audio_stream_type_t stream,
                               uint32_t* latency);

    // DEPRECATED
    static status_t getOutputSamplingRate(int* samplingRate, int stream = AUDIO_STREAM_DEFAULT);

    // DEPRECATED
    static status_t getOutputFrameCount(int* frameCount, int stream = AUDIO_STREAM_DEFAULT);

    // DEPRECATED
    static status_t getOutputLatency(uint32_t* latency, int stream = AUDIO_STREAM_DEFAULT);

    static bool routedToA2dpOutput(audio_stream_type_t streamType);

    static status_t getInputBufferSize(uint32_t sampleRate, audio_format_t format, int channelCount,
        size_t* buffSize);

    static status_t setVoiceVolume(float volume);

    // return the number of audio frames written by AudioFlinger to audio HAL and
    // audio dsp to DAC since the output on which the specified stream is playing
    // has exited standby.
    // returned status (from utils/Errors.h) can be:
    // - NO_ERROR: successful operation, halFrames and dspFrames point to valid data
    // - INVALID_OPERATION: Not supported on current hardware platform
    // - BAD_VALUE: invalid parameter
    // NOTE: this feature is not supported on all hardware platforms and it is
    // necessary to check returned status before using the returned values.
    static status_t getRenderPosition(uint32_t *halFrames, uint32_t *dspFrames, audio_stream_type_t stream = AUDIO_STREAM_DEFAULT);

    static unsigned int  getInputFramesLost(audio_io_handle_t ioHandle);

    static int newAudioSessionId();
    static void acquireAudioSessionId(int audioSession);
    static void releaseAudioSessionId(int audioSession);

    // types of io configuration change events received with ioConfigChanged()
    enum io_config_event {
        OUTPUT_OPENED,
        OUTPUT_CLOSED,
        OUTPUT_CONFIG_CHANGED,
        INPUT_OPENED,
        INPUT_CLOSED,
        INPUT_CONFIG_CHANGED,
        STREAM_CONFIG_CHANGED,
        NUM_CONFIG_EVENTS
    };

    // audio output descriptor used to cache output configurations in client process to avoid frequent calls
    // through IAudioFlinger
    class OutputDescriptor {
    public:
        OutputDescriptor()
        : samplingRate(0), format(AUDIO_FORMAT_DEFAULT), channels(0), frameCount(0), latency(0)  {}

        uint32_t samplingRate;
        int32_t format;
        int32_t channels;
        size_t frameCount;
        uint32_t latency;
    };

    // Events used to synchronize actions between audio sessions.
    // For instance SYNC_EVENT_PRESENTATION_COMPLETE can be used to delay recording start until playback
    // is complete on another audio session.
    // See definitions in MediaSyncEvent.java
    enum sync_event_t {
        SYNC_EVENT_SAME = -1,             // used internally to indicate restart with same event
        SYNC_EVENT_NONE = 0,
        SYNC_EVENT_PRESENTATION_COMPLETE,

        //
        // Define new events here: SYNC_EVENT_START, SYNC_EVENT_STOP, SYNC_EVENT_TIME ...
        //
        SYNC_EVENT_CNT,
    };

    // Timeout for synchronous record start. Prevents from blocking the record thread forever
    // if the trigger event is not fired.
    static const uint32_t kSyncRecordStartTimeOutMs = 30000;

    //
    // IAudioPolicyService interface (see AudioPolicyInterface for method descriptions)
    //
	/*
    static status_t setDeviceConnectionState(audio_devices_t device, audio_policy_dev_state_t state, const char *device_address);
    static audio_policy_dev_state_t getDeviceConnectionState(audio_devices_t device, const char *device_address);*/
    static status_t setPhoneState(audio_mode_t state);
    static status_t setForceUse(audio_policy_force_use_t usage, audio_policy_forced_cfg_t config);
    /*static audio_policy_forced_cfg_t getForceUse(audio_policy_force_use_t usage);
    static audio_io_handle_t getOutput(audio_stream_type_t stream,
                                        uint32_t samplingRate = 0,
                                        audio_format_t format = AUDIO_FORMAT_DEFAULT,
                                        uint32_t channels = AUDIO_CHANNEL_OUT_STEREO,
                                        audio_output_flags_t flags = AUDIO_OUTPUT_FLAG_NONE);
    static status_t startOutput(audio_io_handle_t output,
                                audio_stream_type_t stream,
                                int session = 0);
    static status_t stopOutput(audio_io_handle_t output,
                               audio_stream_type_t stream,
                               int session = 0);
    static void releaseOutput(audio_io_handle_t output);
    */
    static audio_io_handle_t getInput(audio_source_t inputSource,
                                    uint32_t samplingRate = 0,
                                    audio_format_t format = AUDIO_FORMAT_DEFAULT,
                                    uint32_t channels = AUDIO_CHANNEL_IN_MONO,
                                    audio_in_acoustics_t acoustics = (audio_in_acoustics_t)0,
                                    int sessionId = 0);
	/*
    static status_t startInput(audio_io_handle_t input);
    static status_t stopInput(audio_io_handle_t input);
    static void releaseInput(audio_io_handle_t input);
    static status_t initStreamVolume(audio_stream_type_t stream,
                                      int indexMin,
                                      int indexMax);
    static status_t setStreamVolumeIndex(audio_stream_type_t stream,
                                         int index,
                                         audio_devices_t device);
    static status_t getStreamVolumeIndex(audio_stream_type_t stream,
                                         int *index,
                                         audio_devices_t device);

    static uint32_t getStrategyForStream(audio_stream_type_t stream);
    static audio_devices_t getDevicesForStream(audio_stream_type_t stream);

    static audio_io_handle_t getOutputForEffect(effect_descriptor_t *desc);
    static status_t registerEffect(effect_descriptor_t *desc,
                                    audio_io_handle_t io,
                                    uint32_t strategy,
                                    int session,
                                    int id);
    static status_t unregisterEffect(int id);
    static status_t setEffectEnabled(int id, bool enabled);

    // clear stream to output mapping cache (gStreamOutputMap)
    // and output configuration cache (gOutputs)
    static void clearAudioConfigCache();
    */
private:
	class AudioSystemImpl *mImpl;

};

class AudioSystemImpl{
public:
	bool mApi18;
	static bool init(Library *lib);
	static AudioSystemImpl *get(){
		return sImpl;
	}
	Function2<status_t,int*,int> mGetOutputSamplingRate;
	Function2<status_t, int *, int> mGetOutputFrameCount;
	Function2<status_t, uint32_t *, int> mGetOutputLatency;
	Function2<status_t,audio_io_handle_t,const String8 &> mSetParameters;
	Function1<status_t, audio_mode_t> mSetPhoneState;
	Function2<status_t, audio_policy_force_use_t, audio_policy_forced_cfg_t> mSetForceUse;
	Function0<int> mNewAudioSessionId;
	//Function6<audio_io_handle_t,audio_source_t,uint32_t,audio_format_t,uint32_t,audio_in_acoustics_t,int> mGetInput;
private:
	AudioSystemImpl(Library *lib);
	static AudioSystemImpl *sImpl;
};

class RefBaseImpl{
public:
	static bool init(Library *lib);
	static RefBaseImpl * get(){
		return sImpl;
	}
	Function1<void,void*> mCtor;
	Function2<void,void*,const void*> mIncStrong;
	Function2<void,void*,const void*> mDecStrong;
	Function1<int32_t,void*> mGetStrongCount;
private:
	RefBaseImpl(Library *lib);
	static RefBaseImpl *sImpl;
};

class RefBase{
public:
	RefBase();
	virtual ~RefBase();
	void incStrong(const void* id) const;
	void decStrong(const void* id) const;
	int32_t getStrongCount() const;
protected:
	virtual void *getRealThis()const =0;
	virtual bool isRefCounted()const =0;
	virtual void destroy()const=0; //used when the object is not refcounted*/
private:
	RefBaseImpl *mImpl;
	mutable int mCnt;
};

template <typename _T>
class sp{
public:
	sp() : mPtr(0){
	}
	sp(_T *p) : mPtr(0){
		assign(p);
	}
	sp(const sp<_T> &other) : mPtr(0){
		assign(other.mPtr);
	}
	void reset(){
		assign(0);
	}
	~sp(){
		reset();
	}
	_T * operator->(){
		return mPtr;
	}
	bool operator==(const sp<_T> & other)const{
		return mPtr==other.mPtr;
	}
	sp<_T> & operator=(const sp<_T> & other){
		assign(other.mPtr);
		return *this;
	}
	sp<_T> & operator=(_T *ptr){
		assign(ptr);
		return *this;
	}
	bool operator!(){
		return !mPtr;
	}
	bool operator!=(const sp<_T> & other){
		return mPtr!=other.mPtr;
	}
	_T * get()const{
		return mPtr;
	}
private:
	void assign(_T *ptr){
		if (ptr) ptr->incStrong(this);
		if (mPtr) {
			mPtr->decStrong(this);
			mPtr=0;
		}
		mPtr=ptr;
	}
	_T *mPtr;
};

ptrdiff_t findRefbaseOffset(void *obj, size_t size);

void dumpMemory(void *obj, size_t size);

};  // namespace android

#endif  /*ANDROID_AUDIOSYSTEM_H_*/
