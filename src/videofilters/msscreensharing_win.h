/*
 * Copyright (c) 2010-2024 Belledonne Communications SARL.
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

#ifndef MS_SHARED_SCREEN_WIN_H_
#define MS_SHARED_SCREEN_WIN_H_

#include "mediastreamer2/msasync.h"
#include "msscreensharing_private.h"

#include <d3d11.h>
#include <d3d11_1.h>
#include <dxgi1_2.h>
#include <vector>

class MsScreenSharing_win : public MsScreenSharing {
public:
	MsScreenSharing_win();
	virtual ~MsScreenSharing_win();
	MsScreenSharing_win(const MsScreenSharing_win &) = delete;
	virtual void setSource(MSScreenSharingDesc sourceDesc, FormatData formatData) override;

	virtual void init() override;
	virtual void uninit() override;
	bool initDisplay();
	void clean();
	virtual void stopProcess() override;

	virtual void getWindowSize(int *windowX, int *windowY, int *windowWidth, int *windowHeight) const override;
	virtual bool prepareImage() override;
	virtual void finalizeImage() override;

	class Processing {
	public:
		Processing(MsScreenSharing_win *parent) : mParent(parent) {
		}
		virtual ~Processing() {
		}
		virtual bool initDisplay() = 0;
		virtual bool prepareImage() = 0;
		virtual void finalizeImage() = 0;
		virtual void clean() = 0;
		virtual void stopProcess() {
		}

		MsScreenSharing_win *mParent;
	};
	// API : Screen Duplication
	class ScreenProcessor : public Processing {
	public:
		ScreenProcessor(MsScreenSharing_win *parent) : Processing(parent) {
		}
		virtual ~ScreenProcessor();
		virtual bool initDisplay() override;
		virtual bool prepareImage() override;
		virtual void finalizeImage() override;
		virtual void clean() override;

		ID3D11Device *mDevice = nullptr;
		ID3D11DeviceContext *mImmediateContext = nullptr; // Associate to device

		class ScreenDuplication {
		public:
			ScreenDuplication(IDXGIOutputDuplication *duplication,
			                  ID3D11Texture2D *drawingImage,
			                  ID3D11Texture2D *destImage,
			                  DXGI_OUTPUT_DESC description,
			                  DXGI_OUTDUPL_DESC imageDescription)
			    : mDuplication(duplication), mDrawingImage(drawingImage), mDestImage(destImage),
			      mDescription(description), mImageDescription(imageDescription) {
			}

			IDXGIOutputDuplication *mDuplication;
			ID3D11Texture2D *mDrawingImage;
			ID3D11Texture2D *mDestImage;
			DXGI_OUTPUT_DESC mDescription;
			DXGI_OUTDUPL_DESC mImageDescription;
		};
		std::vector<ScreenDuplication> mScreenDuplications;
	};

	// API: PrintWindow
	class WindowProcessor : public Processing {
	public:
		WindowProcessor(MsScreenSharing_win *parent) : Processing(parent) {
		}
		virtual ~WindowProcessor();
		virtual bool initDisplay() override;
		virtual bool prepareImage() override;
		virtual void finalizeImage() override;
		virtual void clean() override;
		virtual void stopProcess() override;

		HBITMAP mHBitmap = NULL;

		class PrintWindowAsync {
		public:
			PrintWindowAsync();
			bool start(HWND windowId, HDC hDC);

			MSTask *mTask = nullptr;
			static MSWorkerThread *gWorkerThread;
			static void destroyWorkerThread();
		};
		PrintWindowAsync mPrintWindowAsync;
	};

	Processing *mProcess = nullptr;

	HWND mWindowId;

	unsigned int frame_count;
};

#endif
