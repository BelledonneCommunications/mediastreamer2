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

package org.linphone.mediastream;

import android.media.AudioManager;
import android.bluetooth.BluetoothAdapter;
import android.bluetooth.BluetoothHeadset;
import android.content.Context;
import android.content.BroadcastReceiver;
import android.content.Intent;
import android.content.IntentFilter;

public class MediastreamerAudioBroadcastReceiver extends BroadcastReceiver {

	long contextPtr;

	public MediastreamerAudioBroadcastReceiver() {
		super();
		contextPtr = 0;
		Log.i("MediastreamerAudioBroadcastReceiver created");
	}

	public IntentFilter getIntentFilter() {
		IntentFilter intentFilter = new IntentFilter();

		intentFilter.addAction(BluetoothAdapter.ACTION_STATE_CHANGED);
		intentFilter.addAction(BluetoothHeadset.ACTION_AUDIO_STATE_CHANGED);
		intentFilter.addAction(BluetoothHeadset.ACTION_CONNECTION_STATE_CHANGED);
		intentFilter.addAction(BluetoothHeadset.ACTION_VENDOR_SPECIFIC_HEADSET_EVENT);
		intentFilter.addAction(AudioManager.ACTION_SCO_AUDIO_STATE_UPDATED);
		intentFilter.addAction(AudioManager.ACTION_AUDIO_BECOMING_NOISY);
		intentFilter.addAction(AudioManager.ACTION_HEADSET_PLUG);
		intentFilter.addAction(AudioManager.ACTION_SPEAKERPHONE_STATE_CHANGED);

		return intentFilter;
	}

	public static String BTStateToString(int state) {
		switch(state) {
			case BluetoothHeadset.STATE_AUDIO_DISCONNECTED:
				return "DISCONNECTED";
			case BluetoothHeadset.STATE_AUDIO_CONNECTED:
				return "CONNECTED";
			default:
				return "BT UNKNOWN - ID" + state;
		}
	}


	public static String AudioMgrStateToString(int state) {
		switch(state) {
			case AudioManager.SCO_AUDIO_STATE_DISCONNECTED:
				return "DISCONNECTED";
			case AudioManager.SCO_AUDIO_STATE_CONNECTED:
				return "CONNECTED";
			case AudioManager.SCO_AUDIO_STATE_CONNECTING:
				return "CONNECTING";
			default:
				return "Audio Mgr UNKNOWN - ID" + state;
		}
	}

