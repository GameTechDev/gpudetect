
#include <d3d11.h>
#ifdef _WIN32_WINNT_WIN10
#include <d3d11_3.h>
#endif
#include <intrin.h>

#include "GPUDetect.h"

#define MAX_KEY_LENGTH 255

// This should only be needed for reading data from the counter
struct IntelDeviceInfo
{
	DWORD GPUMaxFreq;
	DWORD GPUMinFreq;
	DWORD GPUArchitecture;   // INTEL_GPU_ARCHITECTURE
	DWORD EUCount;
	DWORD PackageTDP;
	DWORD MaxFillRate;
};

namespace GPUDetect
{
	// Returns 0 if successfully initialized
	int InitAdapter( struct GPUData* const gpuData, IDXGIAdapter** pAdapter, int adapterIndex = -1 );
	int GetIntelDeviceInfo( IntelDeviceInfo* deviceInfo, IDXGIAdapter* pAdapter );
}

int GPUDetect::InitExtensionInfo(  struct GPUData* const gpuData, int adapterIndex /* = -1 */ )
{
#ifdef GPUDETECT_CHECK_PRECONDITIONS
	// Check preconditions
	if( !( adapterIndex >= -1 ) )
	{
		return GPUDETECT_ERROR_BAD_DATA;
	}
#endif

	if( adapterIndex != -1 )
	{
		gpuData->adapterIndex = adapterIndex;
	}
	
	IDXGIAdapter* pAdapter = nullptr;
	int adapterReturnCode = InitAdapter( gpuData, &pAdapter, adapterIndex );
	if( adapterReturnCode != EXIT_SUCCESS )
	{
		return adapterReturnCode;
	}

	DXGI_ADAPTER_DESC AdapterDesc = {};
	if( FAILED( pAdapter->GetDesc( &AdapterDesc ) ) )
	{
		pAdapter->Release();
		return GPUDETECT_ERROR_DXGI_ADAPTER_CREATION;
	}

	gpuData->vendorID = AdapterDesc.VendorId;
	gpuData->deviceID = AdapterDesc.DeviceId;
	wcscpy_s( gpuData->description, 128, AdapterDesc.Description );
	gpuData->architectureCounter = getIntelGPUArchitecture( gpuData->deviceID );

	if( AdapterDesc.VendorId == 0x8086 && AdapterDesc.DedicatedVideoMemory <= 512 * 1024 * 1024 )
	{
		gpuData->isUMAArchitecture = true;
	}

#ifdef _WIN32_WINNT_WIN10
	ID3D11Device* pDevice = nullptr;
	if( SUCCEEDED( D3D11CreateDevice( pAdapter, D3D_DRIVER_TYPE_UNKNOWN, NULL, 0, NULL, 0, D3D11_SDK_VERSION, &pDevice, NULL, NULL ) ) )
	{
		ID3D11Device3* pDevice3 = nullptr;
		if( SUCCEEDED( pDevice->QueryInterface( __uuidof( ID3D11Device3 ), (void**) &pDevice3 ) ) )
		{
			D3D11_FEATURE_DATA_D3D11_OPTIONS2 FeatureData = {};
			if( SUCCEEDED( pDevice3->CheckFeatureSupport( D3D11_FEATURE_D3D11_OPTIONS2, &FeatureData, sizeof( FeatureData ) ) ) )
			{
				gpuData->isUMAArchitecture = FeatureData.UnifiedMemoryArchitecture == TRUE;
			}
			pDevice3->Release();
		}
		pDevice->Release();
	}
#endif // _WIN32_WINNT_WIN10

	if( gpuData->isUMAArchitecture )
	{
		gpuData->videoMemory = (unsigned __int64) AdapterDesc.SharedSystemMemory;
	}
	else
	{
		gpuData->videoMemory = (unsigned __int64) AdapterDesc.DedicatedVideoMemory;
	}

	if( !FAILED( D3D11CreateDevice( pAdapter, D3D_DRIVER_TYPE_UNKNOWN, NULL, 0, NULL, 0, D3D11_SDK_VERSION, &pDevice, NULL, NULL ) ) )
	{
		ID3D10::CAPS_EXTENSION intelExtCaps = {};
		if( S_OK == GetExtensionCaps( pDevice, &intelExtCaps ) )
		{
			gpuData->extentionVersion = intelExtCaps.DriverVersion;
		}
		pDevice->Release();

		gpuData->intelExtentionAvalibility = ( gpuData->extentionVersion >= ID3D10::EXTENSION_INTERFACE_VERSION_1_0 );
	}

	gpuData->dxAdapterAvalibility = true;
	pAdapter->Release();

	return EXIT_SUCCESS;
}

