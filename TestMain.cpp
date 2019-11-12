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

#include <conio.h>
#include <d3d11.h>
#include <intrin.h>
#include <stdio.h>
#include <string>

#include "DeviceId.h"
#include "ID3D10Extensions.h"

namespace
{

	// Define settings to reflect Fidelity abstraction levels you need
	enum PresetLevel
	{
		NotCompatible,  // Found GPU is not compatible with the app
		Low,
		Medium,
		MediumPlus,
		High,
		Undefined  // No predefined setting found in cfg file. 
				   // Use a default level for unknown video cards.
	};

	char const* getIntelGPUArchitectureString( INTEL_GPU_ARCHITECTURE arch )
	{
		switch( arch )
		{
		case IGFX_SANDYBRIDGE: return "Sandy Bridge";
		case IGFX_IVYBRIDGE:   return "Ivy Bridge";
		case IGFX_HASWELL:     return "Haswell";
		case IGFX_VALLEYVIEW:  return "ValleyView";
		case IGFX_BROADWELL:   return "Broadwell";
		case IGFX_CHERRYVIEW:  return "Cherryview";
		case IGFX_SKYLAKE:     return "Skylake";
		case IGFX_KABYLAKE:    return "Kabylake";
		case IGFX_COFFEELAKE:  return "Coffeelake";
		case IGFX_GEMINILAKE:  return "Geminilake";
		case IGFX_WHISKEYLAKE: return "Whiskeylake";
		case IGFX_ICELAKE:     return "Icelake";
		}

		return "Unknown";
	}

	bool isnumber( char* s )
	{
		for( ; *s != '\0'; ++s )
		{
			if( !isdigit( *s ) )
			{
				return false;
			}
		}
		return true;
	}

