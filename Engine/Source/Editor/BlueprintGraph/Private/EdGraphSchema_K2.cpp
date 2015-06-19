// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "BlueprintGraphPrivatePCH.h"

#include "Engine/LevelScriptBlueprint.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Kismet/KismetArrayLibrary.h"
#include "GraphEditorActions.h"
#include "GraphEditorSettings.h"
#include "ScopedTransaction.h"
#include "ComponentAssetBroker.h"
#include "BlueprintEditorSettings.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Kismet2/KismetDebugUtilities.h"
#include "KismetCompiler.h"
#include "ComponentAssetBroker.h"
#include "AssetData.h"
#include "Editor/UnrealEd/Public/EdGraphUtilities.h"
#include "DefaultValueHelper.h"
#include "ObjectEditorUtils.h"
#include "ActorEditorUtils.h"
#include "ComponentTypeRegistry.h"
#include "BlueprintComponentNodeSpawner.h"
#include "AssetRegistryModule.h"
#include "Blueprint/AIBlueprintHelperLibrary.h"

#include "K2Node_CastByteToEnum.h"
#include "K2Node_ClassDynamicCast.h"
#include "K2Node_GetEnumeratorName.h"
#include "K2Node_GetEnumeratorNameAsString.h"
#include "K2Node_Tunnel.h"
#include "K2Node_SetFieldsInStruct.h"
#include "GenericCommands.h"


//////////////////////////////////////////////////////////////////////////
// FBlueprintMetadata

const FName FBlueprintMetadata::MD_AllowableBlueprintVariableType(TEXT("BlueprintType"));
const FName FBlueprintMetadata::MD_NotAllowableBlueprintVariableType(TEXT("NotBlueprintType"));

const FName FBlueprintMetadata::MD_BlueprintSpawnableComponent(TEXT("BlueprintSpawnableComponent"));
const FName FBlueprintMetadata::MD_IsBlueprintBase(TEXT("IsBlueprintBase"));
const FName FBlueprintMetadata::MD_RestrictedToClasses(TEXT("RestrictedToClasses"));
const FName FBlueprintMetadata::MD_ChildCanTick(TEXT("ChildCanTick"));
const FName FBlueprintMetadata::MD_ChildCannotTick(TEXT("ChildCannotTick"));
const FName FBlueprintMetadata::MD_IgnoreCategoryKeywordsInSubclasses(TEXT("IgnoreCategoryKeywordsInSubclasses"));


const FName FBlueprintMetadata::MD_Protected(TEXT("BlueprintProtected"));
const FName FBlueprintMetadata::MD_Latent(TEXT("Latent"));
const FName FBlueprintMetadata::MD_UnsafeForConstructionScripts(TEXT("UnsafeDuringActorConstruction"));
const FName FBlueprintMetadata::MD_FunctionCategory(TEXT("Category"));
const FName FBlueprintMetadata::MD_DeprecatedFunction(TEXT("DeprecatedFunction"));
const FName FBlueprintMetadata::MD_DeprecationMessage(TEXT("DeprecationMessage"));
const FName FBlueprintMetadata::MD_CompactNodeTitle(TEXT("CompactNodeTitle"));
const FName FBlueprintMetadata::MD_FriendlyName(TEXT("FriendlyName"));

const FName FBlueprintMetadata::MD_ExposeOnSpawn(TEXT("ExposeOnSpawn"));
const FName FBlueprintMetadata::MD_DefaultToSelf(TEXT("DefaultToSelf"));
const FName FBlueprintMetadata::MD_WorldContext(TEXT("WorldContext"));
const FName FBlueprintMetadata::MD_CallableWithoutWorldContext(TEXT("CallableWithoutWorldContext"));
const FName FBlueprintMetadata::MD_AutoCreateRefTerm(TEXT("AutoCreateRefTerm"));

const FName FBlueprintMetadata::MD_ShowWorldContextPin(TEXT("ShowWorldContextPin"));
const FName FBlueprintMetadata::MD_Private(TEXT("BlueprintPrivate"));

const FName FBlueprintMetadata::MD_BlueprintInternalUseOnly(TEXT("BlueprintInternalUseOnly"));
const FName FBlueprintMetadata::MD_NeedsLatentFixup(TEXT("NeedsLatentFixup"));

const FName FBlueprintMetadata::MD_LatentCallbackTarget(TEXT("LatentCallbackTarget"));
const FName FBlueprintMetadata::MD_AllowPrivateAccess(TEXT("AllowPrivateAccess"));

const FName FBlueprintMetadata::MD_ExposeFunctionCategories(TEXT("ExposeFunctionCategories"));

const FName FBlueprintMetadata::MD_CannotImplementInterfaceInBlueprint(TEXT("CannotImplementInterfaceInBlueprint"));
const FName FBlueprintMetadata::MD_ProhibitedInterfaces(TEXT("ProhibitedInterfaces"));

const FName FBlueprintMetadata::MD_FunctionKeywords(TEXT("Keywords"));

const FName FBlueprintMetadata::MD_ExpandEnumAsExecs(TEXT("ExpandEnumAsExecs"));

const FName FBlueprintMetadata::MD_CommutativeAssociativeBinaryOperator(TEXT("CommutativeAssociativeBinaryOperator"));
const FName FBlueprintMetadata::MD_MaterialParameterCollectionFunction(TEXT("MaterialParameterCollectionFunction"));

const FName FBlueprintMetadata::MD_Tooltip(TEXT("Tooltip"));

const FName FBlueprintMetadata::MD_CallInEditor(TEXT("CallInEditor"));

const FName FBlueprintMetadata::MD_DataTablePin(TEXT("DataTablePin"));

const FName FBlueprintMetadata::MD_NativeMakeFunction(TEXT("HasNativeMake"));
const FName FBlueprintMetadata::MD_NativeBreakFunction(TEXT("HasNativeBreak"));

const FName FBlueprintMetadata::MD_DynamicOutputType(TEXT("DeterminesOutputType"));
const FName FBlueprintMetadata::MD_DynamicOutputParam(TEXT("DynamicOutputParam"));

const FName FBlueprintMetadata::MD_ArrayParam(TEXT("ArrayParm"));
const FName FBlueprintMetadata::MD_ArrayDependentParam(TEXT("ArrayTypeDependentParams"));

//////////////////////////////////////////////////////////////////////////

#define LOCTEXT_NAMESPACE "KismetSchema"

UEdGraphSchema_K2::FPinTypeTreeInfo::FPinTypeTreeInfo(const FText& InFriendlyCategoryName, const FString& CategoryName, const UEdGraphSchema_K2* Schema, const FText& InTooltip, bool bInReadOnly/*=false*/)
{
	Init(InFriendlyCategoryName, CategoryName, Schema, InTooltip, bInReadOnly);
}

/** Helper class to gather variable types */
class FGatherTypesHelper
{
private:
	typedef TSharedPtr<UEdGraphSchema_K2::FPinTypeTreeInfo> FPinTypeTreeInfoPtr;
	struct FCompareChildren
	{
		FORCEINLINE bool operator()(const FPinTypeTreeInfoPtr A, const FPinTypeTreeInfoPtr B) const
		{
			return (A->GetDescription().ToString() < B->GetDescription().ToString());
		}
	};

	/** Helper function to add an unloaded asset to the children, does no validation on passed data */
	static void AddUnloadedAsset(const FAssetData& InAsset, const FString& InCategoryName, TArray<FPinTypeTreeInfoPtr>& OutChildren)
	{
		const FString* TooltipPtr = InAsset.TagsAndValues.Find(TEXT("Tooltip"));
		const FString Tooltip = (TooltipPtr && !TooltipPtr->IsEmpty()) ? *TooltipPtr : InAsset.ObjectPath.ToString();

		FPinTypeTreeInfoPtr TypeTreeInfo = MakeShareable(new UEdGraphSchema_K2::FPinTypeTreeInfo(InCategoryName, InAsset.ToStringReference(), FText::FromString(Tooltip)));
		TypeTreeInfo->FriendlyName = FText::FromString(FName::NameToDisplayString(InAsset.AssetName.ToString(), false));
		OutChildren.Add(TypeTreeInfo);
	}

	/**
	 * Gets a list of variable subtypes that are valid for the specified type
	 *
	 * @param	Type			The type to grab subtypes for
	 * @param	SubtypesList	(out) Upon return, this will be a list of valid subtype objects for the specified type
	 */
	static void GetVariableSubtypes(const FString& Type, TArray<UObject*>& OutSubtypesList)
	{
		OutSubtypesList.Empty();

		if (Type == UEdGraphSchema_K2::PC_Struct)
		{
			// Find script structs marked with "BlueprintType=true" in their metadata, and add to the list
			for (TObjectIterator<UScriptStruct> StructIt; StructIt; ++StructIt)
			{
				UScriptStruct* ScriptStruct = *StructIt;
				if (UEdGraphSchema_K2::IsAllowableBlueprintVariableType(ScriptStruct))
				{
					OutSubtypesList.Add(ScriptStruct);
				}
			}
		}
		else if (Type == UEdGraphSchema_K2::PC_Class)
		{
			// Generate a list of all potential objects which have "BlueprintType=true" in their metadata
			for (TObjectIterator<UClass> ClassIt; ClassIt; ++ClassIt)
			{
				UClass* CurrentClass = *ClassIt;
				if (UEdGraphSchema_K2::IsAllowableBlueprintVariableType(CurrentClass) && !CurrentClass->HasAnyClassFlags(CLASS_Deprecated))
				{
					OutSubtypesList.Add(CurrentClass);
				}
			}
		}
		else if (Type == UEdGraphSchema_K2::PC_Object)
		{
			// Generate a list of all potential objects which have "BlueprintType=true" in their metadata (that aren't interfaces)
			for (TObjectIterator<UClass> ClassIt; ClassIt; ++ClassIt)
			{
				UClass* CurrentClass = *ClassIt;
				if (!CurrentClass->IsChildOf(UInterface::StaticClass()) && UEdGraphSchema_K2::IsAllowableBlueprintVariableType(CurrentClass) && !CurrentClass->HasAnyClassFlags(CLASS_Deprecated))
				{
					OutSubtypesList.Add(CurrentClass);
				}
			}
		}
		else if (Type == UEdGraphSchema_K2::PC_Interface)
		{
			// Generate a list of all potential objects which have "BlueprintType=true" in their metadata (only ones that are interfaces)
			for (TObjectIterator<UClass> ClassIt; ClassIt; ++ClassIt)
			{
				UClass* CurrentClass = *ClassIt;
				if (CurrentClass->IsChildOf(UInterface::StaticClass()) && UEdGraphSchema_K2::IsAllowableBlueprintVariableType(CurrentClass))
				{
					OutSubtypesList.Add(CurrentClass);
				}
			}
		}
		else if (Type == UEdGraphSchema_K2::PC_Enum)
		{
			// Generate a list of all potential enums which have "BlueprintType=true" in their metadata
			for (TObjectIterator<UEnum> EnumIt; EnumIt; ++EnumIt)
			{
				UEnum* CurrentEnum = *EnumIt;
				if (UEdGraphSchema_K2::IsAllowableBlueprintVariableType(CurrentEnum))
				{
					OutSubtypesList.Add(CurrentEnum);
				}
			}
		}
	}

public:
	/**
	 * Gathers all valid sub-types (loaded and unloaded) of a passed category and sorts them alphabetically
	 * @param FriendlyName		Friendly name to be used for the tooltip if there is no available data
	 * @param CategoryName		Category (type) to find sub-types of
	 * @param Schema			Schema to use
	 * @param OutChildren		All the gathered children
	 */
	static void Gather(const FText& FriendlyName, const FString& CategoryName, const UEdGraphSchema_K2* Schema, TArray<FPinTypeTreeInfoPtr>& OutChildren)
	{
		// Gather any loaded subtype children first
		TArray<UObject*> LoadedSubTypes;
		GetVariableSubtypes(CategoryName, LoadedSubTypes);

		FEdGraphPinType LoadedPinSubtype;
		LoadedPinSubtype.PinCategory = (CategoryName == UEdGraphSchema_K2::PC_Enum ? UEdGraphSchema_K2::PC_Byte : CategoryName);
		LoadedPinSubtype.PinSubCategory = TEXT("");
		LoadedPinSubtype.PinSubCategoryObject = NULL;

		// Add any loaded subtypes to the children list
		for (auto it = LoadedSubTypes.CreateIterator(); it; ++it)
		{
			FText SubtypeTooltip;
			UStruct* Struct = Cast<UStruct>(*it);
			if(Struct != NULL)
			{
				SubtypeTooltip = (Struct ? Struct->GetToolTipText() : FriendlyName);
			}
			
			OutChildren.Add( MakeShareable(new UEdGraphSchema_K2::FPinTypeTreeInfo(LoadedPinSubtype.PinCategory, *it, SubtypeTooltip)) );
		}

		UClass* AssetRegistrySearchClass = nullptr;
		if(Schema->PC_Struct == CategoryName)
		{
			AssetRegistrySearchClass = UUserDefinedStruct::StaticClass();
		}
		else if(Schema->PC_Enum == CategoryName)
		{
			AssetRegistrySearchClass = UUserDefinedEnum::StaticClass();
		}
		else if(Schema->PC_Object == CategoryName || Schema->PC_Class == CategoryName || Schema->PC_Interface == CategoryName)
		{
			AssetRegistrySearchClass = UBlueprint::StaticClass();
		}

		// Gather all assets of the specified class
		const FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
		TArray<FAssetData> AssetData;
		AssetRegistryModule.Get().GetAssetsByClass(AssetRegistrySearchClass->GetFName(), AssetData);

		// Search through all the unloaded assets, filtered by certain rules based on struct/enum or object/class/interface
		if (Schema->PC_Struct == CategoryName || Schema->PC_Enum == CategoryName)
		{
			for (int32 AssetIndex = 0; AssetIndex < AssetData.Num(); ++AssetIndex)
			{
				const FAssetData& Asset = AssetData[AssetIndex];
				if (Asset.IsValid() && !Asset.IsAssetLoaded())
				{
					AddUnloadedAsset(Asset, CategoryName, OutChildren);
				}
			}
		}
		else if(Schema->PC_Object == CategoryName || Schema->PC_Class == CategoryName || Schema->PC_Interface == CategoryName)
		{
			const FString BPTypeAllowed = (Schema->PC_Interface == CategoryName)? TEXT("BPTYPE_Interface") : TEXT("BPTYPE_Normal");

			for (int32 AssetIndex = 0; AssetIndex < AssetData.Num(); ++AssetIndex)
			{
				const FAssetData& Asset = AssetData[AssetIndex];

				if (Asset.IsValid() && !Asset.IsAssetLoaded())
				{
					// Based on the category, only certain Blueprint types are available
					const FString* BlueprintTypeStr = Asset.TagsAndValues.Find("BlueprintType");
					if(BlueprintTypeStr && *BlueprintTypeStr == BPTypeAllowed)
					{
						uint32 ClassFlags = 0;
						const FString* ClassFlagsStr = Asset.TagsAndValues.Find("ClassFlags");
						if(ClassFlagsStr)
						{
							ClassFlags = FCString::Atoi(**ClassFlagsStr);
						}

						// Do not allow deprecated Blueprints to be added
						if(!(ClassFlags & CLASS_Deprecated))
						{
							AddUnloadedAsset(Asset, CategoryName, OutChildren);
						}
					}
				}
			}
		}
		OutChildren.Sort(FCompareChildren());
	}

	/** Loads an asset based on the AssetReference through the asset registry */
	static UObject* LoadAsset(const FStringAssetReference& AssetReference)
	{
		if (AssetReference.IsValid())
		{
			const FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
			const FAssetData AssetData = AssetRegistryModule.Get().GetAssetByObjectPath(*AssetReference.ToString());
			return AssetData.GetAsset();
		}
		return nullptr;
	}
};

const FEdGraphPinType& UEdGraphSchema_K2::FPinTypeTreeInfo::GetPinType(bool bForceLoadedSubCategoryObject)
{
	if (bForceLoadedSubCategoryObject)
	{
		// Only attempt to load the sub category object if we need to
		if ( SubCategoryObjectAssetReference.IsValid() && (!PinType.PinSubCategoryObject.IsValid() || FStringAssetReference(PinType.PinSubCategoryObject.Get()) != SubCategoryObjectAssetReference) )
		{
			UObject* LoadedObject = FGatherTypesHelper::LoadAsset(SubCategoryObjectAssetReference);

			if(UBlueprint* BlueprintObject = Cast<UBlueprint>(LoadedObject))
			{
				PinType.PinSubCategoryObject = *BlueprintObject->GeneratedClass;
			}
			else
			{
				PinType.PinSubCategoryObject = LoadedObject;
			}
		}
	}
	else
	{
		if (SubCategoryObjectAssetReference.IsValid())
		{
			const FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
			const FAssetData AssetData = AssetRegistryModule.Get().GetAssetByObjectPath(*SubCategoryObjectAssetReference.ToString());

			if(!AssetData.IsAssetLoaded())
			{
				UObject* LoadedObject = FindObject<UClass>(ANY_PACKAGE, *AssetData.AssetClass.ToString());

				// If the unloaded asset is a Blueprint, we need to pull the generated class and assign that
				if(UBlueprint* BlueprintObject = Cast<UBlueprint>(LoadedObject))
				{
					PinType.PinSubCategoryObject = *BlueprintObject->GeneratedClass;
				}
				else
				{
					PinType.PinSubCategoryObject = LoadedObject;
				}
			}
			else
			{
				PinType.PinSubCategoryObject = AssetData.GetAsset();
			}
		}
	}
	return PinType;
}

void UEdGraphSchema_K2::FPinTypeTreeInfo::Init(const FText& InFriendlyName, const FString& CategoryName, const UEdGraphSchema_K2* Schema, const FText& InTooltip, bool bInReadOnly)
{
	check( !CategoryName.IsEmpty() );
	check( Schema );

	FriendlyName = InFriendlyName;
	Tooltip = InTooltip;
	PinType.PinCategory = (CategoryName == PC_Enum? PC_Byte : CategoryName);
	PinType.PinSubCategory = TEXT("");
	PinType.PinSubCategoryObject = NULL;

	bReadOnly = bInReadOnly;

	if (Schema->DoesTypeHaveSubtypes(CategoryName))
	{
		FGatherTypesHelper::Gather(InFriendlyName, CategoryName, Schema, Children);
	}
}

UEdGraphSchema_K2::FPinTypeTreeInfo::FPinTypeTreeInfo(const FString& CategoryName, UObject* SubCategoryObject, const FText& InTooltip, bool bInReadOnly/*=false*/)
{
	check( !CategoryName.IsEmpty() );
	check( SubCategoryObject );

	Tooltip = InTooltip;
	PinType.PinCategory = CategoryName;
	PinType.PinSubCategoryObject = SubCategoryObject;

	bReadOnly = bInReadOnly;
}

UEdGraphSchema_K2::FPinTypeTreeInfo::FPinTypeTreeInfo(const FString& CategoryName, const FStringAssetReference& SubCategoryObject, const FText& InTooltip, bool bInReadOnly)
{
	check(!CategoryName.IsEmpty());
	check(SubCategoryObject.IsValid());

	Tooltip = InTooltip;
	PinType.PinCategory = CategoryName;

	SubCategoryObjectAssetReference = SubCategoryObject;
	PinType.PinSubCategoryObject = SubCategoryObjectAssetReference.ResolveObject();

	bReadOnly = bInReadOnly;
}

FText UEdGraphSchema_K2::FPinTypeTreeInfo::GetDescription() const
{
	if (!FriendlyName.IsEmpty())
	{
		return FriendlyName;
	}
	else if (PinType.PinSubCategoryObject.IsValid())
	{
		FText DisplayName;
		if (UField* SubCategoryField = Cast<UField>(PinType.PinSubCategoryObject.Get()))
		{
			DisplayName = SubCategoryField->GetDisplayNameText();
		}
		else
		{
			DisplayName = FText::FromString(FName::NameToDisplayString(PinType.PinSubCategoryObject->GetName(), PinType.PinCategory == PC_Boolean));
		}

		return DisplayName;
	}
	else
	{
		return LOCTEXT("PinDescriptionError", "Error!");
	}
}

const FString UEdGraphSchema_K2::PC_Exec(TEXT("exec"));
const FString UEdGraphSchema_K2::PC_Boolean(TEXT("bool"));
const FString UEdGraphSchema_K2::PC_Byte(TEXT("byte"));
const FString UEdGraphSchema_K2::PC_Class(TEXT("class"));
const FString UEdGraphSchema_K2::PC_Int(TEXT("int"));
const FString UEdGraphSchema_K2::PC_Float(TEXT("float"));
const FString UEdGraphSchema_K2::PC_Name(TEXT("name"));
const FString UEdGraphSchema_K2::PC_Delegate(TEXT("delegate"));
const FString UEdGraphSchema_K2::PC_MCDelegate(TEXT("mcdelegate"));
const FString UEdGraphSchema_K2::PC_Object(TEXT("object"));
const FString UEdGraphSchema_K2::PC_Interface(TEXT("interface"));
const FString UEdGraphSchema_K2::PC_String(TEXT("string"));
const FString UEdGraphSchema_K2::PC_Text(TEXT("text"));
const FString UEdGraphSchema_K2::PC_Struct(TEXT("struct"));
const FString UEdGraphSchema_K2::PC_Wildcard(TEXT("wildcard"));
const FString UEdGraphSchema_K2::PC_Enum(TEXT("enum"));
const FString UEdGraphSchema_K2::PSC_Self(TEXT("self"));
const FString UEdGraphSchema_K2::PSC_Index(TEXT("index"));
const FString UEdGraphSchema_K2::PN_Execute(TEXT("execute"));
const FString UEdGraphSchema_K2::PN_Then(TEXT("then"));
const FString UEdGraphSchema_K2::PN_Completed(TEXT("Completed"));
const FString UEdGraphSchema_K2::PN_DelegateEntry(TEXT("delegate"));
const FString UEdGraphSchema_K2::PN_EntryPoint(TEXT("EntryPoint"));
const FString UEdGraphSchema_K2::PN_Self(TEXT("self"));
const FString UEdGraphSchema_K2::PN_Else(TEXT("else"));
const FString UEdGraphSchema_K2::PN_Loop(TEXT("loop"));
const FString UEdGraphSchema_K2::PN_After(TEXT("after"));
const FString UEdGraphSchema_K2::PN_ReturnValue(TEXT("ReturnValue"));
const FString UEdGraphSchema_K2::PN_ObjectToCast(TEXT("Object"));
const FString UEdGraphSchema_K2::PN_Condition(TEXT("Condition"));
const FString UEdGraphSchema_K2::PN_Start(TEXT("Start"));
const FString UEdGraphSchema_K2::PN_Stop(TEXT("Stop"));
const FString UEdGraphSchema_K2::PN_Index(TEXT("Index"));
const FString UEdGraphSchema_K2::PN_Item(TEXT("Item"));
const FString UEdGraphSchema_K2::PN_CastSucceeded(TEXT("then"));
const FString UEdGraphSchema_K2::PN_CastFailed(TEXT("CastFailed"));
const FString UEdGraphSchema_K2::PN_CastedValuePrefix(TEXT("As"));
const FString UEdGraphSchema_K2::PN_MatineeFinished(TEXT("Finished"));

const FName UEdGraphSchema_K2::FN_UserConstructionScript(TEXT("UserConstructionScript"));
const FName UEdGraphSchema_K2::FN_ExecuteUbergraphBase(TEXT("ExecuteUbergraph"));
const FName UEdGraphSchema_K2::GN_EventGraph(TEXT("EventGraph"));
const FName UEdGraphSchema_K2::GN_AnimGraph(TEXT("AnimGraph"));
const FName UEdGraphSchema_K2::VR_DefaultCategory(TEXT("Default"));

const int32 UEdGraphSchema_K2::AG_LevelReference = 100;

const UScriptStruct* UEdGraphSchema_K2::VectorStruct = nullptr;
const UScriptStruct* UEdGraphSchema_K2::RotatorStruct = nullptr;
const UScriptStruct* UEdGraphSchema_K2::TransformStruct = nullptr;
const UScriptStruct* UEdGraphSchema_K2::LinearColorStruct = nullptr;
const UScriptStruct* UEdGraphSchema_K2::ColorStruct = nullptr;

bool UEdGraphSchema_K2::bGeneratingDocumentation = false;
int32 UEdGraphSchema_K2::CurrentCacheRefreshID = 0;

UEdGraphSchema_K2::UEdGraphSchema_K2(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// Initialize cached static references to well-known struct types
	if (VectorStruct == nullptr)
	{
		VectorStruct = GetBaseStructure(TEXT("Vector"));
		RotatorStruct = GetBaseStructure(TEXT("Rotator"));
		TransformStruct = GetBaseStructure(TEXT("Transform"));
		LinearColorStruct = GetBaseStructure(TEXT("LinearColor"));
		ColorStruct = GetBaseStructure(TEXT("Color"));
	}
}

bool UEdGraphSchema_K2::DoesFunctionHaveOutParameters( const UFunction* Function ) const
{
	if ( Function != NULL )
	{
		for ( TFieldIterator<UProperty> PropertyIt(Function); PropertyIt; ++PropertyIt )
		{
			if ( PropertyIt->PropertyFlags & CPF_OutParm )
			{
				return true;
			}
		}
	}

	return false;
}

bool UEdGraphSchema_K2::CanFunctionBeUsedInGraph(const UClass* InClass, const UFunction* InFunction, const UEdGraph* InDestGraph, uint32 InAllowedFunctionTypes, bool bInCalledForEach, FText* OutReason) const
{
	if (CanUserKismetCallFunction(InFunction))
	{
		bool bLatentFuncsAllowed = true;
		bool bIsConstructionScript = false;

		if(InDestGraph != nullptr)
		{
			bLatentFuncsAllowed = (GetGraphType(InDestGraph) == GT_Ubergraph || (GetGraphType(InDestGraph) == GT_Macro));
			bIsConstructionScript = IsConstructionScript(InDestGraph);
		}

		const bool bIsPureFunc = (InFunction->HasAnyFunctionFlags(FUNC_BlueprintPure) != false);
		if (bIsPureFunc)
		{
			const bool bAllowPureFuncs = (InAllowedFunctionTypes & FT_Pure) != 0;
			if (!bAllowPureFuncs)
			{
				if(OutReason != nullptr)
				{
					*OutReason = LOCTEXT("PureFunctionsNotAllowed", "Pure functions are not allowed.");
				}

				return false;
			}
		}
		else
		{
			const bool bAllowImperativeFuncs = (InAllowedFunctionTypes & FT_Imperative) != 0;
			if (!bAllowImperativeFuncs)
			{
				if(OutReason != nullptr)
				{
					*OutReason = LOCTEXT("ImpureFunctionsNotAllowed", "Impure functions are not allowed.");
				}

				return false;
			}
		}

		const bool bIsConstFunc = (InFunction->HasAnyFunctionFlags(FUNC_Const) != false);
		const bool bAllowConstFuncs = (InAllowedFunctionTypes & FT_Const) != 0;
		if (bIsConstFunc && !bAllowConstFuncs)
		{
			if(OutReason != nullptr)
			{
				*OutReason = LOCTEXT("ConstFunctionsNotAllowed", "Const functions are not allowed.");
			}

			return false;
		}

		const bool bIsLatent = InFunction->HasMetaData(FBlueprintMetadata::MD_Latent);
		if (bIsLatent && !bLatentFuncsAllowed)
		{
			if(OutReason != nullptr)
			{
				*OutReason = LOCTEXT("LatentFunctionsNotAllowed", "Latent functions cannot be used here.");
			}

			return false;
		}

		const bool bIsProtected = InFunction->GetBoolMetaData(FBlueprintMetadata::MD_Protected);
		const bool bFuncBelongsToSubClass = InClass->IsChildOf(InFunction->GetOuterUClass());
		if (bIsProtected)
		{
			const bool bAllowProtectedFuncs = (InAllowedFunctionTypes & FT_Protected) != 0;
			if (!bAllowProtectedFuncs)
			{
				if(OutReason != nullptr)
				{
					*OutReason = LOCTEXT("ProtectedFunctionsNotAllowed", "Protected functions are not allowed.");
				}

				return false;
			}

			if (!bFuncBelongsToSubClass)
			{
				if(OutReason != nullptr)
				{
					*OutReason = LOCTEXT("ProtectedFunctionInaccessible", "Function is protected and inaccessible.");
				}

				return false;
			}
		}

		const bool bIsPrivate = InFunction->GetBoolMetaData(FBlueprintMetadata::MD_Private);
		const bool bFuncBelongsToClass = bFuncBelongsToSubClass && (InFunction->GetOuterUClass() == InClass);
		if (bIsPrivate && !bFuncBelongsToClass)
		{
			if(OutReason != nullptr)
			{
				*OutReason = LOCTEXT("PrivateFunctionInaccessible", "Function is private and inaccessible.");
			}

			return false;
		}

		const bool bIsUnsafeForConstruction = InFunction->GetBoolMetaData(FBlueprintMetadata::MD_UnsafeForConstructionScripts);	
		if (bIsUnsafeForConstruction && bIsConstructionScript)
		{
			if(OutReason != nullptr)
			{
				*OutReason = LOCTEXT("FunctionUnsafeForConstructionScript", "Function cannot be used in a Construction Script.");
			}

			return false;
		}

		const bool bRequiresWorldContext = InFunction->HasMetaData(FBlueprintMetadata::MD_WorldContext);
		if (bRequiresWorldContext)
		{
			if (InDestGraph && !InFunction->HasMetaData(FBlueprintMetadata::MD_CallableWithoutWorldContext))
			{
				auto BP = FBlueprintEditorUtils::FindBlueprintForGraph(InDestGraph);
				const bool bIsFunctLib = BP && (EBlueprintType::BPTYPE_FunctionLibrary == BP->BlueprintType);
				UClass* ParentClass = BP ? BP->ParentClass : NULL;
				const bool bIncompatibleParrent = ParentClass && (!ParentClass->GetDefaultObject()->ImplementsGetWorld() && !ParentClass->HasMetaData(FBlueprintMetadata::MD_ShowWorldContextPin));
				if (!bIsFunctLib && bIncompatibleParrent)
				{
					if(OutReason != nullptr)
					{
						*OutReason = LOCTEXT("FunctionRequiresWorldContext", "Function requires a world context.");
					}

					return false;
				}
			}
		}

		const bool bFunctionStatic = InFunction->HasAllFunctionFlags(FUNC_Static);
		const bool bHasReturnParams = (InFunction->GetReturnProperty() != NULL);
		const bool bHasArrayPointerParms = InFunction->HasMetaData(FBlueprintMetadata::MD_ArrayParam);

		const bool bAllowForEachCall = !bFunctionStatic && !bIsLatent && !bIsPureFunc && !bIsConstFunc && !bHasReturnParams && !bHasArrayPointerParms;
		if (bInCalledForEach && !bAllowForEachCall)
		{
			if(OutReason != nullptr)
			{
				if(bFunctionStatic)
				{
					*OutReason = LOCTEXT("StaticFunctionsNotAllowedInForEachContext", "Static functions cannot be used within a ForEach context.");
				}
				else if(bIsLatent)
				{
					*OutReason = LOCTEXT("LatentFunctionsNotAllowedInForEachContext", "Latent functions cannot be used within a ForEach context.");
				}
				else if(bIsPureFunc)
				{
					*OutReason = LOCTEXT("PureFunctionsNotAllowedInForEachContext", "Pure functions cannot be used within a ForEach context.");
				}
				else if(bIsConstFunc)
				{
					*OutReason = LOCTEXT("ConstFunctionsNotAllowedInForEachContext", "Const functions cannot be used within a ForEach context.");
				}
				else if(bHasReturnParams)
				{
					*OutReason = LOCTEXT("FunctionsWithReturnValueNotAllowedInForEachContext", "Functions that return a value cannot be used within a ForEach context.");
				}
				else if(bHasArrayPointerParms)
				{
					*OutReason = LOCTEXT("FunctionsWithArrayParmsNotAllowedInForEachContext", "Functions with array parameters cannot be used within a ForEach context.");
				}
				else
				{
					*OutReason = LOCTEXT("FunctionNotAllowedInForEachContext", "Function cannot be used within a ForEach context.");
				}
			}

			return false;
		}

		return true;
	}

	if(OutReason != nullptr)
	{
		*OutReason = LOCTEXT("FunctionInvalid", "Invalid function.");
	}

	return false;
}

