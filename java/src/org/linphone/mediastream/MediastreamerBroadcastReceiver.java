package org.linphone.mediastream;

import android.media.AudioManager;
import android.bluetooth.BluetoothHeadset;
import android.content.Context;
import android.content.BroadcastReceiver;
import android.content.Intent;
import android.content.IntentFilter;

public class MediastreamerBroadcastReceiver extends BroadcastReceiver {
	public MediastreamerBroadcastReceiver() {
		super();
		Log.d("MediastreamerBroadcastReceiver created");
	}

	public IntentFilter getIntentFilter() {
		IntentFilter intentFilter = new IntentFilter();

		intentFilter.addAction(BluetoothHeadset.ACTION_CONNECTION_STATE_CHANGED);
		intentFilter.addAction(BluetoothHeadset.ACTION_AUDIO_STATE_CHANGED);
		intentFilter.addAction(AudioManager.ACTION_SCO_AUDIO_STATE_UPDATED);

		return intentFilter;
	}

	@Override
	public void onReceive(Context context, Intent intent) {
		Log.d("DEBUG onReceive: intent=" + intent);

		String action = intent.getAction();

		Log.d("DEBUG action=" + action);

	}
}