	IDXGIAdapter* getAdapter( int AdapterIndex )
	{
		//
		// We are relying on DXGI (supported on Windows Vista and later) to query
		// the adapter, so fail if it is not available.
		//
		// DXGIFactory1 is required by Windows Store Apps so try that first.
		//
		HMODULE hDXGI = LoadLibrary( L"dxgi.dll" );
		if( hDXGI == NULL )
		{
			return nullptr;
		}

		typedef HRESULT( WINAPI*LPCREATEDXGIFACTORY )( REFIID riid, void** ppFactory );

		LPCREATEDXGIFACTORY pCreateDXGIFactory = (LPCREATEDXGIFACTORY) GetProcAddress( hDXGI, "CreateDXGIFactory1" );
		if( pCreateDXGIFactory == NULL )
		{
			pCreateDXGIFactory = (LPCREATEDXGIFACTORY) GetProcAddress( hDXGI, "CreateDXGIFactory" );

			if( pCreateDXGIFactory == NULL )
			{
				FreeLibrary( hDXGI );
				return nullptr;
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
			return nullptr;
		}

		IDXGIAdapter* pAdapter = NULL;
		if( FAILED( pFactory->EnumAdapters( AdapterIndex, (IDXGIAdapter**) &pAdapter ) ) )
		{
			pFactory->Release();
			FreeLibrary( hDXGI );
			return nullptr;
		}

		pFactory->Release();
		FreeLibrary( hDXGI );
		return pAdapter;
	}

	/*******************************************************************************
	 * getDefaultFidelityPresets
	 *
	 *     Function to find the default fidelity preset level, based on the type of
	 *     graphics adapter present.
	 *
	 *     The guidelines for graphics preset levels for Intel devices is a generic
	 *     one based on our observations with various contemporary games. You would
	 *     have to change it if your game already plays well on the older hardware
	 *     even at high settings.
	 *
	 ******************************************************************************/
	PresetLevel getDefaultFidelityPresets( unsigned int GPUVendorId, unsigned int GPUDeviceId )
	{
		PresetLevel presets = Undefined;

		//
		// Look for a config file that qualifies devices from any vendor
		// The code here looks for a file with one line per recognized graphics
		// device in the following format:
		//
		// VendorIDHex, DeviceIDHex, CapabilityEnum      ;Commented name of card
		//

		FILE *fp = NULL;
		const char *cfgFileName = nullptr;

		switch( GPUVendorId )
		{
		case 0x8086:
			cfgFileName = "IntelGfx.cfg";
			break;

			//case 0x1002:
			//    cfgFileName =  "ATI.cfg"; // not provided
			//    break;

			//case 0x10DE:
			//    cfgFileName = "Nvidia.cfg"; // not provided
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
				if( ( vId == GPUVendorId ) && ( dId == GPUDeviceId ) )
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

}

/*******************************************************************************
 * main
 *
 *     Function represents the game or application. The application checks for
 *     graphics capabilities here and makes whatever decisions it needs to
 *     based on the results.
 *
 ******************************************************************************/
int main( int argc, char** argv )
{
	int adapterIndex = 0;
	if( argc == 2 && isnumber( argv[ 1 ] ) )
	{
		adapterIndex = atoi( argv[ 1 ] );
	}
	else if( argc != 1 )
	{
		fprintf( stderr, "error: unexpected arguments.\n" );
		fprintf( stderr, "usage: GPUDetect adapter_index\n" );
		return 1;
	}

	//
	// First check if the CPU is an Intel processor.  This is also a good place
	// to check for other CPU capabilites such as AVX support.
	//
	// The brand string can be parsed to discover some information about the
	// type of processor.
	//
	std::string CPUBrand;
	std::string CPUVendor;
	getCPUInfo( &CPUBrand, &CPUVendor );
	printf( "CPU Vendor: %s\n", CPUVendor.c_str() );
	printf( "CPU Brand: %s\n", CPUBrand.c_str() );

	if( CPUVendor == "GenuineIntel" )
	{
		if( CPUBrand.find( "i9" ) != std::string::npos )
		{
			printf( "           i9 Brand Found\n" );
		}
		else if( CPUBrand.find( "i7" ) != std::string::npos )
		{
			printf( "           i7 Brand Found\n" );
		}
		else if( CPUBrand.find( "i5" ) != std::string::npos )
		{
			printf( "           i5 Brand Found\n" );
		}
		else if( CPUBrand.find( "i3" ) != std::string::npos )
		{
			printf( "           i3 Brand Found\n" );
		}
		else if( CPUBrand.find( "Celeron" ) != std::string::npos )
		{
			printf( "           Celeron Brand Found\n" );
		}
		else if( CPUBrand.find( "Pentium" ) != std::string::npos )
		{
			printf( "           Pentium Brand Found\n" );
		}
	}

	//
	// Information about the GPUs is exposed through Windows as indexed
	// adapters.  Index 0 is the primary or default GPU.  Information about
	// other GPUs can be queried by using a different index.
	//
	IDXGIAdapter* pAdapter = getAdapter( adapterIndex );
	if( pAdapter == NULL )
	{
		fprintf( stderr, "error: could not create adapter #%u\n", adapterIndex );
		return 2;
	}

	unsigned int GPUVendorId;
	unsigned int GPUDeviceId;
	unsigned __int64 VideoMemory;
	std::wstring GPUDescription;
	if( !getGraphicsDeviceInfo( pAdapter, &GPUVendorId, &GPUDeviceId, &VideoMemory, &GPUDescription ) )
	{
		fprintf( stderr, "error: failed to get GPU info\n" );
		pAdapter->Release();
		return 3;
	}

	printf( "Graphics Device #%u\n", adapterIndex );
	printf( "-----------------------\n" );
	printf( "Vendor: 0x%x\n", GPUVendorId );
	printf( "Device: 0x%x\n", GPUDeviceId );
	printf( "Video Memory: %I64u MB\n", VideoMemory / ( 1024 * 1024 ) );
	printf( "Description: %S\n", GPUDescription.c_str() );

	//
	// Similar to the CPU brand, we can also parse the GPU description string
	// for information like whether the GPU is an Intel Iris or Iris Pro part.
	//
	if( GPUVendorId == INTEL_VENDOR_ID )
	{
		if( GPUDescription.find( L"Iris" ) != std::wstring::npos )
		{
			if( GPUDescription.find( L"Pro" ) != std::wstring::npos )
			{
				printf( "             Iris Pro Graphics Brand Found\n" );
			}
			else
			{
				printf( "             Iris Graphics Brand Found\n" );
			}
		}
	}

	//
	// This sample includes a .cfg file that maps known vendor and device IDs
	// to example quality presets.  This looks up the preset for the IDs
	// queried above.
	//
	PresetLevel defPresets = getDefaultFidelityPresets( GPUVendorId, GPUDeviceId );
	switch( defPresets )
	{
	case NotCompatible: printf( "Default Presets: NotCompatible\n" ); break;
	case Low:           printf( "Default Presets: Low\n" ); break;
	case Medium:        printf( "Default Presets: Medium\n" ); break;
	case MediumPlus:    printf( "Default Presets: Medium+\n" ); break;
	case High:          printf( "Default Presets: High\n" ); break;
	case Undefined:     printf( "Default Presets: Undefined\n" ); break;
	}

	//
	// Check if Intel DirectX extensions are a vailable on this sytem.
	//
	unsigned int extensionVersion = checkDxExtensionVersion( pAdapter );
	if( extensionVersion >= ID3D10::EXTENSION_INTERFACE_VERSION_1_0 )
	{
		printf( "Supports Intel Iris Graphics extensions:\n" );
		printf( "\tpixel sychronization\n" );
		printf( "\tinstant access of graphics memory\n" );
	}
	else
	{
		printf( "Does not support Intel Iris Graphics extensions\n" );
	}

	//
	// In DirectX, Intel exposes additional information through the driver that can be obtained
	// querying a special DX counter
	//
	if( GPUVendorId == INTEL_VENDOR_ID )
	{
		INTEL_GPU_ARCHITECTURE arch = getIntelGPUArchitecture( GPUDeviceId );
		printf( "Architecture (from device id): %s (0x%x)\n", getIntelGPUArchitectureString( arch ), arch );

		IntelDeviceInfo info = {};
		if( getIntelDeviceInfo( pAdapter, &info ) )
		{
			//
			// Older versions of the IntelDeviceInfo query only return
			// GPUMaxFreq and GPUMinFreq, all other members will be zero.
			//
			if( info.GPUArchitecture != 0 )
			{
				arch = (INTEL_GPU_ARCHITECTURE) info.GPUArchitecture;
				printf( "Architecture (from device info): %s (0x%x)\n", getIntelGPUArchitectureString( arch ), arch );
				printf( "EU Count:          %d\n", info.EUCount );
				printf( "Package TDP:       %d W\n", info.PackageTDP );
				printf( "Max Fill Rate:     %d pixel/clock\n", info.MaxFillRate );
			}

			printf( "GPU Max Frequency: %d MHz\n", info.GPUMaxFreq );
			printf( "GPU Min Frequency: %d MHz\n", info.GPUMinFreq );
		}
	}

	printf( "\n" );
	printf( "Press any key to exit...\n" );

	_getch();

	pAdapter->Release();
	return 0;
}

