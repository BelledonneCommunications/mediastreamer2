/*
 * Copyright (c) 2010-2022 Belledonne Communications SARL.
 *
 * This file is part of mediastreamer2 
 * (see https://gitlab.linphone.org/BC/public/mediastreamer2).
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as
 * published by the Free Software Foundation, either version 3 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */
package org.linphone.mediastream.video.capture;

import java.util.List;

import org.linphone.mediastream.Log;
import org.linphone.mediastream.Version;
import org.linphone.mediastream.video.AndroidVideoWindowImpl;
import org.linphone.mediastream.video.capture.hwconf.AndroidCameraConfiguration;
import org.linphone.mediastream.video.capture.hwconf.AndroidCameraConfiguration.AndroidCamera;

import android.graphics.Canvas;
import android.graphics.Rect;
import android.graphics.SurfaceTexture;
import android.hardware.Camera;
import android.hardware.Camera.Parameters;
import android.view.Surface;
import android.view.SurfaceHolder;
import android.view.SurfaceView;
import android.view.TextureView;

/**
 * Wrapper for Android Camera API. Used by Mediastreamer to record
 * video from webcam.
 * This file depends only on Android SDK superior or egal to 5
 */
public class AndroidVideoApi5JniWrapper {
	public static boolean isRecording = false;

	public static native void putImage(long nativePtr, byte[] buffer);

	static public int detectCamerasCount() {
		AndroidCamera[] cameras = AndroidCameraConfiguration.retrieveCameras();
		return cameras.length;
	}

