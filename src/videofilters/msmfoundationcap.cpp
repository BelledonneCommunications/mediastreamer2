/*
 * Copyright (c) 2010-2020 Belledonne Communications SARL.
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

#include "msmfoundationcap.h"


#include "mediastreamer2/mswebcam.h"
#include "mediastreamer2/msticker.h"

#include "filter-wrapper/filter-wrapper-base.h"



//#include <camera/camera_api.h>
#include <Mferror.h>//MF_E_NO_MORE_TYPES


#pragma comment(lib,"Mfplat.lib")
#pragma comment(lib,"Mf.lib")
#pragma comment(lib,"Mfreadwrite.lib")
#pragma comment(lib,"mfuuid.lib")
#pragma comment(lib,"shlwapi.lib")


class MFDevices {// Store device description with helper
public:
	IMFActivate ** mDevices;
	IMFAttributes * mAttributes;
	UINT32 mDevicesCount;

	MFDevices(){
		mDevices = NULL;
		mAttributes = NULL;
		mDevicesCount = 0;
	}
	void clean(){
		if (mAttributes) {
			mAttributes->Release();
			mAttributes = NULL;
		}
		for (DWORD i = 0; i < mDevicesCount; i++) {
			if (&mDevices[i]) {
				mDevices[i]->Release();
				mDevices[i] = NULL;
			}
		}
		CoTaskMemFree(mDevices);
		mDevicesCount = 0;
	}
	HRESULT getDevices(){
		HRESULT hr = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
		UINT32 count = 0;
		if (FAILED(hr)) return hr;
		// Create an attribute store to specify enumeration parameters.
		hr = MFCreateAttributes(&mAttributes, 1);
		if (FAILED(hr)) {
			clean();
			return hr;
		}
		//The attribute to be requested is devices that can capture video
		hr = mAttributes->SetGUID( MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE, MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_VIDCAP_GUID );
		if (FAILED(hr)) {
			clean();
			return hr;
		}
		//Enummerate the video capture devices
		hr =  MFEnumDeviceSources(mAttributes, &mDevices, &mDevicesCount);//[desktop apps only]
		if (FAILED(hr))
			clean();
		return hr;
	}
};


MSMFoundationCap::MSMFoundationCap() {
	InitializeCriticalSection(&mCriticalSection);
	InitializeConditionVariable (&mIsFlushed);
	mWidth = MS_VIDEO_SIZE_CIF_W;
	mHeight = MS_VIDEO_SIZE_CIF_H;
	mPixelFormat = MS_YUV420P;
	mAllocator = ms_yuv_buf_allocator_new();
	mReferenceCount = 1;
	mSourceReader = NULL;
	mRawData = NULL;
	mRunning = FALSE;
	mHaveFrame = FALSE;
	mFPS = 0.0;
	mOrientation = 0;
}

MSMFoundationCap::~MSMFoundationCap() {
	EnterCriticalSection(&mCriticalSection);
	if (mSourceReader) {
		mSourceReader->Release();
		mSourceReader = NULL;
	}

	if (mRawData) {
		delete[] mRawData;
		mRawData = NULL;
	}
	ms_free(mAllocator);
	LeaveCriticalSection(&mCriticalSection);
	DeleteCriticalSection(&mCriticalSection);
}

void MSMFoundationCap::setVSize(MSVideoSize vsize) {
	setMediaConfiguration(mVideoFormat, mStride,vsize.width,vsize.height );
}

void MSMFoundationCap::setDeviceName(const std::string &pName) {
	mDeviceName = pName;
}

void MSMFoundationCap::setFPS(float pFps){
	mFPS=pFps;
	ms_video_init_framerate_controller(&mFramerateController, mFPS);
	ms_average_fps_init(&mAvgFPS,"MSMediaFoundationCap: fps=%f");
}

float MSMFoundationCap::getFPS() const {
	return mFPS;
}

int MSMFoundationCap::getDeviceOrientation() const{
	return mOrientation;
}

void MSMFoundationCap::setDeviceOrientation(int orientation){
	mOrientation = orientation;
}
//----------------------------------------

void MSMFoundationCap::activate() {
	setCaptureDevice(mDeviceName);
}

void MSMFoundationCap::feed(MSFilter * filter) {
	EnterCriticalSection(&mCriticalSection);
	if(mRunning && mHaveFrame) {
		if (isTimeToSend(filter->ticker->time)){
			uint32_t timestamp;	
			mblk_t *om = NULL;
			if(mVideoFormat == MFVideoFormat_NV12){// Process raw data from NV12
				const uint8_t * y = mRawData;
				const uint8_t *cbcr = mRawData + mPlaneSize;
				om = copy_ycbcrbiplanar_to_true_yuv_with_rotation(mAllocator, mRawData, cbcr, mOrientation, mWidth, mHeight, mStride, mStride, TRUE);
			}
			if (om != NULL) {
				timestamp = (uint32_t)(filter->ticker->time * 90);// rtp uses a 90000 Hz clockrate for video
				mblk_set_timestamp_info(om, timestamp);
				ms_queue_put(filter->outputs[0], om);
				ms_average_fps_update(&mAvgFPS,filter->ticker->time);
			}
		}
	}
	LeaveCriticalSection(&mCriticalSection);
}

void MSMFoundationCap::start() {
	if(mSourceReader) {
		EnterCriticalSection(&mCriticalSection);
		HRESULT hr = mSourceReader->ReadSample(MF_SOURCE_READER_FIRST_VIDEO_STREAM, 0, NULL, NULL, NULL, NULL);  // Ask for the first sample.
		if (SUCCEEDED(hr)){
			mRunning = TRUE;// Inside critical section to ensure to not miss the first frame
		}
		LeaveCriticalSection(&mCriticalSection);
	}
}

void MSMFoundationCap::stop() {
	mRunning = FALSE;// No need to protect as it's a boolean and setting are only done in start/stop functions
	if(mSourceReader){
		EnterCriticalSection(&mCriticalSection);
		mSourceReader->Flush(MF_SOURCE_READER_FIRST_VIDEO_STREAM);// Flush the video stream
		SleepConditionVariableCS (&mIsFlushed, &mCriticalSection, INFINITE);// wait for emptying queue. This is done asynchrounsly as the Callback on flush has been implemented
		LeaveCriticalSection(&mCriticalSection);
	}
}

void MSMFoundationCap::deactivate() {
	EnterCriticalSection(&mCriticalSection);
	if (mSourceReader) {
		mSourceReader->Release();
		mSourceReader = NULL;
	}
	mHaveFrame = FALSE;
	LeaveCriticalSection(&mCriticalSection);
}

//-------------------------------------------------------------------------------------------------------------

//From IUnknown
STDMETHODIMP MSMFoundationCap::QueryInterface(REFIID riid, void** ppvObject) {
	static const QITAB qit[] = { QITABENT(MSMFoundationCap, IMFSourceReaderCallback),{ 0 }, };
	return QISearch(this, qit, riid, ppvObject);
}
//From IUnknown
ULONG MSMFoundationCap::Release() {
	ULONG count = InterlockedDecrement(&mReferenceCount);
	if (count == 0)
		delete this;
	// For thread safety
	return count;
}
//From IUnknown
ULONG MSMFoundationCap::AddRef() { return InterlockedIncrement(&mReferenceCount); }
//Method from IMFSourceReaderCallback
STDMETHODIMP MSMFoundationCap::OnEvent(DWORD, IMFMediaEvent *) { return S_OK; }
//Method from IMFSourceReaderCallback
STDMETHODIMP MSMFoundationCap::OnFlush(DWORD) {
	WakeConditionVariable (&mIsFlushed);// Wakeup threads that are waiting for the flush
	return S_OK; 
}

//-------------------------------------------------------------------------------------------------------------

HRESULT MSMFoundationCap::setCaptureDevice(const std::string& pName) {
	MFDevices devices;
	HRESULT hr = devices.getDevices();
	UINT32 currentDeviceIndex = 0;
	bool_t found = FALSE;

	if( FAILED(hr)) return hr;
	while(!found && currentDeviceIndex < devices.mDevicesCount) {
		WCHAR *nameString = NULL;		
		UINT32 cchName; 
		hr = devices.mDevices[currentDeviceIndex]->GetAllocatedString(MF_DEVSOURCE_ATTRIBUTE_FRIENDLY_NAME, &nameString, &cchName);
		if (SUCCEEDED(hr)) {
			size_t inputlen = wcslen(nameString) + 1;
			UINT currentCodePage = GetACP();
			int sizeNeeded = WideCharToMultiByte(currentCodePage, 0, nameString, inputlen, NULL, 0, NULL, NULL);
			char strConversion[256] = {0};
			if(WideCharToMultiByte(currentCodePage, 0, nameString, inputlen, &strConversion[0], sizeNeeded, NULL, NULL) && pName == strConversion) {
					found = TRUE;
			}else
				++currentDeviceIndex;
		}else
			++currentDeviceIndex;
		CoTaskMemFree(nameString);
	}
	if(found){
		hr = setSourceReader(devices.mDevices[currentDeviceIndex]);
		ms_average_fps_init(&mAvgFPS,"MSMediaFoundationCap: fps=%f");
	}
	devices.clean();
	return hr;
}

void MSMFoundationCap::setMediaConfiguration(const GUID &videoFormat, const LONG &stride, const UINT32 &frameWidth, const UINT32 &frameHeight){
	IMFMediaType *mediaType= NULL;
	HRESULT hr = S_OK;
	int newSize = 0;

	EnterCriticalSection(&mCriticalSection);
	if(mVideoFormat != videoFormat){
		if( mSourceReader){
			if ( videoFormat != MFVideoFormat_NV12 ){ //subtype == MFVideoFormat_RGB32 || subtype == MFVideoFormat_RGB24 || subtype == MFVideoFormat_YUY2 ||
				ms_error("MSMediaFoundationCap: The Video format is not supported : %s. Trying to force to MFVideoFormat_NV12", pixFmtToString(videoFormat));
				mSourceReader->GetCurrentMediaType(MF_SOURCE_READER_FIRST_VIDEO_STREAM, &mediaType);
				if(mediaType){
					mediaType->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_NV12);
					hr = mSourceReader->SetCurrentMediaType(MF_SOURCE_READER_FIRST_AUDIO_STREAM, NULL, mediaType);
					mediaType->Release();
				}
			}
		}else
			ms_error("MSMediaFoundationCap: You must set Media configuration while having a source reader.");
		if(SUCCEEDED(hr))
				mVideoFormat = MFVideoFormat_NV12;
	}
	if(mWidth!=frameWidth || mHeight!=frameHeight || mStride!=stride ){
		mHaveFrame = FALSE;
		mWidth = frameWidth;
		mHeight = frameHeight;
		mStride = stride;
		mBytesPerPixel = abs(mStride) / mWidth;
		mPlaneSize = mWidth * mHeight * mBytesPerPixel;
		if( mRawData)
			delete [] mRawData;
		mRawData = new BYTE[(unsigned int)(mWidth*mHeight * mBytesPerPixel*1.5) + 1]();//+1 for potential alignment issues when pixels are odd.
	}
	LeaveCriticalSection(&mCriticalSection);
}

HRESULT MSMFoundationCap::setSourceReader(IMFActivate *device) {
	HRESULT hr = S_OK;
	IMFMediaSource *source = NULL;
	IMFAttributes *attributes = NULL;
	IMFMediaType *mediaType = NULL;

	EnterCriticalSection(&mCriticalSection);
	hr = device->ActivateObject(__uuidof(IMFMediaSource), (void**)&source);
	if (SUCCEEDED(hr)) //Allocate attributes
		hr = MFCreateAttributes(&attributes, 2);
	if (SUCCEEDED(hr)) //get attributes
		hr = attributes->SetUINT32(MF_READWRITE_DISABLE_CONVERTERS, TRUE);	
	if (SUCCEEDED(hr)) // Set the callback pointer.
		hr = attributes->SetUnknown(MF_SOURCE_READER_ASYNC_CALLBACK, this);	
	if (SUCCEEDED(hr)) //Create the source reader
		hr = MFCreateSourceReaderFromMediaSource(source, attributes, &mSourceReader);	
	if (SUCCEEDED(hr)) { // Try to find a suitable output type.
		bool_t found = FALSE;
		for(DWORD i = 0 ; !found && mSourceReader->GetNativeMediaType((DWORD)MF_SOURCE_READER_FIRST_VIDEO_STREAM, i, &mediaType) != MF_E_NO_MORE_TYPES ; ++i ) {
			GUID videoFormat;
			UINT32 frameWidth, frameHeight;
			LONG stride;
			hr = getMediaConfiguration(mediaType, &videoFormat, &stride, &frameWidth, &frameHeight);	// Use temporary variable in order to not change the previous state
			if (mediaType) {
				mediaType->Release();
				mediaType = NULL;
			}
			if (SUCCEEDED(hr)){
				found = TRUE;
				setMediaConfiguration(videoFormat, stride, frameWidth, frameHeight);
			}
		}
	}
// Cleanup
	if (source) { source->Release(); source = NULL; }
	if (attributes) { attributes->Release(); attributes = NULL; }
	if (mediaType) { mediaType->Release(); mediaType = NULL; }

	LeaveCriticalSection(&mCriticalSection);

	return hr;
}

HRESULT MSMFoundationCap::getMediaConfiguration(IMFMediaType *pType, GUID *videoFormat, LONG *stride, UINT32 * frameWidth, UINT32 * frameHeight) {
	HRESULT hr = S_OK;
	BOOL bFound = FALSE;
	GUID subtype = { 0 };
	LONG tempStride = 0;
	
	//Get the stride for this format so we can calculate the number of bytes per pixel
	hr = pType->GetUINT32(MF_MT_DEFAULT_STRIDE, (UINT32*)&tempStride); // Try to get the default stride from the media type.
	if (FAILED(hr)) {
		//Setting this atribute to NULL we can obtain the default stride
		GUID subtype = GUID_NULL;
		// Obtain the subtype
		hr = pType->GetGUID(MF_MT_SUBTYPE, &subtype);
		//obtain the width and height
		if (SUCCEEDED(hr))
			hr = MFGetAttributeSize(pType, MF_MT_FRAME_SIZE, frameWidth, frameHeight);
		//Calculate the stride based on the subtype and width
		if (SUCCEEDED(hr))
			hr = MFGetStrideForBitmapInfoHeader(subtype.Data1, *frameWidth, &tempStride); //[desktop apps only]
		// set the attribute so it can be read
		if (SUCCEEDED(hr))
			(void)pType->SetUINT32(MF_MT_DEFAULT_STRIDE, UINT32(tempStride));
	}
	if (SUCCEEDED(hr))
		*stride = tempStride;
	if (FAILED(hr)) { return hr; }
	hr = pType->GetGUID(MF_MT_SUBTYPE, &subtype);
	*videoFormat = subtype;
	if (FAILED(hr)) { return hr; }
	return hr;
}

//Method from IMFSourceReaderCallback
HRESULT MSMFoundationCap::OnReadSample(HRESULT status, DWORD streamIndex, DWORD streamFlags, LONGLONG timeStamp, IMFSample *sample) {
	HRESULT hr = S_OK;
	IMFMediaBuffer *mediaBuffer = NULL;

	EnterCriticalSection(&mCriticalSection);
	if(mRunning) {
		if (FAILED(status))
			hr = status;
		if (SUCCEEDED(hr)) {
			if (sample) {// Get the video frame buffer from the sample.
				hr = sample->GetBufferByIndex(0, &mediaBuffer);
				if (SUCCEEDED(hr) && mRawData) {
					BYTE* data;
					mediaBuffer->Lock(&data, NULL, &mRawDataLength);
					CopyMemory(mRawData, data, mRawDataLength);
					if(!mHaveFrame)
						mHaveFrame = TRUE;
				}
			}
		}	
		if (SUCCEEDED(hr) && mSourceReader){ // Request the next frame.
			hr = mSourceReader->ReadSample((DWORD)MF_SOURCE_READER_FIRST_VIDEO_STREAM, 0, NULL, NULL, NULL, NULL);
		}
		if (FAILED(hr))
			ms_error("MSMediaFoundationCap: Cannot read sample : %d", hr);
		if (mediaBuffer) { mediaBuffer->Release(); mediaBuffer = NULL; }
	}
	LeaveCriticalSection(&mCriticalSection);
	
	return hr;
}


//----------------------------------------

// Generic code
template <class T> void SafeRelease(T **ppT) {
	if (*ppT) {
		(*ppT)->Release();
		*ppT = NULL;
	}
}

/*******************************************************************************
 * Methods to (de)initialize and run the Media Foundation video capture filter *
 ******************************************************************************/


