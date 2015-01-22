// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IDocumentation.h"
#define LOCTEXT_NAMESPACE "PersonaMeshMode"


/////////////////////////////////////////////////////
// FMeshPropertiesSummoner

struct FMeshPropertiesSummoner : public FWorkflowTabFactory
{
	FMeshPropertiesSummoner(TSharedPtr<class FAssetEditorToolkit> InHostingApp);

	// FWorkflowTabFactory interface
	virtual TSharedRef<SWidget> CreateTabBody(const FWorkflowTabSpawnInfo& Info) const override;

	// Create a tooltip widget for the tab
	virtual TSharedPtr<SToolTip> CreateTabToolTipWidget(const FWorkflowTabSpawnInfo& Info) const override
	{
		return  IDocumentation::Get()->CreateToolTip(LOCTEXT("MeshDetailsTooltip", "The Mesh Details tab lets you edit properties (materials etc) of the current Skeletal Mesh."), NULL, TEXT("Shared/Editors/Persona"), TEXT("MeshDetail_Window"));
	}
	// FWorkflowTabFactory interface
};


/////////////////////////////////////////////////////
// FMeshEditAppMode

class FMeshEditAppMode : public FPersonaAppMode
{
public:
	FMeshEditAppMode(TSharedPtr<FPersona> InPersona);

	// FApplicationMode interface
	// End of FApplicationMode interface
};

#undef LOCTEXT_NAMESPACE