UFunction* UEdGraphSchema_K2::GetCallableParentFunction(UFunction* Function) const
{
	if( Function && Cast<UClass>(Function->GetOuter()) )
	{
		const FName FunctionName = Function->GetFName();

		// Search up the parent scopes
		UClass* ParentClass = CastChecked<UClass>(Function->GetOuter())->GetSuperClass();
		UFunction* ClassFunction = ParentClass->FindFunctionByName(FunctionName);

		return ClassFunction;
	}

	return NULL;
}

bool UEdGraphSchema_K2::CanUserKismetCallFunction(const UFunction* Function)
{
	return Function && 
		(Function->HasAllFunctionFlags(FUNC_BlueprintCallable) && !Function->HasAllFunctionFlags(FUNC_Delegate) && !Function->GetBoolMetaData(FBlueprintMetadata::MD_BlueprintInternalUseOnly) && !Function->HasMetaData(FBlueprintMetadata::MD_DeprecatedFunction));
}

bool UEdGraphSchema_K2::CanKismetOverrideFunction(const UFunction* Function)
{
	return  Function && 
		(Function->HasAllFunctionFlags(FUNC_BlueprintEvent) && !Function->HasAllFunctionFlags(FUNC_Delegate) && !Function->GetBoolMetaData(FBlueprintMetadata::MD_BlueprintInternalUseOnly) && !Function->HasMetaData(FBlueprintMetadata::MD_DeprecatedFunction));
}

bool UEdGraphSchema_K2::HasFunctionAnyOutputParameter(const UFunction* InFunction)
{
	check(InFunction);
	for (TFieldIterator<UProperty> PropIt(InFunction); PropIt && (PropIt->PropertyFlags & CPF_Parm); ++PropIt)
	{
		UProperty* FuncParam = *PropIt;
		if (FuncParam->HasAnyPropertyFlags(CPF_ReturnParm) || (FuncParam->HasAnyPropertyFlags(CPF_OutParm) && !FuncParam->HasAnyPropertyFlags(CPF_ReferenceParm) && !FuncParam->HasAnyPropertyFlags(CPF_ConstParm)))
		{
			return true;
		}
	}

	return false;
}

bool UEdGraphSchema_K2::FunctionCanBePlacedAsEvent(const UFunction* InFunction)
{
	// First check we are override-able, non-static and non-const
	if (!InFunction || !CanKismetOverrideFunction(InFunction) || InFunction->HasAnyFunctionFlags(FUNC_Static|FUNC_Const))
	{
		return false;
	}

	// Then look to see if we have any output, return, or reference params
	return !HasFunctionAnyOutputParameter(InFunction);
}

bool UEdGraphSchema_K2::FunctionCanBeUsedInDelegate(const UFunction* InFunction)
{	
	if (!InFunction || 
		!CanUserKismetCallFunction(InFunction) ||
		InFunction->HasMetaData(FBlueprintMetadata::MD_Latent) ||
		InFunction->HasAllFunctionFlags(FUNC_BlueprintPure))
	{
		return false;
	}

	return true;
}

FText UEdGraphSchema_K2::GetFriendlySignatureName(const UFunction* Function)
{
	return UK2Node_CallFunction::GetUserFacingFunctionName( Function );
}

void UEdGraphSchema_K2::GetAutoEmitTermParameters(const UFunction* Function, TArray<FString>& AutoEmitParameterNames) const
{
	AutoEmitParameterNames.Empty();

	if( Function->HasMetaData(FBlueprintMetadata::MD_AutoCreateRefTerm) )
	{
		FString MetaData = Function->GetMetaData(FBlueprintMetadata::MD_AutoCreateRefTerm);
		MetaData.ParseIntoArray(AutoEmitParameterNames, TEXT(","), true);
	}
}

bool UEdGraphSchema_K2::FunctionHasParamOfType(const UFunction* InFunction, UEdGraph const* InGraph, const FEdGraphPinType& DesiredPinType, bool bWantOutput) const
{
	TSet<FString> HiddenPins;
	FBlueprintEditorUtils::GetHiddenPinsForFunction(InGraph, InFunction, HiddenPins);

	// Iterate over all params of function
	for (TFieldIterator<UProperty> PropIt(InFunction); PropIt && (PropIt->PropertyFlags & CPF_Parm); ++PropIt)
	{
		UProperty* FuncParam = *PropIt;

		// Ensure that this isn't a hidden parameter
		if (!HiddenPins.Contains(FuncParam->GetName()))
		{
			// See if this is the direction we want (input or output)
			const bool bIsFunctionInput = !FuncParam->HasAnyPropertyFlags(CPF_OutParm) || FuncParam->HasAnyPropertyFlags(CPF_ReferenceParm);
			if ((!bIsFunctionInput && bWantOutput) || (bIsFunctionInput && !bWantOutput))
			{
				// See if this pin has compatible types
				FEdGraphPinType ParamPinType;
				bool bConverted = ConvertPropertyToPinType(FuncParam, ParamPinType);
				if (bConverted)
				{
					if (bIsFunctionInput && ArePinTypesCompatible(DesiredPinType, ParamPinType))
					{
						return true;
					}
					else if (!bIsFunctionInput && ArePinTypesCompatible(ParamPinType, DesiredPinType))
					{
						return true;
					}
				}
			}
		}
	}

	// Boo, no pin of this type
	return false;
}

void UEdGraphSchema_K2::AddExtraFunctionFlags(const UEdGraph* CurrentGraph, int32 ExtraFlags) const
{
	for (auto It = CurrentGraph->Nodes.CreateConstIterator(); It; ++It)
	{
		if (UK2Node_FunctionEntry* Node = Cast<UK2Node_FunctionEntry>(*It))
		{
			Node->ExtraFlags |= ExtraFlags;
		}
	}
}

void UEdGraphSchema_K2::MarkFunctionEntryAsEditable(const UEdGraph* CurrentGraph, bool bNewEditable) const
{
	for (auto It = CurrentGraph->Nodes.CreateConstIterator(); It; ++It)
	{
		if (UK2Node_EditablePinBase* Node = Cast<UK2Node_EditablePinBase>(*It))
		{
			Node->bIsEditable = bNewEditable;
		}
	}
}

void UEdGraphSchema_K2::ListFunctionsMatchingSignatureAsDelegates(FGraphContextMenuBuilder& ContextMenuBuilder, const UClass* Class, const UFunction* SignatureToMatch) const
{
	check(Class);

	for (TFieldIterator<UFunction> FunctionIt(Class, EFieldIteratorFlags::IncludeSuper); FunctionIt; ++FunctionIt)
	{
		const UFunction* TrialFunction = *FunctionIt;
		if (CanUserKismetCallFunction(TrialFunction) && TrialFunction->IsSignatureCompatibleWith(SignatureToMatch))
		{
			FString Description(TrialFunction->GetName());
			FString Tooltip = FString::Printf(TEXT("Existing function '%s' as delegate"), *(TrialFunction->GetName())); //@TODO: Need a better tooltip

			// @TODO
		}
	}

}

bool UEdGraphSchema_K2::IsActorValidForLevelScriptRefs(const AActor* TestActor, const UBlueprint* Blueprint) const
{
	check(Blueprint);

	return TestActor
		&& FBlueprintEditorUtils::IsLevelScriptBlueprint(Blueprint)
		&& (TestActor->GetLevel() == FBlueprintEditorUtils::GetLevelFromBlueprint(Blueprint))
		&& FKismetEditorUtilities::IsActorValidForLevelScript(TestActor);
}

void UEdGraphSchema_K2::ReplaceSelectedNode(UEdGraphNode* SourceNode, AActor* TargetActor)
{
	check(SourceNode);

	if (TargetActor != NULL)
	{
		UK2Node_Literal* LiteralNode = (UK2Node_Literal*)(SourceNode);
		if (LiteralNode)
		{
			const FScopedTransaction Transaction( LOCTEXT("ReplaceSelectedNodeUndoTransaction", "Replace Selected Node") );

			LiteralNode->Modify();
			LiteralNode->SetObjectRef( TargetActor );
			LiteralNode->ReconstructNode();
			UBlueprint* Blueprint = FBlueprintEditorUtils::FindBlueprintForGraphChecked(CastChecked<UEdGraph>(SourceNode->GetOuter()));
			FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
		}
	}
}

void UEdGraphSchema_K2::AddSelectedReplaceableNodes( UBlueprint* Blueprint, const UEdGraphNode* InGraphNode, FMenuBuilder* MenuBuilder ) const
{
	//Only allow replace object reference functionality for literal nodes
	const UK2Node_Literal* LiteralNode = Cast<UK2Node_Literal>(InGraphNode);
	if (LiteralNode)
	{
		USelection* SelectedActors = GEditor->GetSelectedActors();
		for(FSelectionIterator Iter(*SelectedActors); Iter; ++Iter)
		{
			// We only care about actors that are referenced in the world for literals, and also in the same level as this blueprint
			AActor* Actor = Cast<AActor>(*Iter);
			if( LiteralNode->GetObjectRef() != Actor && IsActorValidForLevelScriptRefs(Actor, Blueprint) )
			{
				FText Description = FText::Format( LOCTEXT("ChangeToActorName", "Change to <{0}>"), FText::FromString( Actor->GetActorLabel() ) );
				FText ToolTip = LOCTEXT("ReplaceNodeReferenceToolTip", "Replace node reference");
				MenuBuilder->AddMenuEntry( Description, ToolTip, FSlateIcon(), FUIAction(
					FExecuteAction::CreateUObject((UEdGraphSchema_K2*const)this, &UEdGraphSchema_K2::ReplaceSelectedNode, const_cast< UEdGraphNode* >(InGraphNode), Actor) ) );
			}
		}
	}
}



bool UEdGraphSchema_K2::CanUserKismetAccessVariable(const UProperty* Property, const UClass* InClass, EDelegateFilterMode FilterMode)
{
	const bool bIsDelegate = Property->IsA(UMulticastDelegateProperty::StaticClass());
	const bool bIsAccessible = Property->HasAllPropertyFlags(CPF_BlueprintVisible);
	const bool bIsAssignableOrCallable = Property->HasAnyPropertyFlags(CPF_BlueprintAssignable | CPF_BlueprintCallable);
	
	const bool bPassesDelegateFilter = (bIsAccessible && !bIsDelegate && (FilterMode != MustBeDelegate)) || 
		(bIsAssignableOrCallable && bIsDelegate && (FilterMode != CannotBeDelegate));

	const bool bHidden = FObjectEditorUtils::IsVariableCategoryHiddenFromClass(Property, InClass);

	return !Property->HasAnyPropertyFlags(CPF_Parm) && bPassesDelegateFilter && !bHidden;
}

bool UEdGraphSchema_K2::ClassHasBlueprintAccessibleMembers(const UClass* InClass) const
{
	// @TODO Don't show other blueprints yet...
	UBlueprint* ClassBlueprint = UBlueprint::GetBlueprintFromClass(InClass);
	if (!InClass->HasAnyClassFlags(CLASS_Deprecated | CLASS_NewerVersionExists) && (ClassBlueprint == NULL))
	{
		// Find functions
		for (TFieldIterator<UFunction> FunctionIt(InClass, EFieldIteratorFlags::IncludeSuper); FunctionIt; ++FunctionIt)
		{
			UFunction* Function = *FunctionIt;
			const bool bIsBlueprintProtected = Function->GetBoolMetaData(FBlueprintMetadata::MD_Protected);
			const bool bHidden = FObjectEditorUtils::IsFunctionHiddenFromClass(Function, InClass);
			if (UEdGraphSchema_K2::CanUserKismetCallFunction(Function) && !bIsBlueprintProtected && !bHidden)
			{
				return true;
			}
		}

		// Find vars
		for (TFieldIterator<UProperty> PropertyIt(InClass, EFieldIteratorFlags::IncludeSuper); PropertyIt; ++PropertyIt)
		{
			UProperty* Property = *PropertyIt;
			if (CanUserKismetAccessVariable(Property, InClass, CannotBeDelegate))
			{
				return true;
			}
		}
	}

	return false;
}

bool UEdGraphSchema_K2::IsAllowableBlueprintVariableType(const class UEnum* InEnum)
{
	return InEnum && (InEnum->GetBoolMetaData(FBlueprintMetadata::MD_AllowableBlueprintVariableType) || InEnum->IsA<UUserDefinedEnum>());
}

bool UEdGraphSchema_K2::IsAllowableBlueprintVariableType(const class UClass* InClass)
{
	if (InClass)
	{
		// No Skeleton classes or reinstancing classes (they would inherit the BlueprintType metadata)
		if (FKismetEditorUtilities::IsClassABlueprintSkeleton(InClass)
			|| InClass->HasAnyClassFlags(CLASS_NewerVersionExists))
		{
			return false;
		}

		// No Blueprint Macro Libraries
		if (FKismetEditorUtilities::IsClassABlueprintMacroLibrary(InClass))
		{
			return false;
		}

		// UObject is an exception, and is always a blueprint-able type
		if(InClass == UObject::StaticClass())
		{
			return true;
		}

		static const FBoolConfigValueHelper NotBlueprintType(TEXT("EditoronlyBP"), TEXT("bBlueprintIsNotBlueprintType"));
		if (NotBlueprintType && InClass->IsChildOf(UBlueprint::StaticClass()))
		{
			return false;
		}
		
		// cannot have level script variables
		if (InClass->IsChildOf(ALevelScriptActor::StaticClass()))
		{
			return false;
		}

		const UClass* ParentClass = InClass;
		while(ParentClass)
		{
			// Climb up the class hierarchy and look for "BlueprintType" and "NotBlueprintType" to see if this class is allowed.
			if(ParentClass->GetBoolMetaData(FBlueprintMetadata::MD_AllowableBlueprintVariableType)
				|| ParentClass->HasMetaData(FBlueprintMetadata::MD_BlueprintSpawnableComponent))
			{
				return true;
			}
			else if(ParentClass->GetBoolMetaData(FBlueprintMetadata::MD_NotAllowableBlueprintVariableType))
			{
				return false;
			}
			ParentClass = ParentClass->GetSuperClass();
		}
	}
	
	return false;
}

bool UEdGraphSchema_K2::IsAllowableBlueprintVariableType(const class UScriptStruct *InStruct)
{
	if (auto UDStruct = Cast<const UUserDefinedStruct>(InStruct))
	{
		if (EUserDefinedStructureStatus::UDSS_UpToDate != UDStruct->Status.GetValue())
		{
			return false;
		}
	}
	return InStruct && (InStruct->GetBoolMetaDataHierarchical(FBlueprintMetadata::MD_AllowableBlueprintVariableType));
}

bool UEdGraphSchema_K2::DoesGraphSupportImpureFunctions(const UEdGraph* InGraph) const
{
	const EGraphType GraphType = GetGraphType(InGraph);
	const bool bAllowImpureFuncs = GraphType != GT_Animation; //@TODO: It's really more nuanced than this (e.g., in a function someone wants to write as pure)

	return bAllowImpureFuncs;
}

bool UEdGraphSchema_K2::IsPropertyExposedOnSpawn(const UProperty* Property)
{
	if (Property)
	{
		const bool bMeta = Property->HasMetaData(FBlueprintMetadata::MD_ExposeOnSpawn);
		const bool bFlag = Property->HasAllPropertyFlags(CPF_ExposeOnSpawn);
		if (bMeta != bFlag)
		{
			UE_LOG(LogBlueprint, Warning
				, TEXT("ExposeOnSpawn ambiguity. Property '%s', MetaData '%s', Flag '%s'")
				, *Property->GetFullName()
				, bMeta ? *GTrue.ToString() : *GFalse.ToString()
				, bFlag ? *GTrue.ToString() : *GFalse.ToString());
		}
		return bMeta || bFlag;
	}
	return false;
}

// if node is a get/set variable and the variable it refers to does not exist
static bool IsUsingNonExistantVariable(const UEdGraphNode* InGraphNode, UBlueprint* OwnerBlueprint)
{
	bool bNonExistantVariable = false;
	const bool bBreakOrMakeStruct = 
		InGraphNode->IsA(UK2Node_BreakStruct::StaticClass()) || 
		InGraphNode->IsA(UK2Node_MakeStruct::StaticClass());
	if (!bBreakOrMakeStruct)
	{
		if (const UK2Node_Variable* Variable = Cast<const UK2Node_Variable>(InGraphNode))
		{
			if (Variable->VariableReference.IsSelfContext())
			{
				TArray<FName> CurrentVars;
				FBlueprintEditorUtils::GetClassVariableList(OwnerBlueprint, CurrentVars);
				if ( false == CurrentVars.Contains(Variable->GetVarName()) )
				{
					bNonExistantVariable = true;
				}
			}
			else if(Variable->VariableReference.IsLocalScope())
			{
				// If there is no member scope, or we can't find the local variable in the member scope, then it's non-existant
				UStruct* MemberScope = Variable->VariableReference.GetMemberScope(Variable->GetBlueprintClassFromNode());
				if (MemberScope == nullptr || !FBlueprintEditorUtils::FindLocalVariable(OwnerBlueprint, MemberScope, Variable->GetVarName()))
				{
					bNonExistantVariable = true;
				}
			}
		}
	}
	return bNonExistantVariable;
}

bool UEdGraphSchema_K2::PinHasSplittableStructType(const UEdGraphPin* InGraphPin) const
{
	const FEdGraphPinType& PinType = InGraphPin->PinType;
	bool bCanSplit = (!PinType.bIsArray && PinType.PinCategory == PC_Struct);

	if (bCanSplit)
	{
		UScriptStruct* StructType = CastChecked<UScriptStruct>(InGraphPin->PinType.PinSubCategoryObject.Get());
		if (InGraphPin->Direction == EGPD_Input)
		{
			bCanSplit = UK2Node_MakeStruct::CanBeMade(StructType);
			if (!bCanSplit)
			{
				const FString& MetaData = StructType->GetMetaData(TEXT("HasNativeMake"));
				UFunction* Function = FindObject<UFunction>(NULL, *MetaData, true);
				bCanSplit = (Function != NULL);
			}
		}
		else
		{
			bCanSplit = UK2Node_BreakStruct::CanBeBroken(StructType);
			if (!bCanSplit)
			{
				const FString& MetaData = StructType->GetMetaData(TEXT("HasNativeBreak"));
				UFunction* Function = FindObject<UFunction>(NULL, *MetaData, true);
				bCanSplit = (Function != NULL);
			}
		}
	}

	return bCanSplit;
}

bool UEdGraphSchema_K2::PinDefaultValueIsEditable(const UEdGraphPin& InGraphPin) const
{
	// Array types are not currently assignable without a 'make array' node:
	if( InGraphPin.PinType.bIsArray )
	{
		return false;
	}

	// User defined structures (from code or from data) cannot accept default values:
	if( InGraphPin.PinType.PinCategory == PC_Struct )
	{
		// Only the built in struct types are editable as 'default' values on a pin.
		// See FNodeFactory::CreatePinWidget for justification of the above statement!
		UObject const& SubCategoryObject = *InGraphPin.PinType.PinSubCategoryObject;
		return &SubCategoryObject == VectorStruct 
			|| &SubCategoryObject == RotatorStruct
			|| &SubCategoryObject == TransformStruct
			|| &SubCategoryObject == LinearColorStruct
			|| &SubCategoryObject == ColorStruct
			|| &SubCategoryObject == FCollisionProfileName::StaticStruct();
	}

	return true;
}

void UEdGraphSchema_K2::GetContextMenuActions(const UEdGraph* CurrentGraph, const UEdGraphNode* InGraphNode, const UEdGraphPin* InGraphPin, FMenuBuilder* MenuBuilder, bool bIsDebugging) const
{
	check(CurrentGraph);
	UBlueprint* OwnerBlueprint = FBlueprintEditorUtils::FindBlueprintForGraphChecked(CurrentGraph);
	
	if (InGraphPin != NULL)
	{
		MenuBuilder->BeginSection("EdGraphSchemaPinActions", LOCTEXT("PinActionsMenuHeader", "Pin Actions"));
		{
			if (!bIsDebugging)
			{
				// Break pin links
				if (InGraphPin->LinkedTo.Num() > 1)
				{
					MenuBuilder->AddMenuEntry( FGraphEditorCommands::Get().BreakPinLinks );
				}
	
				// Add the change pin type action, if this is a select node
				if (InGraphNode->IsA(UK2Node_Select::StaticClass()))
				{
					MenuBuilder->AddMenuEntry(FGraphEditorCommands::Get().ChangePinType);
				}
	
				// add sub menu for break link to
				if (InGraphPin->LinkedTo.Num() > 0)
				{
					if(InGraphPin->LinkedTo.Num() > 1)
					{
						MenuBuilder->AddSubMenu(
							LOCTEXT("BreakLinkTo", "Break Link To..."),
							LOCTEXT("BreakSpecificLinks", "Break a specific link..."),
							FNewMenuDelegate::CreateUObject( (UEdGraphSchema_K2*const)this, &UEdGraphSchema_K2::GetBreakLinkToSubMenuActions, const_cast<UEdGraphPin*>(InGraphPin)));

						MenuBuilder->AddSubMenu(
							LOCTEXT("JumpToConnection", "Jump to Connection..."),
							LOCTEXT("JumpToSpecificConnection", "Jump to specific connection..."),
							FNewMenuDelegate::CreateUObject( (UEdGraphSchema_K2*const)this, &UEdGraphSchema_K2::GetJumpToConnectionSubMenuActions, const_cast<UEdGraphPin*>(InGraphPin)));
					}
					else
					{
						((UEdGraphSchema_K2*const)this)->GetBreakLinkToSubMenuActions(*MenuBuilder, const_cast<UEdGraphPin*>(InGraphPin));
						((UEdGraphSchema_K2*const)this)->GetJumpToConnectionSubMenuActions(*MenuBuilder, const_cast<UEdGraphPin*>(InGraphPin));
					}
				}
	
				// Conditionally add the var promotion pin if this is an output pin and it's not an exec pin
				if (InGraphPin->PinType.PinCategory != PC_Exec)
				{
					MenuBuilder->AddMenuEntry( FGraphEditorCommands::Get().PromoteToVariable );
				}
	
				if (PinHasSplittableStructType(InGraphPin) && InGraphNode->AllowSplitPins())
				{
					MenuBuilder->AddMenuEntry( FGraphEditorCommands::Get().SplitStructPin );
				}

				if (InGraphPin->ParentPin != NULL)
				{
					MenuBuilder->AddMenuEntry( FGraphEditorCommands::Get().RecombineStructPin );
				}
	
				// Conditionally add the execution path pin removal if this is an execution branching node
				if( InGraphPin->Direction == EGPD_Output && InGraphPin->GetOwningNode())
				{
					if (CastChecked<UK2Node>(InGraphPin->GetOwningNode())->CanEverRemoveExecutionPin())
					{
						MenuBuilder->AddMenuEntry( FGraphEditorCommands::Get().RemoveExecutionPin );
					}
				}

				if (UK2Node_SetFieldsInStruct::ShowCustomPinActions(InGraphPin, true))
				{
					MenuBuilder->AddMenuEntry(FGraphEditorCommands::Get().RemoveThisStructVarPin);
					MenuBuilder->AddMenuEntry(FGraphEditorCommands::Get().RemoveOtherStructVarPins);
				}
			}
		}
		MenuBuilder->EndSection();

		// Add the watch pin / unwatch pin menu items
		MenuBuilder->BeginSection("EdGraphSchemaWatches", LOCTEXT("WatchesHeader", "Watches"));
		{
			if (!IsMetaPin(*InGraphPin))
			{
				const UEdGraphPin* WatchedPin = ((InGraphPin->Direction == EGPD_Input) && (InGraphPin->LinkedTo.Num() > 0)) ? InGraphPin->LinkedTo[0] : InGraphPin;
				if (FKismetDebugUtilities::IsPinBeingWatched(OwnerBlueprint, WatchedPin))
				{
					MenuBuilder->AddMenuEntry( FGraphEditorCommands::Get().StopWatchingPin );
				}
				else
				{
					MenuBuilder->AddMenuEntry( FGraphEditorCommands::Get().StartWatchingPin );
				}
			}
		}
		MenuBuilder->EndSection();
	}
	else if (InGraphNode != NULL)
	{
		if (IsUsingNonExistantVariable(InGraphNode, OwnerBlueprint))
		{
			MenuBuilder->BeginSection("EdGraphSchemaNodeActions", LOCTEXT("NodeActionsMenuHeader", "Node Actions"));
			{
				GetNonExistentVariableMenu(InGraphNode,OwnerBlueprint, MenuBuilder);
			}
			MenuBuilder->EndSection();
		}
		else
		{
			MenuBuilder->BeginSection("EdGraphSchemaNodeActions", LOCTEXT("NodeActionsMenuHeader", "Node Actions"));
			{
				if (!bIsDebugging)
				{
					// Replaceable node display option
					AddSelectedReplaceableNodes( OwnerBlueprint, InGraphNode, MenuBuilder );

					// Node contextual actions
					MenuBuilder->AddMenuEntry( FGenericCommands::Get().Delete );
					MenuBuilder->AddMenuEntry( FGenericCommands::Get().Cut );
					MenuBuilder->AddMenuEntry( FGenericCommands::Get().Copy );
					MenuBuilder->AddMenuEntry( FGenericCommands::Get().Duplicate );
					MenuBuilder->AddMenuEntry( FGraphEditorCommands::Get().ReconstructNodes );
					MenuBuilder->AddMenuEntry( FGraphEditorCommands::Get().BreakNodeLinks );

					// Conditionally add the action to add an execution pin, if this is an execution node
					if( InGraphNode->IsA(UK2Node_ExecutionSequence::StaticClass()) || InGraphNode->IsA(UK2Node_Switch::StaticClass()) )
					{
						MenuBuilder->AddMenuEntry( FGraphEditorCommands::Get().AddExecutionPin );
					}

					// Conditionally add the action to create a super function call node, if this is an event or function entry
					if( InGraphNode->IsA(UK2Node_Event::StaticClass()) || InGraphNode->IsA(UK2Node_FunctionEntry::StaticClass()) )
					{
						MenuBuilder->AddMenuEntry( FGraphEditorCommands::Get().AddParentNode );
					}

					// Conditionally add the actions to add or remove an option pin, if this is a select node
					if (InGraphNode->IsA(UK2Node_Select::StaticClass()))
					{
						MenuBuilder->AddMenuEntry(FGraphEditorCommands::Get().AddOptionPin);
						MenuBuilder->AddMenuEntry(FGraphEditorCommands::Get().RemoveOptionPin);
					}

					// Conditionally add the action to find instances of the node if it is a custom event
					if (InGraphNode->IsA(UK2Node_CustomEvent::StaticClass()))
					{
						MenuBuilder->AddMenuEntry(FGraphEditorCommands::Get().FindInstancesOfCustomEvent);
					}

					// Don't show the "Assign selected Actor" option if more than one actor is selected
					if (InGraphNode->IsA(UK2Node_ActorBoundEvent::StaticClass()) && GEditor->GetSelectedActorCount() == 1)
					{
						MenuBuilder->AddMenuEntry(FGraphEditorCommands::Get().AssignReferencedActor);
					}

					// Add the goto source code action for native functions
					if (InGraphNode->IsA(UK2Node_CallFunction::StaticClass()))
					{
						const UEdGraphNode* ResultEventNode= NULL;
						if(Cast<UK2Node_CallFunction>(InGraphNode)->GetFunctionGraph(ResultEventNode))
						{
							MenuBuilder->AddMenuEntry(FGraphEditorCommands::Get().GoToDefinition);
						}
						else
						{
							MenuBuilder->AddMenuEntry(FGraphEditorCommands::Get().GotoNativeFunctionDefinition);
						}
					}

					// Functions, macros, and composite nodes support going to a definition
					if (InGraphNode->IsA(UK2Node_MacroInstance::StaticClass()) || InGraphNode->IsA(UK2Node_Composite::StaticClass()))
					{
						MenuBuilder->AddMenuEntry(FGraphEditorCommands::Get().GoToDefinition);
					}

					// show search for references for variable nodes and goto source code action
					if (InGraphNode->IsA(UK2Node_Variable::StaticClass()))
					{
						MenuBuilder->AddMenuEntry(FGraphEditorCommands::Get().FindVariableReferences);
						MenuBuilder->AddMenuEntry(FGraphEditorCommands::Get().GotoNativeVariableDefinition);
						GetReplaceVariableMenu(InGraphNode,OwnerBlueprint, MenuBuilder, true);
					}

					if (InGraphNode->IsA(UK2Node_SetFieldsInStruct::StaticClass()))
					{
						MenuBuilder->AddMenuEntry(FGraphEditorCommands::Get().RestoreAllStructVarPins);
					}

					MenuBuilder->AddMenuEntry(FGenericCommands::Get().Rename, NAME_None, LOCTEXT("Rename", "Rename"), LOCTEXT("Rename_Tooltip", "Renames selected function or variable in blueprint.") );
				}

				// Select referenced actors in the level
				MenuBuilder->AddMenuEntry( FGraphEditorCommands::Get().SelectReferenceInLevel );
			}
			MenuBuilder->EndSection(); //EdGraphSchemaNodeActions

			if (!bIsDebugging)
			{
				// Collapse/expand nodes
				MenuBuilder->BeginSection("EdGraphSchemaOrganization", LOCTEXT("OrganizationHeader", "Organization"));
				{
					MenuBuilder->AddMenuEntry( FGraphEditorCommands::Get().CollapseNodes );
					MenuBuilder->AddMenuEntry( FGraphEditorCommands::Get().CollapseSelectionToFunction );
					MenuBuilder->AddMenuEntry( FGraphEditorCommands::Get().CollapseSelectionToMacro );
					MenuBuilder->AddMenuEntry( FGraphEditorCommands::Get().ExpandNodes );

					if(InGraphNode->IsA(UK2Node_Composite::StaticClass()))
					{
						MenuBuilder->AddMenuEntry( FGraphEditorCommands::Get().PromoteSelectionToFunction );
						MenuBuilder->AddMenuEntry( FGraphEditorCommands::Get().PromoteSelectionToMacro );
					}
				}
				MenuBuilder->EndSection();
			}

			// Add breakpoint actions
			if (const UK2Node* K2Node = Cast<const UK2Node>(InGraphNode))
			{
				if (!K2Node->IsNodePure())
				{
					MenuBuilder->BeginSection("EdGraphSchemaBreakpoints", LOCTEXT("BreakpointsHeader", "Breakpoints"));
					{
						MenuBuilder->AddMenuEntry( FGraphEditorCommands::Get().ToggleBreakpoint );
						MenuBuilder->AddMenuEntry( FGraphEditorCommands::Get().AddBreakpoint );
						MenuBuilder->AddMenuEntry( FGraphEditorCommands::Get().RemoveBreakpoint );
						MenuBuilder->AddMenuEntry( FGraphEditorCommands::Get().EnableBreakpoint );
						MenuBuilder->AddMenuEntry( FGraphEditorCommands::Get().DisableBreakpoint );
					}
					MenuBuilder->EndSection();
				}
			}

			MenuBuilder->BeginSection("EdGraphSchemaDocumentation", LOCTEXT("DocumentationHeader", "Documentation"));
			{
				MenuBuilder->AddMenuEntry( FGraphEditorCommands::Get().GoToDocumentation );
			}
			MenuBuilder->EndSection();
		}
	}

	Super::GetContextMenuActions(CurrentGraph, InGraphNode, InGraphPin, MenuBuilder, bIsDebugging);
}


void UEdGraphSchema_K2::OnCreateNonExistentVariable( UK2Node_Variable* Variable,  UBlueprint* OwnerBlueprint)
{
	if (UEdGraphPin* Pin = Variable->FindPin(Variable->GetVarNameString()))
	{
		const FScopedTransaction Transaction( LOCTEXT("CreateMissingVariable", "Create Missing Variable") );

		if (FBlueprintEditorUtils::AddMemberVariable(OwnerBlueprint,Variable->GetVarName(), Pin->PinType))
		{
			Variable->VariableReference.SetSelfMember( Variable->GetVarName() );
		}
	}	
}

