/************************************************************************************

Filename    :   VrApi_ExtPrivate.h
Content     :   VrApi private extensions support
Created     :   February 3, 2016
Authors     :   Cass Everitt

Copyright   :   Copyright 2016 Oculus VR, LLC. All Rights reserved.

*************************************************************************************/

#ifndef OVR_VrApi_ExtPrivate_h
#define OVR_VrApi_ExtPrivate_h

#include "VrApi_Ext.h"


#define VRAPI_CUSTOM_TEXTURE_SAMPLER_EXT	((ovrSystemProperty)(VRAPI_EXT_BASE + 1))

/* ovrFrameLayerFlags allocations */

// CUSTOM_TEXTURE_SAMPLER_EXT
#define VRAPI_FRAME_LAYER_FLAG_CUSTOM_TEXTURE_SAMPLER_EXT (1<<31)

/*
  Which extensions should be compiled in to the VrApi implementation.
  There may still be run-time settings which disable an extension.
  But being compiled in is a necessary condition for vrapi_GetSystemProperty*()
  to return non-zero for an extension.
*/

#define VRAPI_WITH_CUSTOM_TEXTURE_SAMPLER_EXT 1
#define VRAPI_WITH_REMAP_2D_EXT 1
#define VRAPI_WITH_REMAP_2D_HEMICYL_EXT 1
#define VRAPI_WITH_EXTENDED_FRAME_PARMS_EXT 1
#define VRAPI_WITH_OFFCENTER_CUBE_MAP_EXT 1

struct ovrFrameParmsAll
{
	bool hasFrameParms;
	bool hasRemap2D;
	bool hasOffcenterCubeMap;
	ovrFrameParms						frameParms;
	ovrFrameParmsRemap2DExt				remap2D;
	ovrFrameParmsOffcenterCubeMapExt	offcenterCubeMap;
};

static inline void vrapi_SetFrameParmsAll( ovrFrameParmsAll & parmsAll, const ovrFrameParmsExtBase * parmsChain )
{
	parmsAll.hasFrameParms = false;
	parmsAll.hasRemap2D = false;
	parmsAll.hasOffcenterCubeMap = false;

	parmsAll.remap2D = vrapi_DefaultFrameParmsRemap2DExt();
	while ( parmsChain != NULL && parmsChain->Type != VRAPI_STRUCTURE_TYPE_FRAME_PARMS )
	{
		switch ( (unsigned int)parmsChain->Type )
		{
			case VRAPI_STRUCTURE_TYPE_FRAME_PARMS_REMAP_2D_EXT:
				parmsAll.hasRemap2D = true;
				parmsAll.remap2D = *(const ovrFrameParmsRemap2DExt *)parmsChain;
				break;
			case VRAPI_STRUCTURE_TYPE_FRAME_PARMS_OFFCENTER_CUBE_MAP_EXT:
				parmsAll.hasOffcenterCubeMap = true;
				parmsAll.offcenterCubeMap = *(const ovrFrameParmsOffcenterCubeMapExt *)parmsChain;
				break;
			default:
				break;
		}
		parmsChain = parmsChain->Next;
	}

	if ( parmsChain != NULL )
	{
		parmsAll.hasFrameParms = true;
		parmsAll.frameParms = *(const ovrFrameParms *)parmsChain;
	}

}

#endif // OVR_VrApi_ExtPrivate_h
