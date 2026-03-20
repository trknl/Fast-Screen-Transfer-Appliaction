#pragma once
#include <windows.h>
#include <stdio.h>
#include <dxgi1_2.h>
#include <vector>

#pragma comment(lib, "DXGI.lib")

class DXGIEnumerator
{
public:
	std::vector<IDXGIAdapter1*> vAdapters;
	std::vector<std::vector<IDXGIOutput1*>> vOutputs;

	// constructor body
	DXGIEnumerator()
	{
		if (SUCCEEDED(enumerateAdapters()))
		{
			printAdapters();
			if (SUCCEEDED(enumerateOutputs(vAdapters)))
			{
				printOutputs();
			}
		}
	}

	// destructor body
	~DXGIEnumerator()
	{
		// release all Output interfaces
		for (size_t i = 0; i < vOutputs.size(); i++)
		{
			for (size_t j = 0; j < vOutputs[i].size(); j++)
			{
				if (vOutputs[i][j] != nullptr)
				{
					vOutputs[i][j]->Release();
					vOutputs[i][j] = nullptr;
				}
			}
		}

		// release all Adapter interfaces
		for (size_t i = 0; i < vAdapters.size(); i++)
		{
			if (vAdapters[i] != nullptr)
			{
				vAdapters[i]->Release();
				vAdapters[i] = nullptr;
			}
		}
	}


private:

	HRESULT enumerateAdapters()
	{
		// create factory
		IDXGIFactory1* pFactory = nullptr;
		HRESULT hr = CreateDXGIFactory1(__uuidof(IDXGIFactory1), (void**)&pFactory);
		if (FAILED(hr))
		{
			printf("Failed to create IDXGIFactory1.\n");
			return hr; // Return the exact error code
		}

		// enumerate adapters
		UINT i = 0;
		IDXGIAdapter1* pAdapter = nullptr;

		while ((hr = pFactory->EnumAdapters1(i, &pAdapter)) != DXGI_ERROR_NOT_FOUND)
		{
			if (SUCCEEDED(hr))
			{
				vAdapters.push_back(pAdapter);
			}
			i++;
		}

		pFactory->Release();
		return S_OK; // Success
	}

	void printAdapters()
	{
		// printing number of adapters found
		printf("Successfully found %zu video cards (Adapters)\n\n", vAdapters.size());

		// loop through the vector of adapter pointers
		for (size_t i = 0; i < vAdapters.size(); i++)
		{
			// create an empty description structure
			DXGI_ADAPTER_DESC adapterDesc;

			// ask the adapter to fill it out
			if (SUCCEEDED(vAdapters[i]->GetDesc(&adapterDesc)))
			{
				// The GPU's name is stored as a "Wide String" (WCHAR), so we use %ls instead of %s
				printf("Adapter %zu: %ls\n", i, adapterDesc.Description);

				// VRAM is given in raw Bytes. We convert it to Megabytes.
				SIZE_T vramMB = adapterDesc.DedicatedVideoMemory / (1024 * 1024);
				printf("Dedicated VRAM: %zu MB\n", vramMB);

				// You can also check shared system memory
				SIZE_T sharedRAM = adapterDesc.SharedSystemMemory / (1024 * 1024);
				printf("Shared System RAM: %zu MB\n\n", sharedRAM);
			}
			else
			{
				printf("Failed to get description for Adapter %zu\n\n", i);
			}
		}
	}

	HRESULT enumerateOutputs(const std::vector<IDXGIAdapter1*>& adapters)
	{
		// using the class member 'vOutputs', no local variable defined
		vOutputs.clear();
		vOutputs.resize(adapters.size());

		HRESULT hr;

		// loop through the available adapters
		for (size_t i = 0; i < adapters.size(); i++)
		{
			// current target adapter
			IDXGIAdapter1* pTargetAdapter = adapters[i];
			IDXGIOutput* pBaseOutput = nullptr;
			UINT j = 0;

			// EnumOutputs increments the ref count of pBaseOutput
			while ((hr = pTargetAdapter->EnumOutputs(j, &pBaseOutput)) != DXGI_ERROR_NOT_FOUND)
			{
				if (hr == DXGI_ERROR_INVALID_CALL)
				{
					printf("Error: pOutput parameter is NULL...\n\n");
					break;
				}

				IDXGIOutput1* pOutput1 = nullptr;

				// Ask the base output for the version 1 interface
				if (SUCCEEDED(pBaseOutput->QueryInterface(__uuidof(IDXGIOutput1), (void**)&pOutput1)))
				{
					// Store the upgraded interface (Destructor will release it later)
					vOutputs[i].push_back(pOutput1);
				}

				// We MUST release the base output right now, because we only care about pOutput1
				pBaseOutput->Release();
				j++;
			}
		}
		return S_OK;
	}

	void printOutputs()
	{
		printf("Outputs (monitors) attached to adapters:\n\n");

		// Loop through vector of vectors of outputs
		for (size_t i = 0; i < vOutputs.size(); i++)
		{
			printf("Found %zu outputs attached to Adapter %zu\n", vOutputs[i].size(), i);

			// loop through the specific monitors for this adapter
			for (size_t j = 0; j < vOutputs[i].size(); j++)
			{
				// create an empty output description structure
				DXGI_OUTPUT_DESC outputDesc;

				// asking the current specific output to fill it out
				if (SUCCEEDED(vOutputs[i][j]->GetDesc(&outputDesc)))
				{
					// print the name of the monitor
					printf("   Output %zu: %ls\n", j, outputDesc.DeviceName);

					// extract the Desktop Coordinates
					LONG left = outputDesc.DesktopCoordinates.left;
					LONG right = outputDesc.DesktopCoordinates.right;
					LONG top = outputDesc.DesktopCoordinates.top;
					LONG bottom = outputDesc.DesktopCoordinates.bottom;

					// calculate actual resolution
					LONG width = right - left;
					LONG height = bottom - top;

					printf("   Resolution: %ld x %ld\n", width, height);
					printf("   Coordinates: Left:%ld, Top:%ld, Right:%ld, Bottom:%ld\n\n", left, top, right, bottom);
				}
				else
				{
					printf("  Failed to get description for Output %zu\n\n", j);
				}
			}
		}

	}
};