int GPUDetect::InitCounterInfo( GPUData* const gpuData, int adapterIndex /* = -1 */ )
{
#ifdef GPUDETECT_CHECK_PRECONDITIONS
	// Check preconditions
	if( !( gpuData->vendorID != NULL && adapterIndex > -2 ) )
	{
		return GPUDETECT_ERROR_BAD_DATA;
	}
#endif

	if( adapterIndex != -1 )
	{
		gpuData->adapterIndex = adapterIndex;
	}

	IDXGIAdapter* pAdapter = nullptr;
	InitAdapter( gpuData, &pAdapter, gpuData->adapterIndex );

	//
	// In DirectX, Intel exposes additional information through the driver that can be obtained
	// querying a special DX counter
	//
	gpuData->counterAvalibility = gpuData->vendorID == INTEL_VENDOR_ID;
	if( gpuData->counterAvalibility )
	{
		IntelDeviceInfo info = { 0 };
		int deviceInfoReturnCode = GetIntelDeviceInfo( &info, pAdapter );
		if( deviceInfoReturnCode != EXIT_SUCCESS )
		{
			pAdapter->Release();
			return deviceInfoReturnCode;
		}
		else
		{
			gpuData->maxFrequency = info.GPUMaxFreq;
			gpuData->minFrequency = info.GPUMinFreq;

			//
			// Older versions of the IntelDeviceInfo query only return
			// GPUMaxFreq and GPUMinFreq, all other members will be zero.
			//
			if( info.GPUArchitecture != IGFX_UNKNOWN )
			{
				gpuData->advancedCounterDataAvalibility = true;
				gpuData->architectureCounter = (INTEL_GPU_ARCHITECTURE) info.GPUArchitecture;
				gpuData->euCount = info.EUCount;
				gpuData->packageTDP = info.PackageTDP;
				gpuData->maxFillRate = info.MaxFillRate;
			}
		}
	}

	pAdapter->Release();
	return EXIT_SUCCESS;
}

GPUDetect::PresetLevel GPUDetect::GetDefaultFidelityPreset( const struct GPUData* const gpuData )
{
	PresetLevel presets = Undefined;

	// Return if prerequisite info is not 
	if( !gpuData->dxAdapterAvalibility )
	{
		return PresetLevel::Undefined;
	}

	//
	// Look for a config file that qualifies devices from any vendor
	// The code here looks for a file with one line per recognized graphics
	// device in the following format:
	//
	// VendorIDHex, DeviceIDHex, CapabilityEnum      ;Commented name of card
	//

	FILE *fp = NULL;
	const char *cfgFileName = nullptr;

	switch( gpuData->vendorID )
	{
	case 0x8086:
		cfgFileName = "IntelGfx.cfg";
		break;

		// Add other cases in this fashion to allow for additional cfg files
		//case SOME_VENDOR_ID:
		//    cfgFileName =  "OtherBrandGfx.cfg";
		//    break;

	default:
		return presets;
	}

	fopen_s( &fp, cfgFileName, "r" );

	if( fp )
	{
		char line[ 100 ];
		char* context = NULL;

		char* szVendorId = NULL;
		char* szDeviceId = NULL;
		char* szPresetLevel = NULL;

		//
		// read one line at a time till EOF
		//
		while( fgets( line, 100, fp ) )
		{
			//
			// Parse and remove the comment part of any line
			//
			int i; for( i = 0; line[ i ] && line[ i ] != ';'; i++ ); line[ i ] = '\0';

			//
			// Try to extract GPUVendorId, GPUDeviceId and recommended Default Preset Level
			//
			szVendorId = strtok_s( line, ",\n", &context );
			szDeviceId = strtok_s( NULL, ",\n", &context );
			szPresetLevel = strtok_s( NULL, ",\n", &context );

			if( ( szVendorId == NULL ) ||
				( szDeviceId == NULL ) ||
				( szPresetLevel == NULL ) )
			{
				continue;  // blank or improper line in cfg file - skip to next line
			}

			unsigned int vId, dId;
			sscanf_s( szVendorId, "%x", &vId );
			sscanf_s( szDeviceId, "%x", &dId );

			//
			// If current graphics device is found in the cfg file, use the 
			// pre-configured default Graphics Presets setting.
			//
			if( ( vId == gpuData->vendorID ) && ( dId == gpuData->deviceID ) )
			{
				char s[ 10 ];
				sscanf_s( szPresetLevel, "%s", s, (unsigned int) _countof( s ) );

				if( !_stricmp( s, "Low" ) )
					presets = Low;
				else if( !_stricmp( s, "Medium" ) )
					presets = Medium;
				else if( !_stricmp( s, "Medium+" ) )
					presets = MediumPlus;
				else if( !_stricmp( s, "High" ) )
					presets = High;
				else
					presets = NotCompatible;

				break;
			}
		}

		fclose( fp );  // Close open file handle
	}
	else
	{
		printf( "%s not found! Presets undefined.\n", cfgFileName );
	}

	//
	// If the current graphics device was not listed in any of the config
	// files, or if config file not found, use Low settings as default.
	// This should be changed to reflect the desired behavior for unknown
	// graphics devices.
	//
	if( presets == Undefined )
	{
		presets = Low;
	}

	return presets;
}