	static public int detectCameras(int[] indexes, int[] frontFacing, int[] orientation) {
		Log.d("detectCameras\n");
		AndroidCamera[] cameras = AndroidCameraConfiguration.retrieveCameras();

		int nextIndex = 0;
		for (AndroidCamera androidCamera : cameras) {
			if (nextIndex >= indexes.length || nextIndex >= frontFacing.length || nextIndex >= orientation.length) {
				Log.w("Returning only the " + nextIndex + " first cameras (increase buffer size to retrieve all)");
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

	static public void activateAutoFocus(Object cam) {
		Log.d("mediastreamer", "Turning on autofocus on camera " + cam);
		Camera camera = (Camera) cam;
		if (camera != null && (camera.getParameters().getFocusMode() == Parameters.FOCUS_MODE_AUTO || camera.getParameters().getFocusMode() == Parameters.FOCUS_MODE_MACRO))
			camera.autoFocus(null); // We don't need to do anything after the focus finished, so we don't need a callback
	}

	public static Object startRecording(int cameraId, int width, int height, int fps, int rotation, final long nativePtr) {
		Log.d("mediastreamer", "startRecording(" + cameraId + ", " + width + ", " + height + ", " + fps + ", " + rotation + ", " + nativePtr + ")");
		Camera camera = null;
		try{
			camera = Camera.open();

			applyCameraParameters(camera, width, height, fps);

			camera.setPreviewCallback(new Camera.PreviewCallback() {
				public void onPreviewFrame(byte[] data, Camera camera) {
					if (isRecording) {
						// forward image data to JNI
						putImage(nativePtr, data);
					}
				}
			});

			camera.startPreview();
			isRecording = true;
		}catch(Throwable e){
			Log.e("mediastreamer", "AndroidVideoApi5JniWrapper.startRecording(): caught exception " + e);
		}
		Log.d("mediastreamer", "Returning camera object: " + camera);
		return camera;
	}

	public static void stopRecording(Object cam) {
		isRecording = false;
		Log.d("mediastreamer", "stopRecording(" + cam + ")");
		Camera camera = (Camera) cam;

		if (camera != null) {
			try{
				camera.setPreviewCallback(null);
				camera.stopPreview();
				camera.release();
			}catch(Throwable e){
				Log.e("mediastreamer", "AndroidVideoApi5JniWrapper.stopRecording(): caught exception " + e);
			}
		} else {
			Log.i("mediastreamer", "Cannot stop recording ('camera' is null)");
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
				camera.setPreviewDisplay(new SimpleSurfaceHolder((Surface) surf));
			} else {
				AndroidVideoWindowImpl avw = (AndroidVideoWindowImpl) surf;
				camera.setPreviewDisplay(avw.getPreviewSurfaceView().getHolder());
			}
		} catch (Exception exc) {
			Log.e(exc);
			exc.printStackTrace();
		}
	}
	//select nearest resolution equal or above requested, if none, return highest resolution from the supported list
	protected static int[] selectNearestResolutionAvailableForCamera(int id, int requestedW, int requestedH) {
		// inversing resolution since webcams only support landscape ones
		if (requestedH > requestedW) {
			int t = requestedH;
			requestedH = requestedW;
			requestedW = t;
		}

		AndroidCamera[] cameras = AndroidCameraConfiguration.retrieveCameras();
		List<AndroidCamera.Size> supportedSizes = null;
		for(AndroidCamera c: cameras) {
			if (c.id == id)
				supportedSizes = c.resolutions;
		}
		if (supportedSizes == null) {
		Log.e("mediastreamer", "Failed to retrieve supported resolutions.");
			return null;
		}
		Log.i("mediastreamer", supportedSizes.size() + " supported resolutions :");
		for(AndroidCamera.Size s : supportedSizes) {
			Log.i("mediastreamer", "\t" + s.width + "x" + s.height);
		}
		int r[] = null;

		int rW = Math.max(requestedW, requestedH);
		int rH = Math.min(requestedW, requestedH);

		try {
			// look for nearest size
			AndroidCamera.Size result = null;
			int req = rW * rH;
			int minDist = Integer.MAX_VALUE;
			int useDownscale = 0;

			for(AndroidCamera.Size s: supportedSizes) {
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

			if (result == null) {
				/* If we don't have any result then choose the first one that has more pixels */
				minDist = Integer.MAX_VALUE;
				for (AndroidCamera.Size s : supportedSizes) {
					int dist = /*Math.abs*/-1*(req - s.width * s.height);
					if (s.width * s.height > req && dist < minDist) {
						minDist = dist;
						result = s;
					}
				}
			}

			if (result == null) {
				/* If we have still no result then choose the first one */
				result = supportedSizes.get(0);
			}

			r = new int[] {result.width, result.height, useDownscale};
			Log.i("mediastreamer", "resolution selection done (" + r[0] + ", " + r[1] + ", " + r[2] + ")");
			return r;
		} catch (Exception exc) {
			Log.e(exc,"mediastreamer", " resolution selection failed");
			return null;
		}
	}

	protected static void applyCameraParameters(Camera camera, int width, int height, int requestedFps) {
		Parameters params = camera.getParameters();

		params.setPreviewSize(width, height);

		List<Integer> supported = params.getSupportedPreviewFrameRates();
		if (supported != null) {
			int nearest = Integer.MAX_VALUE;
			for(Integer fr: supported) {
				int diff = Math.abs(fr.intValue() - requestedFps);
				if (diff < nearest) {
					nearest = diff;
					params.setPreviewFrameRate(fr.intValue());
				}
			}
			Log.d("mediastreamer", "Preview framerate set:" + params.getPreviewFrameRate());
		}

		camera.setParameters(params);
	}

	private static class SimpleSurfaceHolder implements SurfaceHolder {
		private Surface mSurface;

		public SimpleSurfaceHolder(Surface surface) {
			mSurface = surface;
		}

		@Override public void addCallback(SurfaceHolder.Callback callback) {}
		@Override public Rect getSurfaceFrame() { return null; }
		@Override public boolean isCreating() { return false; }
		@Override public Canvas lockCanvas() { return null; }
		@Override public Canvas lockCanvas(Rect dirty) { return null; }
		@Override public void removeCallback(SurfaceHolder.Callback callback) {}
		@Override public void setFixedSize(int width, int height) {}
		@Override public void setFormat(int format) {}
		@Override public void setKeepScreenOn(boolean screenOn) {}
		@Override public void setSizeFromLayout() {}
		@Override public void setType(int type) {}
		@Override public void unlockCanvasAndPost(Canvas canvas) {}

		@Override
		public Surface getSurface() {
			return mSurface;
		}
	}
}