bool_t MSMFoundationCap::isTimeToSend(uint64_t tickerTime){
	return ms_video_capture_new_frame(&mFramerateController, tickerTime);
}

static void ms_mfoundation_init(MSFilter *filter) {
	MSMFoundationCap *mf = new MSMFoundationCap();
	filter->data = mf;
}

static void ms_mfoundation_preprocess(MSFilter *filter) {
	MSMFoundationCap *mf = static_cast<MSMFoundationCap *>(filter->data);
	mf->activate();
	mf->start();
}

static void ms_mfoundation_process(MSFilter *filter) {
	MSMFoundationCap *mf = static_cast<MSMFoundationCap *>(filter->data);
	mf->feed(filter);
}

static void ms_mfoundation_postprocess(MSFilter *filter) {
	MSMFoundationCap *mf = static_cast<MSMFoundationCap *>(filter->data);
	mf->stop();
	mf->deactivate();
}

static void ms_mfoundation_uninit(MSFilter *filter) {
	MSMFoundationCap *mf = static_cast<MSMFoundationCap *>(filter->data);
	delete mf;
}

static int ms_mfoundation_set_fps(MSFilter *filter, void *arg){
	MSMFoundationCap *mf=(MSMFoundationCap*)filter->data;
	mf->setFPS(*(float*)arg);
	return 0;
}