int GPUDetect::InitDxDriverVersion( GPUData * const gpuData )
{
#ifdef GPUDETECT_CHECK_PRECONDITIONS
	// Check preconditions
	if( !( gpuData->dxAdapterAvalibility == true ) )
	{
		return GPUDETECT_ERROR_BAD_DATA;
	}
#endif

	// Fetch registry data
	HKEY dxKeyHandle;
	DWORD numOfAdapters = 0;
	DWORD subKeyMaxLength;
	LONG returnCode = RegOpenKeyExA( HKEY_LOCAL_MACHINE, "SOFTWARE\\Microsoft\\DirectX", NULL, KEY_READ, &dxKeyHandle );

	if( returnCode != ERROR_SUCCESS )
	{
		return GPUDETECT_ERROR_REG_NO_D3D_KEY; 
	}

	// Find all subkeys

	RegQueryInfoKey(
		dxKeyHandle,
		NULL,
		NULL,
		NULL,
		&numOfAdapters,
		&subKeyMaxLength,
		NULL,
		NULL,
		NULL,
		NULL,
		NULL,
		NULL
	);
	subKeyMaxLength += 1; // include the null character

	uint64_t driverVersionRaw = 0;

	bool foundSubkey = false;
	char* subKeyName = new char[subKeyMaxLength];
	for( unsigned int i = 0; i < numOfAdapters; ++i )
	{
		DWORD subKeyLength = subKeyMaxLength;
		LONG returnCodeVendorID, returnCodeDeviceID;
		LSTATUS returnStatus = RegEnumKeyExA(
			dxKeyHandle,
			i,
			subKeyName,
			&subKeyLength,
			NULL,
			NULL,
			NULL,
			NULL
		);

		if( returnStatus == ERROR_SUCCESS )
		{
			DWORD vendorID;
			DWORD deviceID;
			DWORD dwordSize = sizeof( DWORD );
			DWORD qwordSize = sizeof( uint64_t );

			returnCodeVendorID = RegGetValueA(
				dxKeyHandle,
				subKeyName,
				"VendorId",
				RRF_RT_DWORD,
				NULL,
				&vendorID,
				&dwordSize
			);

			returnCodeDeviceID = RegGetValueA(
				dxKeyHandle,
				subKeyName,
				"DeviceId",
				RRF_RT_DWORD,
				NULL,
				&deviceID,
				&dwordSize
			);

			if( returnCodeDeviceID == ERROR_SUCCESS && returnCodeVendorID == ERROR_SUCCESS // If we were able to retrieve the registry values
				&& vendorID == gpuData->vendorID && deviceID == gpuData->deviceID ) // and if the vendor ID and device ID match
			{
				// We have our registry key! Let's get the driver version num now
				foundSubkey = true;

				returnCode = RegGetValueA(
					dxKeyHandle,
					subKeyName,
					"DriverVersion",
					RRF_RT_QWORD,
					NULL,
					&driverVersionRaw,
					&qwordSize
				);
			}
			
		}
	}
	RegCloseKey( dxKeyHandle );
	delete subKeyName;

	if( !foundSubkey )
	{
		return GPUDETECT_ERROR_REG_MISSING_DRIVER_INFO;
	}

	// Now that we have our driver version as a DWORD, let's process that into something readable
	gpuData->dxDriverVersion[ 0 ] = (unsigned int) ( ( driverVersionRaw & 0xFFFF000000000000 ) >> 16 * 3 );
	gpuData->dxDriverVersion[ 1 ] = (unsigned int) ( ( driverVersionRaw & 0x0000FFFF00000000 ) >> 16 * 2 );
	gpuData->dxDriverVersion[ 2 ] = (unsigned int) ( ( driverVersionRaw & 0x00000000FFFF0000 ) >> 16 * 1 );
	gpuData->dxDriverVersion[ 3 ] = (unsigned int) ( ( driverVersionRaw & 0x000000000000FFFF ) );

	// Further processing for Intel's driver versions
	if( gpuData->vendorID == INTEL_VENDOR_ID )
	{
		// Determine which driver version format is being used
		gpuData->intelDriverInfo.isNewDriverVersionFormat = gpuData->dxDriverVersion[ 2 ] >= 100;

		if( gpuData->intelDriverInfo.isNewDriverVersionFormat )
		{
			gpuData->intelDriverInfo.osVersionID = gpuData->dxDriverVersion[ 0 ];
			gpuData->intelDriverInfo.directXVersionID = gpuData->dxDriverVersion[ 1 ];
			gpuData->intelDriverInfo.driverBuildNumber = gpuData->dxDriverVersion[ 2 ] * 10000 + gpuData->dxDriverVersion[ 3 ];
		}
		else
		{
			gpuData->intelDriverInfo.driverBaselineNumber = gpuData->dxDriverVersion[ 0 ] * 100 + gpuData->dxDriverVersion[ 1 ];
			gpuData->intelDriverInfo.driverReleaseRevision = gpuData->dxDriverVersion[ 2 ];
			gpuData->intelDriverInfo.driverBuildNumber = gpuData->dxDriverVersion[ 3 ];
		}
	}
	
	gpuData->d3dRegistryDataAvalibility = true;
	return EXIT_SUCCESS;
}

