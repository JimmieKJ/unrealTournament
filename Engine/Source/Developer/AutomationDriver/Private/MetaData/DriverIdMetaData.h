// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
#pragma once

class FDriverIdMetaData
	: public ISlateMetaData
{
public:
	SLATE_METADATA_TYPE(FDriverIdMetaData, ISlateMetaData)

	FDriverIdMetaData(FName InId)
		: Id(MoveTemp(InId))
	{ }

	FName Id;
};
