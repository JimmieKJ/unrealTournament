// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.


#include "StaticMeshEditorModule.h"
#include "StaticMeshEditorActions.h"

#include "MouseDeltaTracker.h"
#include "SStaticMeshEditorViewport.h"
#include "AdvancedPreviewScene.h"
#include "Runtime/Engine/Public/Slate/SceneViewport.h"
#include "StaticMeshResources.h"


#include "StaticMeshEditor.h"
#include "FbxMeshUtils.h"
#include "BusyCursor.h"
#include "MeshBuild.h"
#include "ObjectTools.h"

#include "ISocketManager.h"
#include "StaticMeshEditorViewportClient.h"
#include "Editor/UnrealEd/Public/STransformViewportToolbar.h"

#include "../Private/GeomFitUtils.h"
#include "ComponentReregisterContext.h"

#include "Runtime/Analytics/Analytics/Public/Interfaces/IAnalyticsProvider.h"
#include "EngineAnalytics.h"
#include "SDockTab.h"

#if WITH_PHYSX
#include "Editor/UnrealEd/Private/EditorPhysXSupport.h"
#endif
#include "Engine/StaticMeshSocket.h"

#define HITPROXY_SOCKET	1

///////////////////////////////////////////////////////////
// SStaticMeshEditorViewportToolbar

// In-viewport toolbar widget used in the static mesh editor
class SStaticMeshEditorViewportToolbar : public SCommonEditorViewportToolbarBase
{
public:
	SLATE_BEGIN_ARGS(SStaticMeshEditorViewportToolbar) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, TSharedPtr<class ICommonEditorViewportToolbarInfoProvider> InInfoProvider)
	{
		SCommonEditorViewportToolbarBase::Construct(SCommonEditorViewportToolbarBase::FArguments(), InInfoProvider);
	}

	// SCommonEditorViewportToolbarBase interface
	virtual TSharedRef<SWidget> GenerateShowMenu() const override
	{
		GetInfoProvider().OnFloatingButtonClicked();

		TSharedRef<SEditorViewport> ViewportRef = GetInfoProvider().GetViewportWidget();

		const bool bInShouldCloseWindowAfterMenuSelection = true;
		FMenuBuilder ShowMenuBuilder(bInShouldCloseWindowAfterMenuSelection, ViewportRef->GetCommandList());
		{
			auto Commands = FStaticMeshEditorCommands::Get();

			ShowMenuBuilder.AddMenuEntry(Commands.SetShowSockets);
			ShowMenuBuilder.AddMenuEntry(Commands.SetShowPivot);
			ShowMenuBuilder.AddMenuEntry(Commands.SetShowVertices);

			ShowMenuBuilder.AddMenuSeparator();

			ShowMenuBuilder.AddMenuEntry(Commands.SetShowGrid);
			ShowMenuBuilder.AddMenuEntry(Commands.SetShowBounds);
			ShowMenuBuilder.AddMenuEntry(Commands.SetShowCollision);

			ShowMenuBuilder.AddMenuSeparator();

			ShowMenuBuilder.AddMenuEntry(Commands.SetShowNormals);
			ShowMenuBuilder.AddMenuEntry(Commands.SetShowTangents);
			ShowMenuBuilder.AddMenuEntry(Commands.SetShowBinormals);

			//ShowMenuBuilder.AddMenuSeparator();
			//ShowMenuBuilder.AddMenuEntry(Commands.SetShowMeshEdges);
		}

		return ShowMenuBuilder.MakeWidget();
	}
	// End of SCommonEditorViewportToolbarBase
};

///////////////////////////////////////////////////////////
// SStaticMeshEditorViewport