int GPUDetect::CmpDriverVersions( const GPUData * const driverA, const GPUData * const driverB )
{
	if( driverA->intelDriverInfo.isNewDriverVersionFormat != driverB->intelDriverInfo.isNewDriverVersionFormat )
	{
		if( driverA->intelDriverInfo.isNewDriverVersionFormat )
		{
			return 1;
		}
		else
		{
			return -1;
		}
	}
	else
	{
		// Compare OS / Baseline
		if( driverA->intelDriverInfo.isNewDriverVersionFormat )
		{
			int osDiff = driverA->intelDriverInfo.osVersionID - driverB->intelDriverInfo.osVersionID;
			if( osDiff != 0 )
			{
				return osDiff;
			}
		}
		else
		{
			int baselineDiff = driverA->intelDriverInfo.driverBaselineNumber - driverB->intelDriverInfo.driverBaselineNumber;
			if( baselineDiff != 0 )
			{
				return baselineDiff;
			}
		}

		// Compare DirectXVersion / Release Version
		if( driverA->intelDriverInfo.isNewDriverVersionFormat )
		{
			int dxDiff = driverA->intelDriverInfo.directXVersionID - driverB->intelDriverInfo.directXVersionID;
			if( dxDiff != 0 )
			{
				return dxDiff;
			}
		}
		else
		{
			int releaseDiff = driverA->intelDriverInfo.driverReleaseRevision - driverB->intelDriverInfo.driverReleaseRevision;
			if( releaseDiff != 0 )
			{
				return releaseDiff;
			}
		}

		// Compare build numbers (This is the same for both formats)
		return driverA->intelDriverInfo.driverBuildNumber - driverB->intelDriverInfo.driverBuildNumber;
	}
}

