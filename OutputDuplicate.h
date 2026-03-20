#pragma once
#include <windows.h>
#include <stdio.h>
#include <dxgi1_2.h>
#include <d3d11.h>
#include <functional>

#pragma comment(lib, "DXGI.lib")
#pragma comment(lib, "d3d11.lib")


class DesktopDuplicator
{
public:
	// callbacks
	// captures a specific frame and allows to process it in the main code
	using FrameProcessorCallback = std::function<void(BYTE* pData, UINT width, UINT height, UINT rowPitch)>;

private:
	ID3D11Device* pDevice = nullptr;
	ID3D11DeviceContext* pContext = nullptr;
	IDXGIOutputDuplication* pDuplication = nullptr;

	// Staging Texture object
	ID3D11Texture2D* pStagingTexture = nullptr;

	// Helper to create the staging texture once
	HRESULT CreateStagingTexture(ID3D11Texture2D* pBaseTexture)
	{
		if (pStagingTexture) return S_OK; // Already created!

		D3D11_TEXTURE2D_DESC desc;
		pBaseTexture->GetDesc(&desc);

		desc.Usage = D3D11_USAGE_STAGING;
		desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
		desc.BindFlags = 0;
		desc.MiscFlags = 0;

		return pDevice->CreateTexture2D(&desc, nullptr, &pStagingTexture);
	}

	// variables for snapshot
	FrameProcessorCallback m_pendingCallback = nullptr;
	bool m_snapshotRequested = false;

public:
	// pass the sepcific adapter and output you want to capture
	HRESULT Initialize(IDXGIAdapter1* pAdapter, IDXGIOutput1* pOutput1)
	{
		HRESULT hr;

		// create the Direct3D 11 device using the specific Adapter
		// device must be created on the same GPU to which the motinor is connected to
		D3D_FEATURE_LEVEL featureLevel;
		hr = D3D11CreateDevice(
			pAdapter,
			D3D_DRIVER_TYPE_UNKNOWN, // Must be UNKNOWN when passing a specific adapter
			nullptr,
			0, // No special flags for now
			nullptr, 0, // Default feature levels
			D3D11_SDK_VERSION,
			&pDevice,
			&featureLevel,
			&pContext);

		if (FAILED(hr))
		{
			printf("Failed to create D3D11 Device.\n");
			return hr;
		}

		// initialize the desktop duplication API, which creates the
		// pDuplication object acting as the camera pointing at the screen
		hr = pOutput1->DuplicateOutput(pDevice, &pDuplication);

		if (FAILED(hr))
		{
			if (hr == DXGI_ERROR_NOT_CURRENTLY_AVAILABLE)
			{
				printf("Error: Max number of duplication apps running, or fullscreen app is blocking it.\n");
			}
			else
			{
				printf("Failed to create Desktop Duplication.\n");
			}
			return hr;
		}

		printf("Successfully initialized Desktop Duplication!\n");
		return S_OK;
	}

	// the function called from outside to trigger the image save
	void RequestSnapshot(FrameProcessorCallback callback)
	{
		m_pendingCallback = callback;
		m_snapshotRequested = true;
	}

	// capture loop method
	void CaptureFrame()
	{
		if (!pDuplication) return;

		IDXGIResource* pDesktopResource = nullptr;
		DXGI_OUTDUPL_FRAME_INFO frameInfo;

		// wait for the next frame (timeout after 500 miliseconds)
		HRESULT hr = pDuplication->AcquireNextFrame(500, &frameInfo, &pDesktopResource);

		if (hr == DXGI_ERROR_WAIT_TIMEOUT)
		{
			// the screen hasn't changed in 500ms, so Windows didn't bother generating a new frame.
			// this is normal, just loop again!
			printf("Timeout: Screen didn't update.\n");
			return;
		}
		else if (FAILED(hr))
		{
			// if access is lost (e.g., resolution changed, UAC prompt, pressing Ctrl+Alt+Del)
			// you must release pDuplication and re-initialize it.
			printf("Failed to acquire frame or access lost.\n");
			return;
		}

		// we have the frame, pDesktopResource now contains the raw GPU texture
		// stored in the GPU VRAM
		//printf("Successfully grabbed a frame! (Accumulated Frames: %u)\n", frameInfo.AccumulatedFrames);

		ID3D11Texture2D* pTexture = nullptr;
		hr = pDesktopResource->QueryInterface(__uuidof(ID3D11Texture2D), (void**)&pTexture);

		if (SUCCEEDED(hr))
		{
			// ==========================================================
			// PATH A: THE VIDEO STREAM (Runs every single frame)
			// ==========================================================
			// Because we are planning to use Intel Quick Sync, we DO NOT 
			// map the texture here. We will eventually just pass 'pTexture' 
			// directly into the encoder. 
			// e.g., MyEncoder.EncodeFrame(pTexture);


			// ==========================================================
			// PATH B: THE SNAPSHOT (Runs ONLY when requested)
			// ==========================================================
			// This block only triggers if the user specifically asked for a screenshot.
			if (m_snapshotRequested && m_pendingCallback)
			{
				// 1. Initialize the staging texture
				if (pStagingTexture == nullptr)
				{
					CreateStagingTexture(pTexture);
				}

				// 2. Do the expensive hardware copy ONLY for this specific frame
				if (pStagingTexture != nullptr)
				{
					pContext->CopyResource(pStagingTexture, pTexture);

					// 3. Map the memory to CPU space
					D3D11_MAPPED_SUBRESOURCE mapped;
					if (SUCCEEDED(pContext->Map(pStagingTexture, 0, D3D11_MAP_READ, 0, &mapped)))
					{
						BYTE* pRawBytes = static_cast<BYTE*>(mapped.pData);

						D3D11_TEXTURE2D_DESC desc;
						pStagingTexture->GetDesc(&desc);

						// 4. Fire the callback to the main program
						m_pendingCallback(pRawBytes, desc.Width, desc.Height, mapped.RowPitch);

						// 5. Unlock the texture so the GPU can use it again
						pContext->Unmap(pStagingTexture, 0);
					}
				}

				// Reset the request so we don't accidentally capture the next frame too
				m_snapshotRequested = false;
				m_pendingCallback = nullptr;
			}

			// Clean up the base texture pointer
			pTexture->Release();
		}

		// release the resources
		pDesktopResource->Release();

		// you must release the frame to tell Windows you are done looking at the screen
		// otherwise AcquireNextFrame will fail next time
		pDuplication->ReleaseFrame();
	}

	// destructor body
	~DesktopDuplicator()
	{
		if (pStagingTexture) pStagingTexture->Release();
		if (pDuplication) pDuplication->Release();
		if (pContext) pContext->Release();
		if (pDevice) pDevice->Release();
	}
};