void UEdGraphSchema_K2::OnCreateNonExistentLocalVariable( UK2Node_Variable* Variable,  UBlueprint* OwnerBlueprint)
{
	if (UEdGraphPin* Pin = Variable->FindPin(Variable->GetVarNameString()))
	{
		const FScopedTransaction Transaction( LOCTEXT("CreateMissingLocalVariable", "Create Missing Local Variable") );

		FName VarName = Variable->GetVarName();
		if (FBlueprintEditorUtils::AddLocalVariable(OwnerBlueprint, Variable->GetGraph(), VarName, Pin->PinType))
		{
			FGuid LocalVarGuid = FBlueprintEditorUtils::FindLocalVariableGuidByName(OwnerBlueprint, Variable->GetGraph(), VarName);
			if (LocalVarGuid.IsValid())
			{
				Variable->VariableReference.SetLocalMember(VarName, Variable->GetGraph()->GetName(), LocalVarGuid);
			}
		}
	}	
}

void UEdGraphSchema_K2::OnReplaceVariableForVariableNode( UK2Node_Variable* Variable, UBlueprint* OwnerBlueprint, FString VariableName, bool bIsSelfMember)
{
	if(UEdGraphPin* Pin = Variable->FindPin(Variable->GetVarNameString()))
	{
		const FScopedTransaction Transaction( NSLOCTEXT("UnrealEd", "GraphEd_ReplaceVariable", "Replace Variable") );
		Variable->Modify();
		Pin->Modify();

		if (bIsSelfMember)
		{
			Variable->VariableReference.SetSelfMember( FName(*VariableName) );
		}
		else
		{
			UEdGraph* FunctionGraph = FBlueprintEditorUtils::GetTopLevelGraph(Variable->GetGraph());
			Variable->VariableReference.SetLocalMember( FName(*VariableName), FunctionGraph->GetName(), FBlueprintEditorUtils::FindLocalVariableGuidByName(OwnerBlueprint, FunctionGraph, *VariableName));
		}
		Pin->PinName = VariableName;
		Variable->ReconstructNode();
	}
}

void UEdGraphSchema_K2::GetReplaceVariableMenu(FMenuBuilder& MenuBuilder, UK2Node_Variable* Variable,  UBlueprint* OwnerBlueprint, bool bReplaceExistingVariable/*=false*/)
{
	if (UEdGraphPin* Pin = Variable->FindPin(Variable->GetVarNameString()))
	{
		FName ExistingVariableName = bReplaceExistingVariable? Variable->GetVarName() : NAME_None;

		FText ReplaceVariableWithTooltipFormat;
		if(!bReplaceExistingVariable)
		{
			ReplaceVariableWithTooltipFormat = LOCTEXT("ReplaceNonExistantVarToolTip", "Variable '{OldVariable}' does not exist, replace with matching variable '{AlternateVariable}'?");
		}
		else
		{
			ReplaceVariableWithTooltipFormat = LOCTEXT("ReplaceExistantVarToolTip", "Replace Variable '{OldVariable}' with matching variable '{AlternateVariable}'?");
		}

		TArray<FName> Variables;
		FBlueprintEditorUtils::GetNewVariablesOfType(OwnerBlueprint, Pin->PinType, Variables);

		MenuBuilder.BeginSection(NAME_None, LOCTEXT("Variables", "Variables"));
		for (TArray<FName>::TIterator VarIt(Variables); VarIt; ++VarIt)
		{
			if(*VarIt != ExistingVariableName)
			{
				const FText AlternativeVar = FText::FromName( *VarIt );

				FFormatNamedArguments TooltipArgs;
				TooltipArgs.Add(TEXT("OldVariable"), Variable->GetVarNameText());
				TooltipArgs.Add(TEXT("AlternateVariable"), AlternativeVar);
				const FText Desc = FText::Format(ReplaceVariableWithTooltipFormat, TooltipArgs);

				MenuBuilder.AddMenuEntry( AlternativeVar, Desc, FSlateIcon(), FUIAction(
					FExecuteAction::CreateStatic(&UEdGraphSchema_K2::OnReplaceVariableForVariableNode, const_cast<UK2Node_Variable* >(Variable),OwnerBlueprint, (*VarIt).ToString(), /*bIsSelfMember=*/true ) ) );
			}
		}
		MenuBuilder.EndSection();

		FText ReplaceLocalVariableWithTooltipFormat;
		if(!bReplaceExistingVariable)
		{
			ReplaceLocalVariableWithTooltipFormat = LOCTEXT("ReplaceNonExistantLocalVarToolTip", "Variable '{OldVariable}' does not exist, replace with matching local variable '{AlternateVariable}'?");
		}
		else
		{
			ReplaceLocalVariableWithTooltipFormat = LOCTEXT("ReplaceExistantLocalVarToolTip", "Replace Variable '{OldVariable}' with matching local variable '{AlternateVariable}'?");
		}

		TArray<FName> LocalVariables;
		FBlueprintEditorUtils::GetLocalVariablesOfType(Variable->GetGraph(), Pin->PinType, LocalVariables);

		MenuBuilder.BeginSection(NAME_None, LOCTEXT("LocalVariables", "LocalVariables"));
		for (TArray<FName>::TIterator VarIt(LocalVariables); VarIt; ++VarIt)
		{
			if(*VarIt != ExistingVariableName)
			{
				const FText AlternativeVar = FText::FromName( *VarIt );

				FFormatNamedArguments TooltipArgs;
				TooltipArgs.Add(TEXT("OldVariable"), Variable->GetVarNameText());
				TooltipArgs.Add(TEXT("AlternateVariable"), AlternativeVar);
				const FText Desc = FText::Format( ReplaceLocalVariableWithTooltipFormat, TooltipArgs );

				MenuBuilder.AddMenuEntry( AlternativeVar, Desc, FSlateIcon(), FUIAction(
					FExecuteAction::CreateStatic(&UEdGraphSchema_K2::OnReplaceVariableForVariableNode, const_cast<UK2Node_Variable* >(Variable),OwnerBlueprint, (*VarIt).ToString(), /*bIsSelfMember=*/false ) ) );
			}
		}
		MenuBuilder.EndSection();
	}
}

void UEdGraphSchema_K2::GetNonExistentVariableMenu( const UEdGraphNode* InGraphNode, UBlueprint* OwnerBlueprint, FMenuBuilder* MenuBuilder ) const
{

	if (const UK2Node_Variable* Variable = Cast<const UK2Node_Variable>(InGraphNode))
	{
		// Creating missing variables should never occur in a Macro Library or Interface, they do not support variables
		if(OwnerBlueprint->BlueprintType != BPTYPE_MacroLibrary && OwnerBlueprint->BlueprintType != BPTYPE_Interface )
		{		
			// Creating missing member variables should never occur in a Function Library, they do not support variables
			if(OwnerBlueprint->BlueprintType != BPTYPE_FunctionLibrary)
			{
				// create missing variable
				const FText Label = FText::Format( LOCTEXT("CreateNonExistentVar", "Create variable '{0}'"), Variable->GetVarNameText());
				const FText Desc = FText::Format( LOCTEXT("CreateNonExistentVarToolTip", "Variable '{0}' does not exist, create it?"), Variable->GetVarNameText());
				MenuBuilder->AddMenuEntry( Label, Desc, FSlateIcon(), FUIAction(
					FExecuteAction::CreateStatic( &UEdGraphSchema_K2::OnCreateNonExistentVariable, const_cast<UK2Node_Variable* >(Variable),OwnerBlueprint) ) );
			}

			// Only allow creating missing local variables if in a function graph
			if(InGraphNode->GetGraph()->GetSchema()->GetGraphType(InGraphNode->GetGraph()) == GT_Function)
			{
				const FText Label = FText::Format( LOCTEXT("CreateNonExistentLocalVar", "Create local variable '{0}'"), Variable->GetVarNameText());
				const FText Desc = FText::Format( LOCTEXT("CreateNonExistentLocalVarToolTip", "Local variable '{0}' does not exist, create it?"), Variable->GetVarNameText());
				MenuBuilder->AddMenuEntry( Label, Desc, FSlateIcon(), FUIAction(
					FExecuteAction::CreateStatic( &UEdGraphSchema_K2::OnCreateNonExistentLocalVariable, const_cast<UK2Node_Variable* >(Variable),OwnerBlueprint) ) );
			}
		}

		// delete this node
		{			
			const FText Desc = FText::Format( LOCTEXT("DeleteNonExistentVarToolTip", "Referenced variable '{0}' does not exist, delete this node?"), Variable->GetVarNameText());
			MenuBuilder->AddMenuEntry( FGenericCommands::Get().Delete, NAME_None, FGenericCommands::Get().Delete->GetLabel(), Desc);
		}

		GetReplaceVariableMenu(InGraphNode, OwnerBlueprint, MenuBuilder);
	}
}

void UEdGraphSchema_K2::GetReplaceVariableMenu(const UEdGraphNode* InGraphNode, UBlueprint* InOwnerBlueprint, FMenuBuilder* InMenuBuilder, bool bInReplaceExistingVariable/* = false*/) const
{
	if (const UK2Node_Variable* Variable = Cast<const UK2Node_Variable>(InGraphNode))
	{
		// replace with matching variables
		if (UEdGraphPin* Pin = Variable->FindPin(Variable->GetVarNameString()))
		{
			FName ExistingVariableName = bInReplaceExistingVariable? Variable->GetVarName() : NAME_None;

			TArray<FName> Variables;
			FBlueprintEditorUtils::GetNewVariablesOfType(InOwnerBlueprint, Pin->PinType, Variables);
			Variables.RemoveSwap(ExistingVariableName);

			TArray<FName> LocalVariables;
			FBlueprintEditorUtils::GetLocalVariablesOfType(Variable->GetGraph(), Pin->PinType, LocalVariables);
			LocalVariables.RemoveSwap(ExistingVariableName);

			if (Variables.Num() > 0 || LocalVariables.Num() > 0)
			{
				FText ReplaceVariableWithTooltip;
				if(bInReplaceExistingVariable)
				{
					ReplaceVariableWithTooltip = LOCTEXT("ReplaceVariableWithToolTip", "Replace Variable '{0}' with another variable?");
				}
				else
				{
					ReplaceVariableWithTooltip = LOCTEXT("ReplaceMissingVariableWithToolTip", "Variable '{0}' does not exist, replace with another variable?");
				}

				InMenuBuilder->AddSubMenu(
					FText::Format( LOCTEXT("ReplaceVariableWith", "Replace variable '{0}' with..."), Variable->GetVarNameText()),
					FText::Format( ReplaceVariableWithTooltip, Variable->GetVarNameText()),
					FNewMenuDelegate::CreateStatic( &UEdGraphSchema_K2::GetReplaceVariableMenu,
					const_cast<UK2Node_Variable*>(Variable),InOwnerBlueprint, bInReplaceExistingVariable));
			}
		}
	}
}

void UEdGraphSchema_K2::GetBreakLinkToSubMenuActions( class FMenuBuilder& MenuBuilder, UEdGraphPin* InGraphPin )
{
	// Make sure we have a unique name for every entry in the list
	TMap< FString, uint32 > LinkTitleCount;

	// Add all the links we could break from
	for(TArray<class UEdGraphPin*>::TConstIterator Links(InGraphPin->LinkedTo); Links; ++Links)
	{
		UEdGraphPin* Pin = *Links;
		FText Title = Pin->GetOwningNode()->GetNodeTitle(ENodeTitleType::ListView);
		FString TitleString = Title.ToString();
		if ( Pin->PinName != TEXT("") )
		{
			TitleString = FString::Printf(TEXT("%s (%s)"), *TitleString, *Pin->GetDisplayName().ToString());

			// Add name of connection if possible
			FFormatNamedArguments Args;
			Args.Add( TEXT("NodeTitle"), Title );
			Args.Add( TEXT("PinName"), Pin->GetDisplayName() );
			Title = FText::Format( LOCTEXT("BreakDescPin", "{NodeTitle} ({PinName})"), Args );
		}

		uint32 &Count = LinkTitleCount.FindOrAdd( TitleString );

		FText Description;
		FFormatNamedArguments Args;
		Args.Add( TEXT("NodeTitle"), Title );
		Args.Add( TEXT("NumberOfNodes"), Count );

		if ( Count == 0 )
		{
			Description = FText::Format( LOCTEXT("BreakDesc", "Break link to {NodeTitle}"), Args );
		}
		else
		{
			Description = FText::Format( LOCTEXT("BreakDescMulti", "Break link to {NodeTitle} ({NumberOfNodes})"), Args );
		}
		++Count;

		MenuBuilder.AddMenuEntry( Description, Description, FSlateIcon(), FUIAction(
			FExecuteAction::CreateUObject(this, &UEdGraphSchema_K2::BreakSinglePinLink, const_cast< UEdGraphPin* >(InGraphPin), *Links)));
	}
}

void UEdGraphSchema_K2::GetJumpToConnectionSubMenuActions( class FMenuBuilder& MenuBuilder, UEdGraphPin* InGraphPin )
{
	// Make sure we have a unique name for every entry in the list
	TMap< FString, uint32 > LinkTitleCount;

	// Add all the links we could break from
	for(auto PinLink : InGraphPin->LinkedTo )
	{
		FText Title = PinLink->GetOwningNode()->GetNodeTitle(ENodeTitleType::ListView);
		FString TitleString = Title.ToString();
		if ( PinLink->PinName != TEXT("") )
		{
			TitleString = FString::Printf(TEXT("%s (%s)"), *TitleString, *PinLink->GetDisplayName().ToString());

			// Add name of connection if possible
			FFormatNamedArguments Args;
			Args.Add( TEXT("NodeTitle"), Title );
			Args.Add( TEXT("PinName"), PinLink->GetDisplayName() );
			Title = FText::Format( LOCTEXT("JumpToDescPin", "{NodeTitle} ({PinName})"), Args );
		}

		uint32 &Count = LinkTitleCount.FindOrAdd( TitleString );

		FText Description;
		FFormatNamedArguments Args;
		Args.Add( TEXT("NodeTitle"), Title );
		Args.Add( TEXT("NumberOfNodes"), Count );

		if ( Count == 0 )
		{
			Description = FText::Format( LOCTEXT("JumpDesc", "Jump to {NodeTitle}"), Args );
		}
		else
		{
			Description = FText::Format( LOCTEXT("JumpDescMulti", "Jump to {NodeTitle} ({NumberOfNodes})"), Args );
		}
		++Count;

		MenuBuilder.AddMenuEntry( Description, Description, FSlateIcon(), FUIAction(
		FExecuteAction::CreateStatic(&FKismetEditorUtilities::BringKismetToFocusAttentionOnObject, Cast<const UObject>(PinLink), false)));
	}
}

const FPinConnectionResponse UEdGraphSchema_K2::DetermineConnectionResponseOfCompatibleTypedPins(const UEdGraphPin* PinA, const UEdGraphPin* PinB, const UEdGraphPin* InputPin, const UEdGraphPin* OutputPin) const
{
	// Now check to see if there are already connections and this is an 'exclusive' connection
	const bool bBreakExistingDueToExecOutput = IsExecPin(*OutputPin) && (OutputPin->LinkedTo.Num() > 0);
	const bool bBreakExistingDueToDataInput = !IsExecPin(*InputPin) && (InputPin->LinkedTo.Num() > 0);

	bool bMultipleSelfException = false;
	const UK2Node* OwningNode = Cast<UK2Node>(InputPin->GetOwningNode());
	if (bBreakExistingDueToDataInput && 
		IsSelfPin(*InputPin) && 
		OwningNode &&
		OwningNode->AllowMultipleSelfs(false) &&
		!InputPin->PinType.bIsArray &&
		!OutputPin->PinType.bIsArray)
	{
		//check if the node wont be expanded as foreach call, if there is a link to an array
		bool bAnyArrayInput = false;
		for(int InputLinkIndex = 0; InputLinkIndex < InputPin->LinkedTo.Num(); InputLinkIndex++)
		{
			if(const UEdGraphPin* Pin = InputPin->LinkedTo[InputLinkIndex])
			{
				if(Pin->PinType.bIsArray)
				{
					bAnyArrayInput = true;
					break;
				}
			}
		}
		bMultipleSelfException = !bAnyArrayInput;
	}

	if (bBreakExistingDueToExecOutput)
	{
		const ECanCreateConnectionResponse ReplyBreakOutputs = (PinA == OutputPin) ? CONNECT_RESPONSE_BREAK_OTHERS_A : CONNECT_RESPONSE_BREAK_OTHERS_B;
		return FPinConnectionResponse(ReplyBreakOutputs, TEXT("Replace existing output connections"));
	}
	else if (bBreakExistingDueToDataInput && !bMultipleSelfException)
	{
		const ECanCreateConnectionResponse ReplyBreakInputs = (PinA == InputPin) ? CONNECT_RESPONSE_BREAK_OTHERS_A : CONNECT_RESPONSE_BREAK_OTHERS_B;
		return FPinConnectionResponse(ReplyBreakInputs, TEXT("Replace existing input connections"));
	}
	else
	{
		return FPinConnectionResponse(CONNECT_RESPONSE_MAKE, TEXT(""));
	}
}

static FText GetPinIncompatibilityMessage(const UEdGraphPin* PinA, const UEdGraphPin* PinB)
{
	const FEdGraphPinType& PinAType = PinA->PinType;
	const FEdGraphPinType& PinBType = PinB->PinType;

	FFormatNamedArguments MessageArgs;
	MessageArgs.Add(TEXT("PinAName"), PinA->GetDisplayName());
	MessageArgs.Add(TEXT("PinBName"), PinB->GetDisplayName());
	MessageArgs.Add(TEXT("PinAType"), UEdGraphSchema_K2::TypeToText(PinAType));
	MessageArgs.Add(TEXT("PinBType"), UEdGraphSchema_K2::TypeToText(PinBType));

	const UEdGraphPin* InputPin = (PinA->Direction == EGPD_Input) ? PinA : PinB;
	const FEdGraphPinType& InputType  = InputPin->PinType;
	const UEdGraphPin* OutputPin = (InputPin == PinA) ? PinB : PinA;
	const FEdGraphPinType& OutputType = OutputPin->PinType;

	FText MessageFormat = LOCTEXT("DefaultPinIncompatibilityMessage", "{PinAType} is not compatible with {PinBType}.");

	if (OutputType.PinCategory == UEdGraphSchema_K2::PC_Struct)
	{
		if (InputType.PinCategory == UEdGraphSchema_K2::PC_Struct)
		{
			MessageFormat = LOCTEXT("StructsIncompatible", "Only exactly matching structures are considered compatible.");

			const UStruct* OutStruct = Cast<const UStruct>(OutputType.PinSubCategoryObject.Get());
			const UStruct* InStruct  = Cast<const UStruct>(InputType.PinSubCategoryObject.Get());
			if ((OutStruct != nullptr) && (InStruct != nullptr) && OutStruct->IsChildOf(InStruct))
			{
				MessageFormat = LOCTEXT("ChildStructIncompatible", "Only exactly matching structures are considered compatible. Derived structures are disallowed.");
			}
		}
	}
	else if (OutputType.PinCategory == UEdGraphSchema_K2::PC_Class)
	{
		if ((InputType.PinCategory == UEdGraphSchema_K2::PC_Object) || 
			(InputType.PinCategory == UEdGraphSchema_K2::PC_Interface))
		{
			MessageArgs.Add(TEXT("OutputName"), OutputPin->GetDisplayName());
			MessageArgs.Add(TEXT("InputName"),  InputPin->GetDisplayName());
			MessageFormat = LOCTEXT("ClassObjectIncompatible", "'{PinAName}' and '{PinBName}' are incompatible ('{OutputName}' is an object type, and '{InputName}' is a reference to an object instance).");
		}
	}
	else if ((OutputType.PinCategory == UEdGraphSchema_K2::PC_Object) )//|| (OutputType.PinCategory == UEdGraphSchema_K2::PC_Interface))
	{
		if (InputType.PinCategory == UEdGraphSchema_K2::PC_Class)
		{
			MessageArgs.Add(TEXT("OutputName"), OutputPin->GetDisplayName());
			MessageArgs.Add(TEXT("InputName"),  InputPin->GetDisplayName());
			MessageArgs.Add(TEXT("InputType"),  UEdGraphSchema_K2::TypeToText(InputType));

			MessageFormat = LOCTEXT("CannotGetClass", "'{PinAName}' and '{PinBName}' are not inherently compatible ('{InputName}' is an object type, and '{OutputName}' is a reference to an object instance).\nWe cannot use {OutputName}'s class because it is not a child of {InputType}.");
		}
	}

	return FText::Format(MessageFormat, MessageArgs);
}

const FPinConnectionResponse UEdGraphSchema_K2::CanCreateConnection(const UEdGraphPin* PinA, const UEdGraphPin* PinB) const
{
	const UK2Node* OwningNodeA = Cast<UK2Node>(PinA->GetOwningNodeUnchecked());
	const UK2Node* OwningNodeB = Cast<UK2Node>(PinB->GetOwningNodeUnchecked());

	if (!OwningNodeA || !OwningNodeB)
	{
		return FPinConnectionResponse(CONNECT_RESPONSE_DISALLOW, TEXT("Invalid nodes"));
	}

	// Make sure the pins are not on the same node
	if (OwningNodeA == OwningNodeB)
	{
		return FPinConnectionResponse(CONNECT_RESPONSE_DISALLOW, TEXT("Both are on the same node"));
	}

	// node can disallow the connection
	{
		FString RespondMessage;
		if(OwningNodeA && OwningNodeA->IsConnectionDisallowed(PinA, PinB, RespondMessage))
		{
			return FPinConnectionResponse(CONNECT_RESPONSE_DISALLOW, RespondMessage);
		}
		if(OwningNodeB && OwningNodeB->IsConnectionDisallowed(PinB, PinA, RespondMessage))
		{
			return FPinConnectionResponse(CONNECT_RESPONSE_DISALLOW, RespondMessage);
		}
	}

	// Compare the directions
	const UEdGraphPin* InputPin = NULL;
	const UEdGraphPin* OutputPin = NULL;

	if (!CategorizePinsByDirection(PinA, PinB, /*out*/ InputPin, /*out*/ OutputPin))
	{
		return FPinConnectionResponse(CONNECT_RESPONSE_DISALLOW, TEXT("Directions are not compatible"));
	}

	bool bIgnoreArray = false;
	if(const UK2Node* OwningNode = Cast<UK2Node>(InputPin->GetOwningNode()))
	{
		const bool bAllowMultipleSelfs = OwningNode->AllowMultipleSelfs(true); // it applies also to ForEachCall
		const bool bNotAnArrayFunction = !InputPin->PinType.bIsArray;
		const bool bSelfPin = IsSelfPin(*InputPin);
		bIgnoreArray = bAllowMultipleSelfs && bNotAnArrayFunction && bSelfPin;
	}

	// Find the calling context in case one of the pins is of type object and has a value of Self
	UClass* CallingContext = NULL;
	const UBlueprint* Blueprint = FBlueprintEditorUtils::FindBlueprintForNode(PinA->GetOwningNodeUnchecked());
	if (Blueprint)
	{
		CallingContext = (Blueprint->GeneratedClass != NULL) ? Blueprint->GeneratedClass : Blueprint->ParentClass;
	}

	// Compare the types
	const bool bTypesMatch = ArePinsCompatible(OutputPin, InputPin, CallingContext, bIgnoreArray);

	if (bTypesMatch)
	{
		return DetermineConnectionResponseOfCompatibleTypedPins(PinA, PinB, InputPin, OutputPin);
	}
	else
	{
		// Autocasting
		FName DummyName;
		UK2Node* DummyNode;

		const bool bCanAutocast = SearchForAutocastFunction(OutputPin, InputPin, /*out*/ DummyName);
		const bool bCanAutoConvert = FindSpecializedConversionNode(OutputPin, InputPin, false, /* out */ DummyNode);

		if (bCanAutocast || bCanAutoConvert)
		{
			return FPinConnectionResponse(CONNECT_RESPONSE_MAKE_WITH_CONVERSION_NODE, FString::Printf(TEXT("Convert %s to %s"), *TypeToText(OutputPin->PinType).ToString(), *TypeToText(InputPin->PinType).ToString()));
		}
		else
		{
			FText IncompatibilityReasonText = GetPinIncompatibilityMessage(PinA, PinB);
			return FPinConnectionResponse(CONNECT_RESPONSE_DISALLOW, IncompatibilityReasonText.ToString());
		}
	}
}

bool UEdGraphSchema_K2::TryCreateConnection(UEdGraphPin* PinA, UEdGraphPin* PinB) const
{
	UBlueprint* Blueprint = FBlueprintEditorUtils::FindBlueprintForNodeChecked(PinA->GetOwningNode());

	bool bModified = UEdGraphSchema::TryCreateConnection(PinA, PinB);

	if (bModified && !PinA->HasAnyFlags(RF_Transient))
	{
		FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
	}

	return bModified;
}

bool UEdGraphSchema_K2::SearchForAutocastFunction(const UEdGraphPin* OutputPin, const UEdGraphPin* InputPin, /*out*/ FName& TargetFunction) const
{
	// NOTE: Under no circumstances should anyone *ever* add a questionable cast to this function.
	// If it could be at all confusing why a function is provided, to even a novice user, err on the side of do not cast!!!
	// This includes things like string->int (does it do length, atoi, or what?) that would be autocasts in a traditional scripting language

	TargetFunction = NAME_None;

	const UScriptStruct* InputStructType = Cast<const UScriptStruct>(InputPin->PinType.PinSubCategoryObject.Get());
	const UScriptStruct* OutputStructType = Cast<const UScriptStruct>(OutputPin->PinType.PinSubCategoryObject.Get());

	if (OutputPin->PinType.bIsArray != InputPin->PinType.bIsArray)
	{
		// We don't autoconvert between arrays and non-arrays.  Those are handled by specialized conversions
	}
	else if (OutputPin->PinType.PinCategory == PC_Int)
	{
		if (InputPin->PinType.PinCategory == PC_Float)
		{
			TargetFunction = TEXT("Conv_IntToFloat");
		}
		else if (InputPin->PinType.PinCategory == PC_String)
		{
			TargetFunction = TEXT("Conv_IntToString");
		}
		else if ((InputPin->PinType.PinCategory == PC_Byte) && (InputPin->PinType.PinSubCategoryObject == NULL))
		{
			TargetFunction = TEXT("Conv_IntToByte");
		}
		else if (InputPin->PinType.PinCategory == PC_Boolean)
		{
			TargetFunction = TEXT("Conv_IntToBool");
		}
		else if(InputPin->PinType.PinCategory == PC_Text)
		{
			TargetFunction = TEXT("Conv_IntToText");
		}
	}
	else if (OutputPin->PinType.PinCategory == PC_Float)
	{
		if (InputPin->PinType.PinCategory == PC_Int)
		{
			TargetFunction = TEXT("FTrunc");
		}
		else if ((InputPin->PinType.PinCategory == PC_Struct) && (InputStructType == VectorStruct))
		{
			TargetFunction = TEXT("Conv_FloatToVector");
		}
		else if (InputPin->PinType.PinCategory == PC_String)
		{
			TargetFunction = TEXT("Conv_FloatToString");
		}
		else if ((InputPin->PinType.PinCategory == PC_Struct) && (InputStructType == LinearColorStruct))
		{
			TargetFunction = TEXT("Conv_FloatToLinearColor");
		}
		else if(InputPin->PinType.PinCategory == PC_Text)
		{
			TargetFunction = TEXT("Conv_FloatToText");
		}
	}
	else if (OutputPin->PinType.PinCategory == PC_Struct)
	{
		if (OutputStructType == VectorStruct)
		{
			if ((InputPin->PinType.PinCategory == PC_Struct) && (InputStructType == TransformStruct))
			{
				TargetFunction = TEXT("Conv_VectorToTransform");
			}
			else if ((InputPin->PinType.PinCategory == PC_Struct) && (InputStructType == LinearColorStruct))
			{
				TargetFunction = TEXT("Conv_VectorToLinearColor");
			}
			else if ((InputPin->PinType.PinCategory == PC_Struct) && (InputStructType == RotatorStruct))
			{
				TargetFunction = TEXT("Conv_VectorToRotator");
			}
			else if (InputPin->PinType.PinCategory == PC_String)
			{
				TargetFunction = TEXT("Conv_VectorToString");
			}
			// NOTE: Did you see the note above about unsafe and unclear casts?
		}
		else if(OutputStructType == RotatorStruct)
		{
			if ((InputPin->PinType.PinCategory == PC_Struct) && (InputStructType == TransformStruct))
			{
				TargetFunction = TEXT("MakeTransform");
			}
			else if ((InputPin->PinType.PinCategory == PC_Struct) && (InputStructType == VectorStruct))
			{
				TargetFunction = TEXT("Conv_RotatorToVector");
			}
			else if (InputPin->PinType.PinCategory == PC_String)
			{
				TargetFunction = TEXT("Conv_RotatorToString");
			}
			// NOTE: Did you see the note above about unsafe and unclear casts?
		}
		else if(OutputStructType == LinearColorStruct)
		{
			if ((InputPin->PinType.PinCategory == PC_Struct) && (InputStructType == ColorStruct))
			{
				TargetFunction = TEXT("Conv_LinearColorToColor");
			}
			else if (InputPin->PinType.PinCategory == PC_String)
			{
				TargetFunction = TEXT("Conv_ColorToString");
			}
			else if ((InputPin->PinType.PinCategory == PC_Struct) && (InputStructType == VectorStruct))
			{
				TargetFunction = TEXT("Conv_LinearColorToVector");
			}
		}
		else if(OutputStructType == ColorStruct)
		{
			if ((InputPin->PinType.PinCategory == PC_Struct) && (InputStructType == LinearColorStruct))
			{
				TargetFunction = TEXT("Conv_ColorToLinearColor");
			}
		}
	}
	else if (OutputPin->PinType.PinCategory == PC_Boolean)
	{
		if (InputPin->PinType.PinCategory == PC_String)
		{
			TargetFunction = TEXT("Conv_BoolToString");
		}
		else if (InputPin->PinType.PinCategory == PC_Int)
		{
			TargetFunction = TEXT("Conv_BoolToInt");
		}
		else if (InputPin->PinType.PinCategory == PC_Float)
		{
			TargetFunction = TEXT("Conv_BoolToFloat");
		}
		else if ((InputPin->PinType.PinCategory == PC_Byte) && (InputPin->PinType.PinSubCategoryObject == NULL))
		{
			TargetFunction = TEXT("Conv_BoolToByte");
		}
		else if ( InputPin->PinType.PinCategory == PC_Text )
		{
			TargetFunction = TEXT("Conv_BoolToText");
		}
	}
	else if (OutputPin->PinType.PinCategory == PC_Byte &&
			 (OutputPin->PinType.PinSubCategoryObject == NULL || !OutputPin->PinType.PinSubCategoryObject->IsA(UEnum::StaticClass())))
	{
		if (InputPin->PinType.PinCategory == PC_String)
		{
			TargetFunction = TEXT("Conv_ByteToString");
		}
		else if (InputPin->PinType.PinCategory == PC_Int)
		{
			TargetFunction = TEXT("Conv_ByteToInt");
		}
		else if (InputPin->PinType.PinCategory == PC_Float)
		{
			TargetFunction = TEXT("Conv_ByteToFloat");
		}
		else if ( InputPin->PinType.PinCategory == PC_Text )
		{
			TargetFunction = TEXT("Conv_ByteToText");
		}
	}
	else if (OutputPin->PinType.PinCategory == PC_Name)
	{
		if (InputPin->PinType.PinCategory == PC_String)
		{
			TargetFunction = TEXT("Conv_NameToString");
		}
		else if (InputPin->PinType.PinCategory == PC_Text)
		{
			TargetFunction = TEXT("Conv_NameToText");
		}
	}
	else if (OutputPin->PinType.PinCategory == PC_String)
	{
		if (InputPin->PinType.PinCategory == PC_Name)
		{
			TargetFunction = TEXT("Conv_StringToName");
		}
		else if (InputPin->PinType.PinCategory == PC_Int)
		{
			TargetFunction = TEXT("Conv_StringToInt");
		}
		else if (InputPin->PinType.PinCategory == PC_Float)
		{
			TargetFunction = TEXT("Conv_StringToFloat");
		}
		else if (InputPin->PinType.PinCategory == PC_Text)
		{
			TargetFunction = TEXT("Conv_StringToText");
		}
		else
		{
			// NOTE: Did you see the note above about unsafe and unclear casts?
		}
	}
	else if (OutputPin->PinType.PinCategory == PC_Text)
	{
		if(InputPin->PinType.PinCategory == PC_String)
		{
			TargetFunction = TEXT("Conv_TextToString");
		}
	}
	else if ((OutputPin->PinType.PinCategory == PC_Interface) && (InputPin->PinType.PinCategory == PC_Object))
	{
		UClass const* InputClass = Cast<UClass const>(InputPin->PinType.PinSubCategoryObject.Get());

		bool const bInputIsUObject = ((InputClass != NULL) && (InputClass == UObject::StaticClass()));
		if (bInputIsUObject)
		{
			TargetFunction = TEXT("Conv_InterfaceToObject");
		}
	}
	else if (OutputPin->PinType.PinCategory == PC_Object)
	{
		UClass const* OutputClass = Cast<UClass const>(OutputPin->PinType.PinSubCategoryObject.Get());
		if (InputPin->PinType.PinCategory == PC_Class)
		{
			UClass const* InputClass = Cast<UClass const>(InputPin->PinType.PinSubCategoryObject.Get());
			if ((OutputClass != nullptr) &&
				(InputClass != nullptr) &&
				OutputClass->IsChildOf(InputClass))
			{
				TargetFunction = TEXT("GetObjectClass");
			}
		}
		else if (InputPin->PinType.PinCategory == PC_String)
		{
			TargetFunction = TEXT("GetDisplayName");
		}
	}

	return TargetFunction != NAME_None;
}

