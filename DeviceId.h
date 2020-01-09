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

namespace GPUDetect
{
	enum INTEL_GPU_ARCHITECTURE
	{
		IGFX_UNKNOWN = 0x00,
		IGFX_SANDYBRIDGE = 0x0C,
		IGFX_IVYBRIDGE,
		IGFX_HASWELL,
		IGFX_VALLEYVIEW,
		IGFX_BROADWELL,
		IGFX_CHERRYVIEW,
		IGFX_SKYLAKE,
		IGFX_KABYLAKE,
		IGFX_COFFEELAKE,
		IGFX_GEMINILAKE = 0x17,
		IGFX_WHISKEYLAKE, // Has no LP code assigned, so this is using an unused enum slot.
		IGFX_ICELAKE = 0x1D
	};

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
	INTEL_GPU_ARCHITECTURE getIntelGPUArchitecture( unsigned int deviceId );

	/*******************************************************************************
	 * getIntelGPUArchitectureString
	 *
	 *     Convert A INTEL_GPU_ARCHITECTURE to a string.
	 *
	 ******************************************************************************/
	char const* getIntelGPUArchitectureString( INTEL_GPU_ARCHITECTURE arch );
}



