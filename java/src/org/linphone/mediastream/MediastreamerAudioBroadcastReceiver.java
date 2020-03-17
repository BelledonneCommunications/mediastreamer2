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



		return intentFilter;
	}

	@Override
	public void onReceive(Context context, Intent intent) {
		String action = intent.getAction();
		boolean deviceIdNeeded = false;

		if (action.equals(BluetoothAdapter.ACTION_STATE_CHANGED)) {
			int currentState = intent.getIntExtra(BluetoothAdapter.EXTRA_STATE, BluetoothAdapter.ERROR);
			int previousState = intent.getIntExtra(BluetoothAdapter.EXTRA_PREVIOUS_STATE, BluetoothAdapter.ERROR);
			if (
				// ON to OFF transition
				(((previousState == BluetoothAdapter.STATE_ON) || (previousState == BluetoothAdapter.STATE_TURNING_ON)) && ((currentState == BluetoothAdapter.STATE_OFF) || (currentState == BluetoothAdapter.STATE_TURNING_OFF)))
				||
				// OFF to ON transition
				(((previousState == BluetoothAdapter.STATE_OFF) || (previousState == BluetoothAdapter.STATE_TURNING_OFF)) && ((currentState == BluetoothAdapter.STATE_ON) || (currentState == BluetoothAdapter.STATE_TURNING_ON)))
			) {
				Log.i("Bluetooth adapter detected a state change - recomputing device ID");
				deviceIdNeeded = true;
			}

		} else if (action.equals(BluetoothHeadset.ACTION_CONNECTION_STATE_CHANGED)) {
			int currentState = intent.getIntExtra(BluetoothHeadset.EXTRA_STATE, BluetoothHeadset.STATE_AUDIO_DISCONNECTED);
			int previousState = intent.getIntExtra(BluetoothHeadset.EXTRA_PREVIOUS_STATE, BluetoothHeadset.STATE_AUDIO_DISCONNECTED);
			if (
				// ON to OFF transition
				(((previousState == BluetoothHeadset.STATE_CONNECTED) || (previousState == BluetoothHeadset.STATE_CONNECTING)) && ((currentState == BluetoothHeadset.STATE_DISCONNECTED) || (currentState == BluetoothHeadset.STATE_DISCONNECTING)))
				||
				// OFF to ON transition
				(((previousState == BluetoothHeadset.STATE_DISCONNECTED) || (previousState == BluetoothHeadset.STATE_DISCONNECTING)) && ((currentState == BluetoothHeadset.STATE_CONNECTED) || (currentState == BluetoothHeadset.STATE_CONNECTING)))
			) {
				Log.i("Bluetooth headset detected a connection state change - recomputing device ID");
				deviceIdNeeded = true;
			}
		} else if (action.equals(BluetoothHeadset.ACTION_AUDIO_STATE_CHANGED)) {
			int currentState = intent.getIntExtra(BluetoothHeadset.EXTRA_STATE, BluetoothHeadset.STATE_DISCONNECTED);
			int previousState = intent.getIntExtra(BluetoothHeadset.EXTRA_PREVIOUS_STATE, BluetoothHeadset.STATE_DISCONNECTED);
			if (
				// ON to OFF transition
				((previousState == BluetoothHeadset.STATE_AUDIO_CONNECTED) && (currentState == BluetoothHeadset.STATE_AUDIO_DISCONNECTED))
				||
				// OFF to ON transition
				((previousState == BluetoothHeadset.STATE_AUDIO_DISCONNECTED) && (currentState == BluetoothHeadset.STATE_AUDIO_CONNECTED))
			) {
				Log.i("Bluetooth headset detected an audio state change - recomputing device ID");
				deviceIdNeeded = true;
			}
		} else if (action.equals(BluetoothHeadset.ACTION_VENDOR_SPECIFIC_HEADSET_EVENT)) {
			String cmd = intent.getStringExtra(BluetoothHeadset.EXTRA_VENDOR_SPECIFIC_HEADSET_EVENT_CMD);
			String args = intent.getStringExtra(BluetoothHeadset.EXTRA_VENDOR_SPECIFIC_HEADSET_EVENT_ARGS);
			int type = intent.getIntExtra(BluetoothHeadset.EXTRA_VENDOR_SPECIFIC_HEADSET_EVENT_CMD_TYPE, -1);
		} else if (action.equals(AudioManager.ACTION_SCO_AUDIO_STATE_UPDATED)) {
			int currentState = intent.getIntExtra(AudioManager.EXTRA_SCO_AUDIO_STATE, AudioManager.SCO_AUDIO_STATE_DISCONNECTED);
			int previousState = intent.getIntExtra(AudioManager.EXTRA_SCO_AUDIO_PREVIOUS_STATE, AudioManager.SCO_AUDIO_STATE_DISCONNECTED);
			if (
				// ON to OFF transition
				(((previousState == AudioManager.SCO_AUDIO_STATE_CONNECTING) || (previousState == AudioManager.SCO_AUDIO_STATE_CONNECTED)) && (currentState == AudioManager.SCO_AUDIO_STATE_DISCONNECTED))
				||
				// OFF to ON transition
				((previousState == AudioManager.SCO_AUDIO_STATE_DISCONNECTED) && ((currentState == AudioManager.SCO_AUDIO_STATE_CONNECTED) || (currentState == AudioManager.SCO_AUDIO_STATE_CONNECTING)))
			) {
				Log.i("Audio manager detected an audio state change - recomputing device ID");
				deviceIdNeeded = true;
			}
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