bool UEdGraphSchema_K2::FindSpecializedConversionNode(const UEdGraphPin* OutputPin, const UEdGraphPin* InputPin, bool bCreateNode, /*out*/ UK2Node*& TargetNode) const
{
	bool bCanConvert = false;
	TargetNode = NULL;

	// Conversion for scalar -> array
	if( (!OutputPin->PinType.bIsArray && InputPin->PinType.bIsArray) && ArePinTypesCompatible(OutputPin->PinType, InputPin->PinType, NULL, true) )
	{
		bCanConvert = true;
		if(bCreateNode)
		{
			TargetNode = NewObject<UK2Node_MakeArray>();
		}
	}
	// If connecting an object to a 'call function' self pin, and not currently compatible, see if there is a property we can call a function on
	else if (InputPin->GetOwningNode()->IsA(UK2Node_CallFunction::StaticClass()) && IsSelfPin(*InputPin) && 
		((OutputPin->PinType.PinCategory == PC_Object) || (OutputPin->PinType.PinCategory == PC_Interface)))
	{
		UK2Node_CallFunction* CallFunctionNode = (UK2Node_CallFunction*)(InputPin->GetOwningNode());
		UClass* OutputPinClass = Cast<UClass>(OutputPin->PinType.PinSubCategoryObject.Get());

		UClass* FunctionClass = CallFunctionNode->FunctionReference.GetMemberParentClass(CallFunctionNode->GetBlueprintClassFromNode());
		if(FunctionClass != NULL && OutputPinClass != NULL)
		{
			// Iterate over object properties..
			for (TFieldIterator<UObjectProperty> PropIt(OutputPinClass, EFieldIteratorFlags::IncludeSuper); PropIt; ++PropIt)
			{
				UObjectProperty* ObjProp = *PropIt;
				// .. if we have a blueprint visible var, and is of the type which contains this function..
				if(ObjProp->HasAllPropertyFlags(CPF_BlueprintVisible) && ObjProp->PropertyClass->IsChildOf(FunctionClass))
				{
					// say we can convert
					bCanConvert = true;
					// Create 'get variable' node
					if(bCreateNode)
					{
						UK2Node_VariableGet* GetNode = NewObject<UK2Node_VariableGet>();
						GetNode->VariableReference.SetFromField<UProperty>(ObjProp, false);
						TargetNode = GetNode;
					}
				}
			}

		}	
	}

	if(!bCanConvert)
	{
		// CHECK ENUM TO NAME CAST
		const bool bInoputMatch = InputPin && !InputPin->PinType.bIsArray && ((PC_Name == InputPin->PinType.PinCategory) || (PC_String == InputPin->PinType.PinCategory));
		const bool bOutputMatch = OutputPin && !OutputPin->PinType.bIsArray && (PC_Byte == OutputPin->PinType.PinCategory) && (NULL != Cast<UEnum>(OutputPin->PinType.PinSubCategoryObject.Get()));
		if(bOutputMatch && bInoputMatch)
		{
			bCanConvert = true;
			if(bCreateNode)
			{
				check(NULL == TargetNode);
				if(PC_Name == InputPin->PinType.PinCategory)
				{
					TargetNode = NewObject<UK2Node_GetEnumeratorName>();
				}
				else if(PC_String == InputPin->PinType.PinCategory)
				{
					TargetNode = NewObject<UK2Node_GetEnumeratorNameAsString>();
				}
			}
		}
	}

	if (!bCanConvert && InputPin && OutputPin)
	{
		FEdGraphPinType const& InputType  = InputPin->PinType;
		FEdGraphPinType const& OutputType = OutputPin->PinType;

		// CHECK BYTE TO ENUM CAST
		UEnum* Enum = Cast<UEnum>(InputType.PinSubCategoryObject.Get());
		const bool bInputIsEnum = !InputType.bIsArray && (PC_Byte == InputType.PinCategory) && Enum;
		const bool bOutputIsByte = !OutputType.bIsArray && (PC_Byte == OutputType.PinCategory);
		if (bInputIsEnum && bOutputIsByte)
		{
			bCanConvert = true;
			if(bCreateNode)
			{
				auto CastByteToEnum = NewObject<UK2Node_CastByteToEnum>();
				CastByteToEnum->Enum = Enum;
				CastByteToEnum->bSafe = true;
				TargetNode = CastByteToEnum;
			}
		}
		else
		{
			UClass* InputClass  = Cast<UClass>(InputType.PinSubCategoryObject.Get());
			UClass* OutputClass = Cast<UClass>(InputType.PinSubCategoryObject.Get());

			if ((OutputType.PinCategory == PC_Interface) && (InputType.PinCategory == PC_Object))
			{
				bCanConvert = (InputClass && OutputClass) && (InputClass->ImplementsInterface(OutputClass) || OutputClass->IsChildOf(InputClass));
			}
			else if (OutputType.PinCategory == PC_Object)
			{
				UBlueprintEditorSettings const* BlueprintSettings = GetDefault<UBlueprintEditorSettings>();
				if ((InputType.PinCategory == PC_Object) && BlueprintSettings->bAutoCastObjectConnections)
				{
					bCanConvert = (InputClass && OutputClass) && InputClass->IsChildOf(OutputClass);
				}
			}

			if (bCanConvert && bCreateNode)
			{
				UK2Node_DynamicCast* DynCastNode = NewObject<UK2Node_DynamicCast>();
				DynCastNode->TargetType = InputClass;
				DynCastNode->SetPurity(true);
				TargetNode = DynCastNode;
			}
		}
	}

	return bCanConvert;
}

void UEdGraphSchema_K2::AutowireConversionNode(UEdGraphPin* InputPin, UEdGraphPin* OutputPin, UEdGraphNode* ConversionNode) const
{
	bool bAllowInputConnections = true;
	bool bAllowOutputConnections = true;

	for (int32 PinIndex = 0; PinIndex < ConversionNode->Pins.Num(); ++PinIndex)
	{
		UEdGraphPin* TestPin = ConversionNode->Pins[PinIndex];

		if ((TestPin->Direction == EGPD_Input) && (ArePinTypesCompatible(OutputPin->PinType, TestPin->PinType)))
		{
			if(bAllowOutputConnections && TryCreateConnection(TestPin, OutputPin))
			{
				// Successful connection, do not allow more output connections
				bAllowOutputConnections = false;
			}
		}
		else if ((TestPin->Direction == EGPD_Output) && (ArePinTypesCompatible(TestPin->PinType, InputPin->PinType)))
		{
			if(bAllowInputConnections && TryCreateConnection(TestPin, InputPin))
			{
				// Successful connection, do not allow more input connections
				bAllowInputConnections = false;
			}
		}
	}
}

bool UEdGraphSchema_K2::CreateAutomaticConversionNodeAndConnections(UEdGraphPin* PinA, UEdGraphPin* PinB) const
{
	// Determine which pin is an input and which pin is an output
	UEdGraphPin* InputPin = NULL;
	UEdGraphPin* OutputPin = NULL;
	if (!CategorizePinsByDirection(PinA, PinB, /*out*/ InputPin, /*out*/ OutputPin))
	{
		return false;
	}

	FName TargetFunctionName;
	TSubclassOf<UK2Node> ConversionNodeClass;

	UK2Node* TemplateConversionNode = NULL;

	if (SearchForAutocastFunction(OutputPin, InputPin, /*out*/ TargetFunctionName))
	{
		// Create a new call function node for the casting operator
		UClass* ClassContainingConversionFunction = NULL; //@TODO: Should probably return this from the search function too

		UK2Node_CallFunction* TemplateNode = NewObject<UK2Node_CallFunction>();
		TemplateNode->FunctionReference.SetExternalMember(TargetFunctionName, ClassContainingConversionFunction);
		//TemplateNode->bIsBeadFunction = true;

		TemplateConversionNode = TemplateNode;
	}
	else
	{
		FindSpecializedConversionNode(OutputPin, InputPin, true, /*out*/ TemplateConversionNode);
	}

	if (TemplateConversionNode != NULL)
	{
		// Determine where to position the new node (assuming it isn't going to get beaded)
		FVector2D AverageLocation = CalculateAveragePositionBetweenNodes(InputPin, OutputPin);

		UK2Node* ConversionNode = FEdGraphSchemaAction_K2NewNode::SpawnNodeFromTemplate<UK2Node>(InputPin->GetOwningNode()->GetGraph(), TemplateConversionNode, AverageLocation);

		// Connect the cast node up to the output/input pins
		AutowireConversionNode(InputPin, OutputPin, ConversionNode);

		return true;
	}

	return false;
}

FString UEdGraphSchema_K2::IsPinDefaultValid(const UEdGraphPin* Pin, const FString& NewDefaultValue, UObject* NewDefaultObject, const FText& InNewDefaultText) const
{
	const UBlueprint* OwningBP = FBlueprintEditorUtils::FindBlueprintForNodeChecked(Pin->GetOwningNode());
	const bool bIsArray = Pin->PinType.bIsArray;
	const bool bIsReference = Pin->PinType.bIsReference;
	const bool bIsAutoCreateRefTerm = IsAutoCreateRefTerm(Pin);

	FFormatNamedArguments MessageArgs;
	MessageArgs.Add(TEXT("PinName"), Pin->GetDisplayName());

	if (OwningBP->BlueprintType != BPTYPE_Interface)
	{
		if( !bIsAutoCreateRefTerm )
		{
			if( bIsArray )
			{
				FText MsgFormat = LOCTEXT("BadArrayDefaultVal", "Array inputs (like '{PinName}') must have an input wired into them (try connecting a MakeArray node).");
				return FText::Format(MsgFormat, MessageArgs).ToString();
			}
			else if( bIsReference )
			{
				FText MsgFormat = LOCTEXT("BadRefDefaultVal", "'{PinName}' must have an input wired into it (\"by ref\" params expect a valid input to operate on).");
				return FText::Format(MsgFormat, MessageArgs).ToString();
			}
		}
	}

	FString ReturnMsg;
	DefaultValueSimpleValidation(Pin->PinType, Pin->PinName, NewDefaultValue, NewDefaultObject, InNewDefaultText, &ReturnMsg);
	return ReturnMsg;
}

bool UEdGraphSchema_K2::DoesSupportPinWatching() const
{
	return true;
}

bool UEdGraphSchema_K2::IsPinBeingWatched(UEdGraphPin const* Pin) const
{
	// Note: If you crash here; it is likely that you forgot to call Blueprint->OnBlueprintChanged.Broadcast(Blueprint) to invalidate the cached UI state
	UBlueprint* Blueprint = FBlueprintEditorUtils::FindBlueprintForNodeChecked(Pin->GetOwningNode());
	return FKismetDebugUtilities::IsPinBeingWatched(Blueprint, Pin);
}

void UEdGraphSchema_K2::ClearPinWatch(UEdGraphPin const* Pin) const
{
	UBlueprint* Blueprint = FBlueprintEditorUtils::FindBlueprintForNodeChecked(Pin->GetOwningNode());
	FKismetDebugUtilities::RemovePinWatch(Blueprint, Pin);
}

bool UEdGraphSchema_K2::DefaultValueSimpleValidation(const FEdGraphPinType& PinType, const FString& PinName, const FString& NewDefaultValue, UObject* NewDefaultObject, const FText& InNewDefaultText, FString* OutMsg /*= NULL*/) const
{
#ifdef DVSV_RETURN_MSG
	static_assert(false, "Macro redefinition.");
#endif
#define DVSV_RETURN_MSG(Str) if(NULL != OutMsg) { *OutMsg = Str; } return false;

	const FString& PinCategory = PinType.PinCategory;
	const FString& PinSubCategory = PinType.PinSubCategory;
	const UObject* PinSubCategoryObject = PinType.PinSubCategoryObject.Get();

	if (PinType.bIsArray)
	{
		// arrays are validated separately
	}
	//@TODO: FCString::Atoi, FCString::Atof, and appStringToBool will 'accept' any input, but we should probably catch and warn
	// about invalid input (non numeric for int/byte/float, and non 0/1 or yes/no/true/false for bool)

	else if(PinCategory == PC_Boolean)
	{
		// All input is acceptable to some degree
	}
	else if (PinCategory == PC_Byte)
	{
		const UEnum* EnumPtr = Cast<const UEnum>(PinSubCategoryObject);
		if (EnumPtr)
		{
			if (EnumPtr->FindEnumIndex(*NewDefaultValue) == INDEX_NONE)
			{
				DVSV_RETURN_MSG( FString::Printf( TEXT("'%s' is not a valid enumerant of '<%s>'"), *NewDefaultValue, *(EnumPtr->GetName( )) ) );
			}
		}
		else if( !NewDefaultValue.IsEmpty() )
		{
			int32 Value;
			if (!FDefaultValueHelper::ParseInt(NewDefaultValue, Value))
			{
				DVSV_RETURN_MSG( TEXT("Expected a valid unsigned number for a byte property") );
			}
			if ((Value < 0) || (Value > 255))
			{
				DVSV_RETURN_MSG( TEXT("Expected a value between 0 and 255 for a byte property") );
			}
		}
	}
	else if (PinCategory == PC_Class)
	{
		// Should have an object set but no string
		if(!NewDefaultValue.IsEmpty())
		{
			DVSV_RETURN_MSG( FString::Printf(TEXT("String NewDefaultValue '%s' specified on class pin '%s'"), *NewDefaultValue, *(PinName)) );
		}

		if (NewDefaultObject == NULL)
		{
			// Valid self-reference or empty reference
		}
		else
		{
			// Otherwise, we expect to be able to resolve the type at least
			const UClass* DefaultClassType = Cast<const UClass>(NewDefaultObject);
			if (DefaultClassType == NULL)
			{
				DVSV_RETURN_MSG( FString::Printf(TEXT("Literal on pin %s is not a class."), *(PinName)) );
			}
			else
			{
				// @TODO support PinSubCategory == 'self'
				const UClass* PinClassType = Cast<const UClass>(PinSubCategoryObject);
				if (PinClassType == NULL)
				{
					DVSV_RETURN_MSG( FString::Printf(TEXT("Failed to find class for pin %s"), *(PinName)) );
				}
				else
				{
					// Have both types, make sure the specified type is a valid subtype
					if (!DefaultClassType->IsChildOf(PinClassType))
					{
						DVSV_RETURN_MSG( FString::Printf(TEXT("%s isn't a valid subclass of %s (specified on pin %s)"), *NewDefaultObject->GetPathName(), *PinClassType->GetName(), *(PinName)) );
					}
				}
			}
		}
	}
	else if (PinCategory == PC_Float)
	{
		if(!NewDefaultValue.IsEmpty())
		{
			if (!FDefaultValueHelper::IsStringValidFloat(NewDefaultValue))
			{
				DVSV_RETURN_MSG( TEXT("Expected a valid number for an float property") );
			}
		}
	}
	else if (PinCategory == PC_Int)
	{
		if(!NewDefaultValue.IsEmpty())
		{
			if (!FDefaultValueHelper::IsStringValidInteger(NewDefaultValue))
			{
				DVSV_RETURN_MSG( TEXT("Expected a valid number for an integer property") );
			}
		}
	}
	else if (PinCategory == PC_Name)
	{
		// Anything is allowed
	}
	else if ((PinCategory == PC_Object) || (PinCategory == PC_Interface))
	{
		if(PinSubCategoryObject == NULL && PinSubCategory != PSC_Self)
		{
			DVSV_RETURN_MSG( FString::Printf(TEXT("PinSubCategoryObject on pin '%s' is NULL and PinSubCategory is '%s' not 'self'"), *(PinName), *PinSubCategory) );
		}

		if(PinSubCategoryObject != NULL && PinSubCategory != TEXT(""))
		{
			DVSV_RETURN_MSG( FString::Printf(TEXT("PinSubCategoryObject on pin '%s' is non-NULL but PinSubCategory is '%s', should be empty"), *(PinName), *PinSubCategory) );
		}

		// Should have an object set but no string - 'self' is not a valid NewDefaultValue for PC_Object pins
		if(!NewDefaultValue.IsEmpty())
		{
			DVSV_RETURN_MSG( FString::Printf(TEXT("String NewDefaultValue '%s' specified on object pin '%s'"), *NewDefaultValue, *(PinName)) );
		}

		// Check that the object that is set is of the correct class
		const UClass* ObjectClass = Cast<const UClass>(PinSubCategoryObject);
		if(NewDefaultObject != NULL && ObjectClass != NULL && !NewDefaultObject->IsA(ObjectClass))
		{
			DVSV_RETURN_MSG( FString::Printf(TEXT("%s isn't a %s (specified on pin %s)"), *NewDefaultObject->GetPathName(), *ObjectClass->GetName(), *(PinName)) );		
		}
	}
	else if (PinCategory == PC_String)
	{
		// All strings are valid
	}
	else if (PinCategory == PC_Text)
	{
		// Neither of these should ever be true 
		if( InNewDefaultText.IsTransient() )
		{
			DVSV_RETURN_MSG( TEXT("Invalid text literal, text is transient!") );
		}
	}
	else if (PinCategory == PC_Struct)
	{
		if(PinSubCategory != TEXT(""))
		{
			DVSV_RETURN_MSG( FString::Printf(TEXT("Invalid PinSubCategory value '%s' (it should be empty)"), *PinSubCategory) );
		}

		// Only FRotator and FVector properties are currently allowed to have a valid default value
		const UScriptStruct* StructType = Cast<const UScriptStruct>(PinSubCategoryObject);
		if (StructType == NULL)
		{
			//@TODO: MessageLog.Error(*FString::Printf(TEXT("Failed to find struct named %s (passed thru @@)"), *PinSubCategory), SourceObject);
			DVSV_RETURN_MSG( FString::Printf(TEXT("No struct specified for pin '%s'"), *(PinName)) );
		}
		else if(!NewDefaultValue.IsEmpty())
		{
			if (StructType == VectorStruct)
			{
				if (!FDefaultValueHelper::IsStringValidVector(NewDefaultValue))
				{
					DVSV_RETURN_MSG( TEXT("Invalid value for an FVector") );
				}
			}
			else if (StructType == RotatorStruct)
			{
				FRotator Rot;
				if (!FDefaultValueHelper::IsStringValidRotator(NewDefaultValue))
				{
					DVSV_RETURN_MSG( TEXT("Invalid value for an FRotator") );
				}
			}
			else if (StructType == TransformStruct)
			{
				FTransform Transform;
				if ( !Transform.InitFromString(NewDefaultValue))
				{
					DVSV_RETURN_MSG( TEXT("Invalid value for an FTransform") );
				}
			}
			else if (StructType == LinearColorStruct)
			{
				FLinearColor Color;
				// Color form: "(R=%f,G=%f,B=%f,A=%f)"
				if (!Color.InitFromString(NewDefaultValue))
				{
					DVSV_RETURN_MSG( TEXT("Invalid value for an FLinearColor") );
				}
			}
			else
			{
				// Structs must pass validation at this point, because we need a UStructProperty to run ImportText
				// They'll be verified in FKCHandler_CallFunction::CreateFunctionCallStatement()
			}
		}
	}
	else if (PinCategory == TEXT("CommentType"))
	{
		// Anything is allowed
	}
	else
	{
		//@TODO: MessageLog.Error(*FString::Printf(TEXT("Unsupported type %s on @@"), *UEdGraphSchema_K2::TypeToText(Type).ToString()), SourceObject);
		DVSV_RETURN_MSG( FString::Printf(TEXT("Unsupported type %s on pin %s"), *UEdGraphSchema_K2::TypeToText(PinType).ToString(), *(PinName)) ); 
	}

#undef DVSV_RETURN_MSG

	return true;
}

FLinearColor UEdGraphSchema_K2::GetPinTypeColor(const FEdGraphPinType& PinType) const
{
	const FString& TypeString = PinType.PinCategory;
	const UGraphEditorSettings* Settings = GetDefault<UGraphEditorSettings>();

	if (TypeString == PC_Exec)
	{
		return Settings->ExecutionPinTypeColor;
	}
	else if (TypeString == PC_Object)
	{
		return Settings->ObjectPinTypeColor;
	}
	else if (TypeString == PC_Interface)
	{
		return Settings->InterfacePinTypeColor;
	}
	else if (TypeString == PC_Float)
	{
		return Settings->FloatPinTypeColor;
	}
	else if (TypeString == PC_Boolean)
	{
		return Settings->BooleanPinTypeColor;
	}
	else if (TypeString == PC_Byte)
	{
		return Settings->BytePinTypeColor;
	}
	else if (TypeString == PC_Int)
	{
		return Settings->IntPinTypeColor;
	}
	else if (TypeString == PC_Struct)
	{
		if (PinType.PinSubCategoryObject == VectorStruct)
		{
			// vector
			return Settings->VectorPinTypeColor;
		}
		else if (PinType.PinSubCategoryObject == RotatorStruct)
		{
			// rotator
			return Settings->RotatorPinTypeColor;
		}
		else if (PinType.PinSubCategoryObject == TransformStruct)
		{
			// transform
			return Settings->TransformPinTypeColor;
		}
		else
		{
			return Settings->StructPinTypeColor;
		}
	}
	else if (TypeString == PC_String)
	{
		return Settings->StringPinTypeColor;
	}
	else if (TypeString == PC_Text)
	{
		return Settings->TextPinTypeColor;
	}
	else if (TypeString == PC_Wildcard)
	{
		if (PinType.PinSubCategory == PSC_Index)
		{
			return Settings->IndexPinTypeColor;
		}
		else
		{
			return Settings->WildcardPinTypeColor;
		}
	}
	else if (TypeString == PC_Name)
	{
		return Settings->NamePinTypeColor;
	}
	else if (TypeString == PC_Delegate)
	{
		return Settings->DelegatePinTypeColor;
	}
	else if (TypeString == PC_Class)
	{
		return Settings->ClassPinTypeColor;
	}

	// Type does not have a defined color!
	return Settings->DefaultPinTypeColor;
}

FText UEdGraphSchema_K2::GetPinDisplayName(const UEdGraphPin* Pin) const 
{
	FText DisplayName = FText::GetEmpty();

	if (Pin != NULL)
	{
		UEdGraphNode* Node = Pin->GetOwningNode();
		if (Node->ShouldOverridePinNames())
		{
			DisplayName = Node->GetPinNameOverride(*Pin);
		}
		else
		{
			DisplayName = Super::GetPinDisplayName(Pin);
	
			// bit of a hack to hide 'execute' and 'then' pin names
			if ((Pin->PinType.PinCategory == PC_Exec) && 
				((DisplayName.ToString() == PN_Execute) || (DisplayName.ToString() == PN_Then)))
			{
				DisplayName = FText::GetEmpty();
			}
		}

		if( GEditor && GetDefault<UEditorStyleSettings>()->bShowFriendlyNames )
		{
			DisplayName = FText::FromString(FName::NameToDisplayString(DisplayName.ToString(), Pin->PinType.PinCategory == PC_Boolean));
		}
	}
	return DisplayName;
}

void UEdGraphSchema_K2::ConstructBasicPinTooltip(const UEdGraphPin& Pin, const FText& PinDescription, FString& TooltipOut) const
{
	if (bGeneratingDocumentation)
	{
		TooltipOut = PinDescription.ToString();
	}
	else
	{
		FFormatNamedArguments Args;
		Args.Add(TEXT("PinType"), TypeToText(Pin.PinType));

		if (UEdGraphNode* PinNode = Pin.GetOwningNode())
		{
			UEdGraphSchema_K2 const* const K2Schema = Cast<const UEdGraphSchema_K2>(PinNode->GetSchema());
			if (ensure(K2Schema != NULL)) // ensure that this node belongs to this schema
			{
				Args.Add(TEXT("DisplayName"), GetPinDisplayName(&Pin));
				Args.Add(TEXT("LineFeed1"), FText::FromString(TEXT("\n")));
			}
		}
		else
		{
				Args.Add(TEXT("DisplayName"), FText::GetEmpty());
				Args.Add(TEXT("LineFeed1"), FText::GetEmpty());
		}


		if (!PinDescription.IsEmpty())
		{
			Args.Add(TEXT("Description"), PinDescription);
			Args.Add(TEXT("LineFeed2"), FText::FromString(TEXT("\n\n")));
		}
		else
		{
			Args.Add(TEXT("Description"), FText::GetEmpty());
			Args.Add(TEXT("LineFeed2"), FText::GetEmpty());
		}
	
		TooltipOut = FText::Format(LOCTEXT("PinTooltip", "{DisplayName}{LineFeed1}{PinType}{LineFeed2}{Description}"), Args).ToString(); 
	}
}

EGraphType UEdGraphSchema_K2::GetGraphType(const UEdGraph* TestEdGraph) const
{
	if (TestEdGraph)
	{
		//@TODO: Should there be a GT_Subgraph type?	
		UEdGraph* GraphToTest = const_cast<UEdGraph*>(TestEdGraph);

		for (UObject* TestOuter = GraphToTest; TestOuter; TestOuter = TestOuter->GetOuter())
		{
			// reached up to the blueprint for the graph
			if (UBlueprint* Blueprint = Cast<UBlueprint>(TestOuter))
			{
				if (Blueprint->BlueprintType == BPTYPE_MacroLibrary ||
					Blueprint->MacroGraphs.Contains(GraphToTest))
				{
					return GT_Macro;
				}
				else if (Blueprint->UbergraphPages.Contains(GraphToTest))
				{
					return GT_Ubergraph;
				}
				else if (Blueprint->FunctionGraphs.Contains(GraphToTest))
				{
					return GT_Function; 
				}
			}
			else
			{
				GraphToTest = Cast<UEdGraph>(TestOuter);
			}
		}
	}
	
	return Super::GetGraphType(TestEdGraph);
}

bool UEdGraphSchema_K2::IsTitleBarPin(const UEdGraphPin& Pin) const
{
	return IsExecPin(Pin);
}

void UEdGraphSchema_K2::CreateMacroGraphTerminators(UEdGraph& Graph, UClass* Class) const
{
	const FName GraphName = Graph.GetFName();

	UBlueprint* Blueprint = FBlueprintEditorUtils::FindBlueprintForGraphChecked(&Graph);
	
	// Create the entry/exit tunnels
	{
		FGraphNodeCreator<UK2Node_Tunnel> EntryNodeCreator(Graph);
		UK2Node_Tunnel* EntryNode = EntryNodeCreator.CreateNode();
		EntryNode->bCanHaveOutputs = true;
		EntryNodeCreator.Finalize();
		SetNodeMetaData(EntryNode, FNodeMetadata::DefaultGraphNode);
	}

	{
		FGraphNodeCreator<UK2Node_Tunnel> ExitNodeCreator(Graph);
		UK2Node_Tunnel* ExitNode = ExitNodeCreator.CreateNode();
		ExitNode->bCanHaveInputs = true;
		ExitNode->NodePosX = 240;
		ExitNodeCreator.Finalize();
		SetNodeMetaData(ExitNode, FNodeMetadata::DefaultGraphNode);
	}
}

void UEdGraphSchema_K2::LinkDataPinFromOutputToInput(UEdGraphNode* InOutputNode, UEdGraphNode* InInputNode) const
{
	for (auto PinIter = InOutputNode->Pins.CreateIterator(); PinIter; ++PinIter)
	{
		UEdGraphPin* const OutputPin = *PinIter;
		if ((OutputPin->Direction == EGPD_Output) && (!IsExecPin(*OutputPin)))
		{
			UEdGraphPin* const InputPin = InInputNode->FindPinChecked(OutputPin->PinName);
			OutputPin->MakeLinkTo(InputPin);
		}
	}
}

void UEdGraphSchema_K2::CreateFunctionGraphTerminators(UEdGraph& Graph, UClass* Class) const
{
	const FName GraphName = Graph.GetFName();

	UBlueprint* Blueprint = FBlueprintEditorUtils::FindBlueprintForGraphChecked(&Graph);
	check(Blueprint->BlueprintType != BPTYPE_MacroLibrary);

	// Create a function entry node
	FGraphNodeCreator<UK2Node_FunctionEntry> FunctionEntryCreator(Graph);
	UK2Node_FunctionEntry* EntryNode = FunctionEntryCreator.CreateNode();
	EntryNode->SignatureClass = Class;
	EntryNode->SignatureName = GraphName;
	FunctionEntryCreator.Finalize();
	SetNodeMetaData(EntryNode, FNodeMetadata::DefaultGraphNode);

	// See if we need to implement a return node
	UFunction* InterfaceToImplement = FindField<UFunction>(Class, GraphName);
	if (InterfaceToImplement)
	{
		// Add modifier flags from the declaration
		EntryNode->ExtraFlags |= InterfaceToImplement->FunctionFlags & (FUNC_Const | FUNC_Static | FUNC_BlueprintPure);

		UK2Node* NextNode = EntryNode;
		UEdGraphPin* NextExec = FindExecutionPin(*EntryNode, EGPD_Output);
		bool bHasParentNode = false;
		// Create node for call parent function
		if (((Class->GetClassFlags() & CLASS_Interface) == 0)  &&
			(InterfaceToImplement->FunctionFlags & FUNC_BlueprintCallable))
		{
			FGraphNodeCreator<UK2Node_CallParentFunction> FunctionParentCreator(Graph);
			UK2Node_CallParentFunction* ParentNode = FunctionParentCreator.CreateNode();
			ParentNode->SetFromFunction(InterfaceToImplement);
			ParentNode->NodePosX = EntryNode->NodePosX + EntryNode->NodeWidth + 256;
			ParentNode->NodePosY = EntryNode->NodePosY;
			FunctionParentCreator.Finalize();

			UEdGraphPin* ParentNodeExec = FindExecutionPin(*ParentNode, EGPD_Input); 

			// If the parent node has an execution pin, then we should as well (we're overriding them, after all)
			// but perhaps this assumption is not valid in the case where a function becomes pure after being
			// initially declared impure - for that reason I'm checking for validity on both ParentNodeExec and NextExec
			if (ParentNodeExec && NextExec)
			{
				NextExec->MakeLinkTo(ParentNodeExec);
				NextExec = FindExecutionPin(*ParentNode, EGPD_Output);
			}

			NextNode = ParentNode;
			bHasParentNode = true;
		}

		// See if any function params are marked as out
		bool bHasOutParam =  false;
		for( TFieldIterator<UProperty> It(InterfaceToImplement); It && (It->PropertyFlags & CPF_Parm); ++It )
		{
			if( It->PropertyFlags & CPF_OutParm )
			{
				bHasOutParam = true;
				break;
			}
		}

		if( bHasOutParam )
		{
			FGraphNodeCreator<UK2Node_FunctionResult> NodeCreator(Graph);
			UK2Node_FunctionResult* ReturnNode = NodeCreator.CreateNode();
			ReturnNode->SignatureClass = Class;
			ReturnNode->SignatureName = GraphName;
			ReturnNode->NodePosX = NextNode->NodePosX + NextNode->NodeWidth + 256;
			ReturnNode->NodePosY = EntryNode->NodePosY;
			NodeCreator.Finalize();
			SetNodeMetaData(ReturnNode, FNodeMetadata::DefaultGraphNode);

			// Auto-connect the pins for entry and exit, so that by default the signature is properly generated
			UEdGraphPin* ResultNodeExec = FindExecutionPin(*ReturnNode, EGPD_Input);
			if (ResultNodeExec && NextExec)
			{
				NextExec->MakeLinkTo(ResultNodeExec);
			}

			if (bHasParentNode)
			{
				LinkDataPinFromOutputToInput(NextNode, ReturnNode);
			}
		}
	}
}

