// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "DetailCustomizationsPrivatePCH.h"
#include "SceneComponentDetails.h"
#include "AssetSelection.h"
#include "PropertyEditing.h"
#include "PropertyRestriction.h"
#include "IPropertyUtilities.h"
#include "ComponentTransformDetails.h"
#include "SNotificationList.h"
#include "NotificationManager.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "Engine/SCS_Node.h"
#include "Engine/SimpleConstructionScript.h"
#include "Engine/InheritableComponentHandler.h"
#include "Components/LightComponentBase.h"

#define LOCTEXT_NAMESPACE "SceneComponentDetails"


/**
 * A helper for retrieving the simple-construction-script that this component belongs in.
 * 
 * @param  Component	The component you want the SCS for.
 * @return The component's blueprint SCS (NULL if one wasn't found).
 */
static USimpleConstructionScript const* GetSimpleConstructionScript(USceneComponent const* Component)
{
	USimpleConstructionScript const* BlueprintSCS = NULL;

	check(Component);
	UObject const* ComponentOuter = Component->GetOuter();

	if (UBlueprint const* const OuterBlueprint = Cast<UBlueprint const>(ComponentOuter))
	{
		BlueprintSCS = OuterBlueprint->SimpleConstructionScript;
	}
	else if (UBlueprintGeneratedClass const* const GeneratedClass = Cast<UBlueprintGeneratedClass>(ComponentOuter))
	{
		BlueprintSCS = GeneratedClass->SimpleConstructionScript;
	}

	return BlueprintSCS;
}

/**
 * A static helper function for retrieving the simple-construction-script node that
 * corresponds to the specified scene component template.
 * 
 * @param  ComponentObj	The component you want to find a USCS_Node for
 * @return A USCS_Node pointer corresponding to the specified component (NULL if we didn't find one)
 */
static USCS_Node* FindCorrespondingSCSNode(USceneComponent const* ComponentObj)
{
	USimpleConstructionScript const* BlueprintSCS = GetSimpleConstructionScript(ComponentObj);
	if (BlueprintSCS == NULL)
	{
		return NULL;
	}

	TArray<USCS_Node*> AllSCSNodes = BlueprintSCS->GetAllNodes();
	for (int32 SCSNodeIndex = 0; SCSNodeIndex < AllSCSNodes.Num(); ++SCSNodeIndex)
	{
		USCS_Node* SCSNode = AllSCSNodes[SCSNodeIndex];
		if (SCSNode->ComponentTemplate == ComponentObj) 
		{
			return SCSNode;
		}
	}

	return NULL;
}


/**
 * A static helper function used to retrieve a component's scene parent
 * 
 * @param  SceneComponentObject	The component you want the attached parent for
 * @return A pointer to the component's scene parent (NULL if there was not one)
 */
static USceneComponent* GetAttachedParent(USceneComponent const* SceneComponentObject)
{
	USceneComponent* SceneParent = SceneComponentObject->AttachParent;
	if (SceneParent == nullptr)
	{
		USCS_Node* const SCSNode = FindCorrespondingSCSNode(SceneComponentObject);
		// if we didn't find a corresponding simple-construction-script node
		if (SCSNode == nullptr) 
		{
			return nullptr;
		}	

		USimpleConstructionScript const* BlueprintSCS = GetSimpleConstructionScript(SceneComponentObject);
		check(BlueprintSCS != nullptr); 

		USCS_Node* const ParentSCSNode = BlueprintSCS->FindParentNode(SCSNode);
		if (ParentSCSNode != nullptr)
		{
			SceneParent = Cast<USceneComponent>(ParentSCSNode->ComponentTemplate);
		}
	}

	return SceneParent;
}

/**
 * A static helper function used meant as the default for determining if a 
 * component's Mobility should be overridden.
 * 
 * @param  CurrentMobility	The component's current Mobility setting
 * @param  NewMobility		The proposed new Mobility for the component in question
 * @return True if the two mobilities are not equal (false if they are equal)
 */
static bool AreMobilitiesDifferent(EComponentMobility::Type CurrentMobility, EComponentMobility::Type NewMobility)
{
	return CurrentMobility != NewMobility;
}
DECLARE_DELEGATE_RetVal_OneParam(bool, FMobilityQueryDelegate, EComponentMobility::Type);

