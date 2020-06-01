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


#include <dxgi.h>
#include <d3d11.h>
#ifdef _WIN32_WINNT_WIN10
#include <d3d11_3.h>
#endif

#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string>

#include "GPUDetect.h"


// For parsing arguments
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

/*******************************************************************************
 * printError
 *
 *     Prints the error on screen in a nice format.
 *
 ******************************************************************************/
void printError( int errorCode )
{
	fprintf( stderr, "Error: " );

	if( errorCode % GPUDETECT_ERROR_GENERAL_DXGI == 0 )
	{
		fprintf( stderr, "DXGI: " );
	}

	if( errorCode % GPUDETECT_ERROR_GENERAL_DXGI_COUNTER == 0 )
	{
		fprintf( stderr, "DXGI Counter: " );
	}

	if( errorCode % GPUDETECT_ERROR_REG_GENERAL_FAILURE == 0 )
	{
		fprintf( stderr, "Registry: " );
	}

	switch( errorCode )
	{
	case GPUDETECT_ERROR_DXGI_LOAD:
		fprintf( stderr, "Could not load DXGI Library\n" );
		break;

	case GPUDETECT_ERROR_DXGI_ADAPTER_CREATION:
		fprintf( stderr, "Could not create DXGI adapter\n" );
		break;

	case GPUDETECT_ERROR_DXGI_FACTORY_CREATION:
		fprintf( stderr, "Could not create DXGI factory\n" );
		break;

	case GPUDETECT_ERROR_DXGI_DEVICE_CREATION:
		fprintf( stderr, "Could not create DXGI device\n" );
		break;

	case GPUDETECT_ERROR_DXGI_GET_ADAPTER_DESC:
		fprintf( stderr, "Could not get DXGI adapter\n" );
		break;

	case GPUDETECT_ERROR_DXGI_BAD_COUNTER:
		fprintf( stderr, "Invalid DXGI counter data\n" );
		break;

	case GPUDETECT_ERROR_DXGI_COUNTER_CREATION:
		fprintf( stderr, "Could not create DXGI counter\n" );
		break;

	case GPUDETECT_ERROR_DXGI_COUNTER_GET_DATA:
		fprintf( stderr, "Could not get DXGI counter data\n" );
		break;

	case GPUDETECT_ERROR_REG_NO_D3D_KEY:
		fprintf( stderr, "D3D driver info was not located in the expected location in the registry\n" );
		break;

	case GPUDETECT_ERROR_REG_MISSING_DRIVER_INFO:
		fprintf( stderr, "Could not find a D3D driver matching the device ID and vendor ID of this adapter\n" );
		break;

	case GPUDETECT_ERROR_BAD_DATA:
		fprintf( stderr, "Bad input data for function or precondition not met\n" );
		break;

	case GPUDETECT_ERROR_NOT_SUPPORTED:
		fprintf( stderr, "Not supported\n" );
		break;

	default:
		fprintf( stderr, "Unknown error\n" );
		break;
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
	printf("\n\n[ Intel GPUDetect ]\n");
	printf("Build Info: %s, %s\n", __DATE__, __TIME__);

	int adapterIndex = 0;

	if ( argc == 1 )
	{
		fprintf( stdout, "Usage: GPUDetect adapter_index\n" );
		fprintf( stdout, "Defaulting to adapter_index = %d\n", adapterIndex );
	}
	else if( argc == 2 && isnumber( argv[ 1 ] ))
	{
		adapterIndex = atoi( argv[ 1 ] );
		fprintf( stdout, "Choosing adapter_index = %d\n", adapterIndex );
	}
	else
	{
		fprintf( stdout, "Usage: GPUDetect adapter_index\n" );
		fprintf( stderr, "Error: unexpected arguments.\n" );
		return EXIT_FAILURE;
	}
	printf( "\n" );


	IDXGIAdapter* adapter = nullptr;
	int initReturnCode = GPUDetect::InitAdapter( &adapter, adapterIndex );
	if( initReturnCode != EXIT_SUCCESS )
	{
		printError( initReturnCode );
		return EXIT_FAILURE;
	};

	ID3D11Device* device = nullptr;
	initReturnCode = GPUDetect::InitDevice( adapter, &device );
	if( initReturnCode != EXIT_SUCCESS )
	{
		printError( initReturnCode );
		adapter->Release();
		return EXIT_FAILURE;
	};

	GPUDetect::GPUData gpuData = {};
	initReturnCode = GPUDetect::InitExtensionInfo( &gpuData, adapter, device );
	if( initReturnCode != EXIT_SUCCESS )
	{
		printError( initReturnCode );
	}
	else
	{
		printf( "Graphics Device #%d\n", adapterIndex );
		printf( "-----------------------\n" );
		printf( "Vendor: 0x%x\n", gpuData.vendorID );
		printf( "Device: 0x%x\n", gpuData.deviceID );
		printf( "Video Memory: %I64u MB\n", gpuData.videoMemory / ( 1024 * 1024 ) );
		printf( "Description: %S\n", gpuData.description );
		printf( "\n" );

		//
		//  Find and print driver version information
		//
		initReturnCode = GPUDetect::InitDxDriverVersion( &gpuData );
		if( gpuData.d3dRegistryDataAvailability )
		{
			printf( "\nDriver Information\n" );
			printf( "-----------------------\n" );

			char driverVersion[ 19 ] = {};
			GPUDetect::GetDriverVersionAsCString( &gpuData, driverVersion, _countof(driverVersion) );
			printf( "Driver Version: %s\n", driverVersion );

			// Print out decoded data
			printf( "WDDM Version: %1.1f\n", GPUDetect::GetWDDMVersion( &gpuData ));
			printf( "DirectX Version: %1.1f\n", GPUDetect::GetDirectXVersion( &gpuData ));
			printf( "Release Revision: %u\n", gpuData.driverInfo.driverReleaseRevision );
			printf( "Build Number: %u\n", gpuData.driverInfo.driverBuildNumber );
			printf( "\n" );
		}

		//
		// Similar to the CPU brand, we can also parse the GPU description string
		// for information like whether the GPU is an Intel Iris or Iris Pro part.
		//
		if( gpuData.vendorID == GPUDetect::INTEL_VENDOR_ID )
		{
			const std::wstring gpuDescription = gpuData.description;
			if( gpuDescription.find( L"Iris" ) != std::wstring::npos )
			{
				if( gpuDescription.find( L"Pro" ) != std::wstring::npos )
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
		const GPUDetect::PresetLevel defPresets = GPUDetect::GetDefaultFidelityPreset( &gpuData );
		switch( defPresets )
		{
		case GPUDetect::PresetLevel::NotCompatible: printf( "Default Fidelity Preset Level: NotCompatible\n" ); break;
		case GPUDetect::PresetLevel::Low:           printf( "Default Fidelity Preset Level: Low\n" ); break;
		case GPUDetect::PresetLevel::Medium:        printf( "Default Fidelity Preset Level: Medium\n" ); break;
		case GPUDetect::PresetLevel::MediumPlus:    printf( "Default Fidelity Preset Level: Medium+\n" ); break;
		case GPUDetect::PresetLevel::High:          printf( "Default Fidelity Preset Level: High\n" ); break;
		case GPUDetect::PresetLevel::Undefined:     printf( "Default Fidelity Preset Level: Undefined\n" ); break;
		}

		//
		// Check if Intel DirectX extensions are available on this system.
		//
		if( gpuData.vendorID == GPUDetect::INTEL_VENDOR_ID && gpuData.intelExtensionAvailability )
		{
			printf( "Supports Intel Iris Graphics extensions:\n" );
			printf( "\tpixel synchronization\n" );
			printf( "\tinstant access of graphics memory\n" );
		}
		else if( gpuData.vendorID == GPUDetect::INTEL_VENDOR_ID )
		{
			printf( "Does not support Intel Iris Graphics extensions\n" );
		}

		//
		// In DirectX, Intel exposes additional information through the driver that can be obtained
		// querying a special DX counter
		//

		// Populate the GPU architecture data with info from the counter, otherwise gpuDetect will use the value we got from the Dx11 extension
		initReturnCode = GPUDetect::InitCounterInfo( &gpuData, device );
		if( initReturnCode != EXIT_SUCCESS )
		{
			printError( initReturnCode );
		}
		else
		{
			char generationString[256] = "Not set.";
			GPUDetect::GetIntelGraphicsGenerationAsCString( GPUDetect::GetIntelGraphicsGeneration( gpuData.architectureCounter ), generationString, _countof( generationString ));
			printf("Using %s graphics\n", generationString);

			const GPUDetect::INTEL_GPU_ARCHITECTURE arch = GPUDetect::GetIntelGPUArchitecture( gpuData.deviceID );
			printf( "Architecture (from device id): %s (0x%x)\n", GPUDetect::GetIntelGPUArchitectureString( arch ), arch );

			//
			// Older versions of the IntelDeviceInfo query only return
			// GPUMaxFreq and GPUMinFreq, all other members will be zero.
			//
			if( gpuData.advancedCounterDataAvailability )
			{
				printf( "Architecture (from device info): %s (0x%x)\n", GPUDetect::GetIntelGPUArchitectureString( gpuData.architectureCounter ), gpuData.architectureCounter );
				printf( "EU Count:          %u\n", gpuData.euCount );
				printf( "Package TDP:       %u W\n", gpuData.packageTDP );
				printf( "Max Fill Rate:     %u pixels/clock\n", gpuData.maxFillRate );
			}

			printf( "GPU Max Frequency: %u MHz\n", gpuData.maxFrequency );
			printf( "GPU Min Frequency: %u MHz\n", gpuData.minFrequency );
		}

		printf( "\n" );
	}

	device->Release();
	adapter->Release();

	printf( "\n" );

	return 0;
}