void UEdGraphSchema_K2::CreateFunctionGraphTerminators(UEdGraph& Graph, UFunction* FunctionSignature) const
{
	const FName GraphName = Graph.GetFName();

	UBlueprint* Blueprint = FBlueprintEditorUtils::FindBlueprintForGraphChecked(&Graph);
	check(Blueprint->BlueprintType != BPTYPE_MacroLibrary);

	// Create a function entry node
	FGraphNodeCreator<UK2Node_FunctionEntry> FunctionEntryCreator(Graph);
	UK2Node_FunctionEntry* EntryNode = FunctionEntryCreator.CreateNode();
	EntryNode->SignatureClass = NULL;
	EntryNode->SignatureName = GraphName;
	FunctionEntryCreator.Finalize();
	SetNodeMetaData(EntryNode, FNodeMetadata::DefaultGraphNode);

	// We don't have a signature class to base this on permanently, because it's not an override function.
	// so we need to define the pins as user defined so that they are serialized.

	EntryNode->CreateUserDefinedPinsForFunctionEntryExit(FunctionSignature, /*bIsFunctionEntry=*/ true);

	// See if any function params are marked as out
	bool bHasOutParam =  false;
	for ( TFieldIterator<UProperty> It(FunctionSignature); It && ( It->PropertyFlags & CPF_Parm ); ++It )
	{
		if ( It->PropertyFlags & CPF_OutParm )
		{
			bHasOutParam = true;
			break;
		}
	}

	if ( bHasOutParam )
	{
		FGraphNodeCreator<UK2Node_FunctionResult> NodeCreator(Graph);
		UK2Node_FunctionResult* ReturnNode = NodeCreator.CreateNode();
		ReturnNode->SignatureClass = NULL;
		ReturnNode->SignatureName = GraphName;
		ReturnNode->NodePosX = EntryNode->NodePosX + EntryNode->NodeWidth + 256;
		ReturnNode->NodePosY = EntryNode->NodePosY;
		NodeCreator.Finalize();
		SetNodeMetaData(ReturnNode, FNodeMetadata::DefaultGraphNode);

		ReturnNode->CreateUserDefinedPinsForFunctionEntryExit(FunctionSignature, /*bIsFunctionEntry=*/ false);

		// Auto-connect the pins for entry and exit, so that by default the signature is properly generated
		UEdGraphPin* EntryNodeExec = FindExecutionPin(*EntryNode, EGPD_Output);
		UEdGraphPin* ResultNodeExec = FindExecutionPin(*ReturnNode, EGPD_Input);
		EntryNodeExec->MakeLinkTo(ResultNodeExec);
	}
}

bool UEdGraphSchema_K2::ConvertPropertyToPinType(const UProperty* Property, /*out*/ FEdGraphPinType& TypeOut) const
{
	if (Property == NULL)
	{
		TypeOut.PinCategory = TEXT("bad_type");
		return false;
	}

	TypeOut.PinSubCategory = TEXT("");
	
	// Handle whether or not this is an array property
	const UArrayProperty* ArrayProperty = Cast<const UArrayProperty>(Property);
	const UProperty* TestProperty = ArrayProperty ? ArrayProperty->Inner : Property;
	TypeOut.bIsArray = (ArrayProperty != NULL);
	TypeOut.bIsReference = Property->HasAllPropertyFlags(CPF_OutParm|CPF_ReferenceParm);
	TypeOut.bIsConst     = Property->HasAllPropertyFlags(CPF_ConstParm);

	// Check to see if this is the wildcard property for the target array type
	UFunction* Function = Cast<UFunction>(Property->GetOuter());

	if( UK2Node_CallArrayFunction::IsWildcardProperty(Function, Property)
		|| UK2Node_CallFunction::IsStructureWildcardProperty(Function, Property->GetName()))
	{
		TypeOut.PinCategory = PC_Wildcard;
	}
	else if (const UInterfaceProperty* InterfaceProperty = Cast<const UInterfaceProperty>(TestProperty))
	{
		TypeOut.PinCategory = PC_Interface;
		TypeOut.PinSubCategoryObject = InterfaceProperty->InterfaceClass;
	}
	else if (const UClassProperty* ClassProperty = Cast<const UClassProperty>(TestProperty))
	{
		TypeOut.PinCategory = PC_Class;
		TypeOut.PinSubCategoryObject = ClassProperty->MetaClass;
	}
	else if (const UAssetClassProperty* AssetClassProperty = Cast<const UAssetClassProperty>(TestProperty))
	{
		TypeOut.PinCategory = PC_Class;
		TypeOut.PinSubCategoryObject = AssetClassProperty->MetaClass;
	}
	else if (const UObjectPropertyBase* ObjectProperty = Cast<const UObjectPropertyBase>(TestProperty))
	{
		TypeOut.PinCategory = PC_Object;
		TypeOut.PinSubCategoryObject = ObjectProperty->PropertyClass;
		TypeOut.bIsWeakPointer = TestProperty->IsA(UWeakObjectProperty::StaticClass());
	}
	else if (const UStructProperty* StructProperty = Cast<const UStructProperty>(TestProperty))
	{
		TypeOut.PinCategory = PC_Struct;
		TypeOut.PinSubCategoryObject = StructProperty->Struct;
	}
	else if (Cast<const UFloatProperty>(TestProperty) != NULL)
	{
		TypeOut.PinCategory = PC_Float;
	}
	else if (Cast<const UIntProperty>(TestProperty) != NULL)
	{
		TypeOut.PinCategory = PC_Int;
	}
	else if (const UByteProperty* ByteProperty = Cast<const UByteProperty>(TestProperty))
	{
		TypeOut.PinCategory = PC_Byte;
		TypeOut.PinSubCategoryObject = ByteProperty->Enum;
	}
	else if (Cast<const UNameProperty>(TestProperty) != NULL)
	{
		TypeOut.PinCategory = PC_Name;
	}
	else if (Cast<const UBoolProperty>(TestProperty) != NULL)
	{
		TypeOut.PinCategory = PC_Boolean;
	}
	else if (Cast<const UStrProperty>(TestProperty) != NULL)
	{
		TypeOut.PinCategory = PC_String;
	}
	else if (Cast<const UTextProperty>(TestProperty) != NULL)
	{
		TypeOut.PinCategory = PC_Text;
	}
	else if (const UMulticastDelegateProperty* MulticastDelegateProperty = Cast<const UMulticastDelegateProperty>(TestProperty))
	{
		TypeOut.PinCategory = PC_MCDelegate;
		FMemberReference::FillSimpleMemberReference<UFunction>(MulticastDelegateProperty->SignatureFunction, TypeOut.PinSubCategoryMemberReference);
	}
	else if (const UDelegateProperty* DelegateProperty = Cast<const UDelegateProperty>(TestProperty))
	{
		TypeOut.PinCategory = PC_Delegate;
		FMemberReference::FillSimpleMemberReference<UFunction>(DelegateProperty->SignatureFunction, TypeOut.PinSubCategoryMemberReference);
	}
	else
	{
		TypeOut.PinCategory = TEXT("bad_type");
		return false;
	}

	return true;
}

FString UEdGraphSchema_K2::TypeToString(const FEdGraphPinType& Type)
{
	return TypeToText(Type).ToString();
}

FString UEdGraphSchema_K2::TypeToString(UProperty* const Property)
{
	return TypeToText(Property).ToString();
}

FText UEdGraphSchema_K2::TypeToText(UProperty* const Property)
{
	if (UStructProperty* Struct = Cast<UStructProperty>(Property))
	{
		if (Struct->Struct)
		{
			FEdGraphPinType PinType;
			PinType.PinCategory = PC_Struct;
			PinType.PinSubCategoryObject = Struct->Struct;
			return TypeToText(PinType);
		}
	}
	else if (UClassProperty* Class = Cast<UClassProperty>(Property))
	{
		if (Class->MetaClass)
		{
			FEdGraphPinType PinType;
			PinType.PinCategory = PC_Class;
			PinType.PinSubCategoryObject = Class->MetaClass;
			return TypeToText(PinType);
		}
	}
	else if (UInterfaceProperty* Interface = Cast<UInterfaceProperty>(Property))
	{
		if (Interface->InterfaceClass != nullptr)
		{
			FEdGraphPinType PinType;
			PinType.PinCategory = PC_Interface;
			PinType.PinSubCategoryObject = Interface->InterfaceClass;
			return TypeToText(PinType);
		}
	}
	else if (UObjectPropertyBase* Obj = Cast<UObjectPropertyBase>(Property))
	{
		if( Obj->PropertyClass )
		{
			FEdGraphPinType PinType;
			PinType.PinCategory = PC_Object;
			PinType.PinSubCategoryObject = Obj->PropertyClass;
			PinType.bIsWeakPointer = Property->IsA(UWeakObjectProperty::StaticClass());
			return TypeToText(PinType);
		}

		return FText::GetEmpty();
	}
	else if (UArrayProperty* Array = Cast<UArrayProperty>(Property))
	{
		if (Array->Inner)
		{
			FFormatNamedArguments Args;
			Args.Add(TEXT("ArrayType"), TypeToText(Array->Inner));
			return FText::Format(LOCTEXT("ArrayPropertyText", "Array of {ArrayType}"), Args); 
		}
	}
	
	return FText::FromString(Property->GetClass()->GetName());
}

FText UEdGraphSchema_K2::GetCategoryText(const FString& Category, const bool bForMenu)
{
	if (Category.IsEmpty())
	{
		return FText::GetEmpty();
	}

	static TMap<FString, FText> CategoryDescriptions;
	if (CategoryDescriptions.Num() == 0)
	{
		CategoryDescriptions.Add(PC_Exec, LOCTEXT("Exec", "Exec"));
		CategoryDescriptions.Add(PC_Boolean, LOCTEXT("BoolCategory","Boolean"));
		CategoryDescriptions.Add(PC_Byte, LOCTEXT("ByteCategory","Byte"));
		CategoryDescriptions.Add(PC_Class, LOCTEXT("ClassCategory","Class"));
		CategoryDescriptions.Add(PC_Int, LOCTEXT("IntCategory","Integer"));
		CategoryDescriptions.Add(PC_Float, LOCTEXT("FloatCategory","Float"));
		CategoryDescriptions.Add(PC_Name, LOCTEXT("NameCategory","Name"));
		CategoryDescriptions.Add(PC_Delegate, LOCTEXT("DelegateCategory","Delegate"));
		CategoryDescriptions.Add(PC_MCDelegate, LOCTEXT("MulticastDelegateCategory","Multicast Delegate"));
		CategoryDescriptions.Add(PC_Object, LOCTEXT("ObjectCategory","Reference"));
		CategoryDescriptions.Add(PC_Interface, LOCTEXT("InterfaceCategory","Interface"));
		CategoryDescriptions.Add(PC_String, LOCTEXT("StringCategory","String"));
		CategoryDescriptions.Add(PC_Text, LOCTEXT("TextCategory","Text"));
		CategoryDescriptions.Add(PC_Struct, LOCTEXT("StructCategory","Structure"));
		CategoryDescriptions.Add(PC_Wildcard, LOCTEXT("WildcardCategory","Wildcard"));
		CategoryDescriptions.Add(PC_Enum, LOCTEXT("EnumCategory","Enum"));
	}

	if (bForMenu)
	{
		if (Category == PC_Object)
		{
			return LOCTEXT("ObjectCategoryForMenu", "Object Reference");
		}
	}

	if (FText const* TypeDesc = CategoryDescriptions.Find(Category))
	{
		return *TypeDesc;
	}
	else
	{
		return FText::FromString(Category);
	}
}

FText UEdGraphSchema_K2::TypeToText(const FEdGraphPinType& Type)
{
	FText PropertyText;

	if (Type.PinSubCategoryObject != NULL)
	{
		const UEdGraphSchema_K2* Schema = GetDefault<UEdGraphSchema_K2>();
		if (Type.PinCategory == Schema->PC_Byte)
		{
			FFormatNamedArguments Args;
			Args.Add(TEXT("EnumName"), FText::FromString(Type.PinSubCategoryObject->GetName()));
			PropertyText = FText::Format(LOCTEXT("EnumAsText", "{EnumName} Enum"), Args);
		}
		else
		{
			UObject* SubCategoryObj = Type.PinSubCategoryObject.Get();
			FString SubCategoryObjName = SubCategoryObj->GetName();
			if (UField* SubCategoryField = Cast<UField>(SubCategoryObj))
			{
				SubCategoryObjName = SubCategoryField->GetDisplayNameText().ToString();
			}

			if( !Type.bIsWeakPointer )
			{
				UClass* PSCOAsClass = Cast<UClass>(Type.PinSubCategoryObject.Get());
				const bool bIsInterface = PSCOAsClass && PSCOAsClass->HasAnyClassFlags(CLASS_Interface);

				FFormatNamedArguments Args;
				// Don't display the category for "well-known" struct types
				if (Type.PinCategory == PC_Struct && (Type.PinSubCategoryObject == VectorStruct || Type.PinSubCategoryObject == RotatorStruct || Type.PinSubCategoryObject == TransformStruct))
				{
					Args.Add(TEXT("Category"), FText::GetEmpty());
				}
				else
				{
					Args.Add(TEXT("Category"), (!bIsInterface ? GetCategoryText(Type.PinCategory) : GetCategoryText(PC_Interface)));
				}

				Args.Add(TEXT("ObjectName"), FText::FromString(FName::NameToDisplayString(SubCategoryObjName, /*bIsBool =*/false)));
				PropertyText = FText::Format(LOCTEXT("ObjectAsText", "{ObjectName} {Category}"), Args);
			}
			else
			{
				FFormatNamedArguments Args;
				Args.Add(TEXT("Category"), FText::FromString(Type.PinCategory));
				Args.Add(TEXT("ObjectName"), FText::FromString(SubCategoryObjName));
				PropertyText = FText::Format(LOCTEXT("WeakPtrAsText", "{ObjectName} Weak {Category}"), Args);
			}
		}
	}
	else if (Type.PinSubCategory != TEXT(""))
	{
		FFormatNamedArguments Args;
		Args.Add(TEXT("Category"), GetCategoryText(Type.PinCategory));
		Args.Add(TEXT("ObjectName"), FText::FromString(FName::NameToDisplayString(Type.PinSubCategory, false)));
		PropertyText = FText::Format(LOCTEXT("ObjectAsText", "{ObjectName} {Category}"), Args);
	}
	else
	{
		PropertyText = GetCategoryText(Type.PinCategory);
	}

	if (Type.bIsArray)
	{
		FFormatNamedArguments Args;
		Args.Add(TEXT("PropertyTitle"), PropertyText);
		PropertyText = FText::Format(LOCTEXT("ArrayAsText", "Array of {PropertyTitle}s"), Args);
	}
	else if (Type.bIsReference)
	{
		FFormatNamedArguments Args;
		Args.Add(TEXT("PropertyTitle"), PropertyText);
		PropertyText = FText::Format(LOCTEXT("PropertyByRef", "{PropertyTitle} (by ref)"), Args);
	}

	return PropertyText;
}

void UEdGraphSchema_K2::GetVariableTypeTree( TArray< TSharedPtr<FPinTypeTreeInfo> >& TypeTree, bool bAllowExec, bool bAllowWildCard ) const
{
	// Clear the list
	TypeTree.Empty();

	if( bAllowExec )
	{
		TypeTree.Add( MakeShareable( new FPinTypeTreeInfo(GetCategoryText(PC_Exec, true), PC_Exec, this, LOCTEXT("ExecType", "Execution pin")) ) );
	}

	TypeTree.Add( MakeShareable( new FPinTypeTreeInfo(GetCategoryText(PC_Boolean, true), PC_Boolean, this, LOCTEXT("BooleanType", "True or false value")) ) );
	TypeTree.Add( MakeShareable( new FPinTypeTreeInfo(GetCategoryText(PC_Byte, true), PC_Byte, this, LOCTEXT("ByteType", "8 bit number")) ) );
	TypeTree.Add( MakeShareable( new FPinTypeTreeInfo(GetCategoryText(PC_Int, true), PC_Int, this, LOCTEXT("IntegerType", "Integer number")) ) );
	TypeTree.Add( MakeShareable( new FPinTypeTreeInfo(GetCategoryText(PC_Float, true), PC_Float, this, LOCTEXT("FloatType", "Floating point number")) ) );
	TypeTree.Add( MakeShareable( new FPinTypeTreeInfo(GetCategoryText(PC_Name, true), PC_Name, this, LOCTEXT("NameType", "A text name")) ) );
	TypeTree.Add( MakeShareable( new FPinTypeTreeInfo(GetCategoryText(PC_String, true), PC_String, this, LOCTEXT("StringType", "A text string")) ) );
	TypeTree.Add( MakeShareable( new FPinTypeTreeInfo(GetCategoryText(PC_Text, true), PC_Text, this, LOCTEXT("TextType", "A localizable text string")) ) );

	// Add in special first-class struct types
	TypeTree.Add( MakeShareable( new FPinTypeTreeInfo(PC_Struct, VectorStruct, LOCTEXT("VectorType", "A 3D vector")) ) );
	TypeTree.Add( MakeShareable( new FPinTypeTreeInfo(PC_Struct, RotatorStruct, LOCTEXT("RotatorType", "A 3D rotation")) ) );
	TypeTree.Add( MakeShareable( new FPinTypeTreeInfo(PC_Struct, TransformStruct, LOCTEXT("TransformType", "A 3D transformation, including translation, rotation and 3D scale.")) ) );
	
	// Add wildcard type
	if (bAllowWildCard)
	{
		TypeTree.Add( MakeShareable( new FPinTypeTreeInfo(GetCategoryText(PC_Wildcard, true), PC_Wildcard, this, LOCTEXT("WildcardType", "Wildcard type (unspecified).")) ) );
	}

	// Add the types that have subtrees
	TypeTree.Add( MakeShareable( new FPinTypeTreeInfo(GetCategoryText(PC_Struct, true), PC_Struct, this, LOCTEXT("StructType", "Struct (value) types."), true) ) );
	TypeTree.Add( MakeShareable( new FPinTypeTreeInfo(GetCategoryText(PC_Object, true), PC_Object, this, LOCTEXT("ObjectType", "Object pointer."), true) ) );
	TypeTree.Add( MakeShareable( new FPinTypeTreeInfo(GetCategoryText(PC_Interface, true), PC_Interface, this, LOCTEXT("InterfaceType", "Interface pointer."), true) ) );
	TypeTree.Add( MakeShareable( new FPinTypeTreeInfo(GetCategoryText(PC_Class, true), PC_Class, this, LOCTEXT("ClassType", "Class pointers."), true) ) );
	TypeTree.Add( MakeShareable( new FPinTypeTreeInfo(GetCategoryText(PC_Enum, true), PC_Enum, this, LOCTEXT("EnumType", "Enumeration types."), true) ) );
}

void UEdGraphSchema_K2::GetVariableIndexTypeTree( TArray< TSharedPtr<FPinTypeTreeInfo> >& TypeTree, bool bAllowExec, bool bAllowWildcard ) const
{
	// Clear the list
	TypeTree.Empty();

	if( bAllowExec )
	{
		TypeTree.Add( MakeShareable( new FPinTypeTreeInfo(GetCategoryText(PC_Exec, true), PC_Exec, this, LOCTEXT("ExecIndexType", "Execution pin")) ) );
	}

	TypeTree.Add( MakeShareable( new FPinTypeTreeInfo(GetCategoryText(PC_Boolean, true), PC_Boolean, this, LOCTEXT("BooleanIndexType", "True or false value")) ) );
	TypeTree.Add( MakeShareable( new FPinTypeTreeInfo(GetCategoryText(PC_Byte, true), PC_Byte, this, LOCTEXT("ByteIndexType", "8 bit number")) ) );
	TypeTree.Add( MakeShareable( new FPinTypeTreeInfo(GetCategoryText(PC_Int, true), PC_Int, this, LOCTEXT("IntegerIndexType", "Integer number")) ) );

	// Add wildcard type
	if (bAllowWildcard)
	{
		TypeTree.Add( MakeShareable( new FPinTypeTreeInfo(GetCategoryText(PC_Wildcard, true), PC_Wildcard, this, LOCTEXT("WildcardIndexType", "Wildcard type (unspecified).")) ) );
	}

	// Add the types that have subtrees
	TypeTree.Add( MakeShareable( new FPinTypeTreeInfo(GetCategoryText(PC_Enum, true), PC_Enum, this, LOCTEXT("EnumIndexType", "Enumeration types."), true) ) );
}

bool UEdGraphSchema_K2::DoesTypeHaveSubtypes(const FString& Category) const
{
	return (Category == PC_Struct) || (Category == PC_Object) || (Category == PC_Interface) || (Category == PC_Class) || (Category == PC_Enum);
}

struct FWildcardArrayPinHelper
{
	static bool CheckArrayCompatibility(const UEdGraphPin* OutputPin, const UEdGraphPin* InputPin, bool bIgnoreArray)
	{
		if (bIgnoreArray)
		{
			return true;
		}

		const UK2Node* OwningNode = InputPin ? Cast<UK2Node>(InputPin->GetOwningNode()) : NULL;
		const bool bInputWildcardPinAcceptsArray = !OwningNode || OwningNode->DoesInputWildcardPinAcceptArray(InputPin);
		if (bInputWildcardPinAcceptsArray)
		{
			return true;
		}

		const bool bCheckInputPin = (InputPin->PinType.PinCategory == GetDefault<UEdGraphSchema_K2>()->PC_Wildcard) && !InputPin->PinType.bIsArray;
		const bool bArrayOutputPin = OutputPin && OutputPin->PinType.bIsArray;
		return !(bCheckInputPin && bArrayOutputPin);
	}
};

bool UEdGraphSchema_K2::ArePinsCompatible(const UEdGraphPin* PinA, const UEdGraphPin* PinB, const UClass* CallingContext, bool bIgnoreArray /*= false*/) const
{
	if ((PinA->Direction == EGPD_Input) && (PinB->Direction == EGPD_Output))
	{
		return FWildcardArrayPinHelper::CheckArrayCompatibility(PinB, PinA, bIgnoreArray)
			&& ArePinTypesCompatible(PinB->PinType, PinA->PinType, CallingContext, bIgnoreArray);
	}
	else if ((PinB->Direction == EGPD_Input) && (PinA->Direction == EGPD_Output))
	{
		return FWildcardArrayPinHelper::CheckArrayCompatibility(PinA, PinB, bIgnoreArray)
			&& ArePinTypesCompatible(PinA->PinType, PinB->PinType, CallingContext, bIgnoreArray);
	}
	else
	{
		return false;
	}
}

bool UEdGraphSchema_K2::ArePinTypesCompatible(const FEdGraphPinType& Output, const FEdGraphPinType& Input, const UClass* CallingContext, bool bIgnoreArray /*= false*/) const
{
	if( !bIgnoreArray && (Output.bIsArray != Input.bIsArray) && (Input.PinCategory != PC_Wildcard || Input.bIsArray) )
	{
		return false;
	}
	else if (Output.PinCategory == Input.PinCategory)
	{
		if ((Output.PinSubCategory == Input.PinSubCategory) 
			&& (Output.PinSubCategoryObject == Input.PinSubCategoryObject)
			&& (Output.PinSubCategoryMemberReference == Input.PinSubCategoryMemberReference))
		{
			return true;
		}
		else if (Output.PinCategory == PC_Interface)
		{
			UClass const* OutputClass = Cast<UClass const>(Output.PinSubCategoryObject.Get());
			check(OutputClass && OutputClass->IsChildOf(UInterface::StaticClass()));

			UClass const* InputClass = Cast<UClass const>(Input.PinSubCategoryObject.Get());
			check(InputClass && InputClass->IsChildOf(UInterface::StaticClass()));
			
			return OutputClass->IsChildOf(InputClass);
		}
		else if ((Output.PinCategory == PC_Object) || (Output.PinCategory == PC_Struct) || (Output.PinCategory == PC_Class))
		{
			// Subcategory mismatch, but the two could be castable
			// Only allow a match if the input is a superclass of the output
			UStruct const* OutputObject = (Output.PinSubCategory == PSC_Self) ? CallingContext : Cast<UStruct>(Output.PinSubCategoryObject.Get());
			UStruct const* InputObject  = (Input.PinSubCategory == PSC_Self)  ? CallingContext : Cast<UStruct>(Input.PinSubCategoryObject.Get());

			if ((OutputObject != NULL) && (InputObject != NULL))
			{
				if (Output.PinCategory == PC_Struct)
				{
					return OutputObject->IsChildOf(InputObject) && FStructUtils::TheSameLayout(OutputObject, InputObject);
				}

				// Special Case:  Cannot mix interface and non-interface calls, because the pointer size is different under the hood
				const bool bInputIsInterface  = InputObject->IsChildOf(UInterface::StaticClass());
				const bool bOutputIsInterface = OutputObject->IsChildOf(UInterface::StaticClass());

				if (bInputIsInterface != bOutputIsInterface) 
				{
					UClass const* OutputClass = Cast<const UClass>(OutputObject);
					UClass const* InputClass  = Cast<const UClass>(InputObject);

					if (bInputIsInterface && (OutputClass != NULL))
					{
						return OutputClass->ImplementsInterface(InputClass);
					}
					else if (bOutputIsInterface && (InputClass != NULL))
					{
						return InputClass->ImplementsInterface(OutputClass);
					}
				}				

				return OutputObject->IsChildOf(InputObject) && (bInputIsInterface == bOutputIsInterface);
			}
		}
		else if ((Output.PinCategory == PC_Byte) && (Output.PinSubCategory == Input.PinSubCategory))
		{
			// NOTE: This allows enums to be converted to bytes.  Long-term we don't want to allow that, but we need it
			// for now until we have == for enums in order to be able to compare them.
			if (Input.PinSubCategoryObject == NULL)
			{
				return true;
			}
		}
		else if (PC_Delegate == Output.PinCategory || PC_MCDelegate == Output.PinCategory)
		{
			auto CanUseFunction = [](const UFunction* Func) -> bool
			{
				return Func && (Func->HasAllFlags(RF_LoadCompleted) || !Func->HasAnyFlags(RF_NeedLoad | RF_WasLoaded));
			};

			const UFunction* OutFunction = FMemberReference::ResolveSimpleMemberReference<UFunction>(Output.PinSubCategoryMemberReference);
			if (!CanUseFunction(OutFunction))
			{
				OutFunction = NULL;
			}
			if (!OutFunction && Output.PinSubCategoryMemberReference.GetMemberParentClass())
			{
				const UClass* ParentClass = Output.PinSubCategoryMemberReference.GetMemberParentClass();
				const UBlueprint* BPOwner = Cast<UBlueprint>(ParentClass->ClassGeneratedBy);
				if (BPOwner && BPOwner->SkeletonGeneratedClass && (BPOwner->SkeletonGeneratedClass != ParentClass))
				{
					OutFunction = BPOwner->SkeletonGeneratedClass->FindFunctionByName(Output.PinSubCategoryMemberReference.MemberName);
				}
			}
			const UFunction* InFunction = FMemberReference::ResolveSimpleMemberReference<UFunction>(Input.PinSubCategoryMemberReference);
			if (!CanUseFunction(InFunction))
			{
				InFunction = NULL;
			}
			if (!InFunction && Input.PinSubCategoryMemberReference.GetMemberParentClass())
			{
				const UClass* ParentClass = Input.PinSubCategoryMemberReference.GetMemberParentClass();
				const UBlueprint* BPOwner = Cast<UBlueprint>(ParentClass->ClassGeneratedBy);
				if (BPOwner && BPOwner->SkeletonGeneratedClass && (BPOwner->SkeletonGeneratedClass != ParentClass))
				{
					InFunction = BPOwner->SkeletonGeneratedClass->FindFunctionByName(Input.PinSubCategoryMemberReference.MemberName);
				}
			}
			return !OutFunction || !InFunction || OutFunction->IsSignatureCompatibleWith(InFunction);
		}
	}
	else if (Output.PinCategory == PC_Wildcard || Input.PinCategory == PC_Wildcard)
	{
		// If this is an Index Wildcard we have to check compatibility for indexing types
		if (Output.PinSubCategory == PSC_Index)
		{
			return IsIndexWildcardCompatible(Input);
		}
		else if (Input.PinSubCategory == PSC_Index)
		{
			return IsIndexWildcardCompatible(Output);
		}

		return true;
	}
	else if ((Output.PinCategory == PC_Object) && (Input.PinCategory == PC_Interface))
	{
		UClass const* OutputClass    = Cast<UClass const>(Output.PinSubCategoryObject.Get());
		UClass const* InterfaceClass = Cast<UClass const>(Input.PinSubCategoryObject.Get());

		if ((OutputClass == nullptr) && (Output.PinSubCategory == PSC_Self))
		{
			OutputClass = CallingContext;
		}

		return OutputClass && (OutputClass->ImplementsInterface(InterfaceClass) || OutputClass->IsChildOf(InterfaceClass));
	}

	// Pins representing BLueprint objects and subclass of UObject can match when EditoronlyBP.bAllowClassAndBlueprintPinMatching=true (BaseEngine.ini)
	// It's required for converting all UBlueprint references into UClass.
	struct FObjClassAndBlueprintHelper
	{
	private:
		bool bAllow;
	public:
		FObjClassAndBlueprintHelper() : bAllow(false)
		{
			GConfig->GetBool(TEXT("EditoronlyBP"), TEXT("bAllowClassAndBlueprintPinMatching"), bAllow, GEditorIni);
		}

		bool Match(const FEdGraphPinType& A, const FEdGraphPinType& B, const UEdGraphSchema_K2& Schema) const
		{
			if (bAllow && (B.PinCategory == Schema.PC_Object) && (A.PinCategory == Schema.PC_Class))
			{
				const bool bAIsObjectClass = (UObject::StaticClass() == A.PinSubCategoryObject.Get());
				const UClass* BClass = Cast<UClass>(B.PinSubCategoryObject.Get());
				const bool bBIsBlueprintObj = BClass && BClass->IsChildOf(UBlueprint::StaticClass());
				return bAIsObjectClass && bBIsBlueprintObj;
			}
			return false;
		}
	};

	static FObjClassAndBlueprintHelper MatchHelper;
	if (MatchHelper.Match(Input, Output, *this) || MatchHelper.Match(Output, Input, *this))
	{
		return true;
	}

	return false;
}

void UEdGraphSchema_K2::BreakNodeLinks(UEdGraphNode& TargetNode) const
{
	UBlueprint* Blueprint = FBlueprintEditorUtils::FindBlueprintForNodeChecked(&TargetNode);

	Super::BreakNodeLinks(TargetNode);
	
	FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
}

void UEdGraphSchema_K2::BreakPinLinks(UEdGraphPin& TargetPin, bool bSendsNodeNotifcation) const
{
	const FScopedTransaction Transaction( NSLOCTEXT("UnrealEd", "GraphEd_BreakPinLinks", "Break Pin Links") );

	// cache this here, as BreakPinLinks can trigger a node reconstruction invalidating the TargetPin referenceS
	UBlueprint* const Blueprint = FBlueprintEditorUtils::FindBlueprintForNodeChecked(TargetPin.GetOwningNode());



	Super::BreakPinLinks(TargetPin, bSendsNodeNotifcation);

	FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
}

