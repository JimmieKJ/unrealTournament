// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "UnrealEd.h"
#include "Engine/BookMark.h"
#include "StaticMeshResources.h"
#include "EditorSupportDelegates.h"
#include "MouseDeltaTracker.h"
#include "ScopedTransaction.h"
#include "SurfaceIterators.h"
#include "SoundDefinitions.h"
#include "LevelEditor.h"
#include "Toolkits/ToolkitManager.h"
#include "EditorLevelUtils.h"
#include "DynamicMeshBuilder.h"

#include "ActorEditorUtils.h"
#include "EditorStyle.h"
#include "ComponentVisualizer.h"
#include "SNotificationList.h"
#include "NotificationManager.h"
#include "Engine/Selection.h"
#include "EngineUtils.h"
#include "CanvasItem.h"
#include "CanvasTypes.h"
#include "Engine/Polys.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/LevelStreaming.h"

/** Hit proxy used for editable properties */
struct HPropertyWidgetProxy : public HHitProxy
{
	DECLARE_HIT_PROXY();

	/** Name of property this is the widget for */
	FString	PropertyName;

	/** If the property is an array property, the index into that array that this widget is for */
	int32	PropertyIndex;

	/** This property is a transform */
	bool	bPropertyIsTransform;

	HPropertyWidgetProxy(FString InPropertyName, int32 InPropertyIndex, bool bInPropertyIsTransform)
		: HHitProxy(HPP_Foreground)
		, PropertyName(InPropertyName)
		, PropertyIndex(InPropertyIndex)
		, bPropertyIsTransform(bInPropertyIsTransform)
	{}

	/** Show cursor as cross when over this handle */
	virtual EMouseCursor::Type GetMouseCursor()
	{
		return EMouseCursor::Crosshairs;
	}
};

IMPLEMENT_HIT_PROXY(HPropertyWidgetProxy, HHitProxy);


namespace
{
	/**
	 * Returns a reference to the named property value data in the given container.
	 */
	template<typename T>
	T* GetPropertyValuePtrByName(const UStruct* InStruct, void* InContainer, FString PropertyName, int32 ArrayIndex)
	{
		T* ValuePtr = NULL;

		// Extract the vector ptr recursively using the property name
		int32 DelimPos = PropertyName.Find(TEXT("."));
		if(DelimPos != INDEX_NONE)
		{
			// Parse the property name and (optional) array index
			int32 SubArrayIndex = 0;
			FString NameToken = PropertyName.Left(DelimPos);
			int32 ArrayPos = NameToken.Find(TEXT("["));
			if(ArrayPos != INDEX_NONE)
			{
				FString IndexToken = NameToken.RightChop(ArrayPos + 1).LeftChop(1);
				SubArrayIndex = FCString::Atoi(*IndexToken);

				NameToken = PropertyName.Left(ArrayPos);
			}

			// Obtain the property info from the given structure definition
			UProperty* CurrentProp = FindField<UProperty>(InStruct, FName(*NameToken));

			// Check first to see if this is a simple structure (i.e. not an array of structures)
			UStructProperty* StructProp = Cast<UStructProperty>(CurrentProp);
			if(StructProp != NULL)
			{
				// Recursively call back into this function with the structure property and container value
				ValuePtr = GetPropertyValuePtrByName<T>(StructProp->Struct, StructProp->ContainerPtrToValuePtr<void>(InContainer), PropertyName.RightChop(DelimPos + 1), ArrayIndex);
			}
			else
			{
				// Check to see if this is an array
				UArrayProperty* ArrayProp = Cast<UArrayProperty>(CurrentProp);
				if(ArrayProp != NULL)
				{
					// It is an array, now check to see if this is an array of structures
					StructProp = Cast<UStructProperty>(ArrayProp->Inner);
					if(StructProp != NULL)
					{
						FScriptArrayHelper_InContainer ArrayHelper(ArrayProp, InContainer);
						if(ArrayHelper.IsValidIndex(SubArrayIndex))
						{
							// Recursively call back into this function with the array element and container value
							ValuePtr = GetPropertyValuePtrByName<T>(StructProp->Struct, ArrayHelper.GetRawPtr(SubArrayIndex), PropertyName.RightChop(DelimPos + 1), ArrayIndex);
						}
					}
				}
			}
		}
		else
		{
			UProperty* Prop = FindField<UProperty>(InStruct, FName(*PropertyName));
			if(Prop != NULL)
			{
				if( UArrayProperty* ArrayProp = Cast<UArrayProperty>(Prop) )
				{
					check(ArrayIndex != INDEX_NONE);

					// Property is an array property, so make sure we have a valid index specified
					FScriptArrayHelper_InContainer ArrayHelper(ArrayProp, InContainer);
					if( ArrayHelper.IsValidIndex(ArrayIndex) )
					{
						ValuePtr = (T*)ArrayHelper.GetRawPtr(ArrayIndex);
					}
				}
				else
				{
					// Property is a vector property, so access directly
					ValuePtr = Prop->ContainerPtrToValuePtr<T>(InContainer);
				}
			}
		}

		return ValuePtr;
	}

	/**
	 * Returns the value of the property with the given name in the given Actor instance.
	 */
	template<typename T>
	T GetPropertyValueByName(AActor* Actor, FString PropertyName, int32 PropertyIndex)
	{
		T Value;
		T* ValuePtr = GetPropertyValuePtrByName<T>(Actor->GetClass(), Actor, PropertyName, PropertyIndex);
		if(ValuePtr)
		{
			Value = *ValuePtr;
		}
		return Value;
	}