static int ms_mfoundation_get_fps(MSFilter *filter, void *arg){
	MSMFoundationCap *mf=(MSMFoundationCap*)filter->data;
	if (filter->ticker){
		*((float*)arg) = ms_average_fps_get(&mf->mAvgFPS);
	} else {
		*((float*)arg) = mf->getFPS();
	}
	return 0;
}

static int ms_mfoundation_set_pix_fmt(MSFilter *filter, void *arg) {
	MSMFoundationCap *mf = (MSMFoundationCap*)filter->data;
	mf->mPixelFormat = *((MSPixFmt*)arg);
	return 0;
}

static int ms_mfoundation_get_pix_fmt(MSFilter *filter, void *arg) {
	MSMFoundationCap *mf = (MSMFoundationCap*)filter->data;
	if (mf->mPixelFormat == MS_RGB24)
		*((MSPixFmt*)arg) = (MSPixFmt)MS_RGB24_REV;
	else
		*((MSPixFmt*)arg) = (MSPixFmt)mf->mPixelFormat;
	return 0;
}

static int ms_mfoundation_set_vsize(MSFilter *filter, void *arg) {
	MSMFoundationCap *mf = (MSMFoundationCap*)filter->data;
	mf->setVSize(*((MSVideoSize*)arg));
	return 0;
}