void UEdGraphSchema_K2::BreakSinglePinLink(UEdGraphPin* SourcePin, UEdGraphPin* TargetPin)
{
	const FScopedTransaction Transaction( NSLOCTEXT("UnrealEd", "GraphEd_BreakSinglePinLink", "Break Pin Link") );

	UBlueprint* Blueprint = FBlueprintEditorUtils::FindBlueprintForNodeChecked(TargetPin->GetOwningNode());

	Super::BreakSinglePinLink(SourcePin, TargetPin);

	FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
}

void UEdGraphSchema_K2::ReconstructNode(UEdGraphNode& TargetNode, bool bIsBatchRequest/*=false*/) const
{
	{
		TArray<UObject *> NodeChildren;
		GetObjectsWithOuter(&TargetNode, NodeChildren, false);
		for (int32 Iter = 0; Iter < NodeChildren.Num(); ++Iter)
		{
			UEdGraphPin* Pin = Cast<UEdGraphPin>(NodeChildren[Iter]);
			const bool bIsValidPin = !Pin 
				|| (Pin->HasAllFlags(RF_PendingKill) && !Pin->LinkedTo.Num()) 
				|| TargetNode.Pins.Contains(Pin);
			if (!bIsValidPin)
			{
				UE_LOG(LogBlueprint, Error, 
					TEXT("Broken Node: %s keeps removed/invalid pin: %s. Try refresh all nodes."),
					*TargetNode.GetFullName(), *Pin->GetFullName());
			}
		}
	}

	Super::ReconstructNode(TargetNode, bIsBatchRequest);

	// If the reconstruction is being handled by something doing a batch (i.e. the blueprint autoregenerating itself), defer marking the blueprint as modified to prevent multiple recompiles
	if (!bIsBatchRequest)
	{
		const UK2Node* K2Node = Cast<UK2Node>(&TargetNode);
		if (K2Node && K2Node->NodeCausesStructuralBlueprintChange())
		{
			UBlueprint* Blueprint = FBlueprintEditorUtils::FindBlueprintForNodeChecked(&TargetNode);
			FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
		}
	}
}

bool UEdGraphSchema_K2::CanEncapuslateNode(UEdGraphNode const& TestNode) const
{
	// Can't encapsulate entry points (may relax this restriction in the future, but it makes sense for now)
	return !TestNode.IsA(UK2Node_FunctionTerminator::StaticClass()) && 
			TestNode.GetClass() != UK2Node_Tunnel::StaticClass(); //Tunnel nodes getting sucked into collapsed graphs fails badly, want to allow derived types though(composite node/Macroinstances)
}

void UEdGraphSchema_K2::HandleGraphBeingDeleted(UEdGraph& GraphBeingRemoved) const
{
	if (UBlueprint* Blueprint = FBlueprintEditorUtils::FindBlueprintForGraph(&GraphBeingRemoved))
	{
		// Look for collapsed graph nodes that reference this graph
		TArray<UK2Node_Composite*> CompositeNodes;
		FBlueprintEditorUtils::GetAllNodesOfClass<UK2Node_Composite>(Blueprint, /*out*/ CompositeNodes);

		TSet<UK2Node_Composite*> NodesToDelete;
		for (int32 i = 0; i < CompositeNodes.Num(); ++i)
		{
			UK2Node_Composite* CompositeNode = CompositeNodes[i];
			if (CompositeNode->BoundGraph == &GraphBeingRemoved)
			{
				NodesToDelete.Add(CompositeNode);
			}
		}

		// Delete the node that owns us
		ensure(NodesToDelete.Num() <= 1);
		for (TSet<UK2Node_Composite*>::TIterator It(NodesToDelete); It; ++It)
		{
			UK2Node_Composite* NodeToDelete = *It;

			// Prevent re-entrancy here
			NodeToDelete->BoundGraph = NULL;

			NodeToDelete->Modify();
			NodeToDelete->DestroyNode();
		}
	}
}

void UEdGraphSchema_K2::TrySetDefaultValue(UEdGraphPin& Pin, const FString& NewDefaultValue) const
{
	FString UseDefaultValue;
	UObject* UseDefaultObject = NULL;
	FText UseDefaultText;

	if ((Pin.PinType.PinCategory == PC_Object) || (Pin.PinType.PinCategory == PC_Class) || (Pin.PinType.PinCategory == PC_Interface))
	{
		UseDefaultObject = FindObject<UObject>(ANY_PACKAGE, *NewDefaultValue);
		UseDefaultValue = NULL;
	}
	else if(Pin.PinType.PinCategory == PC_Text)
	{
		// Set Text pins by string as if it were text.
		TrySetDefaultText(Pin, FText::FromString(NewDefaultValue));
		UseDefaultObject = nullptr;
		UseDefaultValue.Empty();
		return;
	}
	else
	{
		UseDefaultObject = NULL;
		UseDefaultValue = NewDefaultValue;
	}

	// Check the default value and make it an error if it's bogus
	if (IsPinDefaultValid(&Pin, UseDefaultValue, UseDefaultObject, UseDefaultText) == TEXT(""))
	{
		Pin.DefaultObject = UseDefaultObject;
		Pin.DefaultValue = UseDefaultValue;
		Pin.DefaultTextValue = UseDefaultText;
	}

	UEdGraphNode* Node = Pin.GetOwningNode();
	check(Node);
	Node->PinDefaultValueChanged(&Pin);

	UBlueprint* Blueprint = FBlueprintEditorUtils::FindBlueprintForNodeChecked(Node);
	FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
}

void UEdGraphSchema_K2::TrySetDefaultObject(UEdGraphPin& Pin, UObject* NewDefaultObject) const
{
	FText UseDefaultText;

	// Check the default value and make it an error if it's bogus
	if (IsPinDefaultValid(&Pin, FString(TEXT("")), NewDefaultObject, UseDefaultText) == TEXT(""))
	{
		Pin.DefaultObject = NewDefaultObject;
		Pin.DefaultValue = NULL;
		Pin.DefaultTextValue = UseDefaultText;
	}

	UEdGraphNode* Node = Pin.GetOwningNode();
	check(Node);
	Node->PinDefaultValueChanged(&Pin);

	UBlueprint* Blueprint = FBlueprintEditorUtils::FindBlueprintForNodeChecked(Node);
	FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
}

void UEdGraphSchema_K2::TrySetDefaultText(UEdGraphPin& InPin, const FText& InNewDefaultText) const
{
	// No reason to set the FText if it is not a PC_Text.
	if(InPin.PinType.PinCategory == PC_Text)
	{
		// Check the default value and make it an error if it's bogus
		if (IsPinDefaultValid(&InPin, TEXT(""), NULL, InNewDefaultText) == TEXT(""))
		{
			InPin.DefaultObject = NULL;
			InPin.DefaultValue = NULL;
			if(InNewDefaultText.IsEmpty())
			{
				InPin.DefaultTextValue = InNewDefaultText;
			}
			else
			{
				if(InNewDefaultText.IsCultureInvariant())
				{
					InPin.DefaultTextValue = InNewDefaultText;
				}
				else
				{
					InPin.DefaultTextValue = FText::ChangeKey(TEXT(""), InPin.GetOwningNode()->NodeGuid.ToString() + TEXT("_") + InPin.PinName + FString::FromInt(InPin.GetOwningNode()->Pins.Find(&InPin)), InNewDefaultText);
				}
			}
		}

		UEdGraphNode* Node = InPin.GetOwningNode();
		check(Node);
		Node->PinDefaultValueChanged(&InPin);

		UBlueprint* Blueprint = FBlueprintEditorUtils::FindBlueprintForNodeChecked(Node);
		FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
	}
}

bool UEdGraphSchema_K2::IsAutoCreateRefTerm(const UEdGraphPin* Pin) const
{
	check(Pin != NULL);

	bool bIsAutoCreateRefTerm = false;
	UEdGraphNode* OwningNode = Pin->GetOwningNode();
	UK2Node_CallFunction* FuncNode = Cast<UK2Node_CallFunction>(OwningNode);
	if (FuncNode)
	{
		UFunction* TargetFunction = FuncNode->GetTargetFunction();
		if (TargetFunction && !Pin->PinName.IsEmpty())
		{
			TArray<FString> AutoCreateParameterNames;
			FString MetaData = TargetFunction->GetMetaData(FBlueprintMetadata::MD_AutoCreateRefTerm);
			MetaData.ParseIntoArray(AutoCreateParameterNames, TEXT(","), true);
			bIsAutoCreateRefTerm = AutoCreateParameterNames.Contains(Pin->PinName);
		}
	}

	return bIsAutoCreateRefTerm;
}

bool UEdGraphSchema_K2::ShouldHidePinDefaultValue(UEdGraphPin* Pin) const
{
	check(Pin != NULL);

	if (Pin->bDefaultValueIsIgnored || Pin->PinType.bIsArray || (Pin->PinName == PN_Self && Pin->LinkedTo.Num() > 0) || (Pin->PinType.PinCategory == PC_Exec) || (Pin->PinType.bIsReference && !IsAutoCreateRefTerm(Pin)))
	{
		return true;
	}

	return false;
}

bool UEdGraphSchema_K2::ShouldShowAssetPickerForPin(UEdGraphPin* Pin) const
{
	bool bShow = true;
	if (Pin->PinType.PinCategory == PC_Object)
	{
		UClass* ObjectClass = Cast<UClass>(Pin->PinType.PinSubCategoryObject.Get());
		if (ObjectClass)
		{
			// Don't show literal buttons for component type objects
			bShow = !ObjectClass->IsChildOf(UActorComponent::StaticClass());

			if (bShow && ObjectClass->IsChildOf(AActor::StaticClass()))
			{
				// Only show the picker for Actor classes if the class is placeable and we are in the level script
				bShow = !ObjectClass->HasAllClassFlags(CLASS_NotPlaceable)
							&& FBlueprintEditorUtils::IsLevelScriptBlueprint(FBlueprintEditorUtils::FindBlueprintForNode(Pin->GetOwningNode()));
			}

			if (bShow)
			{
				if (UK2Node_CallFunction* CallFunctionNode = Cast<UK2Node_CallFunction>(Pin->GetOwningNode()))
				{
					if ( UFunction* FunctionRef = CallFunctionNode->GetTargetFunction() )
					{
						const UEdGraphPin* WorldContextPin = CallFunctionNode->FindPin(FunctionRef->GetMetaData(FBlueprintMetadata::MD_WorldContext));
						bShow = ( WorldContextPin != Pin );
					}
				}
			}
		}
	}
	return bShow;
}

void UEdGraphSchema_K2::SetPinDefaultValue(UEdGraphPin* Pin, const UFunction* Function, const UProperty* Param) const
{
	if ((Function != nullptr) && (Param != nullptr))
	{
		bool bHasAutomaticValue = false;

		const FString MetadataDefaultValue = Function->GetMetaData(*Param->GetName());
		if (!MetadataDefaultValue.IsEmpty())
		{
			// Specified default value in the metadata
			Pin->AutogeneratedDefaultValue = MetadataDefaultValue;
			bHasAutomaticValue = true;
		}
		else
		{
			const FName MetadataCppDefaultValueKey( *(FString(TEXT("CPP_Default_")) + Param->GetName()) );
			const FString MetadataCppDefaultValue = Function->GetMetaData(MetadataCppDefaultValueKey);
			if (!MetadataCppDefaultValue.IsEmpty())
			{
				Pin->AutogeneratedDefaultValue = MetadataCppDefaultValue;
				bHasAutomaticValue = true;
			}
		}

		if (bHasAutomaticValue)
		{
			if (Pin->PinType.PinCategory == PC_Text)
			{
				Pin->DefaultTextValue = FText::AsCultureInvariant(Pin->AutogeneratedDefaultValue);
			}
			else
			{
				Pin->DefaultValue = Pin->AutogeneratedDefaultValue;
			}
		}
	}
	
	if (Pin->DefaultValue.Len() == 0)
	{
		// Set the default value to (T)0
		SetPinDefaultValueBasedOnType(Pin);
	}
}

void UEdGraphSchema_K2::SetPinDefaultValueBasedOnType(UEdGraphPin* Pin) const
{
	// Create a useful default value based on the pin type
	if(Pin->PinType.bIsArray)
	{
		Pin->AutogeneratedDefaultValue = TEXT("");
	}
	else if (Pin->PinType.PinCategory == PC_Int)
	{
		Pin->AutogeneratedDefaultValue = TEXT("0");
	}
	else if(Pin->PinType.PinCategory == PC_Byte)
	{
		UEnum* EnumPtr = Cast<UEnum>(Pin->PinType.PinSubCategoryObject.Get());
		if(EnumPtr)
		{
			// First element of enum can change. If the enum is { A, B, C } and the default value is A, 
			// the defult value should not change when enum will be changed into { N, A, B, C }
			Pin->AutogeneratedDefaultValue = TEXT("");
			Pin->DefaultValue = EnumPtr->GetEnumName(0);
			return;
		}
		else
		{
			Pin->AutogeneratedDefaultValue = TEXT("0");
		}
	}
	else if (Pin->PinType.PinCategory == PC_Float)
	{
		Pin->AutogeneratedDefaultValue = TEXT("0.0");
	}
	else if (Pin->PinType.PinCategory == PC_Boolean)
	{
		Pin->AutogeneratedDefaultValue = TEXT("false");
	}
	else if (Pin->PinType.PinCategory == PC_Name)
	{
		Pin->AutogeneratedDefaultValue = TEXT("None");
	}
	else if ((Pin->PinType.PinCategory == PC_Struct) && ((Pin->PinType.PinSubCategoryObject == VectorStruct) || (Pin->PinType.PinSubCategoryObject == RotatorStruct)))
	{
		Pin->AutogeneratedDefaultValue = TEXT("0, 0, 0");
	}
	else
	{
		Pin->AutogeneratedDefaultValue = TEXT("");
	}

	Pin->DefaultValue = Pin->AutogeneratedDefaultValue;
}

void UEdGraphSchema_K2::ValidateExistingConnections(UEdGraphPin* Pin)
{
	const UEdGraphSchema_K2* K2Schema = GetDefault<UEdGraphSchema_K2>();

	// Break any newly invalid links
	TArray<UEdGraphPin*> BrokenLinks;
	for (int32 Index = 0; Index < Pin->LinkedTo.Num(); )
	{
		UEdGraphPin* OtherPin = Pin->LinkedTo[Index];
		if (K2Schema->ArePinsCompatible(Pin, OtherPin))
		{
			++Index;
		}
		else
		{
			OtherPin->LinkedTo.Remove(Pin);
			Pin->LinkedTo.RemoveAtSwap(Index);

			BrokenLinks.Add(OtherPin);
		}
	}

	// Cascade the check for changed pin types
	for (TArray<UEdGraphPin*>::TIterator PinIt(BrokenLinks); PinIt; ++PinIt)
	{
		UEdGraphPin* OtherPin = *PinIt;
		OtherPin->GetOwningNode()->PinConnectionListChanged(OtherPin);
	}
}

UFunction* UEdGraphSchema_K2::FindSetVariableByNameFunction(const FEdGraphPinType& PinType)
{
	//!!!! Keep this function synced with FExposeOnSpawnValidator::IsSupported !!!!

	struct FIsCustomStructureParamHelper
	{
		static bool Is(const UObject* Obj)
		{
			static const FName BlueprintTypeName(TEXT("BlueprintType"));
			const auto Struct = Cast<const UScriptStruct>(Obj);
			return Struct ? Struct->GetBoolMetaData(BlueprintTypeName) : false;
		}
	};

	const UEdGraphSchema_K2* K2Schema = GetDefault<UEdGraphSchema_K2>();

	FName SetFunctionName = NAME_None;
	if(PinType.PinCategory == K2Schema->PC_Int)
	{
		static FName SetIntName(GET_FUNCTION_NAME_CHECKED(UKismetSystemLibrary, SetIntPropertyByName));
		SetFunctionName = SetIntName;
	}
	else if(PinType.PinCategory == K2Schema->PC_Byte)
	{
		static FName SetByteName(GET_FUNCTION_NAME_CHECKED(UKismetSystemLibrary, SetBytePropertyByName));
		SetFunctionName = SetByteName;
	}
	else if(PinType.PinCategory == K2Schema->PC_Float)
	{
		static FName SetFloatName(GET_FUNCTION_NAME_CHECKED(UKismetSystemLibrary, SetFloatPropertyByName));
		SetFunctionName = SetFloatName;
	}
	else if(PinType.PinCategory == K2Schema->PC_Boolean)
	{
		static FName SetBoolName(GET_FUNCTION_NAME_CHECKED(UKismetSystemLibrary, SetBoolPropertyByName));
		SetFunctionName = SetBoolName;
	}
	else if(PinType.PinCategory == K2Schema->PC_Object)
	{
		static FName SetObjectName(GET_FUNCTION_NAME_CHECKED(UKismetSystemLibrary, SetObjectPropertyByName));
		SetFunctionName = SetObjectName;
	}
	else if (PinType.PinCategory == K2Schema->PC_Class)
	{
		static FName SetObjectName(GET_FUNCTION_NAME_CHECKED(UKismetSystemLibrary, SetClassPropertyByName));
		SetFunctionName = SetObjectName;
	}
	else if(PinType.PinCategory == K2Schema->PC_Interface)
	{
		static FName SetObjectName(GET_FUNCTION_NAME_CHECKED(UKismetSystemLibrary, SetObjectPropertyByName));
		SetFunctionName = SetObjectName;
	}
	else if(PinType.PinCategory == K2Schema->PC_String)
	{
		static FName SetStringName(GET_FUNCTION_NAME_CHECKED(UKismetSystemLibrary, SetStringPropertyByName));
		SetFunctionName = SetStringName;
	}
	else if ( PinType.PinCategory == K2Schema->PC_Text)
	{
		static FName SetTextName(GET_FUNCTION_NAME_CHECKED(UKismetSystemLibrary, SetTextPropertyByName));
		SetFunctionName = SetTextName;
	}
	else if(PinType.PinCategory == K2Schema->PC_Name)
	{
		static FName SetNameName(GET_FUNCTION_NAME_CHECKED(UKismetSystemLibrary, SetNamePropertyByName));
		SetFunctionName = SetNameName;
	}
	else if(PinType.PinCategory == K2Schema->PC_Struct && PinType.PinSubCategoryObject == VectorStruct)
	{
		static FName SetVectorName(GET_FUNCTION_NAME_CHECKED(UKismetSystemLibrary, SetVectorPropertyByName));
		SetFunctionName = SetVectorName;
	}
	else if(PinType.PinCategory == K2Schema->PC_Struct && PinType.PinSubCategoryObject == RotatorStruct)
	{
		static FName SetRotatorName(GET_FUNCTION_NAME_CHECKED(UKismetSystemLibrary, SetRotatorPropertyByName));
		SetFunctionName = SetRotatorName;
	}
	else if(PinType.PinCategory == K2Schema->PC_Struct && PinType.PinSubCategoryObject == ColorStruct)
	{
		static FName SetLinearColorName(GET_FUNCTION_NAME_CHECKED(UKismetSystemLibrary, SetLinearColorPropertyByName));
		SetFunctionName = SetLinearColorName;
	}
	else if(PinType.PinCategory == K2Schema->PC_Struct && PinType.PinSubCategoryObject == TransformStruct)
	{
		static FName SetTransformName(GET_FUNCTION_NAME_CHECKED(UKismetSystemLibrary, SetTransformPropertyByName));
		SetFunctionName = SetTransformName;
	}
	else if (PinType.PinCategory == K2Schema->PC_Struct && PinType.PinSubCategoryObject == FCollisionProfileName::StaticStruct())
	{
		static FName SetStructureName(GET_FUNCTION_NAME_CHECKED(UKismetSystemLibrary, SetCollisionProfileNameProperty));
		SetFunctionName = SetStructureName;
	}
	else if (PinType.PinCategory == K2Schema->PC_Struct && FIsCustomStructureParamHelper::Is(PinType.PinSubCategoryObject.Get()))
	{
		static FName SetStructureName(GET_FUNCTION_NAME_CHECKED(UKismetSystemLibrary, SetStructurePropertyByName));
		SetFunctionName = SetStructureName;
	}

	UFunction* Function = NULL;
	if(SetFunctionName != NAME_None)
	{
		if(PinType.bIsArray)
		{
			static FName SetArrayName(GET_FUNCTION_NAME_CHECKED(UKismetArrayLibrary, SetArrayPropertyByName));
			Function = FindField<UFunction>(UKismetArrayLibrary::StaticClass(), SetArrayName);
		}
		else
		{
			Function = FindField<UFunction>(UKismetSystemLibrary::StaticClass(), SetFunctionName);
		}
	}

	return Function;
}

bool UEdGraphSchema_K2::CanPromotePinToVariable( const UEdGraphPin& Pin ) const
{
	const FEdGraphPinType& PinType = Pin.PinType;
	bool bCanPromote = (PinType.PinCategory != PC_Wildcard && PinType.PinCategory != PC_Exec ) ? true : false;

	const UK2Node* Node = Cast<UK2Node>(Pin.GetOwningNode());
	const UBlueprint* OwningBlueprint = Node->GetBlueprint();
	
	if (!OwningBlueprint || (OwningBlueprint->BlueprintType == BPTYPE_MacroLibrary) || (OwningBlueprint->BlueprintType == BPTYPE_FunctionLibrary) || IsStaticFunctionGraph(Node->GetGraph()))
	{
		// Never allow promotion in macros, because there's not a scope to define them in
		bCanPromote = false;
	}
	else
	{
		if (PinType.PinCategory == PC_Delegate)
		{
			bCanPromote = false;
		}
		else if ((PinType.PinCategory == PC_Object) || (PinType.PinCategory == PC_Interface))
		{
			if (PinType.PinSubCategoryObject != NULL)
			{
				if (UClass* Class = Cast<UClass>(PinType.PinSubCategoryObject.Get()))
				{
					bCanPromote = UEdGraphSchema_K2::IsAllowableBlueprintVariableType(Class);
				}	
			}
		}
		else if ((PinType.PinCategory == PC_Struct) && (PinType.PinSubCategoryObject != NULL))
		{
			if (UScriptStruct* Struct = Cast<UScriptStruct>(PinType.PinSubCategoryObject.Get()))
			{
				bCanPromote = UEdGraphSchema_K2::IsAllowableBlueprintVariableType(Struct);
			}
		}
	}
	
	return bCanPromote;
}

bool UEdGraphSchema_K2::CanSplitStructPin( const UEdGraphPin& Pin ) const
{
	return (Pin.LinkedTo.Num() == 0 && PinHasSplittableStructType(&Pin) && Pin.GetOwningNode()->AllowSplitPins());
}

bool UEdGraphSchema_K2::CanRecombineStructPin( const UEdGraphPin& Pin ) const
{
	bool bCanRecombine = (Pin.ParentPin != NULL && Pin.LinkedTo.Num() == 0);
	if (bCanRecombine)
	{
		// Go through all the other subpins and ensure they also are not connected to anything
		TArray<UEdGraphPin*> PinsToExamine = Pin.ParentPin->SubPins;

		int32 PinIndex = 0;
		while (bCanRecombine && PinIndex < PinsToExamine.Num())
		{
			UEdGraphPin* SubPin = PinsToExamine[PinIndex];
			if (SubPin->LinkedTo.Num() > 0)
			{
				bCanRecombine = false;
			}
			else if (SubPin->SubPins.Num() > 0)
			{
				PinsToExamine.Append(SubPin->SubPins);
			}
			++PinIndex;
		}
	}

	return bCanRecombine;
}

void UEdGraphSchema_K2::GetGraphDisplayInformation(const UEdGraph& Graph, /*out*/ FGraphDisplayInfo& DisplayInfo) const
{
	DisplayInfo.DocLink = TEXT("Shared/Editors/BlueprintEditor/GraphTypes");
	DisplayInfo.PlainName = FText::FromString( Graph.GetName() ); // Fallback is graph name

	UFunction* Function = NULL;
	UBlueprint* Blueprint = FBlueprintEditorUtils::FindBlueprintForGraph(&Graph);
	if (Blueprint)
	{
		Function = Blueprint->SkeletonGeneratedClass->FindFunctionByName(Graph.GetFName());
	}

	const EGraphType GraphType = GetGraphType(&Graph);
	if (GraphType == GT_Ubergraph)
	{
		DisplayInfo.DocExcerptName = TEXT("EventGraph");

		if (Graph.GetFName() == GN_EventGraph)
		{
			// localized name for the first event graph
			DisplayInfo.PlainName = LOCTEXT("GraphDisplayName_EventGraph", "EventGraph");
			DisplayInfo.Tooltip = DisplayInfo.PlainName.ToString();
		}
		else
		{
			DisplayInfo.Tooltip = Graph.GetName();
		}
	}
	else if (GraphType == GT_Function)
	{
		if ( Graph.GetFName() == FN_UserConstructionScript )
		{
			DisplayInfo.PlainName = LOCTEXT("GraphDisplayName_ConstructionScript", "ConstructionScript");

			DisplayInfo.Tooltip = LOCTEXT("GraphTooltip_ConstructionScript", "Function executed when Blueprint is placed or modified.").ToString();
			DisplayInfo.DocExcerptName = TEXT("ConstructionScript");
		}
		else
		{
			// If we found a function from this graph..
			if (Function)
			{
				DisplayInfo.PlainName = FText::FromString(Function->GetName());
				DisplayInfo.Tooltip = UK2Node_CallFunction::GetDefaultTooltipForFunction(Function); // grab its tooltip
			}
			else
			{
				DisplayInfo.Tooltip = Graph.GetName();
			}

			DisplayInfo.DocExcerptName = TEXT("FunctionGraph");
		}
	}
	else if (GraphType == GT_Macro)
	{
		// Show macro description if set
		FKismetUserDeclaredFunctionMetadata* MetaData = UK2Node_MacroInstance::GetAssociatedGraphMetadata(&Graph);
		DisplayInfo.Tooltip = (MetaData && MetaData->ToolTip.Len() > 0) ? MetaData->ToolTip : Graph.GetName();

		DisplayInfo.DocExcerptName = TEXT("MacroGraph");
	}
	else if (GraphType == GT_Animation)
	{
		DisplayInfo.PlainName = LOCTEXT("GraphDisplayName_AnimGraph", "AnimGraph");

		DisplayInfo.Tooltip = LOCTEXT("GraphTooltip_AnimGraph", "Graph used to blend together different animations.").ToString();
		DisplayInfo.DocExcerptName = TEXT("AnimGraph");
	}
	else if (GraphType == GT_StateMachine)
	{
		DisplayInfo.Tooltip = Graph.GetName();
		DisplayInfo.DocExcerptName = TEXT("StateMachine");
	}

	// Add pure/static/const to notes if set
	if (Function)
	{
		if(Function->HasAnyFunctionFlags(FUNC_BlueprintPure))
		{
			DisplayInfo.Notes.Add(TEXT("pure"));
		}

		// since 'static' is implied in a function library, not going to display it (to be consistent with previous behavior)
		if(Function->HasAnyFunctionFlags(FUNC_Static) && Blueprint->BlueprintType != BPTYPE_FunctionLibrary)
		{
			DisplayInfo.Notes.Add(TEXT("static"));
		}
		else if(Function->HasAnyFunctionFlags(FUNC_Const))
		{
			DisplayInfo.Notes.Add(TEXT("const"));
		}
	}

	// Mark transient graphs as obviously so
	if (Graph.HasAllFlags(RF_Transient))
	{
		DisplayInfo.PlainName = FText::FromString( FString::Printf(TEXT("$$ %s $$"), *DisplayInfo.PlainName.ToString()) );
		DisplayInfo.Notes.Add(TEXT("intermediate build product"));
	}

	if( GEditor && GetDefault<UEditorStyleSettings>()->bShowFriendlyNames )
	{
		DisplayInfo.DisplayName = FText::FromString(FName::NameToDisplayString( DisplayInfo.PlainName.ToString(), false ));
	}
	else
	{
		DisplayInfo.DisplayName = DisplayInfo.PlainName;
	}
}

bool UEdGraphSchema_K2::IsSelfPin(const UEdGraphPin& Pin) const 
{
	return (Pin.PinName == PN_Self);
}

bool UEdGraphSchema_K2::IsDelegateCategory(const FString& Category) const
{
	return (Category == PC_Delegate);
}

FVector2D UEdGraphSchema_K2::CalculateAveragePositionBetweenNodes(UEdGraphPin* InputPin, UEdGraphPin* OutputPin)
{
	UEdGraphNode* InputNode = InputPin->GetOwningNode();
	UEdGraphNode* OutputNode = OutputPin->GetOwningNode();
	const FVector2D InputCorner(InputNode->NodePosX, InputNode->NodePosY);
	const FVector2D OutputCorner(OutputNode->NodePosX, OutputNode->NodePosY);
	
	return (InputCorner + OutputCorner) * 0.5f;
}

bool UEdGraphSchema_K2::IsConstructionScript(const UEdGraph* TestEdGraph) const
{
	TArray<class UK2Node_FunctionEntry*> EntryNodes;
	TestEdGraph->GetNodesOfClass<UK2Node_FunctionEntry>(EntryNodes);

	bool bIsConstructionScript = false;
	if (EntryNodes.Num() > 0)
	{
		UK2Node_FunctionEntry const* const EntryNode = EntryNodes[0];
		bIsConstructionScript = (EntryNode->SignatureName == FN_UserConstructionScript);
	}
	return bIsConstructionScript;
}

bool UEdGraphSchema_K2::IsCompositeGraph( const UEdGraph* TestEdGraph ) const
{
	check(TestEdGraph);

	const EGraphType GraphType = GetGraphType(TestEdGraph);
	if(GraphType == GT_Function) 
	{
		//Find the Tunnel node for composite graph and see if its output is a composite node
		for(auto I = TestEdGraph->Nodes.CreateConstIterator();I;++I)
		{
			UEdGraphNode* Node = *I;
			if(auto Tunnel = Cast<UK2Node_Tunnel>(Node))
			{
				if(auto OutNode = Tunnel->OutputSourceNode)
				{
					if(Cast<UK2Node_Composite>(OutNode))
					{
						return true;
					}
				}
			}
		}
	}
	return false;
}

bool UEdGraphSchema_K2::IsConstFunctionGraph( const UEdGraph* TestEdGraph, bool* bOutIsEnforcingConstCorrectness ) const
{
	check(TestEdGraph);

	const EGraphType GraphType = GetGraphType(TestEdGraph);
	if(GraphType == GT_Function) 
	{
		// Find the entry node for the function graph and see if the 'const' flag is set
		for(auto I = TestEdGraph->Nodes.CreateConstIterator(); I; ++I)
		{
			UEdGraphNode* Node = *I;
			if(auto EntryNode = Cast<UK2Node_FunctionEntry>(Node))
			{
				if(bOutIsEnforcingConstCorrectness != nullptr)
				{
					*bOutIsEnforcingConstCorrectness = EntryNode->bEnforceConstCorrectness;
				}

				return (EntryNode->ExtraFlags & FUNC_Const) != 0;
			}
		}
	}

	if(bOutIsEnforcingConstCorrectness != nullptr)
	{
		*bOutIsEnforcingConstCorrectness = false;
	}

	return false;
}

bool UEdGraphSchema_K2::IsStaticFunctionGraph( const UEdGraph* TestEdGraph ) const
{
	check(TestEdGraph);

	const auto Blueprint = FBlueprintEditorUtils::FindBlueprintForGraph(TestEdGraph);
	if (Blueprint && (EBlueprintType::BPTYPE_FunctionLibrary == Blueprint->BlueprintType))
	{
		return true;
	}

	const EGraphType GraphType = GetGraphType(TestEdGraph);
	if(GraphType == GT_Function) 
	{
		// Find the entry node for the function graph and see if the 'static' flag is set
		for(auto I = TestEdGraph->Nodes.CreateConstIterator(); I; ++I)
		{
			UEdGraphNode* Node = *I;
			if(auto EntryNode = Cast<UK2Node_FunctionEntry>(Node))
			{
				return (EntryNode->ExtraFlags & FUNC_Static) != 0;
			}
		}
	}

	return false;
}

