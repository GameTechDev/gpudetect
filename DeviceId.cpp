////////////////////////////////////////////////////////////////////////////////
// Copyright 2017-2018 Intel Corporation
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
////////////////////////////////////////////////////////////////////////////////

//
// DeviceId.cpp : Implements the GPU Device detection and graphics settings
//                configuration functions.
//

#include <d3d11.h>
#ifdef _WIN32_WINNT_WIN10
#include <d3d11_3.h>
#endif
#include <intrin.h>

#include "DeviceId.h"
#include "ID3D10Extensions.h"

bool getGraphicsDeviceInfo(IDXGIAdapter* pAdapter,
                           unsigned int* VendorId,
                           unsigned int* DeviceId,
                           unsigned __int64* VideoMemory,
                           std::wstring* Description)
{
    if ((pAdapter == NULL) || (VendorId == NULL) || (DeviceId == NULL) || (VideoMemory == NULL) || (Description == NULL)) {
        return false;
    }

    DXGI_ADAPTER_DESC AdapterDesc = {};
    if (FAILED(pAdapter->GetDesc(&AdapterDesc))) {
        return false;
    }

    *VendorId = AdapterDesc.VendorId;
    *DeviceId = AdapterDesc.DeviceId;
    *Description = AdapterDesc.Description;

    //
    // On GPUs with a Unified Memory Architecture (UMA) like Intel integrated
    // GPUs, the CPU system memory is also used for graphics and there is no
    // dedicated VRAM.  Any memory reported as “dedicated” is really a small
    // pool of system memory reserved by the BIOS for interal use. All normal
    // application allocations (buffers, textures, etc.) are allocated from
    // general system "shared" memory.  For this reason, do not use the
    // dedicated memory size as an indication of UMA GPU capability (either
    // performance, nor memory capacity).
    //
    bool isUMA = false;

    //
    // If CheckFeatureSupport() is not avaialble, we use an assumption that low
    // dedicated memory on an Intel GPU implies it is a UMA integrated GPU.  On
    // Intel integrated GPUs, users or other SW can override this value to
    // anything between 0 and 512MB.
    //
    if (AdapterDesc.VendorId == 0x8086 && AdapterDesc.DedicatedVideoMemory <= 512 * 1024 * 1024) {
        isUMA = true;
    }

#ifdef _WIN32_WINNT_WIN10
    ID3D11Device* pDevice = nullptr;
    if (SUCCEEDED(D3D11CreateDevice(pAdapter, D3D_DRIVER_TYPE_UNKNOWN, NULL, 0, NULL, 0, D3D11_SDK_VERSION, &pDevice, NULL, NULL))) {
        ID3D11Device3* pDevice3 = nullptr;
        if (SUCCEEDED(pDevice->QueryInterface(__uuidof(ID3D11Device3), (void**)&pDevice3))) {
            D3D11_FEATURE_DATA_D3D11_OPTIONS2 FeatureData = {};
            if (SUCCEEDED(pDevice3->CheckFeatureSupport(D3D11_FEATURE_D3D11_OPTIONS2, &FeatureData, sizeof(FeatureData)))) {
                isUMA = FeatureData.UnifiedMemoryArchitecture == TRUE;
            }
            pDevice3->Release();
        }
        pDevice->Release();
    }
#endif // _WIN32_WINNT_WIN10

    if (isUMA) {
        *VideoMemory = (unsigned __int64) AdapterDesc.SharedSystemMemory;
    } else {
        *VideoMemory = (unsigned __int64) AdapterDesc.DedicatedVideoMemory;
    }

    return true;
}

