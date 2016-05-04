// Copyright 2010 Intel Corporation
// All Rights Reserved
//
// Permission is granted to use, copy, distribute and prepare derivative works of this
// software for any purpose and without fee, provided, that the above copyright notice
// and this statement appear in all copies.  Intel makes no representations about the
// suitability of this software for any purpose.  THIS SOFTWARE IS PROVIDED ""AS IS.""
// INTEL SPECIFICALLY DISCLAIMS ALL WARRANTIES, EXPRESS OR IMPLIED, AND ALL LIABILITY,
// INCLUDING CONSEQUENTIAL AND OTHER INDIRECT DAMAGES, FOR THE USE OF THIS SOFTWARE,
// INCLUDING LIABILITY FOR INFRINGEMENT OF ANY PROPRIETARY RIGHTS, AND INCLUDING THE
// WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.  Intel does not
// assume any responsibility for any errors which may appear in this software nor any
// responsibility to update it.

//
// DeviceId.cpp : Implements the GPU Device detection and graphics settings
//                configuration functions.
//

#include <wrl.h>
#include "DeviceId.h"
#include <stdio.h>
#include <string>

#include <InitGuid.h>
#include <D3D11.h>
#include <D3DCommon.h>
#include <DXGI1_4.h>

#include <oleauto.h>
#include <wbemidl.h>
#include <ObjBase.h>

/*****************************************************************************************
 * getGraphicsDeviceInfo
 *
 *     Function to retrieve information about the primary graphics driver. This includes
 *     the device's Vendor ID, Device ID, and available memory.
 *
 *****************************************************************************************/

bool getGraphicsDeviceInfo( unsigned int* VendorId,
							unsigned int* DeviceId,
							unsigned __int64* VideoMemory,
							std::wstring* GFXBrand)
{
	if ((VendorId == NULL) || (DeviceId == NULL)) {
		return false;
	}

	//
	// DXGI is supported on Windows Vista and later. Define a function pointer to the
	// CreateDXGIFactory function. DXGIFactory1 is supported by Windows Store Apps so
	// try that first.
	//
	HMODULE hDXGI = LoadLibrary(L"dxgi.dll");
	if (hDXGI == NULL) {
		return false;
	}

	typedef HRESULT(WINAPI*LPCREATEDXGIFACTORY)(REFIID riid, void** ppFactory);

	LPCREATEDXGIFACTORY pCreateDXGIFactory = (LPCREATEDXGIFACTORY)GetProcAddress(hDXGI, "CreateDXGIFactory1");
	if (pCreateDXGIFactory == NULL) {
		pCreateDXGIFactory = (LPCREATEDXGIFACTORY)GetProcAddress(hDXGI, "CreateDXGIFactory");

		if (pCreateDXGIFactory == NULL) {
			FreeLibrary(hDXGI);
			return false;
		}
	}

	//
	// We have the CreateDXGIFactory function so use it to actually create the factory and enumerate
	// through the adapters. Here, we are specifically looking for the Intel gfx adapter. 
	//
	IDXGIAdapter*     pAdapter;
	IDXGIFactory*     pFactory;
	DXGI_ADAPTER_DESC AdapterDesc;
	if (FAILED((*pCreateDXGIFactory)(__uuidof(IDXGIFactory), (void**)(&pFactory)))) {
		FreeLibrary(hDXGI);
		return false;
	}

	if (FAILED(pFactory->EnumAdapters(0, (IDXGIAdapter**)&pAdapter))) {
		FreeLibrary(hDXGI);
		return false;
	}

	unsigned int intelAdapterIndex = 0;
	while (pFactory->EnumAdapters(intelAdapterIndex, &pAdapter) != DXGI_ERROR_NOT_FOUND) {
		pAdapter->GetDesc(&AdapterDesc);
		if (AdapterDesc.VendorId == 0x8086) {
			break;
		}
		intelAdapterIndex++;
	}

	*VendorId = AdapterDesc.VendorId;
	*DeviceId = AdapterDesc.DeviceId;
	*GFXBrand = AdapterDesc.Description;

	//
	// If we are on Windows 10 then the Adapter3 interface should be available. This is recommended over using
	// the AdapterDesc for getting the amount of memory available.
	//
	IDXGIAdapter3* pAdapter3;
	pAdapter->QueryInterface(IID_IDXGIAdapter3, (void**)&pAdapter3);
	if (pAdapter3) {
		DXGI_QUERY_VIDEO_MEMORY_INFO memInfo;
		pAdapter3->QueryVideoMemoryInfo(intelAdapterIndex, DXGI_MEMORY_SEGMENT_GROUP_LOCAL, &memInfo);
		*VideoMemory = memInfo.AvailableForReservation;
		pAdapter3->Release();
	}
	else {
		*VideoMemory = (unsigned __int64)(AdapterDesc.DedicatedVideoMemory + AdapterDesc.SharedSystemMemory);
	}

	pAdapter->Release();
	FreeLibrary(hDXGI);
	return true;
}

