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

#include "GPUDetect.h"
#include "ID3D10Extensions.h"

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
		fprintf( stderr, "D3D Driver info was not located in the expected location in the registry\n" );
		break;

	case GPUDETECT_ERROR_REG_MISSING_DRIVER_INFO:
		fprintf( stderr, "Could not find a D3D driver matching the device ID and vendor ID of this adapter\n" );
		break;

	case GPUDETECT_ERROR_BAD_DATA:
		fprintf( stderr, "Precondition of function not met\n" );

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


	struct GPUDetect::GPUData gpuData = { 0 };
	int initReturnCode = GPUDetect::InitExtensionInfo( &gpuData, adapterIndex );
	if( initReturnCode != EXIT_SUCCESS )
	{
		printError( initReturnCode );
	}
	else
	{
		std::wstring gpuDescription = gpuData.description;

		printf( "Graphics Device #%u\n", adapterIndex );
		printf( "-----------------------\n" );
		printf( "Vendor: 0x%x\n", gpuData.vendorID );
		printf( "Device: 0x%x\n", gpuData.deviceID );
		printf( "Video Memory: %I64u MB\n", gpuData.videoMemory / ( 1024 * 1024 ) );
		printf( "Description: %S\n", gpuData.description );
		printf( "\n" );

		//
		//  Find and print driver version information
		//
		GPUDetect::InitDxDriverVersion( &gpuData );
		if( gpuData.d3dRegistryDataAvalibility )
		{
			printf( "\nDriver Information\n" );
			printf( "-----------------------\n" );
			char driverVersion[ 16 ];
			GPUDetect::GetDriverVersionAsCString( &gpuData, driverVersion );
			printf( "Driver Version: %s\n", driverVersion );

			// Print out additional data
			if( gpuData.vendorID == GPUDetect::INTEL_VENDOR_ID )
			{
				
				if( gpuData.intelDriverInfo.isNewDriverVersionFormat )
				{
					printf( "WDDM Version: %1.1f\n", GPUDetect::GetWDDMVersion( &gpuData ) );
					printf( "DirectX Version: %1.1f\n", GPUDetect::GetDirectXVersion( &gpuData ) );
					printf( "Build Number: %3i.%4i\n", gpuData.intelDriverInfo.driverBuildNumber / 10000, gpuData.intelDriverInfo.driverBuildNumber % 10000 );
				}
				else
				{
					printf( "Version Baseline: %2i.%2i\n", gpuData.intelDriverInfo.driverBaselineNumber / 100, gpuData.intelDriverInfo.driverBaselineNumber % 100 );
					printf( "Release Revision: %i\n", gpuData.intelDriverInfo.driverReleaseRevision );
					printf( "Build Number: %i\n", gpuData.intelDriverInfo.driverBuildNumber );
				}
			}

			printf( "\n" );
		}

		//
		// Similar to the CPU brand, we can also parse the GPU description string
		// for information like whether the GPU is an Intel Iris or Iris Pro part.
		//
		if( gpuData.vendorID == GPUDetect::INTEL_VENDOR_ID )
		{
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
		GPUDetect::PresetLevel defPresets = GPUDetect::GetDefaultFidelityPreset( &gpuData );
		switch( defPresets )
		{
		case GPUDetect::PresetLevel::NotCompatible: printf( "Default Presets: NotCompatible\n" ); break;
		case GPUDetect::PresetLevel::Low:           printf( "Default Presets: Low\n" ); break;
		case GPUDetect::PresetLevel::Medium:        printf( "Default Presets: Medium\n" ); break;
		case GPUDetect::PresetLevel::MediumPlus:    printf( "Default Presets: Medium+\n" ); break;
		case GPUDetect::PresetLevel::High:          printf( "Default Presets: High\n" ); break;
		case GPUDetect::PresetLevel::Undefined:     printf( "Default Presets: Undefined\n" ); break;
		}

		//
		// Check if Intel DirectX extensions are available on this system.
		//
		if( gpuData.intelExtentionAvalibility )
		{
			printf( "Supports Intel Iris Graphics extensions:\n" );
			printf( "\tpixel synchronization\n" );
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

		if( gpuData.vendorID == GPUDetect::INTEL_VENDOR_ID )
		{
			GPUDetect::INTEL_GPU_ARCHITECTURE arch = GPUDetect::getIntelGPUArchitecture( gpuData.deviceID );

			// Populate the GPU architecture data with info from the counter, otherwise gpuDetector will use the value we got from the Dx11 extension
			initReturnCode = GPUDetect::InitCounterInfo( &gpuData, adapterIndex );

			if( initReturnCode != EXIT_SUCCESS )
			{
				printError( initReturnCode );
			}
			else
			{
				printf( "Architecture (from device id): %s (0x%x)\n", getIntelGPUArchitectureString( arch ), arch );

				//
				// Older versions of the IntelDeviceInfo query only return
				// GPUMaxFreq and GPUMinFreq, all other members will be zero.
				//
				if( gpuData.advancedCounterDataAvalibility )
				{
					printf( "Architecture (from device info): %s (0x%x)\n", GPUDetect::getIntelGPUArchitectureString( gpuData.architectureCounter ), gpuData.architectureCounter );
					printf( "EU Count:          %d\n", gpuData.euCount );
					printf( "Package TDP:       %d W\n", gpuData.packageTDP );
					printf( "Max Fill Rate:     %d pixels/clock\n", gpuData.maxFillRate );
				}

				printf( "GPU Max Frequency: %d MHz\n", gpuData.maxFrequency );
				printf( "GPU Min Frequency: %d MHz\n", gpuData.minFrequency );
			}
		}

		printf( "\n" );
	}

	printf( "\n" );
	printf( "Press any key to exit...\n" );

	_getch();

	return 0;
}