/**
 * A static helper function that recursively alters the Mobility property for all 
 * sub-components (descending from the specified USceneComponent)
 * 
 * @param  SceneComponentObject		The component whose sub-components you want to alter
 * @param  NewMobilityType			The Mobility type you want to switch sub-components over to
 * @param  ShouldOverrideMobility	A delegate used to determine if a sub-component's Mobility should be overridden
 *									(if left unset it will default to the AreMobilitiesDifferent() function)
 * @return The number of decedents that had their mobility altered.
 */
static int32 SetDecedentMobility(USceneComponent const* SceneComponentObject, EComponentMobility::Type NewMobilityType, FMobilityQueryDelegate ShouldOverrideMobility = FMobilityQueryDelegate())
{
	if (!ensure(SceneComponentObject != NULL))
	{
		return 0;
	}

	TArray<USceneComponent*> AttachedChildren = SceneComponentObject->AttachChildren;
	// gather children for component templates
	USCS_Node* SCSNode = FindCorrespondingSCSNode(SceneComponentObject);
	if (SCSNode != NULL)
	{
		// gather children from the SCSNode
		for (int32 ChildIndex = 0; ChildIndex < SCSNode->ChildNodes.Num(); ++ChildIndex)
		{
			USCS_Node* SCSChild = SCSNode->ChildNodes[ChildIndex];

			USceneComponent* ChildSceneComponent = Cast<USceneComponent>(SCSChild->ComponentTemplate);
			if (ChildSceneComponent != NULL)
			{
				AttachedChildren.Add(ChildSceneComponent);
			}
		}
	}

	if (!ShouldOverrideMobility.IsBound())
	{
		ShouldOverrideMobility = FMobilityQueryDelegate::CreateStatic(&AreMobilitiesDifferent, NewMobilityType);
	}

	int32 NumDecendentsChanged = 0;
	// recursively alter the mobility for children and deeper decedents 
	for (int32 ChildIndex = 0; ChildIndex < AttachedChildren.Num(); ++ChildIndex)
	{
		USceneComponent* ChildSceneComponent = AttachedChildren[ChildIndex];

		if (ShouldOverrideMobility.Execute(ChildSceneComponent->Mobility))
		{
			// USceneComponents shouldn't be set Stationary 
			if ((NewMobilityType == EComponentMobility::Stationary) && ChildSceneComponent->IsA(UStaticMeshComponent::StaticClass()))
			{
				// make it Movable (because it is acceptable for Stationary parents to have Movable children)
				ChildSceneComponent->Mobility = EComponentMobility::Movable;
			}
			else
			{
				ChildSceneComponent->Mobility = NewMobilityType;
			}
			++NumDecendentsChanged;
		}
		NumDecendentsChanged += SetDecedentMobility(ChildSceneComponent, NewMobilityType, ShouldOverrideMobility);
	}

	return NumDecendentsChanged;
}

/**
 * A static helper function that alters the Mobility property for all ancestor
 * components (ancestors of the specified USceneComponent).
 * 
 * @param  SceneComponentObject		The component whose attached ancestors you want to alter
 * @param  NewMobilityType			The Mobility type you want to switch ancestor components over to
 * @param  ShouldOverrideMobility	A delegate used to determine if a ancestor's Mobility should be overridden
 *									(if left unset it will default to the AreMobilitiesDifferent() function)
 * @return The number of ancestors that had their mobility altered.
 */
static int32 SetAncestorMobility(USceneComponent const* SceneComponentObject, EComponentMobility::Type NewMobilityType, FMobilityQueryDelegate ShouldOverrideMobility = FMobilityQueryDelegate())
{
	if (!ensure(SceneComponentObject != NULL))
	{
		return 0;
	}

	if (!ShouldOverrideMobility.IsBound())
	{
		ShouldOverrideMobility = FMobilityQueryDelegate::CreateStatic(&AreMobilitiesDifferent, NewMobilityType);
	}

	int32 MobilityAlteredCount = 0;
	while(USceneComponent* AttachedParent = GetAttachedParent(SceneComponentObject))
	{
		if (ShouldOverrideMobility.Execute(AttachedParent->Mobility))
		{
			// USceneComponents shouldn't be set Stationary 
			if ((NewMobilityType == EComponentMobility::Stationary) && AttachedParent->IsA(UStaticMeshComponent::StaticClass()))
			{
				// make it Static (because it is acceptable for Stationary children to have Static parents)
				AttachedParent->Mobility = EComponentMobility::Static;
			}
			else
			{
				AttachedParent->Mobility = NewMobilityType;
			}
			++MobilityAlteredCount;
		}
		SceneComponentObject = AttachedParent;
	}

	return MobilityAlteredCount;
}