static int ms_mfoundation_get_vsize(MSFilter *filter, void *arg) {
	MSMFoundationCap *mf = (MSMFoundationCap*)filter->data;
	MSVideoSize *vs = (MSVideoSize*)arg;
	vs->height = mf->mHeight;
	vs->width = mf->mWidth;
	return 0;
}

static int ms_mfoundation_set_device_orientation(MSFilter *filter, void *arg) {
	MSMFoundationCap *mf = static_cast<MSMFoundationCap *>(filter->data);
	int orientation = *((int *)arg);
	if (mf->getDeviceOrientation() != orientation) {
		mf->setDeviceOrientation(orientation);
	}
	return 0;
}

static MSFilterMethod ms_mfoundation_methods[] = {
	{ MS_FILTER_SET_FPS,        ms_mfoundation_set_fps },
	{ MS_FILTER_GET_FPS,        ms_mfoundation_get_fps },
	{ MS_FILTER_SET_PIX_FMT	,	ms_mfoundation_set_pix_fmt },
	{ MS_FILTER_GET_PIX_FMT	,	ms_mfoundation_get_pix_fmt },
	{ MS_FILTER_GET_VIDEO_SIZE,	ms_mfoundation_get_vsize },
	{ MS_FILTER_SET_VIDEO_SIZE,	ms_mfoundation_set_vsize },
	{ MS_VIDEO_CAPTURE_SET_DEVICE_ORIENTATION,     ms_mfoundation_set_device_orientation },
	{ 0,						NULL }
};

