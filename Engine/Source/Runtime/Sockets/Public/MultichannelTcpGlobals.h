// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

SOCKETS_API DECLARE_LOG_CATEGORY_EXTERN(LogMultichannelTCP, Log, All);


/** Magic number used to verify packet header **/
enum
{
	MultichannelMagic = 0xa692339f
};