/**
 * Walks the scene hierarchy looking for inherited components (like ones from
 * a parent class). If it finds one, this returns its mobility setting. 
 * 
 * @param  SceneComponent	The child component who's ancestor hierarchy you want to traverse.
 * @return The mobility of the first scene-component ancestor (EComponentMobility::Static if one wasn't found).
 */
static EComponentMobility::Type GetInheritedMobility(USceneComponent const* const SceneComponent)
{
	// default to "static" since it doesn't restrict anything (in case we don't inherit any at all)
	EComponentMobility::Type InheritedMobility = EComponentMobility::Static;

	USCS_Node* ComponentNode = FindCorrespondingSCSNode(SceneComponent);
	
	if(ComponentNode == NULL)
	{
		return EComponentMobility::Static;
	}

	USimpleConstructionScript const* const SceneSCS = ComponentNode->GetSCS();
	check(SceneSCS != NULL);
	
	do 
	{
		bool const bIsParentInherited = !ComponentNode->ParentComponentOwnerClassName.IsNone();
		// if we can't alter the parent component's mobility from the current blueprint
		if (bIsParentInherited)
		{
			USCS_Node const* ParentNode = NULL;

			UClass* ParentClass = SceneSCS->GetOwnerClass();
			// ParentNode should be null, so we need to find it in the class that owns it...
			// first, find the class:
			while((ParentClass != NULL) && (ParentClass->GetFName() != ComponentNode->ParentComponentOwnerClassName))
			{
				ParentClass = ParentClass->GetSuperClass();
			}
			// now look through this blueprint and find the inherited parent node
			if (UBlueprintGeneratedClass* BlueprintClass = Cast<UBlueprintGeneratedClass>(ParentClass))
			{
				for (USCS_Node const* Node : BlueprintClass->SimpleConstructionScript->GetAllNodes())
				{
					if (Node->VariableName == ComponentNode->ParentComponentOrVariableName)
					{
						ParentNode = Node;
						break;
					}
				}
			}			

			if (ParentNode != NULL)
			{
				if (USceneComponent const* const ParentComponent = Cast<USceneComponent>(ParentNode->ComponentTemplate))
				{
					InheritedMobility = ParentComponent->Mobility;
					break;
				}
			}
		}
		ComponentNode = SceneSCS->FindParentNode(ComponentNode);

	} while (ComponentNode != NULL);

	return InheritedMobility;
}


/**
 * Checks to see if the specified mobility is valid for the passed USceneComponent.
 * 
 * @param  MobilityValue		The mobility you wish to validate.
 * @param  SceneComponent		The component you want to check for.
 * @param  ProhibitedReasonOut	If the mobility is invalid, this will explain why.
 * @return False if the mobility in question is valid, true if it is invalid.
 */
static bool IsMobilitySettingProhibited(EComponentMobility::Type const MobilityValue, USceneComponent const* const SceneComponent, FText& ProhibitedReasonOut)
{
	bool bIsProhibited = false;

	switch (MobilityValue)
	{
	case EComponentMobility::Movable:
		{
			// movable is always an option (parent components can't prevent this from being movable)
			bIsProhibited = false;
			break;
		}

	case EComponentMobility::Stationary:
		{
			if (!SceneComponent->IsA(ULightComponentBase::StaticClass()))
			{
				ProhibitedReasonOut = LOCTEXT("OnlyLightsCanBeStationary", "Only light components can be stationary.");
				bIsProhibited = true;
			}
			else if (GetInheritedMobility(SceneComponent) == EComponentMobility::Movable)
			{
				ProhibitedReasonOut = LOCTEXT("ParentMoreMobileRestriction", "Selected objects cannot be less mobile than their inherited parents.");
				// can't be less movable than what we've inherited
				bIsProhibited = true;
			}
			break;
		}

	case EComponentMobility::Static:
		{
			if (GetInheritedMobility(SceneComponent) != EComponentMobility::Static)
			{
				ProhibitedReasonOut = LOCTEXT("ParentMoreMobileRestriction", "Selected objects cannot be less mobile than their inherited parents.");
				// can't be less movable than what we've inherited
				bIsProhibited = true;
			}
		}
	};	

	return bIsProhibited;
};