/******************************************************************************************************************************************
 * getIntelDeviceInfo
 *
 * Description:
 *       Gets device information which is stored in a D3D counter. First, a D3D device must be created, the Intel counter located, and
 *       finally queried.
 *
 *       Supported device info: GPU Max Frequency, GPU Min Frequency, GT Generation, EU Count, Package TDP, Max Fill Rate
 * 
 * Parameters:
 *         unsigned int VendorId                         - [in]     - Input:  system's vendor id
 *         IntelDeviceInfoHeader *pIntelDeviceInfoHeader - [in/out] - Input:  allocated IntelDeviceInfoHeader *
 *                                                                    Output: Intel device info header, if found
 *         void *pIntelDeviceInfoBuffer                  - [in/out] - Input:  allocated void *
 *                                                                    Output: IntelDeviceInfoV[#], cast based on IntelDeviceInfoHeader
 * Return:
 *         GGF_SUCCESS: Able to find Data is valid
 *         GGF_E_UNSUPPORTED_HARDWARE: Unsupported hardware, data is invalid
 *         GGF_E_UNSUPPORTED_DRIVER: Unsupported driver on Intel, data is invalid
 *
 *****************************************************************************************************************************************/

long getIntelDeviceInfo( unsigned int VendorId, IntelDeviceInfoHeader *pIntelDeviceInfoHeader, void *pIntelDeviceInfoBuffer )
{
	if ( pIntelDeviceInfoBuffer == NULL ) {
		return GGF_ERROR;
	}

	if ( VendorId != INTEL_VENDOR_ID ) {
		return GGF_E_UNSUPPORTED_HARDWARE;
	}

	//
	// First create the Device, must be SandyBridge or later to create D3D11 device
	//
	Microsoft::WRL::ComPtr<ID3D11Device> pDevice = NULL;
	Microsoft::WRL::ComPtr<ID3D11DeviceContext> pImmediateContext = NULL;
	HRESULT hr = NULL;

	hr = D3D11CreateDevice( NULL, D3D_DRIVER_TYPE_HARDWARE, NULL, NULL, NULL, NULL,
							D3D11_SDK_VERSION, &pDevice, NULL, &pImmediateContext);

	if ( FAILED(hr) )
	{
		printf("D3D11CreateDevice failed\n");

		return GGF_ERROR;
	}
	
	//
	// Query the device to find the number of device dependent counters. If LastDeviceDependentCounter
	// is 0 then there are no device dependent counters supported on this device.
	//
	D3D11_COUNTER_INFO counterInfo;
	int numDependentCounters;

	ZeroMemory( &counterInfo, sizeof(D3D11_COUNTER_INFO) );

	pDevice->CheckCounterInfo( &counterInfo );

	if ( counterInfo.LastDeviceDependentCounter == 0 )
	{
		printf("No device dependent counters\n");

		return GGF_E_UNSUPPORTED_DRIVER;
	}

	numDependentCounters = (counterInfo.LastDeviceDependentCounter - D3D11_COUNTER_DEVICE_DEPENDENT_0) + 1;

	//
	// Since there is at least 1 counter, we search for the apporpriate counter - INTEL_DEVICE_INFO_COUNTERS
	//
	D3D11_COUNTER_DESC pIntelCounterDesc;
	UINT uiSlotsRequired, uiNameLength, uiUnitsLength, uiDescLength;
	LPSTR sName, sUnits, sDesc;

	ZeroMemory(&pIntelCounterDesc, sizeof(D3D11_COUNTER_DESC));
	
	for ( int i = 0; i < numDependentCounters; ++i )
	{
		D3D11_COUNTER_DESC counterDescription;
		D3D11_COUNTER_TYPE counterType;

		counterDescription.Counter = static_cast<D3D11_COUNTER>(i + D3D11_COUNTER_DEVICE_DEPENDENT_0);
		counterDescription.MiscFlags = 0;
		uiSlotsRequired = uiNameLength = uiUnitsLength = uiDescLength = 0;
		sName = sUnits = sDesc = NULL;

		if( SUCCEEDED( hr = pDevice->CheckCounter( &counterDescription, &counterType, &uiSlotsRequired, NULL, &uiNameLength, NULL, &uiUnitsLength, NULL, &uiDescLength ) ) )
		{
			LPSTR sName  = new char[uiNameLength];
			LPSTR sUnits = new char[uiUnitsLength];
			LPSTR sDesc  = new char[uiDescLength];
			
			if( SUCCEEDED( hr = pDevice->CheckCounter( &counterDescription, &counterType, &uiSlotsRequired, sName, &uiNameLength, sUnits, &uiUnitsLength, sDesc, &uiDescLength ) ) )
			{
				if ( strcmp( sName, INTEL_DEVICE_INFO_COUNTERS ) == 0 )
				{
					int IntelCounterMajorVersion = 0;
					int IntelCounterSize = 0;

					pIntelCounterDesc.Counter = counterDescription.Counter;

					sscanf_s( sDesc, "Version %d", &IntelCounterMajorVersion);
					
					if (IntelCounterMajorVersion == 2 )
					{
						IntelCounterSize = sizeof( IntelDeviceInfoV2 );
					}
					else // Fall back to version 1.0
					{
						IntelCounterMajorVersion = 1;
						IntelCounterSize = sizeof( IntelDeviceInfoV1 );
					}

					pIntelDeviceInfoHeader->Version = IntelCounterMajorVersion;
					pIntelDeviceInfoHeader->Size = IntelCounterSize;
				}
			}
			
			delete[] sName;
			delete[] sUnits;
			delete[] sDesc;
		}
	}

	//
	// Check if the Device Info Counter was found. If not, then it means
	// the driver doesn't support this counter.
	//
	if ( pIntelCounterDesc.Counter == NULL )
	{
		printf("Could not find counter\n");

		return GGF_E_UNSUPPORTED_DRIVER;
	}
	
	//
	// Now we know the driver supports the counter we are interested in. So create it and
	// capture the data we want.
	//
	Microsoft::WRL::ComPtr<ID3D11Counter> pIntelCounter = NULL;

	hr = pDevice->CreateCounter(&pIntelCounterDesc, &pIntelCounter);
	if ( FAILED(hr) )
	{
		printf("CreateCounter failed\n");

		return GGF_E_D3D_ERROR;
	}

	pImmediateContext->Begin(pIntelCounter.Get());
	pImmediateContext->End(pIntelCounter.Get());

	hr = pImmediateContext->GetData( pIntelCounter.Get(), NULL, 0, 0 );
	if ( FAILED(hr) || hr == S_FALSE )
	{
		printf("Getdata failed \n");

		return GGF_E_D3D_ERROR;
	}
	
	//
	// GetData will return the address of the data, not the actual data.
	//
	DWORD pData[2] = {0};
	hr = pImmediateContext->GetData(pIntelCounter.Get(), pData, 2 * sizeof(DWORD), 0);
	if ( FAILED(hr) || hr == S_FALSE )
	{
		printf("Getdata failed \n");
		return GGF_E_D3D_ERROR;
	}

	//
	// Copy data to passed in parameter and clean up
	//
	void *pDeviceInfoBuffer = *(void**)pData;
	memcpy( pIntelDeviceInfoBuffer, pDeviceInfoBuffer, pIntelDeviceInfoHeader->Size );

	return GGF_SUCCESS;
}