	/**
	 * Sets the property with the given name in the given Actor instance to the given value.
	 */
	template<typename T>
	void SetPropertyValueByName(AActor* Actor, FString PropertyName, int32 PropertyIndex, const T& InValue)
	{
		T* ValuePtr = GetPropertyValuePtrByName<T>(Actor->GetClass(), Actor, PropertyName, PropertyIndex);
		if(ValuePtr)
		{
			*ValuePtr = InValue;
		}
	}
}

//////////////////////////////////
// FEdMode

const FName FEdMode::MD_MakeEditWidget(TEXT("MakeEditWidget"));
const FName FEdMode::MD_ValidateWidgetUsing(TEXT("ValidateWidgetUsing"));

FEdMode::FEdMode()
	: bPendingDeletion(false)
	, CurrentWidgetAxis(EAxisList::None)
	, CurrentTool(nullptr)
	, Owner(nullptr)
	, EditedPropertyIndex(INDEX_NONE)
	, bEditedPropertyIsTransform(false)
{
	bDrawKillZ = true;
}

FEdMode::~FEdMode()
{
}

void FEdMode::OnModeUnregistered( FEditorModeID ModeID )
{
	if( ModeID == Info.ID )
	{
		// This should be synonymous with "delete this"
		Owner->DestroyMode(ModeID);
	}
}

bool FEdMode::MouseEnter( FEditorViewportClient* ViewportClient,FViewport* Viewport,int32 x, int32 y )
{
	if( GetCurrentTool() )
	{
		return GetCurrentTool()->MouseEnter( ViewportClient, Viewport, x, y );
	}

	return false;
}

bool FEdMode::MouseLeave( FEditorViewportClient* ViewportClient,FViewport* Viewport )
{
	if( GetCurrentTool() )
	{
		return GetCurrentTool()->MouseLeave( ViewportClient, Viewport );
	}

	return false;
}

bool FEdMode::MouseMove(FEditorViewportClient* ViewportClient,FViewport* Viewport,int32 x, int32 y)
{
	if( GetCurrentTool() )
	{
		return GetCurrentTool()->MouseMove( ViewportClient, Viewport, x, y );
	}

	return false;
}

bool FEdMode::ReceivedFocus(FEditorViewportClient* ViewportClient,FViewport* Viewport)
{
	if( GetCurrentTool() )
	{
		return GetCurrentTool()->ReceivedFocus( ViewportClient, Viewport );
	}

	return false;
}

bool FEdMode::LostFocus(FEditorViewportClient* ViewportClient,FViewport* Viewport)
{
	if( GetCurrentTool() )
	{
		return GetCurrentTool()->LostFocus( ViewportClient, Viewport );
	}

	return false;
}

bool FEdMode::CapturedMouseMove( FEditorViewportClient* InViewportClient, FViewport* InViewport, int32 InMouseX, int32 InMouseY )
{
	if( GetCurrentTool() )
	{
		return GetCurrentTool()->CapturedMouseMove( InViewportClient, InViewport, InMouseX, InMouseY );
	}

	return false;
}

bool FEdMode::InputKey(FEditorViewportClient* ViewportClient, FViewport* Viewport, FKey Key, EInputEvent Event)
{
	if( GetCurrentTool() && GetCurrentTool()->InputKey( ViewportClient, Viewport, Key, Event ) )
	{
		return true;
	}
	else
	{
		// Pass input up to selected actors if not in a tool mode
		TArray<AActor*> SelectedActors;
		Owner->GetSelectedActors()->GetSelectedObjects<AActor>(SelectedActors);

		for( TArray<AActor*>::TIterator it(SelectedActors); it; ++it )
		{
			// Tell the object we've had a key press
			(*it)->EditorKeyPressed(Key, Event);
		}
	}

	return 0;
}

bool FEdMode::InputAxis(FEditorViewportClient* InViewportClient, FViewport* Viewport, int32 ControllerId, FKey Key, float Delta, float DeltaTime)
{
	FModeTool* Tool = GetCurrentTool();
	if (Tool)
	{
		return Tool->InputAxis(InViewportClient, Viewport, ControllerId, Key, Delta, DeltaTime);
	}

	return false;
}

