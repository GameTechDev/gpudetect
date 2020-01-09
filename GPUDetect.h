#pragma once

#include "DeviceId.h"
#include "ID3D10Extensions.h"

// Error return codes for GPUDetect
// These codes are set up so that (ERROR_CODE % GENERAL_ERROR_CODE) == 0 if
// ERROR_CODE is a subset of GENERAL_ERROR_CODE.
// e.g. GPUDETECT_ERROR_DXGI_LOAD % GPUDETECT_ERROR_DXGI_LOAD == 0
#define GPUDETECT_ERROR_GENERIC 1

/// General DXGI Errors
#define GPUDETECT_ERROR_GENERAL_DXGI            2
#define GPUDETECT_ERROR_DXGI_LOAD               GPUDETECT_ERROR_GENERAL_DXGI * 3
#define GPUDETECT_ERROR_DXGI_ADAPTER_CREATION   GPUDETECT_ERROR_GENERAL_DXGI * 5
#define GPUDETECT_ERROR_DXGI_FACTORY_CREATION   GPUDETECT_ERROR_GENERAL_DXGI * 7
#define GPUDETECT_ERROR_DXGI_DEVICE_CREATION    GPUDETECT_ERROR_GENERAL_DXGI * 11
#define GPUDETECT_ERROR_DXGI_GET_ADAPTER_DESC   GPUDETECT_ERROR_GENERAL_DXGI * 13

/// DXGI Counter Errors
#define GPUDETECT_ERROR_GENERAL_DXGI_COUNTER    17
#define GPUDETECT_ERROR_DXGI_BAD_COUNTER        GPUDETECT_ERROR_GENERAL_DXGI_COUNTER * 19
#define GPUDETECT_ERROR_DXGI_COUNTER_CREATION   GPUDETECT_ERROR_GENERAL_DXGI_COUNTER * 23
#define GPUDETECT_ERROR_DXGI_COUNTER_GET_DATA   GPUDETECT_ERROR_GENERAL_DXGI_COUNTER * 29

namespace GPUDetect
{
	enum
	{
		INTEL_VENDOR_ID = 0x8086,
	};

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

	/*******************************************************************************
	 * InitExtensionInfo
	 *
	 *     Loads available info from the Dx11 extension interface. Use this to force
	 *     the GPUDetector to load this data now instead of when it is first
	 *     requested. Returns EXIT_SUCCESS if no error was encountered, otherwise
	 *     returns an error code.
	 *
	 *     gpuData
	 *         The struct in which the information will be stored.
	 *
	 *     adapterIndex
	 *         The index of the adapter to get the information from. If adapterIndex
	 *         is -1, the adapterIndex value from gpuData will be used.
	 *
	 ******************************************************************************/
	int InitExtensionInfo( struct GPUData* const gpuData, int adapterIndex = -1 );

	/*******************************************************************************
	 * InitCounterInfo
	 *
	 *     Loads available info from Dx11 hardware counters. Use this to force
	 *     the GPUDetector to load this data now instead of when it is first
	 *     requested. Returns EXIT_SUCCESS if no error was encountered, otherwise
	 *     returns an error code.
	 *
	 *     gpuData
	 *         The struct in which the information will be stored.
	 *
	 *     adapterIndex
	 *         The index of the adapter to get the information from. If adapterIndex
	 *         is -1, the adapterIndex value from gpuData will be used.
	 *
	 ******************************************************************************/
	int InitCounterInfo( struct GPUData* const gpuData, int adapterIndex = -1 );

	/*******************************************************************************
	 * GetDefaultFidelityPreset
	 *
	 *     Function to find the default fidelity preset level, based on the type of
	 *     graphics adapter present.
	 *
	 *     The guidelines for graphics preset levels for Intel devices is a generic
	 *     one based on our observations with various contemporary games. You would
	 *     have to change it if your game already plays well on the older hardware
	 *     even at high settings.
	 *
	 *     Presets for Intel are expected in a file named "IntelGFX.cfg". This
	 *     method can also be easily modified to also read similar .cfg files
	 *     detailing presets for other manufacturers.
	 *
	 *     gpuData
	 *         The data for the GPU in question. 
	 *
	 ******************************************************************************/
	PresetLevel GetDefaultFidelityPreset( const struct GPUData* const gpuData );

