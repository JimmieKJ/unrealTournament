// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "IMergeActorsTool.h"

/**
* Mesh Proxy Tool
*/
class FMeshProxyTool : public IMergeActorsTool
{
	friend class SMeshProxyDialog;

public:

	// IMergeActorsTool interface
	virtual TSharedRef<SWidget> GetWidget() override;
	virtual FName GetIconName() const override { return "MergeActors.MeshProxyTool"; }
	virtual FText GetTooltipText() const override;
	virtual FString GetDefaultPackageName() const override;
	virtual bool RunMerge(const FString& PackageName) override;

private:

	FMeshProxySettings ProxySettings;
};