bool FEdMode::InputDelta(FEditorViewportClient* InViewportClient, FViewport* InViewport, FVector& InDrag, FRotator& InRot, FVector& InScale)
{	
	if(UsesPropertyWidgets())
	{
		AActor* SelectedActor = GetFirstSelectedActorInstance();
		if(SelectedActor != NULL && InViewportClient->GetCurrentWidgetAxis() != EAxisList::None)
		{
			GEditor->NoteActorMovement();

			if (EditedPropertyName != TEXT(""))
			{
				FTransform LocalTM = FTransform::Identity;

				if(bEditedPropertyIsTransform)
				{
					LocalTM = GetPropertyValueByName<FTransform>(SelectedActor, EditedPropertyName, EditedPropertyIndex);
				}
				else
				{					
					FVector LocalPos = GetPropertyValueByName<FVector>(SelectedActor, EditedPropertyName, EditedPropertyIndex);
					LocalTM = FTransform(LocalPos);
				}

				// Get actor transform (actor to world)
				FTransform ActorTM = SelectedActor->ActorToWorld();
				// Calculate world transform
				FTransform WorldTM = LocalTM * ActorTM;
				// Calc delta specified by drag
				//FTransform DeltaTM(InRot.Quaternion(), InDrag);
				// Apply delta in world space
				WorldTM.SetTranslation(WorldTM.GetTranslation() + InDrag);
				WorldTM.SetRotation(InRot.Quaternion() * WorldTM.GetRotation());
				// Convert new world transform back into local space
				LocalTM = WorldTM.GetRelativeTransform(ActorTM);
				// Apply delta scale
				LocalTM.SetScale3D(LocalTM.GetScale3D() + InScale);

				SelectedActor->PreEditChange(NULL);

				if(bEditedPropertyIsTransform)
				{
					SetPropertyValueByName<FTransform>(SelectedActor, EditedPropertyName, EditedPropertyIndex, LocalTM);
				}
				else
				{
					SetPropertyValueByName<FVector>(SelectedActor, EditedPropertyName, EditedPropertyIndex, LocalTM.GetLocation());
				}

				SelectedActor->PostEditChange();

				return true;
			}
		}
	}

	if( GetCurrentTool() )
	{
		return GetCurrentTool()->InputDelta(InViewportClient,InViewport,InDrag,InRot,InScale);
	}

	return 0;
}

/**
 * Lets each tool determine if it wants to use the editor widget or not.  If the tool doesn't want to use it, it will be
 * fed raw mouse delta information (not snapped or altered in any way).
 */

bool FEdMode::UsesTransformWidget() const
{
	if( GetCurrentTool() )
	{
		return GetCurrentTool()->UseWidget();
	}

	return 1;
}

/**
 * Lets each mode selectively exclude certain widget types.
 */
bool FEdMode::UsesTransformWidget(FWidget::EWidgetMode CheckMode) const
{
	if(UsesPropertyWidgets())
	{
		AActor* SelectedActor = GetFirstSelectedActorInstance();
		if(SelectedActor != NULL)
		{
			// If editing a vector (not a transform)
			if(EditedPropertyName != TEXT("") && !bEditedPropertyIsTransform)
			{
				return (CheckMode == FWidget::WM_Translate);
			}
		}
	}

	return true;
}

/**
 * Allows each mode/tool to determine a good location for the widget to be drawn at.
 */

FVector FEdMode::GetWidgetLocation() const
{
	if(UsesPropertyWidgets())
	{
		AActor* SelectedActor = GetFirstSelectedActorInstance();
		if(SelectedActor != NULL)
		{
			if(EditedPropertyName != TEXT(""))
			{
				FVector LocalPos = FVector::ZeroVector;

				if(bEditedPropertyIsTransform)
				{
					FTransform LocalTM = GetPropertyValueByName<FTransform>(SelectedActor, EditedPropertyName, EditedPropertyIndex);
					LocalPos = LocalTM.GetLocation();
				}
				else
				{
					LocalPos = GetPropertyValueByName<FVector>(SelectedActor, EditedPropertyName, EditedPropertyIndex);
				}

				FTransform ActorToWorld = SelectedActor->ActorToWorld();
				FVector WorldPos = ActorToWorld.TransformPosition(LocalPos);
				return WorldPos;
			}
		}
	}

	//UE_LOG(LogEditorModes, Log, TEXT("In FEdMode::GetWidgetLocation"));
	return Owner->PivotLocation;
}

/**
 * Lets the mode determine if it wants to draw the widget or not.
 */

bool FEdMode::ShouldDrawWidget() const
{
	return (Owner->GetSelectedActors()->GetTop<AActor>() != NULL);
}

/**
 * Allows each mode to customize the axis pieces of the widget they want drawn.
 *
 * @param	InwidgetMode	The current widget mode
 *
 * @return	A bitfield comprised of AXIS_ values
 */

EAxisList::Type FEdMode::GetWidgetAxisToDraw( FWidget::EWidgetMode InWidgetMode ) const
{
	return EAxisList::All;
}

/**
 * Lets each mode/tool handle box selection in its own way.
 *
 * @param	InBox	The selection box to use, in worldspace coordinates.
 * @return		true if something was selected/deselected, false otherwise.
 */
bool FEdMode::BoxSelect( FBox& InBox, bool InSelect )
{
	bool bResult = false;
	if( GetCurrentTool() )
	{
		bResult = GetCurrentTool()->BoxSelect( InBox, InSelect );
	}
	return bResult;
}

/**
 * Lets each mode/tool handle frustum selection in its own way.
 *
 * @param	InFrustum	The selection frustum to use, in worldspace coordinates.
 * @return	true if something was selected/deselected, false otherwise.
 */
bool FEdMode::FrustumSelect( const FConvexVolume& InFrustum, bool InSelect )
{
	bool bResult = false;
	if( GetCurrentTool() )
	{
		bResult = GetCurrentTool()->FrustumSelect( InFrustum, InSelect );
	}
	return bResult;
}

void FEdMode::SelectNone()
{
	if( GetCurrentTool() )
	{
		GetCurrentTool()->SelectNone();
	}
}

void FEdMode::Tick(FEditorViewportClient* ViewportClient,float DeltaTime)
{
	if( GetCurrentTool() )
	{
		GetCurrentTool()->Tick(ViewportClient,DeltaTime);
	}
}

void FEdMode::ActorSelectionChangeNotify()
{
	EditedPropertyName = TEXT("");
	EditedPropertyIndex = INDEX_NONE;
	bEditedPropertyIsTransform = false;
}