void SStaticMeshEditorViewport::Construct(const FArguments& InArgs)
{
	//PreviewScene = new FAdvancedPreviewScene(FPreviewScene::ConstructionValues(), 

	PreviewScene.SetFloorOffset(-InArgs._ObjectToEdit->ExtendedBounds.Origin.Z + InArgs._ObjectToEdit->ExtendedBounds.BoxExtent.Z);

	StaticMeshEditorPtr = InArgs._StaticMeshEditor;

	StaticMesh = InArgs._ObjectToEdit;

	CurrentViewMode = VMI_Lit;

	SEditorViewport::Construct( SEditorViewport::FArguments() );

	PreviewMeshComponent = NewObject<UStaticMeshComponent>(GetTransientPackage(), NAME_None, RF_Transient );

	SetPreviewMesh(StaticMesh);

	ViewportOverlay->AddSlot()
		.VAlign(VAlign_Top)
		.HAlign(HAlign_Left)
		.Padding(FMargin(10.0f, 40.0f, 10.0f, 10.0f))
		[
			SAssignNew(OverlayTextVerticalBox, SVerticalBox)
		];

	FCoreUObjectDelegates::OnObjectPropertyChanged.AddRaw(this, &SStaticMeshEditorViewport::OnObjectPropertyChanged);
}

SStaticMeshEditorViewport::SStaticMeshEditorViewport()
	: PreviewScene(FPreviewScene::ConstructionValues())
{

}

SStaticMeshEditorViewport::~SStaticMeshEditorViewport()
{
	FCoreUObjectDelegates::OnObjectPropertyChanged.RemoveAll(this);
	if (EditorViewportClient.IsValid())
	{
		EditorViewportClient->Viewport = NULL;
	}
}

void SStaticMeshEditorViewport::PopulateOverlayText(const TArray<FOverlayTextItem>& TextItems)
{
	OverlayTextVerticalBox->ClearChildren();

	for (const auto& TextItem : TextItems)
	{
		OverlayTextVerticalBox->AddSlot()
		[
			SNew(STextBlock)
			.Text(TextItem.Text)
			.TextStyle(FEditorStyle::Get(), TextItem.Style)
		];
	}
}

TSharedRef<class SEditorViewport> SStaticMeshEditorViewport::GetViewportWidget()
{
	return SharedThis(this);
}

TSharedPtr<FExtender> SStaticMeshEditorViewport::GetExtenders() const
{
	TSharedPtr<FExtender> Result(MakeShareable(new FExtender));
	return Result;
}

void SStaticMeshEditorViewport::OnFloatingButtonClicked()
{
}

void SStaticMeshEditorViewport::AddReferencedObjects( FReferenceCollector& Collector )
{
	Collector.AddReferencedObject( PreviewMeshComponent );
	Collector.AddReferencedObject( StaticMesh );
}

void SStaticMeshEditorViewport::RefreshViewport()
{
	// Invalidate the viewport's display.
	SceneViewport->Invalidate();
}

void SStaticMeshEditorViewport::OnObjectPropertyChanged(UObject* ObjectBeingModified, FPropertyChangedEvent& PropertyChangedEvent)
{
	if ( !ensure(ObjectBeingModified) )
	{
		return;
	}

	if( PreviewMeshComponent )
	{
		bool bShouldUpdatePreviewSocketMeshes = (ObjectBeingModified == PreviewMeshComponent->StaticMesh);
		if( !bShouldUpdatePreviewSocketMeshes && PreviewMeshComponent->StaticMesh )
		{
			const int32 SocketCount = PreviewMeshComponent->StaticMesh->Sockets.Num();
			for( int32 i = 0; i < SocketCount; ++i )
			{
				if( ObjectBeingModified == PreviewMeshComponent->StaticMesh->Sockets[i] )
				{
					bShouldUpdatePreviewSocketMeshes = true;
					break;
				}
			}
		}

		if( bShouldUpdatePreviewSocketMeshes )
		{
			UpdatePreviewSocketMeshes();
			RefreshViewport();
		}
	}
}

