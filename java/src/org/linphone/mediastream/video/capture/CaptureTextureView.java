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
package org.linphone.mediastream.video.capture;

import org.linphone.mediastream.Log;

import android.content.Context;
import android.graphics.Matrix;
import android.graphics.RectF;
import android.util.AttributeSet;
import android.view.TextureView;
import android.view.WindowManager;

public class CaptureTextureView extends TextureView {
    protected int mCapturedVideoWidth = 0;
    protected int mCapturedVideoHeight = 0;
    protected int mRotation = 0;
    protected boolean mAlignTopRight = true; // Legacy behavior

    public CaptureTextureView(Context context) {
        this(context, null);
    }

    public CaptureTextureView(Context context, AttributeSet attrs) {
        this(context, attrs, 0);
    }

    public CaptureTextureView(Context context, AttributeSet attrs, int defStyle) {
        super(context, attrs, defStyle);
    }

    public void rotateToMatchDisplayOrientation(int rotation) {
        mRotation = rotation;

        Matrix matrix = new Matrix();
        int width = getMeasuredWidth();
        int height = getMeasuredHeight();
        RectF textureViewRect = new RectF(0, 0, width, height);
        matrix.mapRect(textureViewRect);

        Log.i("[Capture TextureView] Rotating preview texture by " + rotation);
        if (rotation % 180 == 90) {
            float[] src = new float[] { 0.f, 0.f, width, 0.f, 0.f, height, width, height, };
            float[] dst = new float[] { 0.f, height, 0.f, 0.f, width, height, width, 0.f, };
            if (rotation == 270) {
                dst = new float[] { width, 0.f, width, height, 0.f, 0.f, 0.f, height, };
            }
            matrix.setPolyToPoly(src, 0, dst, 0, 4);
        } else if (rotation == 180) {
            matrix.postRotate(180, width / 2, height / 2);
        }

        if (mCapturedVideoWidth != 0 && mCapturedVideoHeight != 0) {
            float ratioWidth = width;
            float ratioHeight = height;

            if (width < height * mCapturedVideoWidth / mCapturedVideoHeight) {
                ratioHeight = width * mCapturedVideoHeight / mCapturedVideoWidth;
            } else {
                ratioWidth = height * mCapturedVideoWidth / mCapturedVideoHeight;
            }

            RectF capturedVideoRect = new RectF(0, 0, ratioWidth, ratioHeight);
            if (mAlignTopRight) {
                capturedVideoRect.offset(width - ratioWidth, 0);
            } else {
                capturedVideoRect.offset(textureViewRect.centerX() - capturedVideoRect.centerX(), textureViewRect.centerY() - capturedVideoRect.centerY());
            }
            Log.i("[Capture TextureView] Scaling from " + width + "x" + height + " to " + ratioWidth + "x" + ratioHeight);

            Matrix addtionalTransform = new Matrix();
            addtionalTransform.setRectToRect(textureViewRect, capturedVideoRect, Matrix.ScaleToFit.FILL);
            matrix.postConcat(addtionalTransform);
        }

        setTransform(matrix);
    }

    public void setAspectRatio(int width, int height) {
        if (width < 0 || height < 0) {
            throw new IllegalArgumentException("Size cannot be negative.");
        }
        
        Log.i("[Capture TextureView] Changing preview texture ratio to match " + width + "x" + height);
        mCapturedVideoWidth = width;
        mCapturedVideoHeight = height;

        rotateToMatchDisplayOrientation(mRotation);
    }

    @Override
    protected void onMeasure(int widthMeasureSpec, int heightMeasureSpec) {
        super.onMeasure(widthMeasureSpec, heightMeasureSpec);

        rotateToMatchDisplayOrientation(mRotation);
    }
}