void UEdGraphSchema_K2::DroppedAssetsOnGraph(const TArray<FAssetData>& Assets, const FVector2D& GraphPosition, UEdGraph* Graph) const 
{
	UBlueprint* Blueprint = FBlueprintEditorUtils::FindBlueprintForGraph(Graph);
	if ((Blueprint != NULL) && FBlueprintEditorUtils::IsActorBased(Blueprint))
	{
		float XOffset = 0.0f;
		for(int32 AssetIdx=0; AssetIdx < Assets.Num(); AssetIdx++)
		{
			FVector2D Position = GraphPosition + (AssetIdx * FVector2D(XOffset, 0.0f));

			UObject* Asset = Assets[AssetIdx].GetAsset();

			UClass* AssetClass = Asset->GetClass();		
			if (UBlueprint* BlueprintAsset = Cast<UBlueprint>(Asset))
			{
				AssetClass = BlueprintAsset->GeneratedClass;
			}

			TSubclassOf<UActorComponent> DestinationComponentType = FComponentAssetBrokerage::GetPrimaryComponentForAsset(AssetClass);
			if ((DestinationComponentType == NULL) && AssetClass->IsChildOf(AActor::StaticClass()))
			{
				DestinationComponentType = UChildActorComponent::StaticClass();
			}

			// Make sure we have an asset type that's registered with the component list
			if (DestinationComponentType != NULL)
			{
				UEdGraph* TempOuter = NewObject<UEdGraph>((UObject*)Blueprint);
				TempOuter->SetFlags(RF_Transient);

				FComponentTypeEntry ComponentType = { FString(), FString(), DestinationComponentType };

				IBlueprintNodeBinder::FBindingSet Bindings;
				Bindings.Add(Asset);
				UBlueprintComponentNodeSpawner::Create(ComponentType)->Invoke(Graph, Bindings, GraphPosition);
			}
		}
	}
}

void UEdGraphSchema_K2::DroppedAssetsOnNode(const TArray<FAssetData>& Assets, const FVector2D& GraphPosition, UEdGraphNode* Node) const
{
	// @TODO: Should dropping on component node change the component?
}

void UEdGraphSchema_K2::DroppedAssetsOnPin(const TArray<FAssetData>& Assets, const FVector2D& GraphPosition, UEdGraphPin* Pin) const
{
	// If dropping onto an 'object' pin, try and set the literal
	if ((Pin->PinType.PinCategory == PC_Object) || (Pin->PinType.PinCategory == PC_Interface))
	{
		UClass* PinClass = Cast<UClass>(Pin->PinType.PinSubCategoryObject.Get());
		if(PinClass != NULL)
		{
			// Find first asset of type of the pin
			UObject* Asset = FAssetData::GetFirstAssetDataOfClass(Assets, PinClass).GetAsset();
			if(Asset != NULL)
			{
				TrySetDefaultObject(*Pin, Asset);
			}
		}
	}
}

void UEdGraphSchema_K2::GetAssetsNodeHoverMessage(const TArray<FAssetData>& Assets, const UEdGraphNode* HoverNode, FString& OutTooltipText, bool& OutOkIcon) const 
{ 
	// No comment at the moment because this doesn't do anything
	OutTooltipText = TEXT("");
	OutOkIcon = false;
}

void UEdGraphSchema_K2::GetAssetsPinHoverMessage(const TArray<FAssetData>& Assets, const UEdGraphPin* HoverPin, FString& OutTooltipText, bool& OutOkIcon) const 
{ 
	OutTooltipText = TEXT("");
	OutOkIcon = false;

	// If dropping onto an 'object' pin, try and set the literal
	if ((HoverPin->PinType.PinCategory == PC_Object) || (HoverPin->PinType.PinCategory == PC_Interface))
	{
		UClass* PinClass = Cast<UClass>(HoverPin->PinType.PinSubCategoryObject.Get());
		if(PinClass != NULL)
		{
			// Find first asset of type of the pin
			FAssetData AssetData = FAssetData::GetFirstAssetDataOfClass(Assets, PinClass);
			if(AssetData.IsValid())
			{
				OutOkIcon = true;
				OutTooltipText = FString::Printf(TEXT("Assign %s to this pin"), *(AssetData.AssetName.ToString()));
			}
			else
			{
				OutOkIcon = false;
				OutTooltipText = FString::Printf(TEXT("Not compatible with this pin"));
			}
		}
	}
}

bool UEdGraphSchema_K2::FadeNodeWhenDraggingOffPin(const UEdGraphNode* Node, const UEdGraphPin* Pin) const
{
	if(Node && Pin && (PC_Delegate == Pin->PinType.PinCategory) && (EGPD_Input == Pin->Direction))
	{
		//When dragging off a delegate pin, we should duck the alpha of all nodes except the Custom Event nodes that are compatible with the delegate signature
		//This would help reinforce the connection between delegates and their matching events, and make it easier to see at a glance what could be matched up.
		if(const UK2Node_Event* EventNode = Cast<const UK2Node_Event>(Node))
		{
			const UEdGraphPin* DelegateOutPin = EventNode->FindPin(UK2Node_Event::DelegateOutputName);
			if ((NULL != DelegateOutPin) && 
				(ECanCreateConnectionResponse::CONNECT_RESPONSE_DISALLOW != CanCreateConnection(DelegateOutPin, Pin).Response))
			{
				return false;
			}
		}

		if(const UK2Node_CreateDelegate* CreateDelegateNode = Cast<const UK2Node_CreateDelegate>(Node))
		{
			const UEdGraphPin* DelegateOutPin = CreateDelegateNode->GetDelegateOutPin();
			if ((NULL != DelegateOutPin) && 
				(ECanCreateConnectionResponse::CONNECT_RESPONSE_DISALLOW != CanCreateConnection(DelegateOutPin, Pin).Response))
			{
				return false;
			}
		}
		
		return true;
	}
	return false;
}

struct FBackwardCompatibilityConversionHelper
{
	static bool ConvertNode(
		UK2Node* OldNode,
		const FString& BlueprintPinName,
		UK2Node* NewNode,
		const FString& ClassPinName,
		const UEdGraphSchema_K2& Schema,
		bool bOnlyWithDefaultBlueprint)
	{
		check(OldNode && NewNode);
		const auto Blueprint = OldNode->GetBlueprint();

		auto Graph = OldNode->GetGraph();
		if (!Graph)
		{
			UE_LOG(LogBlueprint, Warning, TEXT("BackwardCompatibilityNodeConversion Error bp: '%s' node: '%s'. No graph containing the node."),
				Blueprint ? *Blueprint->GetName() : TEXT("Unknown"),
				*OldNode->GetName(),
				*BlueprintPinName);
			return false;
		}

		auto OldBlueprintPin = OldNode->FindPin(BlueprintPinName);
		if (!OldBlueprintPin)
		{
			UE_LOG(LogBlueprint, Warning, TEXT("BackwardCompatibilityNodeConversion Error bp: '%s' node: '%s'. No bp pin found '%s'"),
				Blueprint ? *Blueprint->GetName() : TEXT("Unknown"),
				*OldNode->GetName(),
				*BlueprintPinName);
			return false;
		}

		const bool bNondefaultBPConnected = (OldBlueprintPin->LinkedTo.Num() > 0);
		const bool bTryConvert = !bNondefaultBPConnected || !bOnlyWithDefaultBlueprint;
		if (bTryConvert)
		{
			// CREATE NEW NODE
			NewNode->SetFlags(RF_Transactional);
			Graph->AddNode(NewNode, false, false);
			NewNode->CreateNewGuid();
			NewNode->PostPlacedNewNode();
			NewNode->AllocateDefaultPins();
			NewNode->NodePosX = OldNode->NodePosX;
			NewNode->NodePosY = OldNode->NodePosY;

			const auto ClassPin = NewNode->FindPin(ClassPinName);
			if (!ClassPin)
			{
				UE_LOG(LogBlueprint, Warning, TEXT("BackwardCompatibilityNodeConversion Error bp: '%s' node: '%s'. No class pin found '%s'"),
					Blueprint ? *Blueprint->GetName() : TEXT("Unknown"),
					*NewNode->GetName(),
					*ClassPinName);
				return false;
			}
			auto TargetClass = Cast<UClass>(ClassPin->PinType.PinSubCategoryObject.Get());
			if (!TargetClass)
			{
				UE_LOG(LogBlueprint, Warning, TEXT("BackwardCompatibilityNodeConversion Error bp: '%s' node: '%s'. No class found '%s'"),
					Blueprint ? *Blueprint->GetName() : TEXT("Unknown"),
					*NewNode->GetName(),
					*ClassPinName);
				return false;
			}

			// REPLACE BLUEPRINT WITH CLASS
			if (!bNondefaultBPConnected)
			{
				// DEFAULT VALUE
				const auto UsedBlueprint = Cast<UBlueprint>(OldBlueprintPin->DefaultObject);
				ensure(!OldBlueprintPin->DefaultObject || UsedBlueprint);
				ensure(!UsedBlueprint || *UsedBlueprint->GeneratedClass);
				UClass* UsedClass = UsedBlueprint ? *UsedBlueprint->GeneratedClass : NULL;
				Schema.TrySetDefaultObject(*ClassPin, UsedClass);
				if (ClassPin->DefaultObject != UsedClass)
				{
					auto ErrorStr = Schema.IsPinDefaultValid(ClassPin, FString(), UsedClass, FText());
					UE_LOG(LogBlueprint, Warning, TEXT("BackwardCompatibilityNodeConversion Error 'cannot set class' in blueprint: %s node: '%s' actor bp: %s, reason: %s"),
						Blueprint ? *Blueprint->GetName() : TEXT("Unknown"),
						*OldNode->GetName(),
						UsedBlueprint ? *UsedBlueprint->GetName() : TEXT("Unknown"),
						ErrorStr.IsEmpty() ? TEXT("Unknown") : *ErrorStr);
					return false;
				}
			}
			else
			{
				// LINK
				auto CastNode = NewObject<UK2Node_ClassDynamicCast>(Graph);
				CastNode->SetFlags(RF_Transactional);
				CastNode->TargetType = TargetClass;
				Graph->AddNode(CastNode, false, false);
				CastNode->CreateNewGuid();
				CastNode->PostPlacedNewNode();
				CastNode->AllocateDefaultPins();
				const int32 OffsetOnGraph = 200;
				CastNode->NodePosX = OldNode->NodePosX - OffsetOnGraph;
				CastNode->NodePosY = OldNode->NodePosY;

				auto ExecPin = OldNode->GetExecPin();
				auto ExecCastPin = CastNode->GetExecPin();
				check(ExecCastPin);
				if (!ExecPin || !Schema.MovePinLinks(*ExecPin, *ExecCastPin).CanSafeConnect())
				{
					UE_LOG(LogBlueprint, Warning, TEXT("BackwardCompatibilityNodeConversion Error 'cannot connect' in blueprint: %s, pin: %s"),
						Blueprint ? *Blueprint->GetName() : TEXT("Unknown"),
						*ExecCastPin->PinName);
					return false;
				}

				auto ValidCastPin = CastNode->GetValidCastPin();
				check(ValidCastPin);
				if (!Schema.TryCreateConnection(ValidCastPin, ExecPin))
				{
					UE_LOG(LogBlueprint, Warning, TEXT("BackwardCompatibilityNodeConversion Error 'cannot connect' in blueprint: %s, pin: %s"),
						Blueprint ? *Blueprint->GetName() : TEXT("Unknown"),
						*ValidCastPin->PinName);
					return false;
				}

				auto InValidCastPin = CastNode->GetInvalidCastPin();
				check(InValidCastPin);
				if (!Schema.TryCreateConnection(InValidCastPin, ExecPin))
				{
					UE_LOG(LogBlueprint, Warning, TEXT("BackwardCompatibilityNodeConversion Error 'cannot connect' in blueprint: %s, pin: %s"),
						Blueprint ? *Blueprint->GetName() : TEXT("Unknown"),
						*InValidCastPin->PinName);
					return false;
				}

				auto CastSourcePin = CastNode->GetCastSourcePin();
				check(CastSourcePin);
				if (!Schema.MovePinLinks(*OldBlueprintPin, *CastSourcePin).CanSafeConnect())
				{
					UE_LOG(LogBlueprint, Warning, TEXT("BackwardCompatibilityNodeConversion Error 'cannot connect' in blueprint: %s, pin: %s"),
						Blueprint ? *Blueprint->GetName() : TEXT("Unknown"),
						*CastSourcePin->PinName);
					return false;
				}

				auto CastResultPin = CastNode->GetCastResultPin();
				check(CastResultPin);
				if (!Schema.TryCreateConnection(CastResultPin, ClassPin))
				{
					UE_LOG(LogBlueprint, Warning, TEXT("BackwardCompatibilityNodeConversion Error 'cannot connect' in blueprint: %s, pin: %s"),
						Blueprint ? *Blueprint->GetName() : TEXT("Unknown"),
						*CastResultPin->PinName);
					return false;
				}
			}

			// MOVE OTHER PINS
			TArray<UEdGraphPin*> OldPins;
			OldPins.Add(OldBlueprintPin);
			for (auto PinIter = NewNode->Pins.CreateIterator(); PinIter; ++PinIter)
			{
				UEdGraphPin* const Pin = *PinIter;
				check(Pin);
				if (ClassPin != Pin)
				{
					const auto OldPin = OldNode->FindPin(Pin->PinName);
						if(NULL != OldPin)
					{
						OldPins.Add(OldPin);
						if (!Schema.MovePinLinks(*OldPin, *Pin).CanSafeConnect())
						{
							UE_LOG(LogBlueprint, Warning, TEXT("BackwardCompatibilityNodeConversion Error 'cannot connect' in blueprint: %s, pin: %s"),
								Blueprint ? *Blueprint->GetName() : TEXT("Unknown"),
								*Pin->PinName);
						}
					}
					else
					{
						UE_LOG(LogBlueprint, Warning, TEXT("BackwardCompatibilityNodeConversion Error 'missing old pin' in blueprint: %s"),
							Blueprint ? *Blueprint->GetName() : TEXT("Unknown"),
							Pin ? *Pin->PinName : TEXT("Unknown"));
					}
				}
			}
			OldNode->BreakAllNodeLinks();
			for (auto PinIter = OldNode->Pins.CreateIterator(); PinIter; ++PinIter)
			{
					if(!OldPins.Contains(*PinIter))
				{
					UEdGraphPin* Pin = *PinIter;
					UE_LOG(LogBlueprint, Warning, TEXT("BackwardCompatibilityNodeConversion Error 'missing new pin' in blueprint: %s"),
						Blueprint ? *Blueprint->GetName() : TEXT("Unknown"),
						Pin ? *Pin->PinName : TEXT("Unknown"));
				}
			}
			Graph->RemoveNode(OldNode);
			return true;
		}
		return false;
	}

	struct FFunctionCallParams
	{
		const FName OldFuncName;
		const FName NewFuncName;
		const FString& BlueprintPinName;
		const FString& ClassPinName;
		const UClass* FuncScope;

		FFunctionCallParams(FName InOldFunc, FName InNewFunc, const FString& InBlueprintPinName, const FString& InClassPinName, const UClass* InFuncScope)
			: OldFuncName(InOldFunc), NewFuncName(InNewFunc), BlueprintPinName(InBlueprintPinName), ClassPinName(InClassPinName), FuncScope(InFuncScope)
		{
			check(FuncScope);
		}

		FFunctionCallParams(const FBlueprintCallableFunctionRedirect& FunctionRedirect)
			: OldFuncName(*FunctionRedirect.OldFunctionName)
			, NewFuncName(*FunctionRedirect.NewFunctionName)
			, BlueprintPinName(FunctionRedirect.BlueprintParamName)
			, ClassPinName(FunctionRedirect.ClassParamName)
			, FuncScope(NULL)
		{
			FuncScope = FindObject<UClass>(ANY_PACKAGE, *FunctionRedirect.ClassName);
		}

	};

	static void ConvertFunctionCallNodes(const FFunctionCallParams& ConversionParams, TArray<UK2Node_CallFunction*>& Nodes, UEdGraph* Graph, const UEdGraphSchema_K2& Schema, bool bOnlyWithDefaultBlueprint)
	{
		if (ConversionParams.FuncScope)
		{
		const UFunction* OldFunc = ConversionParams.FuncScope->FindFunctionByName(ConversionParams.OldFuncName);
		check(OldFunc);
		const UFunction* NewFunc = ConversionParams.FuncScope->FindFunctionByName(ConversionParams.NewFuncName);
		check(NewFunc);

		for (auto It = Nodes.CreateIterator(); It; ++It)
		{
			if (OldFunc == (*It)->GetTargetFunction())
			{
				auto NewNode = NewObject<UK2Node_CallFunction>(Graph);
				NewNode->SetFromFunction(NewFunc);
				ConvertNode(*It, ConversionParams.BlueprintPinName, NewNode, 
					ConversionParams.ClassPinName, Schema, bOnlyWithDefaultBlueprint);
			}
		}
	}
	}
};

void UEdGraphSchema_K2::BackwardCompatibilityNodeConversion(UEdGraph* Graph, bool bOnlySafeChanges) const 
{
	if (Graph)
	{
		{
			static const FString BlueprintPinName(TEXT("Blueprint"));
			static const FString ClassPinName(TEXT("Class"));
			TArray<UK2Node_SpawnActor*> SpawnActorNodes;
			Graph->GetNodesOfClass(SpawnActorNodes);
			for (auto It = SpawnActorNodes.CreateIterator(); It; ++It)
			{
				FBackwardCompatibilityConversionHelper::ConvertNode(
					*It, BlueprintPinName, NewObject<UK2Node_SpawnActorFromClass>(Graph),
					ClassPinName, *this, bOnlySafeChanges);
			}
		}

		{
			auto Blueprint = FBlueprintEditorUtils::FindBlueprintForGraph(Graph);
			if (Blueprint && *Blueprint->SkeletonGeneratedClass)
			{
				TArray<UK2Node_CallFunction*> Nodes;
				Graph->GetNodesOfClass(Nodes);
				for (const auto& FunctionRedirect : EditoronlyBPFunctionRedirects)
				{
					FBackwardCompatibilityConversionHelper::ConvertFunctionCallNodes(
						FBackwardCompatibilityConversionHelper::FFunctionCallParams(FunctionRedirect),
						Nodes, Graph, *this, bOnlySafeChanges);
				}
			}
			else
			{
				UE_LOG(LogBlueprint, Log, TEXT("BackwardCompatibilityNodeConversion: Blueprint '%s' cannot be fully converted. It has no skeleton class!"),
					Blueprint ? *Blueprint->GetName() : TEXT("Unknown"));
			}
		}

		/** Fix the old Make/Break Vector, Make/Break Vector 2D, and Make/Break Rotator nodes to use the native function call versions */

		TArray<UK2Node_MakeStruct*> MakeStructNodes;
		Graph->GetNodesOfClass<UK2Node_MakeStruct>(MakeStructNodes);
		for(auto It = MakeStructNodes.CreateIterator(); It; ++It)
		{
			UK2Node_MakeStruct* OldMakeStructNode = *It;
			check(NULL != OldMakeStructNode);

			// user may have since deleted the struct type
			if (OldMakeStructNode->StructType == NULL)
			{
				continue;
			}

			// Check to see if the struct has a native make/break that we should try to convert to.
			if (OldMakeStructNode->StructType && OldMakeStructNode->StructType->HasMetaData(TEXT("HasNativeMake")))
			{
				UFunction* MakeNodeFunction = NULL;

				// If any pins need to change their names during the conversion, add them to the map.
				TMap<FString, FString> OldPinToNewPinMap;

				if(OldMakeStructNode->StructType->GetName() == TEXT("Rotator"))
				{
					MakeNodeFunction = FindObject<UClass>(ANY_PACKAGE, TEXT("KismetMathLibrary"))->FindFunctionByName(TEXT("MakeRot"));
					OldPinToNewPinMap.Add(TEXT("Rotator"), TEXT("ReturnValue"));
				}
				else if(OldMakeStructNode->StructType->GetName() == TEXT("Vector"))
				{
					MakeNodeFunction = FindObject<UClass>(ANY_PACKAGE, TEXT("KismetMathLibrary"))->FindFunctionByName(TEXT("MakeVector"));
					OldPinToNewPinMap.Add(TEXT("Vector"), TEXT("ReturnValue"));
				}
				else if(OldMakeStructNode->StructType->GetName() == TEXT("Vector2D"))
				{
					MakeNodeFunction = FindObject<UClass>(ANY_PACKAGE, TEXT("KismetMathLibrary"))->FindFunctionByName(TEXT("MakeVector2D"));
					OldPinToNewPinMap.Add(TEXT("Vector2D"), TEXT("ReturnValue"));
				}

				if(MakeNodeFunction)
				{
					UK2Node_CallFunction* CallFunctionNode = NewObject<UK2Node_CallFunction>(Graph);
					check(CallFunctionNode);
					CallFunctionNode->SetFlags(RF_Transactional);
					Graph->AddNode(CallFunctionNode, false, false);
					CallFunctionNode->SetFromFunction(MakeNodeFunction);
					CallFunctionNode->CreateNewGuid();
					CallFunctionNode->PostPlacedNewNode();
					CallFunctionNode->AllocateDefaultPins();
					CallFunctionNode->NodePosX = OldMakeStructNode->NodePosX;
					CallFunctionNode->NodePosY = OldMakeStructNode->NodePosY;

					for(int32 PinIdx = 0; PinIdx < OldMakeStructNode->Pins.Num(); ++PinIdx)
					{
						UEdGraphPin* OldPin = OldMakeStructNode->Pins[PinIdx];
						UEdGraphPin* NewPin = NULL;

						// Check to see if the pin name is mapped to a new one, if it is use it, otherwise just search for the pin under the old name
						FString* NewPinNamePtr = OldPinToNewPinMap.Find(OldPin->PinName);
						if(NewPinNamePtr)
						{
							NewPin = CallFunctionNode->FindPin(*NewPinNamePtr);
						}
						else
						{
							NewPin = CallFunctionNode->FindPin(OldPin->PinName);
						}
						check(NewPin);

						if(!Graph->GetSchema()->MovePinLinks(*OldPin, *NewPin).CanSafeConnect())
						{
							const UBlueprint* Blueprint = OldMakeStructNode->GetBlueprint();
							UE_LOG(LogBlueprint, Warning, TEXT("BackwardCompatibilityNodeConversion Error 'cannot safetly move pin %s to %s' in blueprint: %s"), 
								*OldPin->PinName, 
								*NewPin->PinName,
								Blueprint ? *Blueprint->GetName() : TEXT("Unknown"));
						}
					}
					OldMakeStructNode->DestroyNode();
				}
			}
		}

		TArray<UK2Node_BreakStruct*> BreakStructNodes;
		Graph->GetNodesOfClass<UK2Node_BreakStruct>(BreakStructNodes);
		for(auto It = BreakStructNodes.CreateIterator(); It; ++It)
		{
			UK2Node_BreakStruct* OldBreakStructNode = *It;
			check(NULL != OldBreakStructNode);

			// user may have since deleted the struct type
			if (OldBreakStructNode->StructType == NULL)
			{
				continue;
			}

			// Check to see if the struct has a native make/break that we should try to convert to.
			if (OldBreakStructNode->StructType && OldBreakStructNode->StructType->HasMetaData(TEXT("HasNativeBreak")))
			{
				UFunction* BreakNodeFunction = NULL;

				// If any pins need to change their names during the conversion, add them to the map.
				TMap<FString, FString> OldPinToNewPinMap;

				if(OldBreakStructNode->StructType->GetName() == TEXT("Rotator"))
				{
					BreakNodeFunction = FindObject<UClass>(ANY_PACKAGE, TEXT("KismetMathLibrary"))->FindFunctionByName(TEXT("BreakRot"));
					OldPinToNewPinMap.Add(TEXT("Rotator"), TEXT("InRot"));
				}
				else if(OldBreakStructNode->StructType->GetName() == TEXT("Vector"))
				{
					BreakNodeFunction = FindObject<UClass>(ANY_PACKAGE, TEXT("KismetMathLibrary"))->FindFunctionByName(TEXT("BreakVector"));
					OldPinToNewPinMap.Add(TEXT("Vector"), TEXT("InVec"));
				}
				else if(OldBreakStructNode->StructType->GetName() == TEXT("Vector2D"))
				{
					BreakNodeFunction = FindObject<UClass>(ANY_PACKAGE, TEXT("KismetMathLibrary"))->FindFunctionByName(TEXT("BreakVector2D"));
					OldPinToNewPinMap.Add(TEXT("Vector2D"), TEXT("InVec"));
				}

				if(BreakNodeFunction)
				{
					UK2Node_CallFunction* CallFunctionNode = NewObject<UK2Node_CallFunction>(Graph);
					check(CallFunctionNode);
					CallFunctionNode->SetFlags(RF_Transactional);
					Graph->AddNode(CallFunctionNode, false, false);
					CallFunctionNode->SetFromFunction(BreakNodeFunction);
					CallFunctionNode->CreateNewGuid();
					CallFunctionNode->PostPlacedNewNode();
					CallFunctionNode->AllocateDefaultPins();
					CallFunctionNode->NodePosX = OldBreakStructNode->NodePosX;
					CallFunctionNode->NodePosY = OldBreakStructNode->NodePosY;

					for(int32 PinIdx = 0; PinIdx < OldBreakStructNode->Pins.Num(); ++PinIdx)
					{
						UEdGraphPin* OldPin = OldBreakStructNode->Pins[PinIdx];
						UEdGraphPin* NewPin = NULL;

						// Check to see if the pin name is mapped to a new one, if it is use it, otherwise just search for the pin under the old name
						FString* NewPinNamePtr = OldPinToNewPinMap.Find(OldPin->PinName);
						if(NewPinNamePtr)
						{
							NewPin = CallFunctionNode->FindPin(*NewPinNamePtr);
						}
						else
						{
							NewPin = CallFunctionNode->FindPin(OldPin->PinName);
						}
						check(NewPin);

						if(!Graph->GetSchema()->MovePinLinks(*OldPin, *NewPin).CanSafeConnect())
						{
							const UBlueprint* Blueprint = OldBreakStructNode->GetBlueprint();
							UE_LOG(LogBlueprint, Warning, TEXT("BackwardCompatibilityNodeConversion Error 'cannot safetly move pin %s to %s' in blueprint: %s"), 
								*OldPin->PinName, 
								*NewPin->PinName,
								Blueprint ? *Blueprint->GetName() : TEXT("Unknown"));
						}
					}
					OldBreakStructNode->DestroyNode();
				}
			}
		}
	}
}

UEdGraphNode* UEdGraphSchema_K2::CreateSubstituteNode(UEdGraphNode* Node, const UEdGraph* Graph, FObjectInstancingGraph* InstanceGraph) const
{
	// If this is an event node, create a unique custom event node as a substitute
	UK2Node_Event* EventNode = Cast<UK2Node_Event>(Node);
	if(EventNode)
	{
		if(!Graph)
		{
			// Use the node's graph (outer) if an explicit graph was not specified
			Graph = Node->GetGraph();
		}

		// Can only place events in ubergraphs
		if (GetGraphType(Graph) != EGraphType::GT_Ubergraph)
		{
			return NULL;
		}

		// Find the Blueprint that owns the graph
		UBlueprint* Blueprint = Graph ? FBlueprintEditorUtils::FindBlueprintForGraph(Graph) : NULL;
		if(Blueprint && Blueprint->SkeletonGeneratedClass)
		{
			// Gather all names in use by the Blueprint class
			TArray<FName> ExistingNamesInUse;
			FBlueprintEditorUtils::GetFunctionNameList(Blueprint, ExistingNamesInUse);
			FBlueprintEditorUtils::GetClassVariableList(Blueprint, ExistingNamesInUse);

			const ERenameFlags RenameFlags = (Blueprint->bIsRegeneratingOnLoad ? REN_ForceNoResetLoaders : 0);

			// Allow the old object name to be used in the graph
			FName ObjName = EventNode->GetFName();
			UObject* Found = FindObject<UObject>(EventNode->GetOuter(), *ObjName.ToString());
			if(Found)
			{
				Found->Rename(NULL, NULL, REN_DontCreateRedirectors | RenameFlags);
			}

			// Create a custom event node to replace the original event node imported from text
			UK2Node_CustomEvent* CustomEventNode = NewObject<UK2Node_CustomEvent>(EventNode->GetOuter(), ObjName, EventNode->GetFlags(), nullptr, true, InstanceGraph);

			// Ensure that it is editable
			CustomEventNode->bIsEditable = true;

			// Set grid position to match that of the target node
			CustomEventNode->NodePosX = EventNode->NodePosX;
			CustomEventNode->NodePosY = EventNode->NodePosY;

			// Build a function name that is appropriate for the event we're replacing
			FString FunctionName;
			const UK2Node_ActorBoundEvent* ActorBoundEventNode = Cast<const UK2Node_ActorBoundEvent>(EventNode);
			const UK2Node_ComponentBoundEvent* CompBoundEventNode = Cast<const UK2Node_ComponentBoundEvent>(EventNode);
			if(ActorBoundEventNode)
			{
				FString TargetName = TEXT("None");
				if(ActorBoundEventNode->EventOwner)
				{
					TargetName = ActorBoundEventNode->EventOwner->GetActorLabel();
				}

				FunctionName = FString::Printf(TEXT("%s_%s"), *ActorBoundEventNode->DelegatePropertyName.ToString(), *TargetName);
			}
			else if(CompBoundEventNode)
			{
				FunctionName = FString::Printf(TEXT("%s_%s"), *CompBoundEventNode->DelegatePropertyName.ToString(), *CompBoundEventNode->ComponentPropertyName.ToString());
			}
			else if(EventNode->CustomFunctionName != NAME_None)
			{
				FunctionName = EventNode->CustomFunctionName.ToString();
			}
			else if(EventNode->bOverrideFunction)
			{
				FunctionName = EventNode->EventReference.GetMemberName().ToString();
			}
			else
			{
				FunctionName = CustomEventNode->GetName().Replace(TEXT("K2Node_"), TEXT(""), ESearchCase::CaseSensitive);
			}

			// Ensure that the new event name doesn't already exist as a variable or function name
			if(InstanceGraph)
			{
				FunctionName += TEXT("_Copy");
				CustomEventNode->CustomFunctionName = FName(*FunctionName, FNAME_Find);
				if(CustomEventNode->CustomFunctionName != NAME_None
					&& ExistingNamesInUse.Contains(CustomEventNode->CustomFunctionName))
				{
					int32 i = 0;
					FString TempFuncName;

					do 
					{
						TempFuncName = FString::Printf(TEXT("%s_%d"), *FunctionName, ++i);
						CustomEventNode->CustomFunctionName = FName(*TempFuncName, FNAME_Find);
					}
					while(CustomEventNode->CustomFunctionName != NAME_None
						&& ExistingNamesInUse.Contains(CustomEventNode->CustomFunctionName));

					FunctionName = TempFuncName;
				}
			}

			// Should be a unique name now, go ahead and assign it
			CustomEventNode->CustomFunctionName = FName(*FunctionName);

			// Copy the pins from the old node to the new one that's replacing it
			CustomEventNode->Pins = EventNode->Pins;
			CustomEventNode->UserDefinedPins = EventNode->UserDefinedPins;

			// Clear out the pins from the old node so that links aren't broken later when it's destroyed
			EventNode->Pins.Empty();
			EventNode->UserDefinedPins.Empty();

			bool bOriginalWasCustomEvent = Cast<UK2Node_CustomEvent>(Node) != nullptr;

			// Fixup pins
			for(int32 PinIndex = 0; PinIndex < CustomEventNode->Pins.Num(); ++PinIndex)
			{
				UEdGraphPin* Pin = CustomEventNode->Pins[PinIndex];
				check(Pin);

				// Reparent the pin to the new custom event node
				Pin->Rename(*Pin->GetName(), CustomEventNode, RenameFlags);

				// Don't include execution or delegate output pins as user-defined pins
				if(!bOriginalWasCustomEvent && !IsExecPin(*Pin) && !IsDelegateCategory(Pin->PinType.PinCategory))
				{
					// Check to see if this pin already exists as a user-defined pin on the custom event node
					bool bFoundUserDefinedPin = false;
					for(int32 UserDefinedPinIndex = 0; UserDefinedPinIndex < CustomEventNode->UserDefinedPins.Num() && !bFoundUserDefinedPin; ++UserDefinedPinIndex)
					{
						const FUserPinInfo& UserDefinedPinInfo = *CustomEventNode->UserDefinedPins[UserDefinedPinIndex].Get();
						bFoundUserDefinedPin = Pin->PinName == UserDefinedPinInfo.PinName && Pin->PinType == UserDefinedPinInfo.PinType;
					}

					if(!bFoundUserDefinedPin)
					{
						// Add a new entry into the user-defined pin array for the custom event node
						TSharedPtr<FUserPinInfo> UserPinInfo = MakeShareable(new FUserPinInfo());
						UserPinInfo->PinName = Pin->PinName;
						UserPinInfo->PinType = Pin->PinType;
						CustomEventNode->UserDefinedPins.Add(UserPinInfo);
					}
				}
			}

			// Return the new custom event node that we just created as a substitute for the original event node
			return CustomEventNode;
		}
	}

	// Use the default logic in all other cases
	return UEdGraphSchema::CreateSubstituteNode(Node, Graph, InstanceGraph);
}