	// Only trigger a change in the output device when the current state is either CONNECTED or DISCONNECTED
	// Transitional states are ignored because it bring about a race condition between when the new device is searched and if the initialization has already occurred
	// No issues have been noticed for the built-in devices (speaker and earpiece) but an external device such as bluetooth speaker has a longer initialization sequence hence a transitional state may lead to an unwanted outcome
	@Override
	public void onReceive(Context context, Intent intent) {
		String action = intent.getAction();
		boolean deviceIdNeeded = false;

		if (action.equals(BluetoothAdapter.ACTION_STATE_CHANGED)) {
			int currentState = intent.getIntExtra(BluetoothAdapter.EXTRA_STATE, BluetoothAdapter.ERROR);
			int previousState = intent.getIntExtra(BluetoothAdapter.EXTRA_PREVIOUS_STATE, BluetoothAdapter.ERROR);
			if (
				((currentState == BluetoothAdapter.STATE_OFF) || (currentState == BluetoothAdapter.STATE_ON))
			) {
				Log.i("Bluetooth adapter detected a state change - recomputing device ID");
				deviceIdNeeded = true;
			}

		} else if (action.equals(BluetoothHeadset.ACTION_CONNECTION_STATE_CHANGED)) {
			int currentState = intent.getIntExtra(BluetoothHeadset.EXTRA_STATE, BluetoothHeadset.STATE_AUDIO_DISCONNECTED);
			int previousState = intent.getIntExtra(BluetoothHeadset.EXTRA_PREVIOUS_STATE, BluetoothHeadset.STATE_AUDIO_DISCONNECTED);
			if (
				((currentState == BluetoothHeadset.STATE_CONNECTED) || (currentState == BluetoothHeadset.STATE_DISCONNECTED))
			) {
				Log.i("Bluetooth headset detected a connection state change - recomputing device ID");
				deviceIdNeeded = true;
			}
		} else if (action.equals(BluetoothHeadset.ACTION_AUDIO_STATE_CHANGED)) {
			int currentState = intent.getIntExtra(BluetoothHeadset.EXTRA_STATE, BluetoothHeadset.STATE_DISCONNECTED);
			int previousState = intent.getIntExtra(BluetoothHeadset.EXTRA_PREVIOUS_STATE, BluetoothHeadset.STATE_DISCONNECTED);
			Log.i("DEBUG BT current state: " + BTStateToString(currentState) + " previous state " + BTStateToString(previousState));
			if (
				((currentState == BluetoothHeadset.STATE_AUDIO_CONNECTED) || (currentState == BluetoothHeadset.STATE_AUDIO_DISCONNECTED))
			) {
				Log.i("Bluetooth headset detected an audio state change - recomputing device ID");
				deviceIdNeeded = true;
			}
		} else if (action.equals(BluetoothHeadset.ACTION_VENDOR_SPECIFIC_HEADSET_EVENT)) {
			String cmd = intent.getStringExtra(BluetoothHeadset.EXTRA_VENDOR_SPECIFIC_HEADSET_EVENT_CMD);
			String args = intent.getStringExtra(BluetoothHeadset.EXTRA_VENDOR_SPECIFIC_HEADSET_EVENT_ARGS);
			int type = intent.getIntExtra(BluetoothHeadset.EXTRA_VENDOR_SPECIFIC_HEADSET_EVENT_CMD_TYPE, -1);
			Log.i("Bluetooth headset vendor specific command:  type " + type + " cmd " + cmd + " args " + args);
		} else if (action.equals(AudioManager.ACTION_SCO_AUDIO_STATE_UPDATED)) {
			int currentState = intent.getIntExtra(AudioManager.EXTRA_SCO_AUDIO_STATE, AudioManager.SCO_AUDIO_STATE_DISCONNECTED);
			int previousState = intent.getIntExtra(AudioManager.EXTRA_SCO_AUDIO_PREVIOUS_STATE, AudioManager.SCO_AUDIO_STATE_DISCONNECTED);
			Log.i("DEBUG Audio Manager BT current state: " + AudioMgrStateToString(currentState) + " previous state " + AudioMgrStateToString(previousState));
			if (
				((currentState == AudioManager.SCO_AUDIO_STATE_DISCONNECTED) || (currentState == AudioManager.SCO_AUDIO_STATE_CONNECTED))
			) {
				Log.i("Audio manager detected an audio state change - recomputing device ID");
				deviceIdNeeded = true;
			}
		} else if (action.equals(AudioManager.ACTION_SPEAKERPHONE_STATE_CHANGED)) {
			Log.i("Audio manager detected a change in the speakerphone state - recomputing device ID");
			deviceIdNeeded = true;
		} else if (action.equals(AudioManager.ACTION_HEADSET_PLUG)) {
			Log.i("Audio manager detected an headset being plugged or unplugged - recomputing device ID");
			deviceIdNeeded = true;
		} else if (action.equals(AudioManager.ACTION_AUDIO_BECOMING_NOISY)) {
			Log.i("Audio manager detected audio becoming noisy - recomputing device ID");
			deviceIdNeeded = true;
		} else {
		    Log.w("[Mediastreamer Broadcast Receiver] Bluetooth unknown action: " + action);
		}

		if (deviceIdNeeded == true) {
			requestUpdateDeviceId(contextPtr);
		}

	}

	public void setContextPtr(long ptr) {
		Log.i("[MediastreamerAudioBroadcastReceiver] Set context pointer to " + ptr);
		contextPtr = ptr;
	}

	public static native void requestUpdateDeviceId(long ptr);

}

