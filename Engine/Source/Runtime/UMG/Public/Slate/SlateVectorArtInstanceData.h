// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once


struct UMG_API FSlateVectorArtInstanceData
{
public:
	const FVector4& GetData() const { return Data; }
	FVector4& GetData() { return Data; }

	void SetPositionFixedPoint16(FVector2D Position);

	void SetPosition(FVector2D Position);
	void SetScale(float Scale);
	void SetBaseAddress(float Address);

protected:
	FVector4 Data;
};