	struct GPUData
	{
		/*******************************************************************************
		 * adapterIndex
		 *
		 *     The index of the DirectX adapter to gather information from.
		 *
		 ******************************************************************************/
		int adapterIndex;

		/////////////////////////
		// Dx11 Extension Data //
		/////////////////////////

		/*******************************************************************************
		 * dxAdapterAvalibility
		 *
		 *     Is true if Intel driver extension data is populated.
		 *     If this value is false, all other extension data will be null.
		 *
		 ******************************************************************************/
		bool dxAdapterAvalibility;

		/*******************************************************************************
		 * vendorID
		 *
		 *     The vendorID of the GPU.
		 *
		 ******************************************************************************/
		unsigned int vendorID;

		/*******************************************************************************
		 * deviceID
		 *
		 *     The DeviceID of the GPU.
		 *
		 ******************************************************************************/
		unsigned int deviceID;

		/*******************************************************************************
		 * isUMAArchitecture
		 *
		 *     Is true if the GPU uses a uniform memory access architecture.
		 *     On GPUs with a Unified Memory Architecture (UMA) like Intel integrated
		 *     GPUs, the CPU system memory is also used for graphics and there is no
		 *     dedicated VRAM.  Any memory reported as “dedicated” is really a small
		 *     pool of system memory reserved by the BIOS for internal use. All normal
		 *     application allocations (buffers, textures, etc.) are allocated from
		 *     general system "shared" memory.  For this reason, do not use the
		 *     dedicated memory size as an indication of UMA GPU capability (either
		 *     performance, nor memory capacity).
		 *
		 ******************************************************************************/
		bool isUMAArchitecture;

		/*******************************************************************************
		 * videoMemory
		 *
		 *     The amount of Video memory in bytes.
		 *
		 ******************************************************************************/
		unsigned __int64 videoMemory;

		/*******************************************************************************
		 * description
		 *
		 *     The driver-provided description of the GPU.
		 *
		 ******************************************************************************/
		WCHAR description[128];

		/*******************************************************************************
		 * extentionVersion
		 *
		 *     Version number for D3D driver extensions.
		 *
		 ******************************************************************************/
		unsigned int extentionVersion;

		/*******************************************************************************
		 * intelExtentionAvalibility
		 *
		 *     True if Intel driver extensions are available on the GPU.
		 *
		 ******************************************************************************/
		bool intelExtentionAvalibility;

		/////////////////////////////////
		// Dx11 Hardware Counters Data //
		/////////////////////////////////

		/*******************************************************************************
		 * counterAvalibility
		 *
		 *     Returns true if Intel driver extensions are available to gather data from.
		 *     If this value is false, all other extension data will be null.
		 *
		 ******************************************************************************/
		bool counterAvalibility;

		/*******************************************************************************
		 * maxFrequency
		 *
		 *     Returns the maximum frequency of the GPU in MHz.
		 *
		 ******************************************************************************/
		unsigned int maxFrequency;

		/*******************************************************************************
		 * minFrequency
		 *
		 *     Returns the minimum frequency of the GPU in MHz.
		 *
		 ******************************************************************************/
		unsigned int minFrequency;

		/// Advanced counter info

		/*******************************************************************************
		 * advancedCounterDataAvalibility
		 *
		 *     Returns true if advanced counter data is available from this GPU.
		 *     Older Intel products only provide the maximum and minimum GPU frequency 
		 *     via the hardware counters.
		 *
		 ******************************************************************************/
		bool advancedCounterDataAvalibility;

		/*******************************************************************************
		 * architectureCounter
		 *
		 *     A counter that indicates which architecture the GPU uses.
		 *
		 ******************************************************************************/
		enum INTEL_GPU_ARCHITECTURE architectureCounter;

		/*******************************************************************************
		 * euCount
		 *
		 *     Returns the number of execution units (EUs) on the GPU.
		 *
		 ******************************************************************************/
		unsigned int euCount;

		/*******************************************************************************
		 * packageTDP
		 *
		 *     Returns the thermal design power (TDP) of the GPU in watts.
		 *
		 ******************************************************************************/
		unsigned int packageTDP;

		/*******************************************************************************
		 * maxFillRate
		 *
		 *     Returns the maximum fill rate of the GPU in pixels/clock.
		 *
		 ******************************************************************************/
		unsigned int maxFillRate;
	};
}