void SStaticMeshEditorViewport::UpdatePreviewSocketMeshes()
{
	UStaticMesh* const PreviewStaticMesh = PreviewMeshComponent ? PreviewMeshComponent->StaticMesh : NULL;

	if( PreviewStaticMesh )
	{
		const int32 SocketedComponentCount = SocketPreviewMeshComponents.Num();
		const int32 SocketCount = PreviewStaticMesh->Sockets.Num();

		const int32 IterationCount = FMath::Max(SocketedComponentCount, SocketCount);
		for(int32 i = 0; i < IterationCount; ++i)
		{
			if(i >= SocketCount)
			{
				// Handle removing an old component
				UStaticMeshComponent* SocketPreviewMeshComponent = SocketPreviewMeshComponents[i];
				PreviewScene.RemoveComponent(SocketPreviewMeshComponent);
				SocketPreviewMeshComponents.RemoveAt(i, SocketedComponentCount - i);
				break;
			}
			else if(UStaticMeshSocket* Socket = PreviewStaticMesh->Sockets[i])
			{
				UStaticMeshComponent* SocketPreviewMeshComponent = NULL;

				// Handle adding a new component
				if(i >= SocketedComponentCount)
				{
					SocketPreviewMeshComponent = NewObject<UStaticMeshComponent>();
					PreviewScene.AddComponent(SocketPreviewMeshComponent, FTransform::Identity);
					SocketPreviewMeshComponents.Add(SocketPreviewMeshComponent);
					SocketPreviewMeshComponent->SnapTo(PreviewMeshComponent, Socket->SocketName);
				}
				else
				{
					SocketPreviewMeshComponent = SocketPreviewMeshComponents[i];

					// In case of a socket rename, ensure our preview component is still snapping to the proper socket
					if (!SocketPreviewMeshComponent->GetAttachSocketName().IsEqual(Socket->SocketName))
					{
						SocketPreviewMeshComponent->SnapTo(PreviewMeshComponent, Socket->SocketName);
					}

					// Force component to world update to take into account the new socket position.
					SocketPreviewMeshComponent->UpdateComponentToWorld();
				}

				SocketPreviewMeshComponent->SetStaticMesh(Socket->PreviewStaticMesh);
			}
		}
	}
}

void SStaticMeshEditorViewport::SetPreviewMesh(UStaticMesh* InStaticMesh)
{
	// Set the new preview static mesh.
	FComponentReregisterContext ReregisterContext( PreviewMeshComponent );
	PreviewMeshComponent->StaticMesh = InStaticMesh;

	FTransform Transform = FTransform::Identity;
	PreviewScene.AddComponent( PreviewMeshComponent, Transform );

	EditorViewportClient->SetPreviewMesh(InStaticMesh, PreviewMeshComponent);
}

void SStaticMeshEditorViewport::UpdatePreviewMesh(UStaticMesh* InStaticMesh)
{
	{
		const int32 SocketedComponentCount = SocketPreviewMeshComponents.Num();
		for(int32 i = 0; i < SocketedComponentCount; ++i)
		{
			UStaticMeshComponent* SocketPreviewMeshComponent = SocketPreviewMeshComponents[i];
			if( SocketPreviewMeshComponent )
			{
				PreviewScene.RemoveComponent(SocketPreviewMeshComponent);
			}
		}
		SocketPreviewMeshComponents.Empty();
	}

	if (PreviewMeshComponent)
	{
		PreviewScene.RemoveComponent(PreviewMeshComponent);
		PreviewMeshComponent = NULL;
	}

	PreviewMeshComponent = NewObject<UStaticMeshComponent>();

	PreviewMeshComponent->SetStaticMesh(InStaticMesh);

	// Update streaming data for debug viewmode feedback
	PreviewMeshComponent->UpdateStreamingSectionData(FTexCoordScaleMap());

	PreviewScene.AddComponent(PreviewMeshComponent,FTransform::Identity);

	const int32 SocketCount = InStaticMesh->Sockets.Num();
	SocketPreviewMeshComponents.Reserve(SocketCount);
	for(int32 i = 0; i < SocketCount; ++i)
	{
		UStaticMeshSocket* Socket = InStaticMesh->Sockets[i];

		UStaticMeshComponent* SocketPreviewMeshComponent = NULL;
		if( Socket && Socket->PreviewStaticMesh )
		{
			SocketPreviewMeshComponent = NewObject<UStaticMeshComponent>();
			SocketPreviewMeshComponent->SetStaticMesh(Socket->PreviewStaticMesh);
			SocketPreviewMeshComponent->SnapTo(PreviewMeshComponent, Socket->SocketName);
			SocketPreviewMeshComponents.Add(SocketPreviewMeshComponent);
			PreviewScene.AddComponent(SocketPreviewMeshComponent, FTransform::Identity);
		}
	}

	EditorViewportClient->SetPreviewMesh(InStaticMesh, PreviewMeshComponent);
}