bool getIntelDeviceInfo(IDXGIAdapter* pAdapter, IntelDeviceInfo* pIntelDeviceInfo)
{
    if (pIntelDeviceInfo == NULL) {
        return false;
    }

    //
    // First create the Device, must be SandyBridge or later to create D3D11
    // device.
    //
    ID3D11Device* pDevice = nullptr;
    ID3D11DeviceContext* pDeviceContext = nullptr;
    if (FAILED(D3D11CreateDevice(pAdapter, D3D_DRIVER_TYPE_UNKNOWN, NULL, 0, NULL, 0, D3D11_SDK_VERSION, &pDevice, NULL, &pDeviceContext))) {
        return false;
    }

    //
    // Query the device to find the number of device dependent counters.
    //
    D3D11_COUNTER_INFO counterInfo = {};
    pDevice->CheckCounterInfo( &counterInfo );
    if (counterInfo.LastDeviceDependentCounter == 0) {
        pDevice->Release();
        pDeviceContext->Release();
        return false;
    }

    //
    // Search for the "Intel Device Information" counter and, if found, parse
    // it's description to determine the supported version.
    //
    D3D11_COUNTER_DESC counterDesc = {};
    int intelDeviceInfoVersion = 0;
    bool intelDeviceInfo = false;

    for (int i = D3D11_COUNTER_DEVICE_DEPENDENT_0; i <= counterInfo.LastDeviceDependentCounter; ++i) {
        counterDesc.Counter = static_cast<D3D11_COUNTER>(i);

        D3D11_COUNTER_TYPE counterType;
        UINT uiSlotsRequired = 0;
        UINT uiNameLength = 0;
        UINT uiUnitsLength = 0;
        UINT uiDescLength = 0;
        if (FAILED(pDevice->CheckCounter(&counterDesc, &counterType, &uiSlotsRequired, NULL, &uiNameLength, NULL, &uiUnitsLength, NULL, &uiDescLength))) {
            continue;
        }

        LPSTR sName  = new char[uiNameLength];
        LPSTR sUnits = new char[uiUnitsLength];
        LPSTR sDesc  = new char[uiDescLength];

        intelDeviceInfo =
            SUCCEEDED(pDevice->CheckCounter(&counterDesc, &counterType, &uiSlotsRequired, sName, &uiNameLength, sUnits, &uiUnitsLength, sDesc, &uiDescLength)) &&
            strcmp(sName, "Intel Device Information") == 0;

        if (intelDeviceInfo) {
            sscanf_s(sDesc, "Version %d", &intelDeviceInfoVersion);
        }

        delete[] sName;
        delete[] sUnits;
        delete[] sDesc;

        if (intelDeviceInfo) {
            break;
        }
    }

    //
    // Create the information counter, and query it to get the data. GetData()
    // returns a pointer to the data, not the actual data.
    //
    ID3D11Counter* counter = NULL;
    if (!intelDeviceInfo || FAILED(pDevice->CreateCounter(&counterDesc, &counter))) {
        pDevice->Release();
        pDeviceContext->Release();
        return false;
    }

    pDeviceContext->Begin(counter);
    pDeviceContext->End(counter);

    unsigned __int64 dataAddress = 0;
    if (pDeviceContext->GetData(counter, &dataAddress, sizeof(dataAddress), 0) != S_OK) {
        counter->Release();
        pDevice->Release();
        pDeviceContext->Release();
        return false;
    }

    //
    // Copy the information into the user's structure
    //
    size_t infoSize = intelDeviceInfoVersion == 1 ? 8 : 24;
    memset(pIntelDeviceInfo, 0, sizeof(IntelDeviceInfo));
    memcpy(pIntelDeviceInfo, (void*) dataAddress, infoSize);

    counter->Release();
    pDevice->Release();
    pDeviceContext->Release();
    return true;
}

void getCPUInfo(std::string* cpubrand, std::string* cpuvendor)
{
    // Get extended ids.
    int CPUInfo[4] = { -1 };
    __cpuid(CPUInfo, 0x80000000);
    unsigned int nExIds = CPUInfo[0];

    // Get the information associated with each extended ID.
    char CPUBrandString[0x40] = { 0 };
    char CPUVendorString[0x40] = { 0 };

    __cpuid(CPUInfo, 0);
    memcpy_s(CPUVendorString, sizeof(CPUInfo), &CPUInfo[1], sizeof(int));
    memcpy_s(CPUVendorString + 4, sizeof(CPUInfo), &CPUInfo[3], sizeof(int));
    memcpy_s(CPUVendorString + 8, sizeof(CPUInfo), &CPUInfo[2], sizeof(int));
    *cpuvendor = CPUVendorString;

    for (unsigned int i = 0x80000000; i <= nExIds; ++i)
    {
        __cpuid(CPUInfo, i);

        if (i == 0x80000002)
        {

            memcpy_s(CPUBrandString, sizeof(CPUInfo), CPUInfo, 4 * sizeof(int));
        }
        else if (i == 0x80000003)
        {
            memcpy_s(CPUBrandString + 16, sizeof(CPUInfo), CPUInfo, 4 * sizeof(int));
        }
        else if (i == 0x80000004)
        {
            memcpy_s(CPUBrandString + 32, sizeof(CPUInfo), CPUInfo, 4 * sizeof(int));
        }
    }
    *cpubrand = CPUBrandString;

}

INTEL_GPU_ARCHITECTURE getIntelGPUArchitecture(unsigned int deviceId)
{
    unsigned int idhi = deviceId & 0xFF00;
    unsigned int idlo = deviceId & 0x00F0;

    if (idhi == 0x0100) {
        if (idlo == 0x0050 || idlo == 0x0060) {
            return IGFX_IVYBRIDGE;
        }
        return IGFX_SANDYBRIDGE;
    }

    if (idhi == 0x0400 || idhi == 0x0A00 || idhi == 0x0D00) {
        return IGFX_HASWELL;
    }

    if (idhi == 0x1600) {
        return IGFX_BROADWELL;
    }

    if (idhi == 0x1900) {
        return IGFX_SKYLAKE;
    }

    if (idhi == 0x5900) {
        return IGFX_KABYLAKE;
    }
	
	if( idhi == 0x3100 )
	{
		return IGFX_GEMINILAKE;;
	}

    if (idhi == 0x3E00) {
		if( idlo == 0x00A0 || idlo == 0x00A1 )
		{
			return IGFX_WHISKEYLAKE;
		}
		return IGFX_COFFEELAKE;
    }

	if( idhi == 0x8A00 )
	{
		return IGFX_ICELAKE;
	}

    return IGFX_UNKNOWN;
}

unsigned int checkDxExtensionVersion(IDXGIAdapter* pAdapter)
{
    ID3D11Device* pDevice = nullptr;
    if (FAILED(D3D11CreateDevice(pAdapter, D3D_DRIVER_TYPE_UNKNOWN, NULL, 0, NULL, 0, D3D11_SDK_VERSION, &pDevice, NULL, NULL))) {
        return 0;
    }

    ID3D10::CAPS_EXTENSION intelExtCaps = {};
    if (S_OK != GetExtensionCaps(pDevice, &intelExtCaps)) {
        pDevice->Release();
        return 0;
    }

    pDevice->Release();
    return intelExtCaps.DriverVersion;
}
