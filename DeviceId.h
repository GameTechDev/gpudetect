/////////////////////////////////////////////////////////////////////////////////////////////
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
/////////////////////////////////////////////////////////////////////////////////////////////

//
// DeviceId.h
//
#pragma once

#include <dxgi.h>
#include <string>

enum {
    INTEL_VENDOR_ID = 0x8086,
};

enum INTEL_GPU_ARCHITECTURE {
    IGFX_UNKNOWN     = 0x00,
    IGFX_SANDYBRIDGE = 0x0C,
    IGFX_IVYBRIDGE,
    IGFX_HASWELL,
    IGFX_VALLEYVIEW,
    IGFX_BROADWELL,
    IGFX_CHERRYVIEW,
    IGFX_SKYLAKE,
    IGFX_KABYLAKE,
    IGFX_COFFEELAKE,
	IGFX_GEMINILAKE  = 0x17,
	IGFX_WHISKEYLAKE, // Has no LP code assigned, so this is using an unused enum slot.
	IGFX_ICELAKE     = 0x1D
};

struct IntelDeviceInfo {
    DWORD GPUMaxFreq;
    DWORD GPUMinFreq;
    DWORD GPUArchitecture;   // INTEL_GPU_ARCHITECTURE
    DWORD EUCount;
    DWORD PackageTDP;
    DWORD MaxFillRate;
};

/*******************************************************************************
 * getGraphicsDeviceInfo
 *
 *     Query the specified adapter's Vendor ID, Device ID, the amount of memory
 *     availble for graphics (VideoMemory), and the GPU's Description.
 *
 ******************************************************************************/
bool getGraphicsDeviceInfo(IDXGIAdapter* pAdapter, unsigned int* VendorId, unsigned int* DeviceId, unsigned __int64* VideoMemory, std::wstring* Description);

/*******************************************************************************
 * getIntelDeviceInfo
 *
 *     Query the specified adapter for IntelDeviceInfo.  Some GPUs and drivers
 *     may only return GPUMaxFreq and GPUMinFreq (with all other entries left
 *     zero).
 *
 ******************************************************************************/
bool getIntelDeviceInfo(IDXGIAdapter* pAdapter, IntelDeviceInfo* pIntelDeviceInfo);


/*******************************************************************************
 * checkDxExtensionVersion
 *
 *      Returns the Intel DirectX Extension version supported by the specified
 *      adapter. 0 indicates that no Intel DirectX Extensions are supported.
 *      EXTENSION_INTERFACE_VERSION_1_0 supports extensions for pixel
 *      synchronization and instant access of graphics memory.
 *
 ******************************************************************************/
unsigned int checkDxExtensionVersion(IDXGIAdapter* pAdapter);

/*******************************************************************************
 * getIntelGPUArchitecture
 *
 *      Returns the architecture of an Intel GPU by parsing the device id.  It
 *      assumes that it is indeed an Intel GPU device ID (i.e., that VendorID
 *      was INTEL_VENDOR_ID).
 *
 *      You cannot generally compare device IDs to compare architectures; for
 *      example, a newer architecture may have an lower deviceID.
 *
 ******************************************************************************/
INTEL_GPU_ARCHITECTURE getIntelGPUArchitecture(unsigned int deviceId);

/*******************************************************************************
 * getCPUInfo
 *
 *      Parses CPUID output to find the brand and vendor strings.
 *
 ******************************************************************************/
void getCPUInfo(std::string* cpubrand, std::string* cpuvendor);

