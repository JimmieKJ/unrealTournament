// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#if WITH_UNREALEXR

PRAGMA_DISABLE_SHADOW_VARIABLE_WARNINGS
#if defined(_MSC_VER) && USING_CODE_ANALYSIS
	#pragma warning(push)
	#pragma warning(disable:28251)
	#pragma warning(disable:28252)
	#pragma warning(disable:28253)
#endif
#include "ThirdParty/openexr/Deploy/include/ImfIO.h"
#include "ThirdParty/openexr/Deploy/include/ImathBox.h"
#include "ThirdParty/openexr/Deploy/include/ImfChannelList.h"
#include "ThirdParty/openexr/Deploy/include/ImfInputFile.h"
#include "ThirdParty/openexr/Deploy/include/ImfOutputFile.h"
#include "ThirdParty/openexr/Deploy/include/ImfArray.h"
#include "ThirdParty/openexr/Deploy/include/ImfHeader.h"
#include "ThirdParty/openexr/Deploy/include/ImfStdIO.h"
#include "ThirdParty/openexr/Deploy/include/ImfChannelList.h"
#include "ThirdParty/openexr/Deploy/include/ImfRgbaFile.h"
#if defined(_MSC_VER) && USING_CODE_ANALYSIS
	#pragma warning(pop)
#endif
PRAGMA_ENABLE_SHADOW_VARIABLE_WARNINGS

/**
 * OpenEXR implementation of the helper class
 */

class FExrImageWrapper
	: public FImageWrapperBase
{
public:

	/**
	 * Default Constructor.
	 */
	FExrImageWrapper();

public:

	//~ Begin FImageWrapper Interface

	virtual void Compress( int32 Quality ) override;

	virtual void Uncompress( const ERGBFormat::Type InFormat, int32 InBitDepth ) override;
	
	virtual bool SetCompressed( const void* InCompressedData, int32 InCompressedSize ) override;

	//~ End FImageWrapper Interface

private:
	template <Imf::PixelType OutputFormat, typename sourcetype>
	void WriteFrameBufferChannel(Imf::FrameBuffer& ImfFrameBuffer, const char* ChannelName, const sourcetype* SrcData, TArray<uint8>& ChannelBuffer);
	template <Imf::PixelType OutputFormat, typename sourcetype>
	void CompressRaw(const sourcetype* SrcData, bool bIgnoreAlpha);
	const char* GetRawChannelName(int ChannelIndex) const;
	
	bool bUseCompression;
};

#endif // WITH_UNREALEXR