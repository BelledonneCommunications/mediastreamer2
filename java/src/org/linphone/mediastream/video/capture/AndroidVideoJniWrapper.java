/*
AndroidVideoJniWrapper.java
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

import android.graphics.ImageFormat;
import android.graphics.SurfaceTexture;
import android.hardware.Camera;
import android.view.Surface;
import android.view.SurfaceView;
import android.view.TextureView;

import org.linphone.mediastream.Log;
import org.linphone.mediastream.Version;
import org.linphone.mediastream.video.AndroidVideoWindowImpl;
import org.linphone.mediastream.video.capture.hwconf.AndroidCameraConfiguration;

import java.util.List;

public class AndroidVideoJniWrapper {
	public static boolean isRecording = false;

	public static native void putImage(long nativePtr, byte[] buffer);

	static public void activateAutoFocus(Object cam) {
		Log.d("mediastreamer", "Turning on autofocus on camera " + cam);
		Camera camera = (Camera) cam;
		if (camera != null && (camera.getParameters().getFocusMode() == Camera.Parameters.FOCUS_MODE_AUTO || camera.getParameters().getFocusMode() == Camera.Parameters.FOCUS_MODE_MACRO))
			camera.autoFocus(null); // We don't need to do anything after the focus finished, so we don't need a callback
	}

	static public int detectCameras(int[] indexes, int[] frontFacing, int[] orientation) {
		Log.d("detectCameras\n");
		AndroidCameraConfiguration.AndroidCamera[] cameras = AndroidCameraConfiguration.retrieveCameras();

		int nextIndex = 0;
		for (AndroidCameraConfiguration.AndroidCamera androidCamera : cameras) {
			if (nextIndex == 2) {
				Log.w("Returning only the 2 first cameras (increase buffer size to retrieve all)");
				break;
			}
			// skip already added cameras
			indexes[nextIndex] = androidCamera.id;
			frontFacing[nextIndex] = androidCamera.frontFacing?1:0;
			orientation[nextIndex] = androidCamera.orientation;
			nextIndex++;
		}
		return nextIndex;
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
		Log.d("mediastreamer", "selectNearestResolutionAvailable: " + cameraId + ", " + requestedW + "x" + requestedH);

		return selectNearestResolutionAvailableForCamera(cameraId, requestedW, requestedH);
	}

	public static Object startRecording(int cameraId, int width, int height, int fps, int rotation, final long nativePtr) {
		Log.d("startRecording(" + cameraId + ", " + width + ", " + height + ", " + fps + ", " + rotation + ", " + nativePtr + ")");
		try {
			Camera camera = Camera.open(cameraId);
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
			int[] chosenFps = findClosestEnclosingFpsRange(fps * 1000, params.getSupportedPreviewFpsRange());
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
					} else if (AndroidVideoJniWrapper.isRecording) {
						AndroidVideoJniWrapper.putImage(nativePtr, data);
						camera.addCallbackBuffer(data);
					}
				}
			});

			setCameraDisplayOrientation(rotation, cameraId, camera);
			camera.startPreview();
			AndroidVideoJniWrapper.isRecording = true;
			Log.d("Returning camera object: " + camera);
			return camera;
		} catch (Exception exc) {
			exc.printStackTrace();
			return null;
		}
	}

	public static void stopRecording(Object cam) {
		AndroidVideoJniWrapper.isRecording = false;
		Log.d("stopRecording(" + cam + ")");
		Camera camera = (Camera) cam;

		if (camera != null) {
			camera.setPreviewCallbackWithBuffer(null);
			camera.stopPreview();
			camera.release();
		} else {
			Log.i("Cannot stop recording ('camera' is null)");
		}
	}

	public static void setPreviewDisplaySurface(Object cam, Object surf) {
		Log.d("mediastreamer", "setPreviewDisplaySurface(" + cam + ", " + surf + ")");
		Camera camera = (Camera) cam;
		try {
			if (surf instanceof SurfaceView) {
				SurfaceView surface = (SurfaceView) surf;
				camera.setPreviewDisplay(surface.getHolder());
			} else if (surf instanceof TextureView && ((TextureView) surf).isAvailable()) {
				camera.setPreviewTexture(((TextureView) surf).getSurfaceTexture());
			} else if (surf instanceof SurfaceTexture) {
				camera.setPreviewTexture((SurfaceTexture) surf);
			} else if (surf instanceof Surface) {

			} else {
				AndroidVideoWindowImpl avw = (AndroidVideoWindowImpl) surf;
				camera.setPreviewDisplay(avw.getPreviewSurfaceView().getHolder());
			}
		} catch (Exception exc) {
			Log.e(exc);
			exc.printStackTrace();
		}
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

	//select nearest resolution equal or above requested, if none, return highest resolution from the supported list
	protected static int[] selectNearestResolutionAvailableForCamera(int id, int requestedW, int requestedH) {
		// inversing resolution since webcams only support landscape ones
		if (requestedH > requestedW) {
			int t = requestedH;
			requestedH = requestedW;
			requestedW = t;
		}

		AndroidCameraConfiguration.AndroidCamera[] cameras = AndroidCameraConfiguration.retrieveCameras();
		List<AndroidCameraConfiguration.AndroidCamera.Size> supportedSizes = null;
		for(AndroidCameraConfiguration.AndroidCamera c: cameras) {
			if (c.id == id)
				supportedSizes = c.resolutions;
		}
		if (supportedSizes == null) {
			Log.e("mediastreamer", "Failed to retrieve supported resolutions.");
			return null;
		}
		Log.i("mediastreamer", supportedSizes.size() + " supported resolutions :");
		for(AndroidCameraConfiguration.AndroidCamera.Size s : supportedSizes) {
			Log.i("mediastreamer", "\t" + s.width + "x" + s.height);
		}
		int r[] = null;

		int rW = Math.max(requestedW, requestedH);
		int rH = Math.min(requestedW, requestedH);

		try {
			// look for nearest size
			AndroidCameraConfiguration.AndroidCamera.Size result = supportedSizes.get(0); /*by default return first value*/
			int req = rW * rH;
			int minDist = Integer.MAX_VALUE;
			int useDownscale = 0;
			for(AndroidCameraConfiguration.AndroidCamera.Size s: supportedSizes) {
				int dist = /*Math.abs*/-1*(req - s.width * s.height);
				if ( ((s.width >= rW && s.height >= rH) || (s.width >= rH && s.height >= rW)) && dist < minDist) {
					minDist = dist;
					result = s;
					useDownscale = 0;
				}

				/* MS2 has a NEON downscaler, so we test this too */
				int downScaleDist = /*Math.abs*/-1*(req - s.width * s.height / 4);
				if (((s.width/2 >= rW && s.height/2 >= rH) || (s.width/2 >= rH && s.height/2 >= rW)) && downScaleDist < minDist) {
					if (Version.hasFastCpuWithAsmOptim()) {
						minDist = downScaleDist;
						result = s;
						useDownscale = 1;
					} else {
						result = s;
						useDownscale = 0;
					}
				}
				if (s.width == rW && s.height == rH) {
					result = s;
					useDownscale = 0;
					break;
				}
			}
			r = new int[] {result.width, result.height, useDownscale};
			Log.i("mediastreamer", "resolution selection done (" + r[0] + ", " + r[1] + ", " + r[2] + ")");
			return r;
		} catch (Exception exc) {
			Log.e(exc,"mediastreamer", " resolution selection failed");
			return null;
		}
	}
}