void GPUDetect::GetDriverVersionAsCString( const GPUData * const gpuData, char * const outString )
{
	sprintf_s( outString, 15, "%i.%i.%i.%i", gpuData->dxDriverVersion[ 0 ], gpuData->dxDriverVersion[ 1 ], gpuData->dxDriverVersion[ 2 ], gpuData->dxDriverVersion[ 3 ] );
}

float GPUDetect::GetWDDMVersion( const GPUData * const gpuData )
{
	if( !gpuData->d3dRegistryDataAvalibility || gpuData->vendorID != INTEL_VENDOR_ID )
	{
		return -1.0f;
	}

	switch( gpuData->intelDriverInfo.osVersionID )
	{
	// Most of the time, just shift the decimal to the left
	default:
		return (float)gpuData->intelDriverInfo.osVersionID / 10;

	// Versions that can't be derived with shifting the decimal
	case 10: return 1.3f;
	case 9:  return 1.2f;
	case 8:  return 1.1f;
	case 7:  return 1.0f;

	// OS IDs that come before WDDM
	case 6:
	case 5:
	case 4:
		return -1.0f;
	}
}

float GPUDetect::GetDirectXVersion( const GPUData * const gpuData )
{
	if( !gpuData->d3dRegistryDataAvalibility || gpuData->vendorID != INTEL_VENDOR_ID )
	{
		return -1.0f;
	}

	switch( gpuData->intelDriverInfo.directXVersionID )
	{
	case 20: return 12.1f;
	case 19: return 12.0f;
	case 18: return 11.1f;
	case 17: return 11.0f;
	case 15: return 10.f; // 10.x
	case 14: return 9.f;  //  9.x
	case 13: return 8.f;  //  8.x
	case 12: return 7.f;  //  7.x
	case 11: return 6.f;  //  6.x

	default: return -1.0;
	}
}

int GPUDetect::InitAdapter( struct GPUData* const gpuData, IDXGIAdapter** pAdapter, int adapterIndex )
{
#ifdef GPUDETECT_CHECK_PRECONDITIONS
	// Check preconditions
	if( !( gpuData->adapterIndex >= -1 ) )
	{
		return GPUDETECT_ERROR_BAD_DATA;
	}
#endif

	if( adapterIndex != -1 )
	{
		gpuData->adapterIndex = adapterIndex;
	}

	//
	// We are relying on DXGI (supported on Windows Vista and later) to query
	// the adapter, so fail if it is not available.
	//
	// DXGIFactory1 is required by Windows Store Apps so try that first.
	//
	HMODULE hDXGI = LoadLibrary( L"dxgi.dll" );
	if( hDXGI == NULL )
	{
		return GPUDETECT_ERROR_DXGI_LOAD;
	}

	typedef HRESULT( WINAPI*LPCREATEDXGIFACTORY )( REFIID riid, void** ppFactory );

	LPCREATEDXGIFACTORY pCreateDXGIFactory = (LPCREATEDXGIFACTORY) GetProcAddress( hDXGI, "CreateDXGIFactory1" );
	if( pCreateDXGIFactory == NULL )
	{
		pCreateDXGIFactory = (LPCREATEDXGIFACTORY) GetProcAddress( hDXGI, "CreateDXGIFactory" );

		if( pCreateDXGIFactory == NULL )
		{
			FreeLibrary( hDXGI );
			return GPUDETECT_ERROR_DXGI_FACTORY_CREATION;
		}
	}

	//
	// We have the CreateDXGIFactory function so use it to actually create the factory and enumerate
	// through the adapters. Here, we are specifically looking for the Intel gfx adapter.
	//
	IDXGIFactory* pFactory = NULL;
	if( FAILED( ( *pCreateDXGIFactory )( __uuidof( IDXGIFactory ), (void**) ( &pFactory ) ) ) )
	{
		FreeLibrary( hDXGI );
		return GPUDETECT_ERROR_DXGI_FACTORY_CREATION;
	}

	if( FAILED( pFactory->EnumAdapters( gpuData->adapterIndex, (IDXGIAdapter**) pAdapter ) ) )
	{
		pFactory->Release();
		FreeLibrary( hDXGI );
		return GPUDETECT_ERROR_DXGI_ADAPTER_CREATION;
	}

	pFactory->Release();
	FreeLibrary( hDXGI );
	return EXIT_SUCCESS;
}