/*****************************************************************************************
* getCPUInfo
*
*      Parses CPUID output to find the brand and vendor strings.
*
*****************************************************************************************/

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

/*****************************************************************************************
* getGTGeneration
*
*      Returns the generation by parsing the device id. The first two digits of the hex
*      number generally denote the generation. Sandybridge and Ivybridge share the same
*      numbers so they must be further parsed.
*
*      Comparison of the deviceIds (for example to see if a device is more recent than
*      another) does not always work.
*
*****************************************************************************************/

PRODUCT_FAMILY getGTGeneration(unsigned int deviceId)
{
	unsigned int maskedDeviceId = deviceId & 0xFF00;

	//
	// Device is Sandybridge or Ivybridge
	//
	if (maskedDeviceId == 0x0100) {
		if ( 
			((deviceId & 0x0050) == 0x0050) ||
			((deviceId & 0x0060) == 0x0060)
		   ) {
			return IGFX_IVYBRIDGE;
			}
		if (
			((deviceId & 0x0010) == 0x0010) ||
			((deviceId & 0x00F0) == 0x0000)
			) {
			return IGFX_SANDYBRIDGE;
		}
	}

	if (maskedDeviceId == 0x0400 || maskedDeviceId == 0x0A00 || maskedDeviceId == 0x0D00) {
		return IGFX_HASWELL;
	}

	if (maskedDeviceId == 0x1600) {
		return IGFX_BROADWELL;
	}

	if (maskedDeviceId == 0x1900) {
		return IGFX_SKYLAKE;
	}

	return IGFX_UNKNOWN;
}

#include "ID3D10Extensions.h"
UINT checkDxExtensionVersion( )
{
    UINT extensionVersion = 0;
    ID3D10::CAPS_EXTENSION intelExtCaps;
    Microsoft::WRL::ComPtr<ID3D11Device> pDevice = NULL;
	Microsoft::WRL::ComPtr<ID3D11DeviceContext> pImmediateContext = NULL;
	D3D_FEATURE_LEVEL featureLevel;
	HRESULT hr = NULL;

	ZeroMemory( &featureLevel, sizeof(D3D_FEATURE_LEVEL) );

	ID3D11DeviceContext **pContext = &pImmediateContext;

	// First create the Device
	hr = D3D11CreateDevice( NULL, D3D_DRIVER_TYPE_HARDWARE, NULL, NULL, NULL, NULL,
							D3D11_SDK_VERSION, &pDevice, &featureLevel, &pImmediateContext);

	if ( FAILED(hr) )
	{
		printf("D3D11CreateDevice failed\n");
	}
	ZeroMemory( &intelExtCaps, sizeof(ID3D10::CAPS_EXTENSION) );

	if ( pDevice )
	{
		if( S_OK == GetExtensionCaps( pDevice.Get(), &intelExtCaps ) )
		{
			extensionVersion = intelExtCaps.DriverVersion;
		}
	}

	return extensionVersion;
}