TSharedRef<IDetailCustomization> FSceneComponentDetails::MakeInstance()
{
	return MakeShareable( new FSceneComponentDetails );
}

ECheckBoxState FSceneComponentDetails::IsMobilityActive(TWeakPtr<IPropertyHandle> MobilityHandle, EComponentMobility::Type InMobility) const
{
	if ( MobilityHandle.IsValid() )
	{
		uint8 MobilityByte;
		MobilityHandle.Pin()->GetValue(MobilityByte);

		return MobilityByte == InMobility ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
	}

	return ECheckBoxState::Unchecked;
}

FSlateColor FSceneComponentDetails::GetMobilityTextColor(TWeakPtr<IPropertyHandle> MobilityHandle, EComponentMobility::Type InMobility) const
{
	if ( MobilityHandle.IsValid() )
	{
		uint8 MobilityByte;
		MobilityHandle.Pin()->GetValue(MobilityByte);

		return MobilityByte == InMobility ? FSlateColor(FLinearColor(0, 0, 0)) : FSlateColor(FLinearColor(0.72f, 0.72f, 0.72f, 1.f));
	}

	return FSlateColor(FLinearColor(0.72f, 0.72f, 0.72f, 1.f));
}

void FSceneComponentDetails::OnMobilityChanged(ECheckBoxState InCheckedState, TWeakPtr<IPropertyHandle> MobilityHandle, EComponentMobility::Type InMobility)
{
	if ( MobilityHandle.IsValid() && InCheckedState == ECheckBoxState::Checked)
	{
		MobilityHandle.Pin()->SetValue((uint8)InMobility);
	}
}

FText FSceneComponentDetails::GetMobilityToolTip(TWeakPtr<IPropertyHandle> MobilityHandle) const
{
	if ( MobilityHandle.IsValid() )
	{
		return MobilityHandle.Pin()->GetToolTipText();
	}

	return FText::GetEmpty();
}