extern "C" MSFilterDesc ms_mfoundation_read_desc = {
	MS_FILTER_PLUGIN_ID,
	"MSMediaFoundationCap",
	N_("Media Foundation video capture."),
	MS_FILTER_OTHER,
	NULL,
	0,
	1,
	ms_mfoundation_init,
	ms_mfoundation_preprocess,
	ms_mfoundation_process,
	ms_mfoundation_postprocess,
	ms_mfoundation_uninit,
	ms_mfoundation_methods
};
MS_FILTER_DESC_EXPORT(ms_mfoundation_read_desc)

// DETECTION
static MSFilter *ms_mfoundationcap_create_reader(MSWebCam *cam) {
	MSFactory *factory = ms_web_cam_get_factory(cam);
	MSFilter *filter = ms_factory_create_filter_from_desc(factory, &ms_mfoundation_read_desc);
	MSMFoundationCap *mf=(MSMFoundationCap*)filter->data;
	mf->setDeviceName(cam->name);
	return filter;
}

static void ms_mfoundationcap_detect(MSWebCamManager *manager);

extern "C" MSWebCamDesc ms_mfoundationcap_desc = {
	"MSMediaFoundationCap",
	ms_mfoundationcap_detect,
	NULL,
	ms_mfoundationcap_create_reader,
	NULL,
	NULL
};


