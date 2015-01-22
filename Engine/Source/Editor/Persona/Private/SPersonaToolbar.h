// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.


#ifndef __SPersonaToolbar_h__
#define __SPersonaToolbar_h__

#include "Editor/Kismet/Public/SBlueprintEditorToolbar.h"



class FPersonaToolbar : public TSharedFromThis<FPersonaToolbar>
{
public:
	void SetupPersonaToolbar(TSharedPtr<FExtender> Extender, TSharedPtr<FPersona> InPersona);
	
private:
	void FillPersonaModeToolbar(FToolBarBuilder& ToolbarBuilder);
	void FillPersonaToolbar(FToolBarBuilder& ToolbarBuilder);
	
protected:
	/** Returns true if the asset is compatible with the skeleton */
	bool ShouldFilterAssetBasedOnSkeleton(const class FAssetData& AssetData);

	/* Set new reference for skeletal mesh */
	void OnSetSkeletalMeshReference(UObject* Object);

	/* Set new reference for the current animation */
	void OnSetAnimationAssetReference(UObject* Object);

private:
	/** Pointer back to the blueprint editor tool that owns us */
	TWeakPtr<FPersona> Persona;
};



#endif		// __SPersonaToolbar_h__