bool FEdMode::HandleClick(FEditorViewportClient* InViewportClient, HHitProxy *HitProxy, const FViewportClick &Click)
{
	if(UsesPropertyWidgets() && HitProxy)
	{
		if( HitProxy->IsA(HPropertyWidgetProxy::StaticGetType()) )
		{
			HPropertyWidgetProxy* PropertyProxy = (HPropertyWidgetProxy*)HitProxy;
			EditedPropertyName = PropertyProxy->PropertyName;
			EditedPropertyIndex = PropertyProxy->PropertyIndex;
			bEditedPropertyIsTransform = PropertyProxy->bPropertyIsTransform;
			return true;
		}
		// Left clicking on an actor, stop editing a property
		else if( HitProxy->IsA(HActor::StaticGetType()) )
		{
			EditedPropertyName = TEXT("");
			EditedPropertyIndex = INDEX_NONE;
			bEditedPropertyIsTransform = false;
		}
	}

	return false;
}

void FEdMode::Enter()
{
	// Update components for selected actors, in case the mode we just exited
	// was hijacking selection events selection and not updating components.
	for ( FSelectionIterator It( *Owner->GetSelectedActors() ) ; It ; ++It )
	{
		AActor* SelectedActor = CastChecked<AActor>( *It );
		SelectedActor->MarkComponentsRenderStateDirty();
	}

	bPendingDeletion = false;

	FEditorDelegates::EditorModeEnter.Broadcast( this );
	const bool bIsEnteringMode = true;
	Owner->BroadcastEditorModeChanged( this, bIsEnteringMode );
}

void FEdMode::Exit()
{
	const bool bIsEnteringMode = false;
	Owner->BroadcastEditorModeChanged( this, bIsEnteringMode );
	FEditorDelegates::EditorModeExit.Broadcast( this );
}

void FEdMode::SetCurrentTool( EModeTools InID )
{
	CurrentTool = FindTool( InID );
	check( CurrentTool );	// Tool not found!  This can't happen.

	CurrentToolChanged();
}

void FEdMode::SetCurrentTool( FModeTool* InModeTool )
{
	CurrentTool = InModeTool;
	check(CurrentTool);

	CurrentToolChanged();
}

FModeTool* FEdMode::FindTool( EModeTools InID )
{
	for( int32 x = 0 ; x < Tools.Num() ; ++x )
	{
		if( Tools[x]->GetID() == InID )
		{
			return Tools[x];
		}
	}

	UE_LOG(LogEditorModes, Fatal, TEXT("FEdMode::FindTool failed to find tool %d"), (int32)InID);
	return NULL;
}