bool SStaticMeshEditorViewport::IsVisible() const
{
	return ViewportWidget.IsValid() && (!ParentTab.IsValid() || ParentTab.Pin()->IsForeground());
}

UStaticMeshComponent* SStaticMeshEditorViewport::GetStaticMeshComponent() const
{
	return PreviewMeshComponent;
}

void SStaticMeshEditorViewport::SetViewModeWireframe()
{
	if(CurrentViewMode != VMI_Wireframe)
	{
		CurrentViewMode = VMI_Wireframe;
	}
	else
	{
		CurrentViewMode = VMI_Lit;
	}
	if (FEngineAnalytics::IsAvailable())
	{
		FEngineAnalytics::GetProvider().RecordEvent(TEXT("Editor.Usage.StaticMesh.Toolbar"), TEXT("CurrentViewMode"), FString::Printf(TEXT("%d"), static_cast<int32>(CurrentViewMode)));
	}
	EditorViewportClient->SetViewMode(CurrentViewMode);
	SceneViewport->Invalidate();

}

bool SStaticMeshEditorViewport::IsInViewModeWireframeChecked() const
{
	return CurrentViewMode == VMI_Wireframe;
}

void SStaticMeshEditorViewport::SetViewModeVertexColor()
{
	if (!EditorViewportClient->EngineShowFlags.VertexColors)
	{
		EditorViewportClient->EngineShowFlags.SetVertexColors(true);
		EditorViewportClient->EngineShowFlags.SetLighting(false);
		EditorViewportClient->EngineShowFlags.SetIndirectLightingCache(false);
		EditorViewportClient->SetFloorAndEnvironmentVisibility(false);
	}
	else
	{
		EditorViewportClient->EngineShowFlags.SetVertexColors(false);
		EditorViewportClient->EngineShowFlags.SetLighting(true);
		EditorViewportClient->EngineShowFlags.SetIndirectLightingCache(true);
		EditorViewportClient->SetFloorAndEnvironmentVisibility(true);
	}
	if (FEngineAnalytics::IsAvailable())
	{
		FEngineAnalytics::GetProvider().RecordEvent(TEXT("Editor.Usage.StaticMesh.Toolbar"), FAnalyticsEventAttribute(TEXT("VertexColors"), AnalyticsConversion::ToString(EditorViewportClient->EngineShowFlags.VertexColors)));
	}
	SceneViewport->Invalidate();
}

bool SStaticMeshEditorViewport::IsInViewModeVertexColorChecked() const
{
	return EditorViewportClient->EngineShowFlags.VertexColors;
}

void SStaticMeshEditorViewport::ForceLODLevel(int32 InForcedLOD)
{
	PreviewMeshComponent->ForcedLodModel = InForcedLOD;
	{FComponentReregisterContext ReregisterContext(PreviewMeshComponent);}
	SceneViewport->Invalidate();
}

TSet< int32 >& SStaticMeshEditorViewport::GetSelectedEdges()
{
	return EditorViewportClient->GetSelectedEdges();
}

