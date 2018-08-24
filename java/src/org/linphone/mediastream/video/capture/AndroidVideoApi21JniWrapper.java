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
import android.content.Context;
import android.graphics.ImageFormat;
import android.hardware.Camera;
import android.hardware.camera2.CameraDevice;
import android.hardware.camera2.CameraManager;
import android.os.Build;

import org.linphone.mediastream.Log;
import org.linphone.mediastream.MediastreamerAndroidContext;

import java.util.List;

@TargetApi(Build.VERSION_CODES.LOLLIPOP)
public class AndroidVideoApi21JniWrapper {
	static public int detectCameras(int[] indexes, int[] frontFacing, int[] orientation) {
		return AndroidVideoApi5JniWrapper.detectCameras(indexes, frontFacing, orientation);
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
		Context context = MediastreamerAndroidContext.getContext();
		Log.d("Open camera (" + cameraId + ", " + width + ", " + height + ", " + fps + ", " + rotation + ", " + nativePtr + ")");
		try {
			CameraManager manager = (CameraManager) context.getSystemService(Context.CAMERA_SERVICE);
			manager.openCamera(cameraId, cameraStateCallback,null);
		} catch (Exception exc) {
			exc.printStackTrace();
			return null;
		}
		return null;
	}

	public static Object test() {
		/*Camera camera = Camera.open(cameraId);
			Camera.Parameters params = camera.getParameters();


			for (String focusMode : params.getSupportedFocusModes()) {
				if (focusMode.equalsIgnoreCase(Camera.Parameters.FOCUS_MODE_CONTINUOUS_VIDEO)) {
					Log.d("FOCUS_MODE_CONTINUOUS_VIDEO is supported, let's use it");
					params.setFocusMode(Camera.Parameters.FOCUS_MODE_CONTINUOUS_VIDEO);
					break;
				}
			}

			if (params.isVideoStabilizationSupported()) {
				Log.d("Video stabilization is supported, let's use it");
				params.setVideoStabilization(true);
			}

			params.setPreviewSize(width, height);
			int[] chosenFps = findClosestEnclosingFpsRange(fps*1000, params.getSupportedPreviewFpsRange());
			if (chosenFps[0] != chosenFps[1]) {
				// If both values are the same it will most likely break the auto exposure (https://stackoverflow.com/questions/26967490/android-camera-preview-is-dark/28129248#28129248)
				params.setPreviewFpsRange(chosenFps[0], chosenFps[1]);
			}
			camera.setParameters(params);

			int bufferSize = (width * height * ImageFormat.getBitsPerPixel(params.getPreviewFormat())) / 8;
			camera.addCallbackBuffer(new byte[bufferSize]);
			camera.addCallbackBuffer(new byte[bufferSize]);

			camera.setPreviewCallbackWithBuffer(new Camera.PreviewCallback() {
				public void onPreviewFrame(byte[] data, Camera camera) {
					// forward image data to JNI
					if (data == null) {
						// It appears that there is a bug in the camera driver that is asking for a buffer size bigger than it should
						Camera.Parameters params = camera.getParameters();
						Camera.Size size = params.getPreviewSize();
						int bufferSize = (size.width * size.height * ImageFormat.getBitsPerPixel(params.getPreviewFormat())) / 8;
						bufferSize += bufferSize / 20;
						camera.addCallbackBuffer(new byte[bufferSize]);
					} else if (AndroidVideoApi5JniWrapper.isRecording) {
						AndroidVideoApi5JniWrapper.putImage(nativePtr, data);
						camera.addCallbackBuffer(data);
					}
				}
			});

			setCameraDisplayOrientation(rotation, cameraId, camera);
			camera.startPreview();
			AndroidVideoApi5JniWrapper.isRecording = true;
			Log.d("Returning camera object: " + camera);
			return camera;*/
		return null;
	}

	public static void stopRecording(Object cam) {
		AndroidVideoApi5JniWrapper.isRecording = false;
		// STOP
	}

	public static void setPreviewDisplaySurface(Object cam, Object surf) {
		AndroidVideoApi5JniWrapper.setPreviewDisplaySurface(cam, surf);
	}

	private static void setCameraDisplayOrientation(int rotationDegrees, int cameraId, Camera camera) {
		android.hardware.Camera.CameraInfo info = new android.hardware.Camera.CameraInfo();
		android.hardware.Camera.getCameraInfo(cameraId, info);

		int result;
		if (info.facing == Camera.CameraInfo.CAMERA_FACING_FRONT) {
			result = (info.orientation + rotationDegrees) % 360;
			result = (360 - result) % 360; // compensate the mirror
		} else { // back-facing
			result = (info.orientation - rotationDegrees + 360) % 360;
		}

		Log.w("Camera preview orientation: "+ result);
		try {
			camera.setDisplayOrientation(result);
		} catch (Exception exc) {
			Log.e("Failed to execute: camera[" + camera + "].setDisplayOrientation(" + result + ")");
			exc.printStackTrace();
		}
	}

	private static int[] findClosestEnclosingFpsRange(int expectedFps, List<int[]> fpsRanges) {
		Log.d("Searching for closest fps range from " + expectedFps);
		if (fpsRanges == null || fpsRanges.size() == 0) {
			return new int[] { 0, 0 };
		}

		// init with first element
		int[] closestRange = fpsRanges.get(0);
		int measure = Math.abs(closestRange[0] - expectedFps) + Math.abs(closestRange[1] - expectedFps);
		for (int[] curRange : fpsRanges) {
			if (curRange[0] > expectedFps || curRange[1] < expectedFps) continue;
			int curMeasure = Math.abs(curRange[0] - expectedFps) + Math.abs(curRange[1] - expectedFps);
			if (curMeasure < measure && curRange[0] != curRange[1]) { // If both values are the same it will most likely break the auto exposure (https://stackoverflow.com/questions/26967490/android-camera-preview-is-dark/28129248#28129248)
				closestRange = curRange;
				measure = curMeasure;
				Log.d("A better range has been found: w=" + closestRange[0] + ",h=" + closestRange[1]);
			}
		}
		Log.d("The closest fps range is w=" + closestRange[0] + ",h=" + closestRange[1]);
		return closestRange;
	}
}