void FEdMode::Render(const FSceneView* View,FViewport* Viewport,FPrimitiveDrawInterface* PDI)
{
	if( GEditor->bShowBrushMarkerPolys )
	{
		// Draw translucent polygons on brushes and volumes

		for( TActorIterator<ABrush> It(GetWorld()); It; ++ It )
		{
			ABrush* Brush = *It;

			// Brush->Brush is checked to safe from brushes that were created without having their brush members attached.
			if( Brush->Brush && (FActorEditorUtils::IsABuilderBrush(Brush) || Brush->IsVolumeBrush()) && Owner->GetSelectedActors()->IsSelected(Brush) )
			{
				// Build a mesh by basically drawing the triangles of each 
				FDynamicMeshBuilder MeshBuilder;
				int32 VertexOffset = 0;

				for( int32 PolyIdx = 0 ; PolyIdx < Brush->Brush->Polys->Element.Num() ; ++PolyIdx )
				{
					const FPoly* Poly = &Brush->Brush->Polys->Element[PolyIdx];

					if( Poly->Vertices.Num() > 2 )
					{
						const FVector Vertex0 = Poly->Vertices[0];
						FVector Vertex1 = Poly->Vertices[1];

						MeshBuilder.AddVertex(Vertex0, FVector2D::ZeroVector, FVector(1,0,0), FVector(0,1,0), FVector(0,0,1), FColor::White);
						MeshBuilder.AddVertex(Vertex1, FVector2D::ZeroVector, FVector(1,0,0), FVector(0,1,0), FVector(0,0,1), FColor::White);

						for( int32 VertexIdx = 2 ; VertexIdx < Poly->Vertices.Num() ; ++VertexIdx )
						{
							const FVector Vertex2 = Poly->Vertices[VertexIdx];
							MeshBuilder.AddVertex(Vertex2, FVector2D::ZeroVector, FVector(1,0,0), FVector(0,1,0), FVector(0,0,1), FColor::White);
							MeshBuilder.AddTriangle(VertexOffset,VertexOffset + VertexIdx,VertexOffset+VertexIdx-1);
							Vertex1 = Vertex2;
						}

						// Increment the vertex offset so the next polygon uses the correct vertex indices.
						VertexOffset += Poly->Vertices.Num();
					}
				}

				// Allocate the material proxy and register it so it can be deleted properly once the rendering is done with it.
				FDynamicColoredMaterialRenderProxy* MaterialProxy = new FDynamicColoredMaterialRenderProxy(GEngine->EditorBrushMaterial->GetRenderProxy(false),Brush->GetWireColor());
				PDI->RegisterDynamicResource( MaterialProxy );

				// Flush the mesh triangles.
				MeshBuilder.Draw(PDI, Brush->ActorToWorld().ToMatrixWithScale(), MaterialProxy, SDPG_World, 0.f);
			}
		}
	}

	const bool bIsInGameView = !Viewport->GetClient() || Viewport->GetClient()->IsInGameView();
	if (Owner->ShouldDrawBrushVertices() && !bIsInGameView)
	{
		UTexture2D* VertexTexture = GetVertexTexture();
		const float TextureSizeX = VertexTexture->GetSizeX() * 0.170f;
		const float TextureSizeY = VertexTexture->GetSizeY() * 0.170f;

		for (FSelectionIterator It(*Owner->GetSelectedActors()); It; ++It)
		{
			AActor* SelectedActor = static_cast<AActor*>(*It);
			checkSlow(SelectedActor->IsA(AActor::StaticClass()));

			ABrush* Brush = Cast< ABrush >(SelectedActor);
			if (Brush && Brush->Brush && !FActorEditorUtils::IsABuilderBrush(Brush))
			{
				for (int32 p = 0; p < Brush->Brush->Polys->Element.Num(); ++p)
				{
					FTransform BrushTransform = Brush->ActorToWorld();

					FPoly* poly = &Brush->Brush->Polys->Element[p];
					for (int32 VertexIndex = 0; VertexIndex < poly->Vertices.Num(); ++VertexIndex)
					{
						const FVector& PolyVertex = poly->Vertices[VertexIndex];
						const FVector WorldLocation = BrushTransform.TransformPosition(PolyVertex);
						
						const float Scale = View->WorldToScreen( WorldLocation ).W * ( 4.0f / View->ViewRect.Width() / View->ViewMatrices.ProjMatrix.M[0][0] );

						const FColor Color(Brush->GetWireColor());
						PDI->SetHitProxy(new HBSPBrushVert(Brush, &poly->Vertices[VertexIndex]));

						PDI->DrawSprite(WorldLocation, TextureSizeX * Scale, TextureSizeY * Scale, VertexTexture->Resource, Color, SDPG_World, 0.0f, 0.0f, 0.0f, 0.0f, SE_BLEND_Masked );

						PDI->SetHitProxy(NULL);
			
					}
				}
			}
		}
	}

	// Let the current mode tool render if it wants to
	FModeTool* tool = GetCurrentTool();
	if( tool )
	{
		tool->Render( View, Viewport, PDI );
	}

	AGroupActor::DrawBracketsForGroups(PDI, Viewport);

	if(UsesPropertyWidgets())
	{
		bool bHitTesting = PDI->IsHitTesting();
		AActor* SelectedActor = GetFirstSelectedActorInstance();
		if (SelectedActor != NULL)
		{
			UClass* Class = SelectedActor->GetClass();
			TArray<FPropertyWidgetInfo> WidgetInfos;
			GetPropertyWidgetInfos(Class, SelectedActor, WidgetInfos);
			FEditorScriptExecutionGuard ScriptGuard;
			for(int32 i=0; i<WidgetInfos.Num(); i++)
			{
				FString WidgetName = WidgetInfos[i].PropertyName;
				FName WidgetValidator = WidgetInfos[i].PropertyValidationName;
				int32 WidgetIndex = WidgetInfos[i].PropertyIndex;
				bool bIsTransform = WidgetInfos[i].bIsTransform;

				bool bSelected = (WidgetName == EditedPropertyName) && (WidgetIndex == EditedPropertyIndex);
				FColor WidgetColor = bSelected ? FColor::White : FColor(128, 128, 255);

				FVector LocalPos = FVector::ZeroVector;
				if(bIsTransform)
				{
					FTransform LocalTM = GetPropertyValueByName<FTransform>(SelectedActor, WidgetName, WidgetIndex);
					LocalPos = LocalTM.GetLocation();
				}
				else
				{
					LocalPos = GetPropertyValueByName<FVector>(SelectedActor, WidgetName, WidgetIndex);
				}

				FTransform ActorToWorld = SelectedActor->ActorToWorld();
				FVector WorldPos = ActorToWorld.TransformPosition(LocalPos);

				UFunction* ValidateFunc= NULL;
				if(WidgetValidator != NAME_None && 
					(ValidateFunc = SelectedActor->FindFunction(WidgetValidator)) != NULL)
				{
					FString ReturnText;
					SelectedActor->ProcessEvent(ValidateFunc, &ReturnText);

					//if we have a negative result, the widget color is red.
					WidgetColor = ReturnText.IsEmpty() ? WidgetColor : FColor::Red;
				}

				FTranslationMatrix WidgetTM(WorldPos);

				const float WidgetSize = 0.035f;
				const float ZoomFactor = FMath::Min<float>(View->ViewMatrices.ProjMatrix.M[0][0], View->ViewMatrices.ProjMatrix.M[1][1]);
				const float WidgetRadius = View->Project(WorldPos).W * (WidgetSize / ZoomFactor);

				if(bHitTesting) PDI->SetHitProxy( new HPropertyWidgetProxy(WidgetName, WidgetIndex, bIsTransform) );
				DrawWireDiamond(PDI, WidgetTM, WidgetRadius, WidgetColor, SDPG_Foreground );
				if(bHitTesting) PDI->SetHitProxy( NULL );
			}
		}
	}
}

