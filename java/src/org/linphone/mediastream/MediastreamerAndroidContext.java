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
package org.linphone.mediastream;

import android.annotation.TargetApi;
import android.content.Context;
import android.content.pm.PackageManager;
import android.content.IntentFilter;
import java.io.File;
import android.media.AudioManager;
import android.media.AudioDeviceInfo;
import android.os.Build;

import org.linphone.mediastream.MediastreamerAudioBroadcastReceiver;

public class MediastreamerAndroidContext {
	private static final int DEVICE_CHOICE = 0; // The device has the API to tell us it as or not a builtin AEC and we can trust it

	private static Context mContext;
	private static int mDeviceFavoriteSampleRate = 44100;
	private static int mDeviceFavoriteBufferSize = 256;

	private static boolean loadOptionalLibrary(String s) {
		android.util.Log.w("MediastreamerAndroidContext", "Try to load optional library " + s);
		try {
			System.loadLibrary(s);
			return true;
		} catch (Throwable e) {
			android.util.Log.w("MediastreamerAndroidContext", "Unable to load optional library " + s + ": " +e.getMessage());
		}
		return false;
	}

	static {
		// Load libmsaaudio only if it is available - i.e. ignore exception thrown by the JAva VM
		loadOptionalLibrary("msaaudio");
	}

	private MediastreamerAndroidContext() {
	}

	private static MediastreamerAndroidContext instance;

	private static MediastreamerAndroidContext getInstance() {
		if (instance == null)
			instance = new MediastreamerAndroidContext();
		return instance;
	}

	public static Context getContext() {
		return mContext;
	}

	public static int getDeviceFavoriteSampleRate() {
		return mDeviceFavoriteSampleRate;
	}

	public static int getDeviceFavoriteBufferSize() {
		return mDeviceFavoriteBufferSize;
	}

	public static String getNativeLibrariesDirectory() {
		String nativeLibDir = getContext().getApplicationInfo().nativeLibraryDir;

		File directory = new File(nativeLibDir);
		File[] nativeLibs = directory.listFiles();

		if (nativeLibs == null || nativeLibs.length == 0) {
			// This scenario is for app bundle mode packaging, expected path to be like
			// /data/app/org.linphone.debug-2/split_config.armeabi_v7a.apk!/lib/armeabi-v7a
			Log.w("Native library directory is empty, using path to native libs for app bundle mode");
			nativeLibDir = nativeLibDir.substring(0, nativeLibDir.indexOf("/lib"));
			nativeLibDir += "/split_config." + Build.SUPPORTED_ABIS[0].replace("-", "_") + ".apk!/lib/" + Build.SUPPORTED_ABIS[0];
		}

		return nativeLibDir;
	}

	private static MediastreamerAudioBroadcastReceiver mMediastreamerReceiver;

	private static  void deleteBroadcastReceiver(Context currContext) {
		Log.i("[Mediastreamer Android Context] Unegistering mediastreamer receiver");
		if (mMediastreamerReceiver != null) {
			currContext.unregisterReceiver(mMediastreamerReceiver);
			mMediastreamerReceiver = null;
		}

	}

	private static MediastreamerAudioBroadcastReceiver startBroadcastReceiver(Context currContext) {

		deleteBroadcastReceiver(currContext);

		if (mMediastreamerReceiver == null) {
			Log.i("[Mediastreamer Android Context] Registering mediastreamer receiver");
			mMediastreamerReceiver = new MediastreamerAudioBroadcastReceiver();
			IntentFilter filter = mMediastreamerReceiver.getIntentFilter();
			currContext.registerReceiver(mMediastreamerReceiver, filter);
		}

		return mMediastreamerReceiver;

	}

	public static MediastreamerAudioBroadcastReceiver getAudioBroadcastReceiver() {
		return mMediastreamerReceiver;
	}

