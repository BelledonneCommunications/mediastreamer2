/*
AndroidVideoApi21JniWrapper.java
Copyright (C) 2018  Belledonne Communications, Grenoble, France

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
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
*/

package org.linphone.mediastream.video.capture;

import android.annotation.TargetApi;
import android.hardware.camera2.CameraDevice;
import android.os.Build;

import org.linphone.mediastream.Log;

@TargetApi(Build.VERSION_CODES.LOLLIPOP)
public class AndroidVideoApi21JniWrapper {
	static public int detectCameras(int[] indexes, int[] frontFacing, int[] orientation) {
		return AndroidVideoApi9JniWrapper.detectCameras(indexes, frontFacing, orientation);
	}

	/**
	 * Return the hw-available available resolution best matching the requested one.
	 * Best matching meaning :
	 * - try to find the same one
	 * - try to find one just a little bigger (ex: CIF when asked QVGA)
	 * - as a fallback the nearest smaller one
	 * @param cameraId Camera id
	 * @param requestedW Requested video size width
	 * @param requestedH Requested video size height
	 * @return int[width, height] of the chosen resolution, may be null if no
	 * resolution can possibly match the requested one
	 */
	static public int[] selectNearestResolutionAvailable(int cameraId, int requestedW, int requestedH) {
		Log.d("selectNearestResolutionAvailable: " + cameraId + ", " + requestedW + "x" + requestedH);
		return AndroidVideoApi5JniWrapper.selectNearestResolutionAvailableForCamera(cameraId, requestedW, requestedH);
	}

	static private CameraDevice mCameraDevice = null;

	static private CameraDevice.StateCallback cameraStateCallback = new CameraDevice.StateCallback() {
		@Override
		public void onOpened(CameraDevice cameraDevice) {
			Log.d("Mediastreamer Android Camera2: Camera(" + cameraDevice.getId() +") opened ");
			if (mCameraDevice == null) {
				mCameraDevice = cameraDevice;
			}
		}

		@Override
		public void onDisconnected(CameraDevice cameraDevice) {
			if (mCameraDevice == cameraDevice) {
				mCameraDevice = null;
			}
			Log.d("Mediastreamer Android Camera2: Camera(" + cameraDevice.getId() + ") disconnected");
			cameraDevice.close();
		}

		@Override
		public void onError(CameraDevice cameraDevice, int i) {
			Log.d("Mediastreamer Android Camera2: Camera(" + cameraDevice.getId() + ") error");
		}
	};

	public static Object startRecording(int cameraId, int width, int height, int fps, int rotation, final long nativePtr) {
		return AndroidVideoApi9JniWrapper.startRecording(cameraId, width, height, fps, rotation, nativePtr);
	}

	public static void stopRecording(Object cam) {
		AndroidVideoApi9JniWrapper.stopRecording(cam);
	}

	public static void setPreviewDisplaySurface(Object cam, Object surf) {
		AndroidVideoApi9JniWrapper.setPreviewDisplaySurface(cam, surf);
	}
}