void FEdMode::DrawHUD(FEditorViewportClient* ViewportClient,FViewport* Viewport,const FSceneView* View,FCanvas* Canvas)
{
	// Render the drag tool.
	ViewportClient->RenderDragTool( View, Canvas );

	// Let the current mode tool draw a HUD if it wants to
	FModeTool* tool = GetCurrentTool();
	if( tool )
	{
		tool->DrawHUD( ViewportClient, Viewport, View, Canvas );
	}

	if (ViewportClient->IsPerspective() && GetDefault<ULevelEditorViewportSettings>()->bHighlightWithBrackets)
	{
		DrawBrackets( ViewportClient, Viewport, View, Canvas );
	}

	// If this viewport doesn't show mode widgets or the mode itself doesn't want them, leave.
	if( !(ViewportClient->EngineShowFlags.ModeWidgets) || !ShowModeWidgets() )
	{
		return;
	}

	// Clear Hit proxies
	const bool bIsHitTesting = Canvas->IsHitTesting();
	if ( !bIsHitTesting )
	{
		Canvas->SetHitProxy(NULL);
	}

	// Draw vertices for selected BSP brushes and static meshes if the large vertices show flag is set.
	if ( !ViewportClient->bDrawVertices )
	{
		return;
	}

	const bool bLargeVertices		= View->Family->EngineShowFlags.LargeVertices;
	const bool bShowBrushes			= View->Family->EngineShowFlags.Brushes;
	const bool bShowBSP				= View->Family->EngineShowFlags.BSP;
	const bool bShowBuilderBrush	= View->Family->EngineShowFlags.BuilderBrush != 0;

	UTexture2D* VertexTexture = GetVertexTexture();
	const float TextureSizeX		= VertexTexture->GetSizeX() * ( bLargeVertices ? 1.0f : 0.5f );
	const float TextureSizeY		= VertexTexture->GetSizeY() * ( bLargeVertices ? 1.0f : 0.5f );

	// Temporaries.
	TArray<FVector> Vertices;

	for ( FSelectionIterator It( *Owner->GetSelectedActors() ) ; It ; ++It )
	{
		AActor* SelectedActor = static_cast<AActor*>( *It );
		checkSlow( SelectedActor->IsA(AActor::StaticClass()) );

		if( bLargeVertices )
		{
			FCanvasItemTestbed::bTestState = !FCanvasItemTestbed::bTestState;

			// Static mesh vertices
			AStaticMeshActor* Actor = Cast<AStaticMeshActor>( SelectedActor );
			if( Actor && Actor->GetStaticMeshComponent() && Actor->GetStaticMeshComponent()->StaticMesh
				&& Actor->GetStaticMeshComponent()->StaticMesh->RenderData )
			{
				FTransform ActorToWorld = Actor->ActorToWorld();
				Vertices.Empty();
				const FPositionVertexBuffer& VertexBuffer = Actor->GetStaticMeshComponent()->StaticMesh->RenderData->LODResources[0].PositionVertexBuffer;
				for( uint32 i = 0 ; i < VertexBuffer.GetNumVertices() ; i++ )
				{
					Vertices.AddUnique( ActorToWorld.TransformPosition( VertexBuffer.VertexPosition(i) ) );
				}

				FCanvasTileItem TileItem( FVector2D( 0.0f, 0.0f ), FVector2D( 0.0f, 0.0f ), FLinearColor::White );
				TileItem.BlendMode = SE_BLEND_Translucent;
				for( int32 VertexIndex = 0 ; VertexIndex < Vertices.Num() ; ++VertexIndex )
				{				
					const FVector& Vertex = Vertices[VertexIndex];
					FVector2D PixelLocation;
					if(View->ScreenToPixel(View->WorldToScreen(Vertex),PixelLocation))
					{
						const bool bOutside =
							PixelLocation.X < 0.0f || PixelLocation.X > View->ViewRect.Width() ||
							PixelLocation.Y < 0.0f || PixelLocation.Y > View->ViewRect.Height();
						if( !bOutside )
						{
							const float X = PixelLocation.X - (TextureSizeX/2);
							const float Y = PixelLocation.Y - (TextureSizeY/2);
							if( bIsHitTesting ) 
							{
								Canvas->SetHitProxy( new HStaticMeshVert(Actor,Vertex) );
							}
							TileItem.Texture = VertexTexture->Resource;
							
							TileItem.Size = FVector2D( TextureSizeX, TextureSizeY );
							Canvas->DrawItem( TileItem, FVector2D( X, Y ) );							
							if( bIsHitTesting )
							{
								Canvas->SetHitProxy( NULL );
							}
						}
					}
				}
			}
		}
	}

	if(UsesPropertyWidgets())
	{
		AActor* SelectedActor = GetFirstSelectedActorInstance();
		if (SelectedActor != NULL)
		{
			FEditorScriptExecutionGuard ScriptGuard;

			const int32 HalfX = 0.5f * Viewport->GetSizeXY().X;
			const int32 HalfY = 0.5f * Viewport->GetSizeXY().Y;

			UClass* Class = SelectedActor->GetClass();		
			TArray<FPropertyWidgetInfo> WidgetInfos;
			GetPropertyWidgetInfos(Class, SelectedActor, WidgetInfos);
			for(int32 i=0; i<WidgetInfos.Num(); i++)
			{
				FString WidgetName = WidgetInfos[i].PropertyName;
				FName WidgetValidator = WidgetInfos[i].PropertyValidationName;
				int32 WidgetIndex = WidgetInfos[i].PropertyIndex;
				bool bIsTransform = WidgetInfos[i].bIsTransform;

				FVector LocalPos = FVector::ZeroVector;
				if(bIsTransform)
				{
					FTransform LocalTM = GetPropertyValueByName<FTransform>(SelectedActor, WidgetName, WidgetIndex);
					LocalPos = LocalTM.GetLocation();
				}
				else
				{
					LocalPos = GetPropertyValueByName<FVector>(SelectedActor, WidgetName, WidgetIndex);
				}

				FTransform ActorToWorld = SelectedActor->ActorToWorld();
				FVector WorldPos = ActorToWorld.TransformPosition(LocalPos);

				UFunction* ValidateFunc = NULL;
				FString FinalString;
				if(WidgetValidator != NAME_None && 
					(ValidateFunc = SelectedActor->FindFunction(WidgetValidator)) != NULL)
				{
					SelectedActor->ProcessEvent(ValidateFunc, &FinalString);
				}

				const FPlane Proj = View->Project( WorldPos );

				//do some string fixing
				const uint32 VectorIndex = WidgetInfos[i].PropertyIndex;
				const FString WidgetDisplayName = WidgetInfos[i].DisplayName + ((VectorIndex != INDEX_NONE) ? FString::Printf(TEXT("[%d]"), VectorIndex) : TEXT(""));
				FinalString = FinalString.IsEmpty() ? WidgetDisplayName : FinalString;

				if(Proj.W > 0.f)
				{
					const int32 XPos = HalfX + ( HalfX * Proj.X );
					const int32 YPos = HalfY + ( HalfY * (Proj.Y * -1.f) );
					FCanvasTextItem TextItem( FVector2D( XPos + 5, YPos), FText::FromString( FinalString ), GEngine->GetSmallFont(), FLinearColor::White );
					Canvas->DrawItem( TextItem );
				}
			}
		}
	}
}