FStaticMeshEditorViewportClient& SStaticMeshEditorViewport::GetViewportClient()
{
	return *EditorViewportClient;
}


TSharedRef<FEditorViewportClient> SStaticMeshEditorViewport::MakeEditorViewportClient()
{
	EditorViewportClient = MakeShareable( new FStaticMeshEditorViewportClient(StaticMeshEditorPtr, SharedThis(this), PreviewScene, StaticMesh, NULL) );

	EditorViewportClient->bSetListenerPosition = false;

	EditorViewportClient->SetRealtime( true );
	EditorViewportClient->VisibilityDelegate.BindSP( this, &SStaticMeshEditorViewport::IsVisible );

	return EditorViewportClient.ToSharedRef();
}

TSharedPtr<SWidget> SStaticMeshEditorViewport::MakeViewportToolbar()
{
	return SNew(SStaticMeshEditorViewportToolbar, SharedThis(this));
}

EVisibility SStaticMeshEditorViewport::OnGetViewportContentVisibility() const
{
	return IsVisible() ? EVisibility::Visible : EVisibility::Collapsed;
}

void SStaticMeshEditorViewport::BindCommands()
{
	SEditorViewport::BindCommands();

	const FStaticMeshEditorCommands& Commands = FStaticMeshEditorCommands::Get();

	TSharedRef<FStaticMeshEditorViewportClient> EditorViewportClientRef = EditorViewportClient.ToSharedRef();

	CommandList->MapAction(
		Commands.SetShowWireframe,
		FExecuteAction::CreateSP( this, &SStaticMeshEditorViewport::SetViewModeWireframe ),
		FCanExecuteAction(),
		FIsActionChecked::CreateSP( this, &SStaticMeshEditorViewport::IsInViewModeWireframeChecked ) );

	CommandList->MapAction(
		Commands.SetShowVertexColor,
		FExecuteAction::CreateSP( this, &SStaticMeshEditorViewport::SetViewModeVertexColor ),
		FCanExecuteAction(),
		FIsActionChecked::CreateSP( this, &SStaticMeshEditorViewport::IsInViewModeVertexColorChecked ) );

	CommandList->MapAction(
		Commands.ResetCamera,
		FExecuteAction::CreateSP( EditorViewportClientRef, &FStaticMeshEditorViewportClient::ResetCamera ) );

	CommandList->MapAction(
		Commands.SetDrawUVs,
		FExecuteAction::CreateSP( EditorViewportClientRef, &FStaticMeshEditorViewportClient::SetDrawUVOverlay ),
		FCanExecuteAction(),
		FIsActionChecked::CreateSP( EditorViewportClientRef, &FStaticMeshEditorViewportClient::IsSetDrawUVOverlayChecked ) );

	CommandList->MapAction(
		Commands.SetShowGrid,
		FExecuteAction::CreateSP( EditorViewportClientRef, &FStaticMeshEditorViewportClient::SetShowGrid ),
		FCanExecuteAction(),
		FIsActionChecked::CreateSP( EditorViewportClientRef, &FStaticMeshEditorViewportClient::IsSetShowGridChecked ) );

	CommandList->MapAction(
		Commands.SetShowBounds,
		FExecuteAction::CreateSP( EditorViewportClientRef, &FStaticMeshEditorViewportClient::ToggleShowBounds ),
		FCanExecuteAction(),
		FIsActionChecked::CreateSP( EditorViewportClientRef, &FStaticMeshEditorViewportClient::IsSetShowBoundsChecked ) );

	CommandList->MapAction(
		Commands.SetShowCollision,
		FExecuteAction::CreateSP( EditorViewportClientRef, &FStaticMeshEditorViewportClient::SetShowWireframeCollision ),
		FCanExecuteAction(),
		FIsActionChecked::CreateSP( EditorViewportClientRef, &FStaticMeshEditorViewportClient::IsSetShowWireframeCollisionChecked ) );

	CommandList->MapAction(
		Commands.SetShowSockets,
		FExecuteAction::CreateSP( EditorViewportClientRef, &FStaticMeshEditorViewportClient::SetShowSockets ),
		FCanExecuteAction(),
		FIsActionChecked::CreateSP( EditorViewportClientRef, &FStaticMeshEditorViewportClient::IsSetShowSocketsChecked ) );

	// Menu
	CommandList->MapAction(
		Commands.SetShowNormals,
		FExecuteAction::CreateSP( EditorViewportClientRef, &FStaticMeshEditorViewportClient::SetShowNormals ),
		FCanExecuteAction(),
		FIsActionChecked::CreateSP( EditorViewportClientRef, &FStaticMeshEditorViewportClient::IsSetShowNormalsChecked ) );

	CommandList->MapAction(
		Commands.SetShowTangents,
		FExecuteAction::CreateSP( EditorViewportClientRef, &FStaticMeshEditorViewportClient::SetShowTangents ),
		FCanExecuteAction(),
		FIsActionChecked::CreateSP( EditorViewportClientRef, &FStaticMeshEditorViewportClient::IsSetShowTangentsChecked ) );

	CommandList->MapAction(
		Commands.SetShowBinormals,
		FExecuteAction::CreateSP( EditorViewportClientRef, &FStaticMeshEditorViewportClient::SetShowBinormals ),
		FCanExecuteAction(),
		FIsActionChecked::CreateSP( EditorViewportClientRef, &FStaticMeshEditorViewportClient::IsSetShowBinormalsChecked ) );

	CommandList->MapAction(
		Commands.SetShowPivot,
		FExecuteAction::CreateSP( EditorViewportClientRef, &FStaticMeshEditorViewportClient::SetShowPivot ),
		FCanExecuteAction(),
		FIsActionChecked::CreateSP( EditorViewportClientRef, &FStaticMeshEditorViewportClient::IsSetShowPivotChecked ) );

	CommandList->MapAction(
		Commands.SetDrawAdditionalData,
		FExecuteAction::CreateSP( EditorViewportClientRef, &FStaticMeshEditorViewportClient::SetDrawAdditionalData ),
		FCanExecuteAction(),
		FIsActionChecked::CreateSP( EditorViewportClientRef, &FStaticMeshEditorViewportClient::IsSetDrawAdditionalData ) );

	CommandList->MapAction(
		Commands.SetShowVertices,
		FExecuteAction::CreateSP(EditorViewportClientRef, &FStaticMeshEditorViewportClient::SetDrawVertices ),
		FCanExecuteAction(),
		FIsActionChecked::CreateSP(EditorViewportClientRef, &FStaticMeshEditorViewportClient::IsSetDrawVerticesChecked ) );
}

void SStaticMeshEditorViewport::OnFocusViewportToSelection()
{
	// If we have selected sockets, focus on them
	UStaticMeshSocket* SelectedSocket = StaticMeshEditorPtr.Pin()->GetSelectedSocket();
	if( SelectedSocket && PreviewMeshComponent )
	{
		FTransform SocketTransform;
		SelectedSocket->GetSocketTransform( SocketTransform, PreviewMeshComponent );

		const FVector Extent(30.0f);

		const FVector Origin = SocketTransform.GetLocation();
		const FBox Box(Origin - Extent, Origin + Extent);

		EditorViewportClient->FocusViewportOnBox( Box );
		return;
	}

	// If we have selected primitives, focus on them 
	FBox Box(0);
	const bool bSelectedPrim = StaticMeshEditorPtr.Pin()->CalcSelectedPrimsAABB(Box);
	if (bSelectedPrim)
	{
		EditorViewportClient->FocusViewportOnBox(Box);
		return;
	}

	// Fallback to focusing on the mesh, if nothing else
	if( PreviewMeshComponent )
	{
		EditorViewportClient->FocusViewportOnBox( PreviewMeshComponent->Bounds.GetBox() );
		return;
	}
}