	@TargetApi(Build.VERSION_CODES.KITKAT)
	public static void setContext(Object c) {
		if (c == null)
			return;

		mContext = (Context)c;

		boolean hasLowLatencyFeature = mContext.getPackageManager().hasSystemFeature(PackageManager.FEATURE_AUDIO_LOW_LATENCY);
		boolean hasProFeature = mContext.getPackageManager().hasSystemFeature(PackageManager.FEATURE_AUDIO_PRO);
		Log.i("[Device] hasLowLatencyFeature: " + hasLowLatencyFeature + ", hasProFeature: " + hasProFeature);

		int bufferSize = 256;
		int sampleRate = 44100;
		MediastreamerAndroidContext mac = getInstance();
		// When using the OpenSLES sound card, the system is capable of giving us the best values to use for the buffer size and the sample rate
		if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.KITKAT)
		{
			AudioManager audiomanager = (AudioManager)mContext.getSystemService(Context.AUDIO_SERVICE);

			String bufferProperty = audiomanager.getProperty(AudioManager.PROPERTY_OUTPUT_FRAMES_PER_BUFFER);
			bufferSize = parseInt(bufferProperty, bufferSize);
			String sampleRateProperty = audiomanager.getProperty(AudioManager.PROPERTY_OUTPUT_SAMPLE_RATE);
			sampleRate = parseInt(sampleRateProperty, sampleRate);
			Log.i("[Device] Output frames per buffer: " + bufferSize + ", output sample rate: " + sampleRate + ".");
			mDeviceFavoriteSampleRate = sampleRate;
			mDeviceFavoriteBufferSize = bufferSize;
			MediastreamerAudioBroadcastReceiver rec = startBroadcastReceiver(mContext);
		} else {
			Log.i("Android < 4.4 detected, android context not used.");
		}
	}

	public static boolean getSpeakerphoneAlwaysOn(org.linphone.mediastream.Factory factory) {
		//For special devices, speakerphone always on
		return ((factory.getDeviceFlags() & Factory.DEVICE_USE_ANDROID_CAMCORDER)!=0);
	}

	private static int parseInt(String value, int defaultValue) {
		int returnedValue = defaultValue;
		try {
			returnedValue = Integer.parseInt(value);
		} catch (NumberFormatException nfe) {
			Log.e("Can't parse " + value + " to integer ; using default value " + defaultValue);
		}
		return returnedValue;
	}
	native private int enableFilterFromNameImpl(String name,boolean enable);
	native private boolean filterFromNameEnabledImpl(String name);
	/**
	 * Specifies if a filter is enabled or not.
	 * @param name   A name for the filter. refer to ms2 internals to get list of filters
	 * @param enable, true/false
	 * @throws MediastreamException if filter name is unknown
	 * @deprecated
	 * */
	public static void enableFilterFromName(String name,boolean enable) throws MediastreamException {
		if (getInstance().enableFilterFromNameImpl(name,enable) != 0)
			throw new MediastreamException("Cannot "+(enable?"enable":"disable") + " filter  name ["+name+"]");
	}
	/**
	 * Specifies if a filter is enabled or not.
	 * @param name   A name for the filter. refer to ms2 internals to get list of filters
	 * @return enable, true/false
	 * @deprecated
	 * */
	public static boolean filterFromNameEnabled(String name) {
		return getInstance().filterFromNameEnabledImpl(name);
	}

	/* Device ID */

	public static int[] fillDeviceIdArray(final AudioDeviceInfo[] devices, final int[] type) {
		int[] id = new int[type.length];
		for (int idx = 0; idx < id.length; idx++) {
			// Initialize device ID to -1
			id[idx] = -1;
		}
		for (AudioDeviceInfo device : devices) {
			for (int idx = 0; idx < type.length; idx++) {
				if (device.getType() == type[idx]) {
					id[idx] = device.getId();
				}
			}
		}

		return id;
	}

	// Choose device type for the player based on the streamType
	public static int[] fillPlayerDeviceTypeArray(final String streamType) {
		int numDeviceType = 2;
		int[] type = new int[numDeviceType];
		for (int idx = 0; idx < type.length; idx++) {
			// Initialize device type to unknown
			type[idx] = AudioDeviceInfo.TYPE_UNKNOWN;
		}

		switch (streamType) {
			case "VOICE":
				type[0] = AudioDeviceInfo.TYPE_BLUETOOTH_SCO;
				type[1] = AudioDeviceInfo.TYPE_BUILTIN_EARPIECE;
				break;
			case "RING":
				type[0] = AudioDeviceInfo.TYPE_BLUETOOTH_SCO;
				// Speaker specifically tuned for outputting sounds like notifiations and alarms
				type[1] = AudioDeviceInfo.TYPE_BUILTIN_SPEAKER;
				// type[1] = AudioDeviceInfo.TYPE_BUILTIN_SPEAKER_SAFE;
				break;
			case "MEDIA":
				type[0] = AudioDeviceInfo.TYPE_BLUETOOTH_SCO;
				type[1] = AudioDeviceInfo.TYPE_BUILTIN_SPEAKER;
				break;
			case "DTMF":
				type[0] = AudioDeviceInfo.TYPE_BUILTIN_SPEAKER;
				// type[1] = AudioDeviceInfo.TYPE_BUILTIN_SPEAKER_SAFE;
				type[1] = AudioDeviceInfo.TYPE_BUILTIN_EARPIECE;
				break;
			default:
				Log.e("[Audio Manager] [Get Player Device ID] Invalid stream type " + streamType);
				break;
		}

		return type;
	}

	public static int chooseDeviceId(final AudioDeviceInfo[] devices, final int[] type) {

		// fill devince id array based on device list and device type
		final int[] deviceID = fillDeviceIdArray(devices, type);

		int preferredDeviceID = -1;

		// Devices are sorted by preference, therefore going through the list in reverse order in
		// order to store the preferred one as last
		for (int idx = (deviceID.length - 1); idx >= 0; idx--) {
			int ID = deviceID[idx];
			if (ID != -1) {
				preferredDeviceID = ID;
			}
		}

		return preferredDeviceID;
	}

	public static int getDefaultPlayerDeviceId(final String streamType) {
		AudioManager audiomanager = (AudioManager)getContext().getSystemService(Context.AUDIO_SERVICE);
		final AudioDeviceInfo[] devices = audiomanager.getDevices(AudioManager.GET_DEVICES_OUTPUTS);
		final int[] deviceType = fillPlayerDeviceTypeArray(streamType);
		final int deviceId = chooseDeviceId(devices, deviceType);

		Log.i("[Audio Manager] [Get Device ID] stream type " + streamType + " device ID " + deviceId);

		return deviceId;
	}

	public static boolean isAudioRoutedToBluetooth() {
		AudioManager audiomanager = (AudioManager)getContext().getSystemService(Context.AUDIO_SERVICE);
		return audiomanager.isBluetoothScoOn();
	}

	public static  boolean isAudioRoutedToSpeaker() {
		AudioManager audiomanager = (AudioManager)getContext().getSystemService(Context.AUDIO_SERVICE);
		return audiomanager.isSpeakerphoneOn() && !isAudioRoutedToBluetooth();
	}

	public static boolean isAudioRoutedToEarpiece() {
		AudioManager audiomanager = (AudioManager)getContext().getSystemService(Context.AUDIO_SERVICE);
		return audiomanager.isSpeakerphoneOn() && !isAudioRoutedToBluetooth();
	}

	public static int getOutputAudioDeviceId() {

		int[] deviceType = new int[1];

		if (isAudioRoutedToSpeaker()) {
			deviceType[0] = AudioDeviceInfo.TYPE_BUILTIN_SPEAKER;
		} else if (isAudioRoutedToEarpiece()) {
			deviceType[0] = AudioDeviceInfo.TYPE_BUILTIN_EARPIECE;
		} else if (isAudioRoutedToBluetooth()) {
			deviceType[0] = AudioDeviceInfo.TYPE_BLUETOOTH_SCO;
		} else {
			Log.e("[Audio Manager] Unknown Audio routing");
			deviceType[0] = AudioDeviceInfo.TYPE_UNKNOWN;
		}

		AudioManager audiomanager = (AudioManager)getContext().getSystemService(Context.AUDIO_SERVICE);
		final AudioDeviceInfo[] devices = audiomanager.getDevices(AudioManager.GET_DEVICES_OUTPUTS);
		final int deviceId = chooseDeviceId(devices, deviceType);

		return deviceId;
	}

}