// Draw brackets around all selected objects
void FEdMode::DrawBrackets( FEditorViewportClient* ViewportClient, FViewport* Viewport, const FSceneView* View, FCanvas* Canvas )
{
	USelection& SelectedActors = *Owner->GetSelectedActors();
	for( int32 CurSelectedActorIndex = 0; CurSelectedActorIndex < SelectedActors.Num(); ++CurSelectedActorIndex )
	{
		AActor* SelectedActor = Cast<AActor>( SelectedActors.GetSelectedObject(CurSelectedActorIndex ) );
		if( SelectedActor != NULL )
		{
			// Draw a bracket for selected "paintable" static mesh actors
			const bool bIsValidActor = ( Cast< AStaticMeshActor >( SelectedActor ) != NULL );

			const FLinearColor SelectedActorBoxColor( 0.6f, 0.6f, 1.0f );
			const bool bDrawBracket = bIsValidActor;
			ViewportClient->DrawActorScreenSpaceBoundingBox( Canvas, View, Viewport, SelectedActor, SelectedActorBoxColor, bDrawBracket );
		}
	}
}

bool FEdMode::UsesToolkits() const
{
	return false;
}

bool FEdMode::StartTracking(FEditorViewportClient* InViewportClient, FViewport* InViewport)
{
	bool bResult = false;
	if( GetCurrentTool() )
	{
		bResult = GetCurrentTool()->StartModify();
	}
	return bResult;
}

bool FEdMode::EndTracking(FEditorViewportClient* InViewportClient, FViewport* InViewport)
{
	bool bResult = false;
	if( GetCurrentTool() )
	{
		bResult = GetCurrentTool()->EndModify();
	}
	return bResult;
}

FVector FEdMode::GetWidgetNormalFromCurrentAxis( void* InData )
{
	// Figure out the proper coordinate system.

	FMatrix matrix = FMatrix::Identity;
	if( Owner->GetCoordSystem() == COORD_Local )
	{
		GetCustomDrawingCoordinateSystem( matrix, InData );
	}

	// Get a base normal from the current axis.

	FVector BaseNormal(1,0,0);		// Default to X axis
	switch( CurrentWidgetAxis )
	{
		case EAxisList::Y:	BaseNormal = FVector(0,1,0);	break;
		case EAxisList::Z:	BaseNormal = FVector(0,0,1);	break;
		case EAxisList::XY:	BaseNormal = FVector(1,1,0);	break;
		case EAxisList::XZ:	BaseNormal = FVector(1,0,1);	break;
		case EAxisList::YZ:	BaseNormal = FVector(0,1,1);	break;
		case EAxisList::XYZ:	BaseNormal = FVector(1,1,1);	break;
	}

	// Transform the base normal into the proper coordinate space.
	return matrix.TransformPosition( BaseNormal );
}

AActor* FEdMode::GetFirstSelectedActorInstance() const
{
	return Owner->GetSelectedActors()->GetTop<AActor>();
}

bool FEdMode::CanCreateWidgetForStructure(const UStruct* InPropStruct)
{
	return InPropStruct && (InPropStruct->GetFName() == NAME_Vector || InPropStruct->GetFName() == NAME_Transform);
}

bool FEdMode::CanCreateWidgetForProperty(UProperty* InProp)
{
	UStructProperty* TestProperty = Cast<UStructProperty>(InProp);
	if( !TestProperty )
	{
		UArrayProperty* ArrayProperty = Cast<UArrayProperty>(InProp);
		if( ArrayProperty )
		{
			TestProperty = Cast<UStructProperty>(ArrayProperty->Inner);
		}
	}
	return (TestProperty != NULL) && CanCreateWidgetForStructure(TestProperty->Struct);
}