int GPUDetect::GetIntelDeviceInfo(IntelDeviceInfo* deviceInfo, IDXGIAdapter* pAdapter )
{
	//
	// First create the Device, must be SandyBridge or later to create D3D11
	// device.
	//
	ID3D11Device* pDevice = nullptr;
	ID3D11DeviceContext* pDeviceContext = nullptr;
	if( FAILED( D3D11CreateDevice( pAdapter, D3D_DRIVER_TYPE_UNKNOWN, NULL, 0, NULL, 0, D3D11_SDK_VERSION, &pDevice, NULL, &pDeviceContext ) ) )
	{
		return GPUDETECT_ERROR_DXGI_DEVICE_CREATION;
	}

	//
	// Query the device to find the number of device dependent counters.
	//
	D3D11_COUNTER_INFO counterInfo = {};
	pDevice->CheckCounterInfo( &counterInfo );
	if( counterInfo.LastDeviceDependentCounter == 0 )
	{
		pDevice->Release();
		pDeviceContext->Release();
		return GPUDETECT_ERROR_DXGI_BAD_COUNTER;
	}

	//
	// Search for the "Intel Device Information" counter and, if found, parse
	// it's description to determine the supported version.
	//
	D3D11_COUNTER_DESC counterDesc = {};
	int intelDeviceInfoVersion = 0;
	bool intelDeviceInfo = false;

	for( int i = D3D11_COUNTER_DEVICE_DEPENDENT_0; i <= counterInfo.LastDeviceDependentCounter; ++i )
	{
		counterDesc.Counter = static_cast<D3D11_COUNTER>( i );

		D3D11_COUNTER_TYPE counterType;
		UINT uiSlotsRequired = 0;
		UINT uiNameLength = 0;
		UINT uiUnitsLength = 0;
		UINT uiDescLength = 0;
		if( FAILED( pDevice->CheckCounter( &counterDesc, &counterType, &uiSlotsRequired, NULL, &uiNameLength, NULL, &uiUnitsLength, NULL, &uiDescLength ) ) )
		{
			continue;
		}

		LPSTR sName = new char[ uiNameLength ];
		LPSTR sUnits = new char[ uiUnitsLength ];
		LPSTR sDesc = new char[ uiDescLength ];

		intelDeviceInfo =
			SUCCEEDED( pDevice->CheckCounter( &counterDesc, &counterType, &uiSlotsRequired, sName, &uiNameLength, sUnits, &uiUnitsLength, sDesc, &uiDescLength ) ) &&
			strcmp( sName, "Intel Device Information" ) == 0;

		if( intelDeviceInfo )
		{
			sscanf_s( sDesc, "Version %d", &intelDeviceInfoVersion );
		}
		delete[] sName;
		delete[] sUnits;
		delete[] sDesc;


		if( intelDeviceInfo )
		{
			break;
		}
	}

	//
	// Create the information counter, and query it to get the data. GetData()
	// returns a pointer to the data, not the actual data.
	//
	ID3D11Counter* counter = NULL;
	if( !intelDeviceInfo || FAILED( pDevice->CreateCounter( &counterDesc, &counter ) ) )
	{
		pDevice->Release();
		pDeviceContext->Release();
		return GPUDETECT_ERROR_DXGI_COUNTER_CREATION;
	}

	pDeviceContext->Begin( counter );
	pDeviceContext->End( counter );

	unsigned __int64 dataAddress = 0;
	if( pDeviceContext->GetData( counter, &dataAddress, sizeof( dataAddress ), 0 ) != S_OK )
	{
		counter->Release();
		pDevice->Release();
		pDeviceContext->Release();
		return GPUDETECT_ERROR_DXGI_COUNTER_GET_DATA;
	}

	//
	// Copy the information into the user's structure
	//
	size_t infoSize = intelDeviceInfoVersion == 1 ? 8 : 24;
	memcpy( deviceInfo, (void*) dataAddress, infoSize );

	counter->Release();
	pDevice->Release();
	pDeviceContext->Release();
	return EXIT_SUCCESS;
}
