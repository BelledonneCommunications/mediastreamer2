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

import org.linphone.mediastream.Version;

public class AndroidVideoJniWrapper {

	public static native void putImage(long nativePtr, byte[] buffer);

	public static void activateAutoFocus(Object cam) {
		AndroidVideoApi5JniWrapper.activateAutoFocus(cam);
	}

	public static int detectCameras(int[] indexes, int[] frontFacing, int[] orientation) {
		if (Version.sdk() >= 21) {
			return AndroidVideoApi21JniWrapper.detectCameras(indexes, frontFacing, orientation);
		} else if (Version.sdk() >= 9) {
			return AndroidVideoApi9JniWrapper.detectCameras(indexes, frontFacing, orientation);
		} else if (Version.sdk() >= 8) {
			return AndroidVideoApi8JniWrapper.detectCameras(indexes, frontFacing, orientation);
		} else {
			return AndroidVideoApi5JniWrapper.detectCameras(indexes, frontFacing, orientation);
		}
	}

	public static int[] selectNearestResolutionAvailable(int cameraId, int requestedW, int requestedH) {
		if (Version.sdk() >= 21) {
			return AndroidVideoApi21JniWrapper.selectNearestResolutionAvailable(cameraId, requestedW, requestedH);
		} else if (Version.sdk() >= 9) {
			return AndroidVideoApi9JniWrapper.selectNearestResolutionAvailable(cameraId, requestedW, requestedH);
		} else if (Version.sdk() >= 8) {
			return AndroidVideoApi8JniWrapper.selectNearestResolutionAvailable(cameraId, requestedW, requestedH);
		} else {
			return AndroidVideoApi5JniWrapper.selectNearestResolutionAvailable(cameraId, requestedW, requestedH);
		}
	}

	public static Object startRecording(int cameraId, int width, int height, int fps, int rotation, final long nativePtr) {
		if (Version.sdk() >= 21) {
			return AndroidVideoApi21JniWrapper.startRecording(cameraId, width, height, fps, rotation, nativePtr);
		} else if (Version.sdk() >= 9) {
			return AndroidVideoApi9JniWrapper.startRecording(cameraId, width, height, fps, rotation, nativePtr);
		} else if (Version.sdk() >= 8) {
			return AndroidVideoApi8JniWrapper.startRecording(cameraId, width, height, fps, rotation, nativePtr);
		} else {
			return AndroidVideoApi5JniWrapper.startRecording(cameraId, width, height, fps, rotation, nativePtr);
		}
	}

	public static void stopRecording(Object cam) {
		if (Version.sdk() >= 21) {
			AndroidVideoApi21JniWrapper.stopRecording(cam);
		} else if (Version.sdk() >= 9) {
			AndroidVideoApi9JniWrapper.stopRecording(cam);
		} else if (Version.sdk() >= 8) {
			AndroidVideoApi8JniWrapper.stopRecording(cam);
		} else {
			AndroidVideoApi5JniWrapper.stopRecording(cam);
		}
	}

	public static void setPreviewDisplaySurface(Object cam, Object surf) {
		if (Version.sdk() >= 21) {
			AndroidVideoApi21JniWrapper.setPreviewDisplaySurface(cam, surf);
		} else if (Version.sdk() >= 9) {
			AndroidVideoApi9JniWrapper.setPreviewDisplaySurface(cam, surf);
		} else if (Version.sdk() >= 8) {
			AndroidVideoApi8JniWrapper.setPreviewDisplaySurface(cam, surf);
		} else {
			AndroidVideoApi5JniWrapper.setPreviewDisplaySurface(cam, surf);
		}
	}
}