void FSceneComponentDetails::CustomizeDetails( IDetailLayoutBuilder& DetailBuilder )
{
	MakeTransformDetails( DetailBuilder );

	// Put mobility property in Transform section
	IDetailCategoryBuilder& TransformCategory = DetailBuilder.EditCategory( "TransformCommon", LOCTEXT("TransformCommonCategory", "Transform"), ECategoryPriority::Transform );
	TSharedPtr<IPropertyHandle> MobilityProperty = DetailBuilder.GetProperty("Mobility");

	uint8 RestrictedMobilityBits = 0u;
	enum 
	{
		StaticMobilityBitMask     = (1u << EComponentMobility::Static),
		StationaryMobilityBitMask = (1u << EComponentMobility::Stationary),
		MovableMobilityBitMask    = (1u << EComponentMobility::Movable),
	};
	// see if any of the selected objects have mobility restrictions
	DetailBuilder.GetObjectsBeingCustomized( CachedSelectedSceneComponents );
	for (TArray<TWeakObjectPtr<UObject>>::TConstIterator ObjectIt(CachedSelectedSceneComponents); ObjectIt; ++ObjectIt)
	{
		if (!ObjectIt->IsValid())
		{
			continue;
		}

		USceneComponent const* SceneComponent = Cast<USceneComponent>((*ObjectIt).Get());
		if (SceneComponent == NULL)
		{
			continue;
		}

		// if we haven't restricted the "Static" option yet
		if (!(RestrictedMobilityBits & StaticMobilityBitMask))
		{
			FText RestrictReason;
			if (IsMobilitySettingProhibited(EComponentMobility::Static, SceneComponent, RestrictReason))
			{
				 TSharedPtr<FPropertyRestriction> StaticRestriction = MakeShareable(new FPropertyRestriction(RestrictReason));
				 StaticRestriction->AddValue(TEXT("Static"));
				 MobilityProperty->AddRestriction(StaticRestriction.ToSharedRef());

				 RestrictedMobilityBits |= StaticMobilityBitMask;
			}			
		}

		// if we haven't restricted the "Stationary" option yet
		if (!(RestrictedMobilityBits & StationaryMobilityBitMask))
		{
			FText RestrictReason;
			if (IsMobilitySettingProhibited(EComponentMobility::Stationary, SceneComponent, RestrictReason))
			{
				TSharedPtr<FPropertyRestriction> StationaryRestriction = MakeShareable(new FPropertyRestriction(RestrictReason));
				StationaryRestriction->AddValue(TEXT("Stationary"));
				MobilityProperty->AddRestriction(StationaryRestriction.ToSharedRef());

				RestrictedMobilityBits |= StationaryMobilityBitMask;
			}
		}

		// no need to go through all of them if we can't restrict any more
		if ((RestrictedMobilityBits & StaticMobilityBitMask) && (RestrictedMobilityBits & StationaryMobilityBitMask))
		{
			break;
		}
	}

	FSimpleDelegate OnMobilityChangedDelegate = FSimpleDelegate::CreateSP(this, &FSceneComponentDetails::OnMobilityChanged, MobilityProperty);
	MobilityProperty->SetOnPropertyValueChanged(OnMobilityChangedDelegate);


	// Build Editor for Mobility
	TWeakPtr<IPropertyHandle> MobilityPropertyWeak(MobilityProperty);

	TSharedPtr<SUniformGridPanel> ButtonOptionsPanel;

	IDetailPropertyRow& MobilityRow = TransformCategory.AddProperty(MobilityProperty);
	MobilityRow.CustomWidget()
	.NameContent()
	[
		SNew(STextBlock)
		.Text(LOCTEXT("Mobility", "Mobility"))
		.ToolTipText(this, &FSceneComponentDetails::GetMobilityToolTip, MobilityPropertyWeak)
		.Font(IDetailLayoutBuilder::GetDetailFont())
	]
	.ValueContent()
	.MaxDesiredWidth(0)
	[
		SAssignNew(ButtonOptionsPanel, SUniformGridPanel)
	];

	bool bShowStatic = !( RestrictedMobilityBits & StaticMobilityBitMask );
	bool bShowStationary = !( RestrictedMobilityBits & StationaryMobilityBitMask );

	int32 ColumnIndex = 0;

	if ( bShowStatic )
	{
		// Static Mobility
		ButtonOptionsPanel->AddSlot(0, 0)
		[
			SNew(SCheckBox)
			.Style(FEditorStyle::Get(), "Property.ToggleButton.Start")
			.IsChecked(this, &FSceneComponentDetails::IsMobilityActive, MobilityPropertyWeak, EComponentMobility::Static)
			.OnCheckStateChanged(this, &FSceneComponentDetails::OnMobilityChanged, MobilityPropertyWeak, EComponentMobility::Static)
			.ToolTipText(LOCTEXT("Mobility_Static_Tooltip", "A static object can't be changed in game.\n● Allows Baked Lighting\n● Fastest Rendering"))
			[
				SNew(SHorizontalBox)

				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				.Padding(3, 2)
				[
					SNew(SImage)
					.Image(FEditorStyle::GetBrush("Mobility.Static"))
				]

				+ SHorizontalBox::Slot()
				.FillWidth(1.0f)
				.VAlign(VAlign_Center)
				.HAlign(HAlign_Center)
				.Padding(6, 2)
				[
					SNew(STextBlock)
					.Text(LOCTEXT("Static", "Static"))
					.Font(IDetailLayoutBuilder::GetDetailFont())
					.ColorAndOpacity(this, &FSceneComponentDetails::GetMobilityTextColor, MobilityPropertyWeak, EComponentMobility::Static)
				]
			]
		];

		ColumnIndex++;
	}

	// Stationary Mobility
	if ( bShowStationary )
	{
		ButtonOptionsPanel->AddSlot(ColumnIndex, 0)
		[
			SNew(SCheckBox)
			.IsChecked(this, &FSceneComponentDetails::IsMobilityActive, MobilityPropertyWeak, EComponentMobility::Stationary)
			.Style(FEditorStyle::Get(), ( ColumnIndex == 0 ) ? "Property.ToggleButton.Start" : "Property.ToggleButton.Middle")
			.OnCheckStateChanged(this, &FSceneComponentDetails::OnMobilityChanged, MobilityPropertyWeak, EComponentMobility::Stationary)
			.ToolTipText(LOCTEXT("Mobility_Stationary_Tooltip", "A stationary light will only have its shadowing and bounced lighting from static geometry baked by Lightmass, all other lighting will be dynamic.  It can change color and intensity in game.\n● Can't Move\n● Allows Partial Baked Lighting\n● Dynamic Shadows"))
			[
				SNew(SHorizontalBox)

				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				.Padding(3, 2)
				[
					SNew(SImage)
					.Image(FEditorStyle::GetBrush("Mobility.Stationary"))
				]

				+ SHorizontalBox::Slot()
				.FillWidth(1.0f)
				.VAlign(VAlign_Center)
				.HAlign(HAlign_Center)
				.Padding(6, 2)
				[
					SNew(STextBlock)
					.Text(LOCTEXT("Stationary", "Stationary"))
					.Font(IDetailLayoutBuilder::GetDetailFont())
					.ColorAndOpacity(this, &FSceneComponentDetails::GetMobilityTextColor, MobilityPropertyWeak, EComponentMobility::Stationary)
				]
			]
		];

		ColumnIndex++;
	}

	// Movable Mobility
	ButtonOptionsPanel->AddSlot(ColumnIndex, 0)
	[
		SNew(SCheckBox)
		.IsChecked(this, &FSceneComponentDetails::IsMobilityActive, MobilityPropertyWeak, EComponentMobility::Movable)
		.Style(FEditorStyle::Get(), ( ColumnIndex == 0 ) ? "Property.ToggleButton" : "Property.ToggleButton.End")
		.OnCheckStateChanged(this, &FSceneComponentDetails::OnMobilityChanged, MobilityPropertyWeak, EComponentMobility::Movable)
		.ToolTipText(LOCTEXT("Mobility_Movable_Tooltip", "Movable objects can be moved and changed in game.\n● Totally Dynamic\n● Allows Dynamic Shadows\n● Slowest Rendering"))
		[
			SNew(SHorizontalBox)

			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(3, 2)
			[
				SNew(SImage)
				.Image(FEditorStyle::GetBrush("Mobility.Movable"))
			]

			+ SHorizontalBox::Slot()
			.FillWidth(1.0f)
			.VAlign(VAlign_Center)
			.HAlign(HAlign_Center)
			.Padding(6, 2)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("Movable", "Movable"))
				.Font(IDetailLayoutBuilder::GetDetailFont())
				.ColorAndOpacity(this, &FSceneComponentDetails::GetMobilityTextColor, MobilityPropertyWeak, EComponentMobility::Movable)
			]
		]
	];
}