static void ms_mfoundationcap_detect(MSWebCamManager *manager) {
	MFDevices devices;
	if (FAILED(devices.getDevices())) return;
	for (UINT32 i = 0; i < devices.mDevicesCount; ++i) { // Get the human-friendly name of the device
		WCHAR *nameString = NULL;		
		UINT32 cchName; 
		HRESULT hr = devices.mDevices[i]->GetAllocatedString(MF_DEVSOURCE_ATTRIBUTE_FRIENDLY_NAME, &nameString, &cchName);
		if (SUCCEEDED(hr)) {
			size_t inputlen = wcslen(nameString) + 1;
			UINT currentCodePage = GetACP();
			int sizeNeeded = WideCharToMultiByte(currentCodePage, 0, nameString, inputlen, NULL, 0, NULL, NULL);
			std::string strConversion( sizeNeeded, 0 );
			char *nameStr = NULL;
			if(WideCharToMultiByte(currentCodePage, 0, nameString, inputlen, &strConversion[0], sizeNeeded, NULL, NULL)){
				nameStr = (char *)ms_malloc(strConversion.length()+1 );
				strcpy(nameStr, strConversion.c_str());
			}
			if(!nameStr){
				ms_error("MSMediaFoundationCap: Cannot convert webcam name to multi-byte string.");
				ms_free(nameStr);
			}else {
				MSWebCam *cam = ms_web_cam_new(&ms_mfoundationcap_desc);
				cam->name = nameStr;
				ms_web_cam_manager_add_cam(manager, cam);
			}
		}
		CoTaskMemFree(nameString);
	}
	devices.clean();
}


#ifndef IF_EQUAL_RETURN
#define IF_EQUAL_RETURN(param, val) if(val == param) return #val
#endif

