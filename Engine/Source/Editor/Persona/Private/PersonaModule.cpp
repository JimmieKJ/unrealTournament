// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.


#include "PersonaPrivatePCH.h"
#include "PersonaModule.h"
#include "Persona.h"
#include "BlueprintUtilities.h"
#include "AnimGraphDefinitions.h"
#include "Toolkits/ToolkitManager.h"
#include "Runtime/AssetRegistry/Public/AssetRegistryModule.h"
#include "PropertyEditorModule.h"
#include "SkeletalMeshSocketDetails.h"
#include "AnimNotifyDetails.h"
#include "AnimGraphNodeDetails.h"
#include "AnimInstanceDetails.h"
#include "Editor/UnrealEd/Public/Kismet2/KismetEditorUtilities.h"
#include "Animation/AnimInstance.h"

IMPLEMENT_MODULE( FPersonaModule, Persona );

const FName PersonaAppName = FName(TEXT("PersonaApp"));


void FPersonaModule::StartupModule()
{
	MenuExtensibilityManager = MakeShareable(new FExtensibilityManager);
	ToolBarExtensibilityManager = MakeShareable(new FExtensibilityManager);

	//Call this to make sure AnimGraph module is setup
	FModuleManager::Get().LoadModuleChecked(TEXT("AnimGraph"));

	// Load all blueprint animnotifies from asset registry so they are available from drop downs in anim segment detail views
	{
		FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));

		// Collect a full list of assets with the specified class
		TArray<FAssetData> AssetData;
		AssetRegistryModule.Get().GetAssetsByClass(UBlueprint::StaticClass()->GetFName(), AssetData);

		const FName BPParentClassName( TEXT( "ParentClass" ) );
		const FString BPAnimNotify( TEXT("Class'/Script/Engine.AnimNotify'" ));

		for (int32 AssetIndex = 0; AssetIndex < AssetData.Num(); ++AssetIndex)
		{
			FString TagValue = AssetData[ AssetIndex ].GetTagValueRef<FString>(BPParentClassName);
			if (TagValue == BPAnimNotify)
			{
				FString BlueprintPath = AssetData[AssetIndex].ObjectPath.ToString();
				LoadObject<UBlueprint>(NULL, *BlueprintPath, NULL, 0, NULL);
			}
		}
	}
	{
		FPropertyEditorModule& PropertyModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
		PropertyModule.RegisterCustomClassLayout( "SkeletalMeshSocket", FOnGetDetailCustomizationInstance::CreateStatic( &FSkeletalMeshSocketDetails::MakeInstance ) );
		PropertyModule.RegisterCustomClassLayout( "EditorNotifyObject", FOnGetDetailCustomizationInstance::CreateStatic(&FAnimNotifyDetails::MakeInstance));
		PropertyModule.RegisterCustomClassLayout( "AnimGraphNode_Base", FOnGetDetailCustomizationInstance::CreateStatic( &FAnimGraphNodeDetails::MakeInstance ) );
		PropertyModule.RegisterCustomClassLayout( "AnimInstance", FOnGetDetailCustomizationInstance::CreateStatic(&FAnimInstanceDetails::MakeInstance));

		PropertyModule.RegisterCustomPropertyTypeLayout( "InputScaleBias", FOnGetPropertyTypeCustomizationInstance::CreateStatic( &FInputScaleBiasCustomization::MakeInstance ) );
		PropertyModule.RegisterCustomPropertyTypeLayout( "BoneReference", FOnGetPropertyTypeCustomizationInstance::CreateStatic( &FBoneReferenceCustomization::MakeInstance ) );
	}

	FKismetEditorUtilities::RegisterOnBlueprintCreatedCallback(this, UAnimInstance::StaticClass(), FKismetEditorUtilities::FOnBlueprintCreated::CreateRaw(this, &FPersonaModule::OnNewBlueprintCreated));
}

void FPersonaModule::ShutdownModule()
{
	FKismetEditorUtilities::UnregisterAutoBlueprintNodeCreation(this);

	MenuExtensibilityManager.Reset();
	ToolBarExtensibilityManager.Reset();

	// Unregister when shut down
	if(FModuleManager::Get().IsModuleLoaded("PropertyEditor"))
	{
		FPropertyEditorModule& PropertyModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
		PropertyModule.UnregisterCustomClassLayout("SkeletalMeshSocket");
		PropertyModule.UnregisterCustomClassLayout("EditorNotifyObject");
		PropertyModule.UnregisterCustomClassLayout("AnimGraphNode_Base");

		PropertyModule.UnregisterCustomPropertyTypeLayout("InputScaleBias");
		PropertyModule.UnregisterCustomPropertyTypeLayout("BoneReference");
	}
}


TSharedRef<IBlueprintEditor> FPersonaModule::CreatePersona( const EToolkitMode::Type Mode, const TSharedPtr< IToolkitHost >& InitToolkitHost, class USkeleton* Skeleton, class UAnimBlueprint* Blueprint, class UAnimationAsset* AnimationAsset, class USkeletalMesh * Mesh )
{
	TSharedRef< FPersona > NewPersona( new FPersona() );
	NewPersona->InitPersona( Mode, InitToolkitHost, Skeleton, Blueprint, AnimationAsset, Mesh );
	return NewPersona;
}

void FPersonaModule::OnNewBlueprintCreated(UBlueprint* InBlueprint)
{
	if (ensure(InBlueprint->UbergraphPages.Num() > 0))
	{
		UEdGraph* EventGraph = InBlueprint->UbergraphPages[0];

		int32 SafeXPosition = 0;
		int32 SafeYPosition = 0;

		if (EventGraph->Nodes.Num() != 0)
		{
			SafeXPosition = EventGraph->Nodes[0]->NodePosX;
			SafeYPosition = EventGraph->Nodes[EventGraph->Nodes.Num() - 1]->NodePosY + EventGraph->Nodes[EventGraph->Nodes.Num() - 1]->NodeHeight + 100;
		}

		// add try get owner node
		UK2Node_CallFunction* GetOwnerNode = NewObject<UK2Node_CallFunction>(EventGraph);
		UFunction* MakeNodeFunction = UAnimInstance::StaticClass()->FindFunctionByName(GET_FUNCTION_NAME_CHECKED(UAnimInstance, TryGetPawnOwner));
		GetOwnerNode->CreateNewGuid();
		GetOwnerNode->PostPlacedNewNode();
		GetOwnerNode->SetFromFunction(MakeNodeFunction);
		GetOwnerNode->SetFlags(RF_Transactional);
		GetOwnerNode->AllocateDefaultPins();
		GetOwnerNode->NodePosX = SafeXPosition;
		GetOwnerNode->NodePosY = SafeYPosition;
		UEdGraphSchema_K2::SetNodeMetaData(GetOwnerNode, FNodeMetadata::DefaultGraphNode);
		GetOwnerNode->DisableNode();

		EventGraph->AddNode(GetOwnerNode);
	}
}