void FSceneComponentDetails::MakeTransformDetails( IDetailLayoutBuilder& DetailBuilder )
{
	const TArray<TWeakObjectPtr<AActor> >& SelectedActors = DetailBuilder.GetDetailsView().GetSelectedActors();
	const FSelectedActorInfo& SelectedActorInfo = DetailBuilder.GetDetailsView().GetSelectedActorInfo();


	// Hide the transform properties so they don't show up
	DetailBuilder.HideProperty( DetailBuilder.GetProperty( GET_MEMBER_NAME_CHECKED(USceneComponent, bAbsoluteLocation) ) );
	DetailBuilder.HideProperty( DetailBuilder.GetProperty( GET_MEMBER_NAME_CHECKED(USceneComponent, bAbsoluteRotation) ) );
	DetailBuilder.HideProperty( DetailBuilder.GetProperty( GET_MEMBER_NAME_CHECKED(USceneComponent, bAbsoluteScale) ) );
	DetailBuilder.HideProperty( DetailBuilder.GetProperty( GET_MEMBER_NAME_CHECKED(USceneComponent, RelativeLocation) ) );
	DetailBuilder.HideProperty( DetailBuilder.GetProperty( GET_MEMBER_NAME_CHECKED(USceneComponent, RelativeRotation) ) );
	DetailBuilder.HideProperty( DetailBuilder.GetProperty( GET_MEMBER_NAME_CHECKED(USceneComponent, RelativeScale3D) ) );

	// Determine whether or not we are editing Class Defaults through the CDO
	bool bIsEditingBlueprintDefaults = false;
	const TArray<TWeakObjectPtr<UObject> >& SelectedObjects = DetailBuilder.GetDetailsView().GetSelectedObjects();
	for(int32 ObjectIndex = 0; ObjectIndex < SelectedObjects.Num(); ++ObjectIndex)
	{
		UObject* SelectedObject = SelectedObjects[ObjectIndex].Get();
		if(SelectedObject && SelectedObject->HasAnyFlags(RF_ClassDefaultObject))
		{
			bIsEditingBlueprintDefaults = !!UBlueprint::GetBlueprintFromClass(SelectedObject->GetClass());
			if(!bIsEditingBlueprintDefaults)
			{
				break;
			}
		}
	}

	// If there are any actors selected and we're not editing Class Defaults, the transform section is shown as part of the actor customization
	if( SelectedActors.Num() == 0 || bIsEditingBlueprintDefaults)
	{
		TArray< TWeakObjectPtr<UObject> > SceneComponentObjects;
		DetailBuilder.GetObjectsBeingCustomized( SceneComponentObjects );

		// Default to showing the transform for all components unless we are viewing a non-Blueprint class default object (the transform is not used in that case)
		bool bShouldShowTransform = !DetailBuilder.GetDetailsView().HasClassDefaultObject() || bIsEditingBlueprintDefaults;

		for (int32 ComponentIndex = 0; bShouldShowTransform && ComponentIndex < SceneComponentObjects.Num(); ++ComponentIndex)
		{
			USceneComponent* SceneComponent = Cast<USceneComponent>(SceneComponentObjects[ComponentIndex].Get());
			if (SceneComponent->AttachParent == NULL && SceneComponent->GetOuter()->HasAnyFlags(RF_ClassDefaultObject))
			{
				bShouldShowTransform = false;
			}
			else if (const USimpleConstructionScript* SCS = GetSimpleConstructionScript(SceneComponent))
			{
				const TArray<USCS_Node*>& RootNodes = SCS->GetRootNodes();
				for(int32 RootNodeIndex = 0; bShouldShowTransform && RootNodeIndex < RootNodes.Num(); ++RootNodeIndex)
				{
					USCS_Node* RootNode = RootNodes[RootNodeIndex];
					check(RootNode);

					if(RootNode->ComponentTemplate == SceneComponent && RootNode->ParentComponentOrVariableName == NAME_None)
					{
						bShouldShowTransform = false;
					}
				}
			}

			if (bShouldShowTransform && SceneComponent && SceneComponent->HasAnyFlags(RF_InheritableComponentTemplate))
			{
				auto OwnerClass = Cast<UClass>(SceneComponent->GetOuter());
				auto Bluepirnt = UBlueprint::GetBlueprintFromClass(OwnerClass);
				auto InheritableComponentHandler = Bluepirnt ? Bluepirnt->GetInheritableComponentHandler(false) : nullptr;
				auto ComponentKey = InheritableComponentHandler ? InheritableComponentHandler->FindKey(SceneComponent) : FComponentKey();
				auto SCSNode = ComponentKey.FindSCSNode();
				const bool bProperRootNodeFound = ComponentKey.IsValid() && SCSNode && SCSNode->IsRootNode() && (SCSNode->ParentComponentOrVariableName == NAME_None);
				if (bProperRootNodeFound)
				{
					bShouldShowTransform = false;
				}
			}
		}

		TSharedRef<FComponentTransformDetails> TransformDetails = MakeShareable( new FComponentTransformDetails( bIsEditingBlueprintDefaults ? SceneComponentObjects : DetailBuilder.GetDetailsView().GetSelectedObjects(), SelectedActorInfo, DetailBuilder ) );

		if(!bShouldShowTransform)
		{
			TransformDetails->HideTransformField(ETransformField::Location);
			TransformDetails->HideTransformField(ETransformField::Rotation);
		}

		IDetailCategoryBuilder& TransformCategory = DetailBuilder.EditCategory( "TransformCommon", LOCTEXT("TransformCommonCategory", "Transform"), ECategoryPriority::Transform );

		TransformCategory.AddCustomBuilder( TransformDetails );
	}

}