const char *MSMFoundationCap::pixFmtToString(const GUID &guid){
	IF_EQUAL_RETURN(guid, MFVideoFormat_AI44); //     FCC('AI44')
    IF_EQUAL_RETURN(guid, MFVideoFormat_ARGB32); //   D3DFMT_A8R8G8B8 
    IF_EQUAL_RETURN(guid, MFVideoFormat_AYUV); //     FCC('AYUV')
    IF_EQUAL_RETURN(guid, MFVideoFormat_DV25); //     FCC('dv25')
    IF_EQUAL_RETURN(guid, MFVideoFormat_DV50); //     FCC('dv50')
    IF_EQUAL_RETURN(guid, MFVideoFormat_DVH1); //     FCC('dvh1')
    IF_EQUAL_RETURN(guid, MFVideoFormat_DVSD); //     FCC('dvsd')
    IF_EQUAL_RETURN(guid, MFVideoFormat_DVSL); //     FCC('dvsl')
    IF_EQUAL_RETURN(guid, MFVideoFormat_H264); //     FCC('H264')
    IF_EQUAL_RETURN(guid, MFVideoFormat_I420); //     FCC('I420')
    IF_EQUAL_RETURN(guid, MFVideoFormat_IYUV); //     FCC('IYUV')
    IF_EQUAL_RETURN(guid, MFVideoFormat_M4S2); //     FCC('M4S2')
    IF_EQUAL_RETURN(guid, MFVideoFormat_MJPG);
    IF_EQUAL_RETURN(guid, MFVideoFormat_MP43); //     FCC('MP43')
    IF_EQUAL_RETURN(guid, MFVideoFormat_MP4S); //     FCC('MP4S')
    IF_EQUAL_RETURN(guid, MFVideoFormat_MP4V); //     FCC('MP4V')
    IF_EQUAL_RETURN(guid, MFVideoFormat_MPG1); //     FCC('MPG1')
    IF_EQUAL_RETURN(guid, MFVideoFormat_MSS1); //     FCC('MSS1')
    IF_EQUAL_RETURN(guid, MFVideoFormat_MSS2); //     FCC('MSS2')
    IF_EQUAL_RETURN(guid, MFVideoFormat_NV11); //     FCC('NV11')
    IF_EQUAL_RETURN(guid, MFVideoFormat_NV12); //     FCC('NV12')
    IF_EQUAL_RETURN(guid, MFVideoFormat_P010); //     FCC('P010')
    IF_EQUAL_RETURN(guid, MFVideoFormat_P016); //     FCC('P016')
    IF_EQUAL_RETURN(guid, MFVideoFormat_P210); //     FCC('P210')
    IF_EQUAL_RETURN(guid, MFVideoFormat_P216); //     FCC('P216')
    IF_EQUAL_RETURN(guid, MFVideoFormat_RGB24); //    D3DFMT_R8G8B8 
    IF_EQUAL_RETURN(guid, MFVideoFormat_RGB32); //    D3DFMT_X8R8G8B8 
    IF_EQUAL_RETURN(guid, MFVideoFormat_RGB555); //   D3DFMT_X1R5G5B5 
    IF_EQUAL_RETURN(guid, MFVideoFormat_RGB565); //   D3DFMT_R5G6B5 
    IF_EQUAL_RETURN(guid, MFVideoFormat_RGB8);
    IF_EQUAL_RETURN(guid, MFVideoFormat_UYVY); //     FCC('UYVY')
    IF_EQUAL_RETURN(guid, MFVideoFormat_v210); //     FCC('v210')
    IF_EQUAL_RETURN(guid, MFVideoFormat_v410); //     FCC('v410')
    IF_EQUAL_RETURN(guid, MFVideoFormat_WMV1); //     FCC('WMV1')
    IF_EQUAL_RETURN(guid, MFVideoFormat_WMV2); //     FCC('WMV2')
    IF_EQUAL_RETURN(guid, MFVideoFormat_WMV3); //     FCC('WMV3')
    IF_EQUAL_RETURN(guid, MFVideoFormat_WVC1); //     FCC('WVC1')
    IF_EQUAL_RETURN(guid, MFVideoFormat_Y210); //     FCC('Y210')
    IF_EQUAL_RETURN(guid, MFVideoFormat_Y216); //     FCC('Y216')
    IF_EQUAL_RETURN(guid, MFVideoFormat_Y410); //     FCC('Y410')
    IF_EQUAL_RETURN(guid, MFVideoFormat_Y416); //     FCC('Y416')
    IF_EQUAL_RETURN(guid, MFVideoFormat_Y41P);
    IF_EQUAL_RETURN(guid, MFVideoFormat_Y41T);
    IF_EQUAL_RETURN(guid, MFVideoFormat_YUY2); //     FCC('YUY2')
    IF_EQUAL_RETURN(guid, MFVideoFormat_YV12); //     FCC('YV12')
    IF_EQUAL_RETURN(guid, MFVideoFormat_YVYU);

	return "Bad format";
}