int32 UEdGraphSchema_K2::GetNodeSelectionCount(const UEdGraph* Graph) const
{
	UBlueprint* Blueprint = FBlueprintEditorUtils::FindBlueprintForGraph(Graph);
	int32 SelectionCount = 0;
	
	if( Blueprint )
	{
		SelectionCount = FKismetEditorUtilities::GetNumberOfSelectedNodes(Blueprint);
	}
	return SelectionCount;
}

TSharedPtr<FEdGraphSchemaAction> UEdGraphSchema_K2::GetCreateCommentAction() const
{
	return TSharedPtr<FEdGraphSchemaAction>(static_cast<FEdGraphSchemaAction*>(new FEdGraphSchemaAction_K2AddComment));
}

TSharedPtr<FEdGraphSchemaAction> UEdGraphSchema_K2::GetCreateDocumentNodeAction() const
{
	return TSharedPtr<FEdGraphSchemaAction>(static_cast<FEdGraphSchemaAction*>(new FEdGraphSchemaAction_K2AddDocumentation));
}

bool UEdGraphSchema_K2::CanDuplicateGraph(UEdGraph* InSourceGraph) const
{
	if(GetGraphType(InSourceGraph) == GT_Function)
	{
		UBlueprint* SourceBP = FBlueprintEditorUtils::FindBlueprintForGraph(InSourceGraph);

		// Do not duplicate graphs in Blueprint Interfaces
		if(SourceBP->BlueprintType == BPTYPE_Interface)
		{
			return false;
		}

		// Do not duplicate functions from implemented interfaces
		if( FBlueprintEditorUtils::FindFunctionInImplementedInterfaces(SourceBP, InSourceGraph->GetFName()) )
		{
			return false;
		}

		// Do not duplicate inherited functions
		if( FindField<UFunction>(SourceBP->ParentClass, InSourceGraph->GetFName()) )
		{
			return false;
		}
	}

	return true;
}

UEdGraph* UEdGraphSchema_K2::DuplicateGraph(UEdGraph* GraphToDuplicate) const
{
	UEdGraph* NewGraph = NULL;

	if (CanDuplicateGraph(GraphToDuplicate))
	{
		UBlueprint* Blueprint = FBlueprintEditorUtils::FindBlueprintForGraph(GraphToDuplicate);

		NewGraph = FEdGraphUtilities::CloneGraph(GraphToDuplicate, Blueprint);

		if (NewGraph)
		{
			FName NewGraphName = FBlueprintEditorUtils::FindUniqueKismetName(Blueprint, GraphToDuplicate->GetFName().GetPlainNameString());
			FEdGraphUtilities::RenameGraphCloseToName(NewGraph,NewGraphName.ToString());
			// can't have two graphs with the same guid... that'd be silly!
			NewGraph->GraphGuid = FGuid::NewGuid();

			//Rename the entry node or any further renames will not update the entry node, also fixes a duplicate node issue on compile
			for (int32 NodeIndex = 0; NodeIndex < NewGraph->Nodes.Num(); ++NodeIndex)
			{
				UEdGraphNode* Node = NewGraph->Nodes[NodeIndex];
				if (UK2Node_FunctionEntry* EntryNode = Cast<UK2Node_FunctionEntry>(Node))
				{
					if (EntryNode->SignatureName == GraphToDuplicate->GetFName())
					{
						EntryNode->Modify();
						EntryNode->SignatureName = NewGraph->GetFName();
						break;
					}
				}
				// Rename any custom events to be unique
				else if (Node->GetClass()->GetFName() ==  TEXT("K2Node_CustomEvent"))
				{
					UK2Node_CustomEvent* CustomEvent = Cast<UK2Node_CustomEvent>(Node);
					CustomEvent->RenameCustomEventCloseToName();
				}
			}
		}
	}
	return NewGraph;
}

/**
 * Attempts to best-guess the height of the node. This is necessary because we don't know the actual
 * size of the node until the next Slate tick
 *
 * @param Node The node to guess the height of
 * @return The estimated height of the specified node
 */
float UEdGraphSchema_K2::EstimateNodeHeight( UEdGraphNode* Node )
{
	float HeightEstimate = 0.0f;

	if ( Node != NULL )
	{
		float BaseNodeHeight = 48.0f;
		bool bConsiderNodePins = false;
		float HeightPerPin = 18.0f;

		if ( Node->IsA( UK2Node_CallFunction::StaticClass() ) )
		{
			BaseNodeHeight = 80.0f;
			bConsiderNodePins = true;
			HeightPerPin = 18.0f;
		}
		else if ( Node->IsA( UK2Node_Event::StaticClass() ) )
		{
			BaseNodeHeight = 48.0f;
			bConsiderNodePins = true;
			HeightPerPin = 16.0f;
		}

		HeightEstimate = BaseNodeHeight;

		if ( bConsiderNodePins )
		{
			int32 NumInputPins = 0;
			int32 NumOutputPins = 0;

			for ( int32 PinIndex = 0; PinIndex < Node->Pins.Num(); PinIndex++ )
			{
				UEdGraphPin* CurrentPin = Node->Pins[PinIndex];
				if ( CurrentPin != NULL && !CurrentPin->bHidden )
				{
					switch ( CurrentPin->Direction )
					{
						case EGPD_Input:
							{
								NumInputPins++;
							}
							break;
						case EGPD_Output:
							{
								NumOutputPins++;
							}
							break;
					}
				}
			}

			float MaxNumPins = float(FMath::Max<int32>( NumInputPins, NumOutputPins ));

			HeightEstimate += MaxNumPins * HeightPerPin;
		}
	}

	return HeightEstimate;
}


bool UEdGraphSchema_K2::CollapseGatewayNode(UK2Node* InNode, UEdGraphNode* InEntryNode, UEdGraphNode* InResultNode, FKismetCompilerContext* CompilerContext) const
{
	bool bSuccessful = true;

	// We iterate the array in reverse so we can both remove the subpins safely after we've read them and
	// so we have split nested structs we combine them back together in the right order
	for (int32 BoundaryPinIndex = InNode->Pins.Num() - 1; BoundaryPinIndex >= 0; --BoundaryPinIndex)
	{
		UEdGraphPin* const BoundaryPin = InNode->Pins[BoundaryPinIndex];

		bool bFunctionNode = InNode->IsA(UK2Node_CallFunction::StaticClass());

		// For each pin in the gateway node, find the associated pin in the entry or result node.
		UEdGraphNode* const GatewayNode = (BoundaryPin->Direction == EGPD_Input) ? InEntryNode : InResultNode;
		UEdGraphPin* GatewayPin = NULL;
		if (GatewayNode)
		{
			// First handle struct combining if necessary
			if (BoundaryPin->SubPins.Num() > 0)
			{
				InNode->ExpandSplitPin(CompilerContext, InNode->GetGraph(), BoundaryPin);
			}

			for (int32 PinIdx = GatewayNode->Pins.Num() - 1; PinIdx >= 0; --PinIdx)
			{
				UEdGraphPin* const Pin = GatewayNode->Pins[PinIdx];

				// Expand any gateway pins as needed
				if (Pin->SubPins.Num() > 0)
				{
					InNode->ExpandSplitPin(CompilerContext, GatewayNode->GetGraph(), Pin);
				}

				// Function graphs have a single exec path through them, so only one exec pin for input and another for output. In this fashion, they must not be handled by name.
				if(InNode->GetClass() == UK2Node_CallFunction::StaticClass() && Pin->PinType.PinCategory == PC_Exec && BoundaryPin->PinType.PinCategory == PC_Exec && (Pin->Direction != BoundaryPin->Direction))
				{
					GatewayPin = Pin;
					break;
				}
				else if ((Pin->PinName == BoundaryPin->PinName) && (Pin->Direction != BoundaryPin->Direction))
				{
					GatewayPin = Pin;
					break;
				}
			}
		}

		if (GatewayPin)
		{
			CombineTwoPinNetsAndRemoveOldPins(BoundaryPin, GatewayPin);
		}
		else
		{
			if (BoundaryPin->LinkedTo.Num() > 0 && BoundaryPin->ParentPin == NULL)
			{
				UBlueprint* OwningBP = InNode->GetBlueprint();
				if( OwningBP )
				{
					// We had an input/output with a connection that wasn't twinned
					bSuccessful = false;
					OwningBP->Message_Warn( FString::Printf(*NSLOCTEXT("K2Node", "PinOnBoundryNode_Warning", "Warning: Pin '%s' on boundary node '%s' could not be found in the composite node '%s'").ToString(),
						*(BoundaryPin->PinName),
						(GatewayNode != NULL) ? *(GatewayNode->GetName()) : TEXT("(null)"),
						*(GetName()))					
						);
				}
				else
				{
					UE_LOG(LogBlueprint, Warning, TEXT("%s"), *FString::Printf(*NSLOCTEXT("K2Node", "PinOnBoundryNode_Warning", "Warning: Pin '%s' on boundary node '%s' could not be found in the composite node '%s'").ToString(),
						*(BoundaryPin->PinName),
						(GatewayNode != NULL) ? *(GatewayNode->GetName()) : TEXT("(null)"),
						*(GetName()))					
						);
				}
			}
			else
			{
				// Associated pin was not found but there were no links on this side either, so no harm no foul
			}
		}
	}

	return bSuccessful;
}

void UEdGraphSchema_K2::CombineTwoPinNetsAndRemoveOldPins(UEdGraphPin* InPinA, UEdGraphPin* InPinB) const
{
	check(InPinA != NULL);
	check(InPinB != NULL);
	ensure(InPinA->Direction != InPinB->Direction);

	if ((InPinA->LinkedTo.Num() == 0) && (InPinA->Direction == EGPD_Input))
	{
		// Push the literal value of A to InPinB's connections
		for (int32 IndexB = 0; IndexB < InPinB->LinkedTo.Num(); ++IndexB)
		{
			UEdGraphPin* FarB = InPinB->LinkedTo[IndexB];
			// TODO: Michael N. says this if check should be unnecessary once the underlying issue is fixed.
			// (Probably should use a check() instead once it's removed though.  See additional cases below.
			if (FarB != NULL)
			{
				FarB->DefaultValue = InPinA->DefaultValue;
				FarB->DefaultObject = InPinA->DefaultObject;
				FarB->DefaultTextValue = InPinA->DefaultTextValue;
			}
		}
	}
	else if ((InPinB->LinkedTo.Num() == 0) && (InPinB->Direction == EGPD_Input))
	{
		// Push the literal value of B to InPinA's connections
		for (int32 IndexA = 0; IndexA < InPinA->LinkedTo.Num(); ++IndexA)
		{
			UEdGraphPin* FarA = InPinA->LinkedTo[IndexA];
			// TODO: Michael N. says this if check should be unnecessary once the underlying issue is fixed.
			// (Probably should use a check() instead once it's removed though.  See additional cases above and below.
			if (FarA != NULL)
			{
				FarA->DefaultValue = InPinB->DefaultValue;
				FarA->DefaultObject = InPinB->DefaultObject;
				FarA->DefaultTextValue = InPinB->DefaultTextValue;
			}
		}
	}
	else
	{
		// Make direct connections between the things that connect to A or B, removing A and B from the picture
		for (int32 IndexA = 0; IndexA < InPinA->LinkedTo.Num(); ++IndexA)
		{
			UEdGraphPin* FarA = InPinA->LinkedTo[IndexA];
			// TODO: Michael N. says this if check should be unnecessary once the underlying issue is fixed.
			// (Probably should use a check() instead once it's removed though.  See additional cases above.
			if (FarA != NULL)
			{
				for (int32 IndexB = 0; IndexB < InPinB->LinkedTo.Num(); ++IndexB)
				{
					UEdGraphPin* FarB = InPinB->LinkedTo[IndexB];
					FarA->Modify();
					FarB->Modify();
					FarA->MakeLinkTo(FarB);
				}
			}
		}
	}

	InPinA->BreakAllPinLinks();
	InPinB->BreakAllPinLinks();
}

UK2Node* UEdGraphSchema_K2::CreateSplitPinNode(UEdGraphPin* Pin, FKismetCompilerContext* CompilerContext, UEdGraph* SourceGraph) const
{
	UEdGraphNode* GraphNode = Pin->GetOwningNode();
	UEdGraph* Graph = GraphNode->GetGraph();
	UScriptStruct* StructType = CastChecked<UScriptStruct>(Pin->PinType.PinSubCategoryObject.Get(), ECastCheckedType::NullAllowed);
	if (!StructType)
	{
		if (CompilerContext)
		{
			CompilerContext->MessageLog.Error(TEXT("No structure in SubCategoryObject in pin @@"), Pin);
		}
		StructType = GetFallbackStruct();
	}
	UK2Node* SplitPinNode = NULL;

	if (Pin->Direction == EGPD_Input)
	{
		if (UK2Node_MakeStruct::CanBeMade(StructType))
		{
			UK2Node_MakeStruct* MakeStructNode = (CompilerContext ? CompilerContext->SpawnIntermediateNode<UK2Node_MakeStruct>(GraphNode, SourceGraph) : NewObject<UK2Node_MakeStruct>(Graph));
			MakeStructNode->StructType = StructType;
			SplitPinNode = MakeStructNode;
		}
		else
		{
			const FString& MetaData = StructType->GetMetaData(TEXT("HasNativeMake"));
			const UFunction* Function = FindObject<UFunction>(NULL, *MetaData, true);

			UK2Node_CallFunction* CallFunctionNode = (CompilerContext ? CompilerContext->SpawnIntermediateNode<UK2Node_CallFunction>(GraphNode, SourceGraph) : NewObject<UK2Node_CallFunction>(Graph));
			CallFunctionNode->SetFromFunction(Function);
			SplitPinNode = CallFunctionNode;
		}
	}
	else
	{
		if (UK2Node_BreakStruct::CanBeBroken(StructType))
		{
			UK2Node_BreakStruct* BreakStructNode = (CompilerContext ? CompilerContext->SpawnIntermediateNode<UK2Node_BreakStruct>(GraphNode, SourceGraph) : NewObject<UK2Node_BreakStruct>(Graph));
			BreakStructNode->StructType = StructType;
			SplitPinNode = BreakStructNode;
		}
		else
		{
			const FString& MetaData = StructType->GetMetaData(TEXT("HasNativeBreak"));
			const UFunction* Function = FindObject<UFunction>(NULL, *MetaData, true);

			UK2Node_CallFunction* CallFunctionNode = (CompilerContext ? CompilerContext->SpawnIntermediateNode<UK2Node_CallFunction>(GraphNode, SourceGraph) : NewObject<UK2Node_CallFunction>(Graph));
			CallFunctionNode->SetFromFunction(Function);
			SplitPinNode = CallFunctionNode;
		}
	}

	SplitPinNode->AllocateDefaultPins();

	return SplitPinNode;
}

void UEdGraphSchema_K2::SplitPin(UEdGraphPin* Pin) const
{
	// Under some circumstances we can get here when PinSubCategoryObject is not set, so we just can't split the pin in that case
	UScriptStruct* StructType = Cast<UScriptStruct>(Pin->PinType.PinSubCategoryObject.Get());
	if (StructType == nullptr)
	{
		return;
	}

	UEdGraphNode* GraphNode = Pin->GetOwningNode();
	UK2Node* K2Node = Cast<UK2Node>(GraphNode);
	UEdGraph* Graph = CastChecked<UEdGraph>(GraphNode->GetOuter());

	GraphNode->Modify();
	Pin->Modify();

	Pin->bHidden = true;

	UK2Node* ProtoExpandNode = CreateSplitPinNode(Pin);
			
	for (UEdGraphPin* ProtoPin : ProtoExpandNode->Pins)
	{
		if (ProtoPin->Direction == Pin->Direction && !ProtoPin->bHidden)
		{
			const FString PinName = FString::Printf(TEXT("%s_%s"), *Pin->PinName, *ProtoPin->PinName);
			const FEdGraphPinType& ProtoPinType = ProtoPin->PinType;
			UEdGraphPin* SubPin = GraphNode->CreatePin(Pin->Direction, ProtoPinType.PinCategory, ProtoPinType.PinSubCategory, ProtoPinType.PinSubCategoryObject.Get(), ProtoPinType.bIsArray, false, PinName);

			if (K2Node != nullptr && K2Node->ShouldDrawCompact() && !Pin->ParentPin)
			{
				SubPin->PinFriendlyName = ProtoPin->GetDisplayName();
			}
			else
			{
				FFormatNamedArguments Arguments;
				Arguments.Add(TEXT("PinDisplayName"), Pin->GetDisplayName());
				Arguments.Add(TEXT("ProtoPinDisplayName"), ProtoPin->GetDisplayName());
				SubPin->PinFriendlyName = FText::Format(LOCTEXT("SplitPinFriendlyNameFormat", "{PinDisplayName} {ProtoPinDisplayName}"), Arguments);
			}

			SubPin->DefaultValue = ProtoPin->DefaultValue;
			SubPin->AutogeneratedDefaultValue = ProtoPin->AutogeneratedDefaultValue;

			SubPin->ParentPin = Pin;

			// CreatePin puts the Pin in the array, but we are going to insert it later, so pop it back out
			GraphNode->Pins.Pop(/*bAllowShrinking=*/ false);

			Pin->SubPins.Add(SubPin);
		}
	}

	ProtoExpandNode->DestroyNode();

	if (Pin->Direction == EGPD_Input)
	{
		TArray<FString> OriginalDefaults;
		if (   StructType == GetBaseStructure(TEXT("Vector"))
			|| StructType == GetBaseStructure(TEXT("Rotator")))
		{
			Pin->DefaultValue.ParseIntoArray(OriginalDefaults, TEXT(","), false);
			for (FString& Default : OriginalDefaults)
			{
				Default = FString::SanitizeFloat(FCString::Atof(*Default));
			}
			// In some cases (particularly wildcards) the default value may not accurately reflect the normal component elements
			while (OriginalDefaults.Num() < 3)
			{
				OriginalDefaults.Add(TEXT("0.0"));
			}
		}
		else if (StructType == GetBaseStructure(TEXT("Vector2D")))
		{
			FVector2D V2D;
			V2D.InitFromString(Pin->DefaultValue);

			OriginalDefaults.Add(FString::SanitizeFloat(V2D.X));
			OriginalDefaults.Add(FString::SanitizeFloat(V2D.Y));
		}
		else if (StructType == GetBaseStructure(TEXT("LinearColor")))
		{
			FLinearColor LC;
			LC.InitFromString(Pin->DefaultValue);

			OriginalDefaults.Add(FString::SanitizeFloat(LC.R));
			OriginalDefaults.Add(FString::SanitizeFloat(LC.G));
			OriginalDefaults.Add(FString::SanitizeFloat(LC.B));
			OriginalDefaults.Add(FString::SanitizeFloat(LC.A));
		}

		check(OriginalDefaults.Num() == 0 || OriginalDefaults.Num() == Pin->SubPins.Num());

		for (int32 SubPinIndex = 0; SubPinIndex < OriginalDefaults.Num(); ++SubPinIndex)
		{
			UEdGraphPin* SubPin = Pin->SubPins[SubPinIndex];
			SubPin->DefaultValue = OriginalDefaults[SubPinIndex];
		}
	}

	GraphNode->Pins.Insert(Pin->SubPins, GraphNode->Pins.Find(Pin) + 1);

	Graph->NotifyGraphChanged();

	UBlueprint* Blueprint = FBlueprintEditorUtils::FindBlueprintForGraphChecked(Graph);
	FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
}

void UEdGraphSchema_K2::RecombinePin(UEdGraphPin* Pin) const
{
	UEdGraphNode* GraphNode = Pin->GetOwningNode();
	UEdGraphPin* ParentPin = Pin->ParentPin;

	GraphNode->Modify();
	ParentPin->Modify();

	ParentPin->bHidden = false;

	for (int32 SubPinIndex = 0; SubPinIndex < ParentPin->SubPins.Num(); ++SubPinIndex)
	{
		UEdGraphPin* SubPin = ParentPin->SubPins[SubPinIndex];

		if (SubPin->SubPins.Num() > 0)
		{
			RecombinePin(SubPin->SubPins[0]);
		}

		GraphNode->Pins.Remove(SubPin);
	}

	if (Pin->Direction == EGPD_Input)
	{
		UScriptStruct* StructType = CastChecked<UScriptStruct>(ParentPin->PinType.PinSubCategoryObject.Get());

		TArray<FString> OriginalDefaults;
		if (   StructType == GetBaseStructure(TEXT("Vector"))
			|| StructType == GetBaseStructure(TEXT("Rotator")))
		{
			ParentPin->DefaultValue = ParentPin->SubPins[0]->DefaultValue + TEXT(",") 
									+ ParentPin->SubPins[1]->DefaultValue + TEXT(",")
									+ ParentPin->SubPins[2]->DefaultValue;
		}
		else if (StructType == GetBaseStructure(TEXT("Vector2D")))
		{
			FVector2D V2D;
			V2D.X = FCString::Atof(*ParentPin->SubPins[0]->DefaultValue);
			V2D.Y = FCString::Atof(*ParentPin->SubPins[1]->DefaultValue);
			
			ParentPin->DefaultValue = V2D.ToString();
		}
		else if (StructType == GetBaseStructure(TEXT("LinearColor")))
		{
			FLinearColor LC;
			LC.R = FCString::Atof(*ParentPin->SubPins[0]->DefaultValue);
			LC.G = FCString::Atof(*ParentPin->SubPins[1]->DefaultValue);
			LC.B = FCString::Atof(*ParentPin->SubPins[2]->DefaultValue);
			LC.A = FCString::Atof(*ParentPin->SubPins[3]->DefaultValue);

			ParentPin->DefaultValue = LC.ToString();
		}
	}


	ParentPin->SubPins.Empty();

	UEdGraph* Graph = CastChecked<UEdGraph>(GraphNode->GetOuter());
	Graph->NotifyGraphChanged();

	UBlueprint* Blueprint = FBlueprintEditorUtils::FindBlueprintForGraphChecked(Graph);
	FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
}

void UEdGraphSchema_K2::OnPinConnectionDoubleCicked(UEdGraphPin* PinA, UEdGraphPin* PinB, const FVector2D& GraphPosition) const
{
	const FScopedTransaction Transaction(LOCTEXT("CreateRerouteNodeOnWire", "Create Reroute Node"));

	//@TODO: This constant is duplicated from inside of SGraphNodeKnot
	const FVector2D NodeSpacerSize(42.0f, 24.0f);
	const FVector2D KnotTopLeft = GraphPosition - (NodeSpacerSize * 0.5f);

	// Create a new knot
	UEdGraph* ParentGraph = PinA->GetOwningNode()->GetGraph();
	UK2Node_Knot* NewKnot = FEdGraphSchemaAction_K2NewNode::SpawnNodeFromTemplate<UK2Node_Knot>(ParentGraph, NewObject<UK2Node_Knot>(), KnotTopLeft);

	// Move the connections across (only notifying the knot, as the other two didn't really change)
	PinA->BreakLinkTo(PinB);
	PinA->MakeLinkTo((PinA->Direction == EGPD_Output) ? NewKnot->GetInputPin() : NewKnot->GetOutputPin());
	PinB->MakeLinkTo((PinB->Direction == EGPD_Output) ? NewKnot->GetInputPin() : NewKnot->GetOutputPin());
	NewKnot->PostReconstructNode();

	// Dirty the blueprint
	UBlueprint* Blueprint = FBlueprintEditorUtils::FindBlueprintForGraphChecked(CastChecked<UEdGraph>(ParentGraph));
	FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
}

void UEdGraphSchema_K2::ConfigureVarNode(UK2Node_Variable* InVarNode, FName InVariableName, UStruct* InVariableSource, UBlueprint* InTargetBlueprint)
{
	// See if this is a 'self context' (ie. blueprint class is owner (or child of owner) of dropped var class)
	if ((InVariableSource == NULL) || InTargetBlueprint->SkeletonGeneratedClass->IsChildOf(InVariableSource))
	{
		InVarNode->VariableReference.SetSelfMember(InVariableName);
	}
	else if (InVariableSource->IsA(UClass::StaticClass()))
	{
		InVarNode->VariableReference.SetExternalMember(InVariableName, CastChecked<UClass>(InVariableSource));
	}
	else
	{
		FGuid LocalVarGuid = FBlueprintEditorUtils::FindLocalVariableGuidByName(InTargetBlueprint, InVariableSource, InVariableName);
		if (LocalVarGuid.IsValid())
		{
			InVarNode->VariableReference.SetLocalMember(InVariableName, InVariableSource, LocalVarGuid);
		}
	}
}

UK2Node_VariableGet* UEdGraphSchema_K2::SpawnVariableGetNode(const FVector2D GraphPosition, class UEdGraph* ParentGraph, FName VariableName, UStruct* Source) const
{
	UK2Node_VariableGet* NodeTemplate = NewObject<UK2Node_VariableGet>();
	UBlueprint* Blueprint = FBlueprintEditorUtils::FindBlueprintForGraph(ParentGraph);

	UEdGraphSchema_K2::ConfigureVarNode(NodeTemplate, VariableName, Source, Blueprint);

	return FEdGraphSchemaAction_K2NewNode::SpawnNodeFromTemplate<UK2Node_VariableGet>(ParentGraph, NodeTemplate, GraphPosition);
}

UK2Node_VariableSet* UEdGraphSchema_K2::SpawnVariableSetNode(const FVector2D GraphPosition, class UEdGraph* ParentGraph, FName VariableName, UStruct* Source) const
{
	UK2Node_VariableSet* NodeTemplate = NewObject<UK2Node_VariableSet>();
	UBlueprint* Blueprint = FBlueprintEditorUtils::FindBlueprintForGraph(ParentGraph);

	UEdGraphSchema_K2::ConfigureVarNode(NodeTemplate, VariableName, Source, Blueprint);

	return FEdGraphSchemaAction_K2NewNode::SpawnNodeFromTemplate<UK2Node_VariableSet>(ParentGraph, NodeTemplate, GraphPosition);
}

UEdGraphPin* UEdGraphSchema_K2::DropPinOnNode(UEdGraphNode* InTargetNode, const FString& InSourcePinName, const FEdGraphPinType& InSourcePinType, EEdGraphPinDirection InSourcePinDirection) const
{
	UEdGraphPin* ResultPin = nullptr;
	if (UK2Node_EditablePinBase* EditablePinNode = Cast<UK2Node_EditablePinBase>(InTargetNode))
	{
		EditablePinNode->Modify();

		if (InSourcePinDirection == EGPD_Output && Cast<UK2Node_FunctionEntry>(InTargetNode))
		{
			if (UK2Node_EditablePinBase* ResultNode = FBlueprintEditorUtils::FindOrCreateFunctionResultNode(EditablePinNode))
			{
				EditablePinNode = ResultNode;
			}
			else
			{
				// If we did not successfully find or create a result node, just fail out
				return nullptr;
			}
		}
		else if (InSourcePinDirection == EGPD_Input && Cast<UK2Node_FunctionResult>(InTargetNode))
		{
			TArray<UK2Node_FunctionEntry*> FunctionEntryNode;
			InTargetNode->GetGraph()->GetNodesOfClass(FunctionEntryNode);

			if (FunctionEntryNode.Num() == 1)
			{
				EditablePinNode = FunctionEntryNode[0];
			}
			else
			{
				// If we did not successfully find the entry node, just fail out
				return nullptr;
			}
		}

		FString NewPinName = InSourcePinName;
		ResultPin = EditablePinNode->CreateUserDefinedPin(NewPinName, InSourcePinType, (InSourcePinDirection == EGPD_Input)? EGPD_Output : EGPD_Input);

		FParamsChangedHelper ParamsChangedHelper;
		ParamsChangedHelper.ModifiedBlueprints.Add(FBlueprintEditorUtils::FindBlueprintForNode(InTargetNode));
		FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(FBlueprintEditorUtils::FindBlueprintForNode(InTargetNode));

		ParamsChangedHelper.Broadcast(FBlueprintEditorUtils::FindBlueprintForNode(InTargetNode), EditablePinNode, InTargetNode->GetGraph());

		for (auto GraphIt = ParamsChangedHelper.ModifiedGraphs.CreateIterator(); GraphIt; ++GraphIt)
		{
			if(UEdGraph* ModifiedGraph = *GraphIt)
			{
				ModifiedGraph->NotifyGraphChanged();
			}
		}

		// Now update all the blueprints that got modified
		for (auto BlueprintIt = ParamsChangedHelper.ModifiedBlueprints.CreateIterator(); BlueprintIt; ++BlueprintIt)
		{
			if(UBlueprint* Blueprint = *BlueprintIt)
			{
				FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
				Blueprint->BroadcastChanged();
			}
		}
	}
	return ResultPin;
}

bool UEdGraphSchema_K2::SupportsDropPinOnNode(UEdGraphNode* InTargetNode, const FEdGraphPinType& InSourcePinType, EEdGraphPinDirection InSourcePinDirection, FText& OutErrorMessage) const
{
	bool bIsSupported = false;
	if (UK2Node_EditablePinBase* EditablePinNode = Cast<UK2Node_EditablePinBase>(InTargetNode))
	{
		if (InSourcePinDirection == EGPD_Output && Cast<UK2Node_FunctionEntry>(InTargetNode))
		{
			// Just check with the Function Entry and see if it's legal, we'll create/use a result node if the user drops
			bIsSupported = EditablePinNode->CanCreateUserDefinedPin(InSourcePinType, InSourcePinDirection, OutErrorMessage);

			if (bIsSupported)
			{
				OutErrorMessage = LOCTEXT("AddConnectResultNode", "Add Pin to Result Node");
			}
		}
		else if (InSourcePinDirection == EGPD_Input && Cast<UK2Node_FunctionResult>(InTargetNode))
		{
			// Just check with the Function Result and see if it's legal, we'll create/use a result node if the user drops
			bIsSupported = EditablePinNode->CanCreateUserDefinedPin(InSourcePinType, InSourcePinDirection, OutErrorMessage);

			if (bIsSupported)
			{
				OutErrorMessage = LOCTEXT("AddPinEntryNode", "Add Pin to Entry Node");
			}
		}
		else
		{
			bIsSupported = EditablePinNode->CanCreateUserDefinedPin(InSourcePinType, (InSourcePinDirection == EGPD_Input)? EGPD_Output : EGPD_Input, OutErrorMessage);
			if (bIsSupported)
			{
				OutErrorMessage = LOCTEXT("AddPinToNode", "Add Pin to Node");
			}
		}
	}
	return bIsSupported;
}

bool UEdGraphSchema_K2::IsCacheVisualizationOutOfDate(int32 InVisualizationCacheID) const
{
	return CurrentCacheRefreshID != InVisualizationCacheID;
}

int32 UEdGraphSchema_K2::GetCurrentVisualizationCacheID() const
{
	return CurrentCacheRefreshID;
}

void UEdGraphSchema_K2::ForceVisualizationCacheClear() const
{
	++CurrentCacheRefreshID;
}

/////////////////////////////////////////////////////

#undef LOCTEXT_NAMESPACE