void FSceneComponentDetails::OnMobilityChanged(TSharedPtr<IPropertyHandle> MobilityProperty)
{
	if (!MobilityProperty.IsValid() || CachedSelectedSceneComponents.Num() == 0)
	{
		return;
	}

	// in case GetValue() fails (like with multiple values selected)
	uint8 MobilityValue = Cast<const USceneComponent>(CachedSelectedSceneComponents[0].Get())->Mobility;
	MobilityProperty->GetValue(MobilityValue);

	// Attached parent components can't be more mobile than their children. This means that 
	// certain mobility hierarchy structures are disallowed. So we have to walk the hierarchy 
	// and alter parent/child components as a result of this property change.

	// track how many other components we had to change
	int32 NumMobilityChanges = 0; 

	// Movable components can only have movable sub-components
	if (MobilityValue == EComponentMobility::Movable) 
	{
		for (TArray< TWeakObjectPtr<UObject> >::TConstIterator ObjectIt(CachedSelectedSceneComponents); ObjectIt; ++ObjectIt)
		{
			NumMobilityChanges += SetDecedentMobility(Cast<const USceneComponent>(ObjectIt->Get()), EComponentMobility::Movable);
		}
	}
	else if (MobilityValue == EComponentMobility::Stationary)
	{
		// a functor for checking if we should change a component's Mobility
		struct FMobilityEqualityFunctor
		{
			bool operator()(EComponentMobility::Type CurrentMobility, EComponentMobility::Type CheckValue)
			{
				return CurrentMobility == CheckValue;
			}
		};
		FMobilityEqualityFunctor EquivalenceFunctor;

		// a delegate for checking if components are Static
		FMobilityQueryDelegate IsStaticDelegate  = FMobilityQueryDelegate::CreateRaw(&EquivalenceFunctor, &FMobilityEqualityFunctor::operator(), EComponentMobility::Static);
		// a delegate for checking if components are Movable
		FMobilityQueryDelegate IsMovableDelegate = FMobilityQueryDelegate::CreateRaw(&EquivalenceFunctor, &FMobilityEqualityFunctor::operator(), EComponentMobility::Movable);

		// Stationary components can't have Movable parents, and can't have Static children
		for (TArray< TWeakObjectPtr<UObject> >::TConstIterator ObjectIt(CachedSelectedSceneComponents); ObjectIt; ++ObjectIt)
		{
			USceneComponent const* SelectedSceneComponent = Cast<const USceneComponent>(ObjectIt->Get());

			// if any decedents are static, change them to stationary (or movable for static meshes)
			NumMobilityChanges += SetDecedentMobility(SelectedSceneComponent, EComponentMobility::Stationary, IsStaticDelegate);

			// if any ancestors are movable, change them to stationary (or static for static meshes)
			NumMobilityChanges += SetAncestorMobility(SelectedSceneComponent, EComponentMobility::Stationary, IsMovableDelegate);
		}
	}
	else // if MobilityValue == Static
	{
		// ensure we have the mobility we expected (in case someone adds a new one)
		ensure(MobilityValue == EComponentMobility::Static); 

		// Static components can only have Static parents
		for (TArray< TWeakObjectPtr<UObject> >::TConstIterator ObjectIt(CachedSelectedSceneComponents); ObjectIt; ++ObjectIt)
		{
			NumMobilityChanges += SetAncestorMobility(Cast<const USceneComponent>(ObjectIt->Get()), EComponentMobility::Static);
		}
	}

	// if we altered any components (other than the ones selected), then notify the user
	if (NumMobilityChanges > 0)
	{
		FText NotificationText = LOCTEXT("MobilityAlteredSingularNotification", "Caused 1 component to also change Mobility");
		if (NumMobilityChanges > 1)
		{
			NotificationText = FText::Format(LOCTEXT("MobilityAlteredPluralNotification", "Caused {0} other components to also change Mobility"), FText::AsNumber(NumMobilityChanges));
		}		
		FNotificationInfo Info(NotificationText);
		Info.bFireAndForget = true;
		Info.bUseThrobber   = true;
		FSlateNotificationManager::Get().AddNotification(Info);
	}
}

#undef LOCTEXT_NAMESPACE
