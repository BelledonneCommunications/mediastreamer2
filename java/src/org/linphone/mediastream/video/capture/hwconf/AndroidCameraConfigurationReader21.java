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
package org.linphone.mediastream.video.capture.hwconf;

import android.annotation.TargetApi;
import android.content.Context;
import android.hardware.Camera;
import android.hardware.camera2.CameraAccessException;
import android.hardware.camera2.CameraCharacteristics;
import android.hardware.camera2.CameraManager;
import android.os.Build;

import org.linphone.mediastream.Log;
import org.linphone.mediastream.MediastreamerAndroidContext;
import org.linphone.mediastream.video.capture.hwconf.AndroidCameraConfiguration.AndroidCamera;

import java.util.ArrayList;
import java.util.List;

/**
 * Android cameras detection, using SDK superior or egal to 21.
 */
public class AndroidCameraConfigurationReader21 {
	@TargetApi(Build.VERSION_CODES.LOLLIPOP)
	static public AndroidCameraConfiguration.AndroidCamera[] probeCameras() {
		Context context = MediastreamerAndroidContext.getContext();
		if (context != null) {
			CameraManager manager = (CameraManager) context.getSystemService(Context.CAMERA_SERVICE);
			int numOfAvailableCameras = 0;
			String[] cameraList;

			try {
				cameraList = manager.getCameraIdList();
				final List<AndroidCamera> cam = new ArrayList<AndroidCamera>(cameraList.length);
				for (int i = 0; i < cameraList.length; i++) {
					String cameraId = cameraList[i];
					CameraCharacteristics characteristics = manager.getCameraCharacteristics(cameraId);
					int camFacing = characteristics.get(CameraCharacteristics.LENS_FACING);
					boolean frontFacing = false;
					if (camFacing == CameraCharacteristics.LENS_FACING_FRONT) {
						frontFacing = true;
					}
					int camOrientation = characteristics.get(CameraCharacteristics.SENSOR_ORIENTATION);

					try {
						Camera c = Camera.open(Integer.valueOf(cameraId));
						cam.add(new AndroidCamera(i, frontFacing, camOrientation, c.getParameters().getSupportedPreviewSizes()));
						c.release();

						numOfAvailableCameras++;
					} catch (RuntimeException e) {
					    Log.e("Cannot open camera " + cameraId + ": " + e.getMessage());
                    }
				}

				AndroidCamera[] result = new AndroidCamera[numOfAvailableCameras];
				result = cam.toArray(result);

				return result;

			} catch (CameraAccessException exp) {
				Log.e(exp);
			}
		}
		return new AndroidCamera[0]; //empty array
	}
}