bool FEdMode::ShouldCreateWidgetForProperty(UProperty* InProp)
{
	return CanCreateWidgetForProperty(InProp) && InProp->HasMetaData(MD_MakeEditWidget);
}

static bool IsTransformProperty(UProperty* InProp)
{
	UStructProperty* StructProp = Cast<UStructProperty>(InProp);
	return (StructProp != NULL && StructProp->Struct->GetFName() == NAME_Transform);

}

void FEdMode::GetPropertyWidgetInfos(const UStruct* InStruct, const void* InContainer, TArray<FPropertyWidgetInfo>& OutInfos, FString PropertyNamePrefix, FString DisplayNamePrefix) const
{
	if(PropertyNamePrefix.Len() == 0)
	{
		OutInfos.Empty();
	}

	for (TFieldIterator<UProperty> PropertyIt(InStruct, EFieldIteratorFlags::IncludeSuper); PropertyIt; ++PropertyIt)
	{
		UProperty* CurrentProp = *PropertyIt;
		check(CurrentProp);
		FString DisplayName = CurrentProp->GetMetaData(TEXT("DisplayName"));
		if (!PropertyNamePrefix.IsEmpty() && DisplayNamePrefix.IsEmpty()) // Display name is already invalid
		{
			DisplayName.Empty();
		}
		if (!DisplayName.IsEmpty()) //Display name cannot be only the prefix.
		{
			DisplayName = DisplayNamePrefix + DisplayName;
		}

		if(	ShouldCreateWidgetForProperty(CurrentProp) )
		{
			if( UArrayProperty* ArrayProp = Cast<UArrayProperty>(CurrentProp) )
			{
				check(InContainer != NULL);

				FScriptArrayHelper_InContainer ArrayHelper(ArrayProp, InContainer);

				// See how many widgets we need to make for the array property
				uint32 ArrayDim = ArrayHelper.Num();
				for( uint32 i = 0; i < ArrayDim; i++ )
				{
					//create a new widget info struct
					FPropertyWidgetInfo WidgetInfo;

					//fill it in with the struct name
					WidgetInfo.PropertyName = PropertyNamePrefix + CurrentProp->GetFName().ToString();
					WidgetInfo.DisplayName = DisplayName.IsEmpty() ? WidgetInfo.PropertyName : DisplayName;

					//And see if we have any meta data that matches the MD_ValidateWidgetUsing name
					WidgetInfo.PropertyValidationName = FName(*CurrentProp->GetMetaData(MD_ValidateWidgetUsing));

					WidgetInfo.PropertyIndex = i;

					// See if its a transform
					WidgetInfo.bIsTransform = IsTransformProperty(ArrayProp->Inner);

					//Add it to our out array
					OutInfos.Add(WidgetInfo);
				}
			}
			else
			{

				//create a new widget info struct
				FPropertyWidgetInfo WidgetInfo;

				//fill it in with the struct name
				WidgetInfo.PropertyName = PropertyNamePrefix + CurrentProp->GetFName().ToString();
				WidgetInfo.DisplayName = DisplayName.IsEmpty() ? WidgetInfo.PropertyName : DisplayName;

				//And see if we have any meta data that matches the MD_ValidateWidgetUsing name
				WidgetInfo.PropertyValidationName = FName(*CurrentProp->GetMetaData(MD_ValidateWidgetUsing));

				// See if its a transform
				WidgetInfo.bIsTransform = IsTransformProperty(CurrentProp);

				//Add it to our out array
				OutInfos.Add(WidgetInfo);
			}

		}
		else
		{
			UStructProperty* StructProp = Cast<UStructProperty>(CurrentProp);
			if(StructProp != NULL)
			{
				// Recursively traverse into structures, looking for additional vector properties to expose
				GetPropertyWidgetInfos(StructProp->Struct
					, StructProp->ContainerPtrToValuePtr<void>(InContainer)
					, OutInfos
					, PropertyNamePrefix + StructProp->GetFName().ToString() + TEXT(".")
					, !DisplayName.IsEmpty() ? (DisplayName + TEXT(".")) : FString());
			}
			else
			{
				// Recursively traverse into arrays of structures, looking for additional vector properties to expose
				UArrayProperty* ArrayProp = Cast<UArrayProperty>(CurrentProp);
				if(ArrayProp != NULL)
				{
					StructProp = Cast<UStructProperty>(ArrayProp->Inner);
					if(StructProp != NULL)
					{
						FScriptArrayHelper_InContainer ArrayHelper(ArrayProp, InContainer);
						for(int32 ArrayIndex = 0; ArrayIndex < ArrayHelper.Num(); ++ArrayIndex)
						{
							if(ArrayHelper.IsValidIndex(ArrayIndex))
							{
								const FString ArrayPostfix = FString::Printf(TEXT("[%d]"), ArrayIndex) + TEXT(".");
								GetPropertyWidgetInfos(StructProp->Struct
									, ArrayHelper.GetRawPtr(ArrayIndex)
									, OutInfos
									, PropertyNamePrefix + ArrayProp->GetFName().ToString() + ArrayPostfix
									, !DisplayName.IsEmpty() ? (DisplayName + ArrayPostfix) : FString());
							}
						}
					}
				}
			}
		}
	}
}

bool FEdMode::IsSnapRotationEnabled()
{
	return GetDefault<ULevelEditorViewportSettings>()->RotGridEnabled;
}
