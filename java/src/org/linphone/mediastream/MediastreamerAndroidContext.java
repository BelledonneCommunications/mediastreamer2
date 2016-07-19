/*
AndroidContext.java
Copyright (C) 2014  Belledonne Communications, Grenoble, France

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
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
*/
package org.linphone.mediastream;

import android.annotation.TargetApi;
import android.content.Context;
import android.media.AudioManager;
import android.os.Build;

public class MediastreamerAndroidContext {
	private static final int DEVICE_CHOICE = 0; // The device has the API to tell us it as or not a builtin AEC and we can trust it
	private static final int DEVICE_HAS_BUILTIN_AEC  = 1; // Says the device has a builtin AEC because the API that would tell us that isn't available
	private static final int DEVICE_HAS_BUILTIN_AEC_CRAPPY = 2; // The device has the API to tell us it has a builtin AEC but we shouldn't trust it (so we'll use software AEC)
	private static final int DEVICE_USE_ANDROID_MIC = 4;
	private static final int DEVICE_HAS_BUILTIN_OPENSLES_AEC = 8; // The device has a builtin AEC and it is working with OpenSLES (which is uncommon)
	
	private native void setDeviceFavoriteSampleRate(int samplerate);
	private native void setDeviceFavoriteBufferSize(int bufferSize);
	private native void addSoundDeviceDescription(String manufacturer, String model, String platform, int flags, int delay, int recommended_rate);
	
	private MediastreamerAndroidContext() {
		
	}
	
	private static MediastreamerAndroidContext instance;
	
	private static MediastreamerAndroidContext getInstance() {
		if (instance == null)
			instance = new MediastreamerAndroidContext();
		return instance;
	}
	
	public static void addSoundDeviceDesc(String manufacturer, String model, String platform, int flags, int delay, int recommended_rate) {
		getInstance().addSoundDeviceDescription(manufacturer, model, platform, flags, delay, recommended_rate);
	}
	
	@TargetApi(Build.VERSION_CODES.KITKAT)
	public static void setContext(Object c) {
		if (c == null)
			return;
		
		Context context = (Context)c;
		int bufferSize = 64;
		int sampleRate = 44100;
		MediastreamerAndroidContext mac = getInstance();
		// When using the OpenSLES sound card, the system is capable of giving us the best values to use for the buffer size and the sample rate
		if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.KITKAT)
		{
			AudioManager audiomanager = (AudioManager)context.getSystemService(Context.AUDIO_SERVICE);
			String bufferProperty = audiomanager.getProperty(AudioManager.PROPERTY_OUTPUT_FRAMES_PER_BUFFER);
			bufferSize = parseInt(bufferProperty, bufferSize);
			String sampleRateProperty = audiomanager.getProperty(AudioManager.PROPERTY_OUTPUT_SAMPLE_RATE);
			sampleRate = parseInt(sampleRateProperty, sampleRate);
			Log.i("Setting buffer size to " + bufferSize + " and sample rate to " + sampleRate + " for OpenSLES MS sound card.");
			mac.setDeviceFavoriteSampleRate(sampleRate);
			mac.setDeviceFavoriteBufferSize(bufferSize);
		} else {
			Log.i("Android < 4.4 detected, android context not used.");
		}
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
	 * @throw MediastreamException if filter name is unknown
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
	 ** */
	public static boolean filterFromNameEnabled(String name) {
		return getInstance().filterFromNameEnabledImpl(name);
	}
}
