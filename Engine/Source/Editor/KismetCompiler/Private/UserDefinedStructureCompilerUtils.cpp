// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "KismetCompilerPrivatePCH.h"
#include "UserDefinedStructureCompilerUtils.h"
#include "KismetCompiler.h"
#include "Editor/UnrealEd/Public/Kismet2/StructureEditorUtils.h"
#include "Editor/UnrealEd/Public/Kismet2/BlueprintEditorUtils.h"
#include "Editor/UnrealEd/Public/EditorModes.h"
#include "Engine/UserDefinedStruct.h"
#include "UserDefinedStructure/UserDefinedStructEditorData.h"

#define LOCTEXT_NAMESPACE "StructureCompiler"

struct FUserDefinedStructureCompilerInner
{
	static void ReplaceStructWithTempDuplicate(
		UUserDefinedStruct* StructureToReinstance, 
		TSet<UBlueprint*>& BlueprintsToRecompile,
		TArray<UUserDefinedStruct*>& ChangedStructs)
	{
		if (StructureToReinstance)
		{
			UUserDefinedStruct* DuplicatedStruct = NULL;
			{
				const FString ReinstancedName = FString::Printf(TEXT("STRUCT_REINST_%s"), *StructureToReinstance->GetName());
				const FName UniqueName = MakeUniqueObjectName(GetTransientPackage(), UUserDefinedStruct::StaticClass(), FName(*ReinstancedName));

				TGuardValue<bool> IsDuplicatingClassForReinstancing(GIsDuplicatingClassForReinstancing, true);
				DuplicatedStruct = (UUserDefinedStruct*)StaticDuplicateObject(StructureToReinstance, GetTransientPackage(), *UniqueName.ToString(), ~RF_Transactional); 
			}

			DuplicatedStruct->Guid = StructureToReinstance->Guid;
			DuplicatedStruct->Bind();
			DuplicatedStruct->StaticLink(true);
			DuplicatedStruct->PrimaryStruct = StructureToReinstance;
			DuplicatedStruct->Status = EUserDefinedStructureStatus::UDSS_Duplicate;
			DuplicatedStruct->SetFlags(RF_Transient);
			DuplicatedStruct->AddToRoot();
			CastChecked<UUserDefinedStructEditorData>(DuplicatedStruct->EditorData)->RecreateDefaultInstance();

			for (auto StructProperty : TObjectRange<UStructProperty>(RF_ClassDefaultObject | RF_PendingKill))
			{
				if (StructProperty && (StructureToReinstance == StructProperty->Struct))
				{
					if (auto OwnerClass = Cast<UBlueprintGeneratedClass>(StructProperty->GetOwnerClass()))
					{
						if (UBlueprint* FoundBlueprint = Cast<UBlueprint>(OwnerClass->ClassGeneratedBy))
						{
							BlueprintsToRecompile.Add(FoundBlueprint);
							StructProperty->Struct = DuplicatedStruct;
						}
					}
					else if (auto OwnerStruct = Cast<UUserDefinedStruct>(StructProperty->GetOwnerStruct()))
					{
						check(OwnerStruct != DuplicatedStruct);
						const bool bValidStruct = (OwnerStruct->GetOutermost() != GetTransientPackage())
							&& !OwnerStruct->HasAnyFlags(RF_PendingKill)
							&& (EUserDefinedStructureStatus::UDSS_Duplicate != OwnerStruct->Status.GetValue());

						if (bValidStruct)
						{
							ChangedStructs.AddUnique(OwnerStruct);
							StructProperty->Struct = DuplicatedStruct;
						}
					}
					else
					{
						UE_LOG(LogK2Compiler, Error, TEXT("ReplaceStructWithTempDuplicate unknown owner"));
					}
				}
			}

			DuplicatedStruct->RemoveFromRoot();
		}
	}

	static void CleanAndSanitizeStruct(UUserDefinedStruct* StructToClean)
	{
		check(StructToClean);

		if (auto EditorData = Cast<UUserDefinedStructEditorData>(StructToClean->EditorData))
		{
			EditorData->CleanDefaultInstance();
		}

		const FString TransientString = FString::Printf(TEXT("TRASHSTRUCT_%s"), *StructToClean->GetName());
		const FName TransientName = MakeUniqueObjectName(GetTransientPackage(), UUserDefinedStruct::StaticClass(), FName(*TransientString));
		UUserDefinedStruct* TransientStruct = NewObject<UUserDefinedStruct>(GetTransientPackage(), TransientName, RF_Public | RF_Transient);

		TArray<UObject*> SubObjects;
		GetObjectsWithOuter(StructToClean, SubObjects, true);
		SubObjects.Remove(StructToClean->EditorData);
		for( auto SubObjIt = SubObjects.CreateIterator(); SubObjIt; ++SubObjIt )
		{
			UObject* CurrSubObj = *SubObjIt;
			CurrSubObj->Rename(NULL, TransientStruct, REN_DontCreateRedirectors);
			if( UProperty* Prop = Cast<UProperty>(CurrSubObj) )
			{
				FKismetCompilerUtilities::InvalidatePropertyExport(Prop);
			}
			else
			{
				FLinkerLoad::InvalidateExport(CurrSubObj);
			}
		}

		StructToClean->SetSuperStruct(NULL);
		StructToClean->Children = NULL;
		StructToClean->Script.Empty();
		StructToClean->MinAlignment = 0;
		StructToClean->RefLink = NULL;
		StructToClean->PropertyLink = NULL;
		StructToClean->DestructorLink = NULL;
		StructToClean->ScriptObjectReferences.Empty();
		StructToClean->PropertyLink = NULL;
		StructToClean->ErrorMessage.Empty();
	}

	static void LogError(UUserDefinedStruct* Struct, FCompilerResultsLog& MessageLog, const FString& ErrorMsg)
	{
		MessageLog.Error(*ErrorMsg);
		if (Struct && Struct->ErrorMessage.IsEmpty())
		{
			Struct->ErrorMessage = ErrorMsg;
		}
	}

	static void CreateVariables(UUserDefinedStruct* Struct, const class UEdGraphSchema_K2* Schema, FCompilerResultsLog& MessageLog)
	{
		check(Struct && Schema);

		//FKismetCompilerUtilities::LinkAddedProperty push property to begin, so we revert the order
		for (int32 VarDescIdx = FStructureEditorUtils::GetVarDesc(Struct).Num() - 1; VarDescIdx >= 0; --VarDescIdx)
		{
			FStructVariableDescription& VarDesc = FStructureEditorUtils::GetVarDesc(Struct)[VarDescIdx];
			VarDesc.bInvalidMember = true;

			FEdGraphPinType VarType = VarDesc.ToPinType();

			FString ErrorMsg;
			if(!FStructureEditorUtils::CanHaveAMemberVariableOfType(Struct, VarType, &ErrorMsg))
			{
				LogError(Struct, MessageLog, FString::Printf(*LOCTEXT("StructureGeneric_Error", "Structure: %s Error: %s").ToString(), *Struct->GetFullName(), *ErrorMsg));
				continue;
			}

			UProperty* NewProperty = FKismetCompilerUtilities::CreatePropertyOnScope(Struct, VarDesc.VarName, VarType, NULL, 0, Schema, MessageLog);
			if (NewProperty != NULL)
			{
				NewProperty->SetFlags(RF_LoadCompleted);
				FKismetCompilerUtilities::LinkAddedProperty(Struct, NewProperty);
			}
			else
			{
				LogError(Struct, MessageLog, FString::Printf(*LOCTEXT("VariableInvalidType_Error", "The variable %s declared in %s has an invalid type %s").ToString(),
					*VarDesc.VarName.ToString(), *Struct->GetName(), *UEdGraphSchema_K2::TypeToText(VarType).ToString()));
				continue;
			}

			NewProperty->SetPropertyFlags(CPF_Edit | CPF_BlueprintVisible);
			if (VarDesc.bDontEditoOnInstance)
			{
				NewProperty->SetPropertyFlags(CPF_DisableEditOnInstance);
			}
			if (VarDesc.bEnable3dWidget)
			{
				NewProperty->SetMetaData(FEdMode::MD_MakeEditWidget, TEXT("true"));
			}
			NewProperty->SetMetaData(TEXT("DisplayName"), *VarDesc.FriendlyName);
			NewProperty->SetMetaData(FBlueprintMetadata::MD_Tooltip, *VarDesc.ToolTip);
			NewProperty->RepNotifyFunc = NAME_None;

			if (!VarDesc.DefaultValue.IsEmpty())
			{
				NewProperty->SetMetaData(TEXT("MakeStructureDefaultValue"), *VarDesc.DefaultValue);
			}
			VarDesc.CurrentDefaultValue = VarDesc.DefaultValue;

			VarDesc.bInvalidMember = false;
		}
	}

	static void InnerCompileStruct(UUserDefinedStruct* Struct, const class UEdGraphSchema_K2* K2Schema, class FCompilerResultsLog& MessageLog)
	{
		check(Struct);
		const int32 ErrorNum = MessageLog.NumErrors;

		Struct->SetMetaData(FBlueprintMetadata::MD_Tooltip, *FStructureEditorUtils::GetTooltip(Struct));

		auto EditorData = CastChecked<UUserDefinedStructEditorData>(Struct->EditorData);
		Struct->SetSuperStruct(EditorData->NativeBase);

		CreateVariables(Struct, K2Schema, MessageLog);

		Struct->Bind();
		Struct->StaticLink(true);

		if (Struct->GetStructureSize() <= 0)
		{
			LogError(Struct, MessageLog, FString::Printf(*LOCTEXT("StructurEmpty_Error", "Structure '%s' is empty ").ToString(), *Struct->GetFullName()));
		}

		FString DefaultInstanceError;
		EditorData->RecreateDefaultInstance(&DefaultInstanceError);
		if (!DefaultInstanceError.IsEmpty())
		{
			LogError(Struct, MessageLog, DefaultInstanceError);
		}

		const bool bNoErrorsDuringCompilation = (ErrorNum == MessageLog.NumErrors);
		Struct->Status = bNoErrorsDuringCompilation ? EUserDefinedStructureStatus::UDSS_UpToDate : EUserDefinedStructureStatus::UDSS_Error;
	}

	static bool ShouldBeCompiled(const UUserDefinedStruct* Struct)
	{
		if (Struct && (EUserDefinedStructureStatus::UDSS_UpToDate == Struct->Status))
		{
			return false;
		}
		return true;
	}

	static void BuildDependencyMapAndCompile(const TArray<UUserDefinedStruct*>& ChangedStructs, FCompilerResultsLog& MessageLog)
	{
		struct FDependencyMapEntry
		{
			UUserDefinedStruct* Struct;
			TSet<UUserDefinedStruct*> StructuresToWaitFor;

			FDependencyMapEntry() : Struct(NULL) {}

			void Initialize(UUserDefinedStruct* ChangedStruct, const TArray<UUserDefinedStruct*>& AllChangedStructs) 
			{ 
				Struct = ChangedStruct;
				check(Struct);

				auto Schema = GetDefault<UEdGraphSchema_K2>();
				for (auto& VarDesc : FStructureEditorUtils::GetVarDesc(Struct))
				{
					auto StructType = Cast<UUserDefinedStruct>(VarDesc.SubCategoryObject.Get());
					if (StructType && (VarDesc.Category == Schema->PC_Struct) && AllChangedStructs.Contains(StructType))
					{
						StructuresToWaitFor.Add(StructType);
					}
				}
			}
		};

		TArray<FDependencyMapEntry> DependencyMap;
		for (auto Iter = ChangedStructs.CreateConstIterator(); Iter; ++Iter)
		{
			DependencyMap.Add(FDependencyMapEntry());
			DependencyMap.Last().Initialize(*Iter, ChangedStructs);
		}

		while (DependencyMap.Num())
		{
			int32 StructureToCompileIndex = INDEX_NONE;
			for (int32 EntryIndex = 0; EntryIndex < DependencyMap.Num(); ++EntryIndex)
			{
				if(0 == DependencyMap[EntryIndex].StructuresToWaitFor.Num())
				{
					StructureToCompileIndex = EntryIndex;
					break;
				}
			}
			check(INDEX_NONE != StructureToCompileIndex);
			UUserDefinedStruct* Struct = DependencyMap[StructureToCompileIndex].Struct;
			check(Struct);

			FUserDefinedStructureCompilerInner::CleanAndSanitizeStruct(Struct);
			FUserDefinedStructureCompilerInner::InnerCompileStruct(Struct, GetDefault<UEdGraphSchema_K2>(), MessageLog);

			DependencyMap.RemoveAtSwap(StructureToCompileIndex);

			for (auto EntryIter = DependencyMap.CreateIterator(); EntryIter; ++EntryIter)
			{
				(*EntryIter).StructuresToWaitFor.Remove(Struct);
			}
		}
	}
};

void FUserDefinedStructureCompilerUtils::CompileStruct(class UUserDefinedStruct* Struct, class FCompilerResultsLog& MessageLog, bool bForceRecompile)
{
	if (FStructureEditorUtils::UserDefinedStructEnabled() && Struct)
	{
		TSet<UBlueprint*> BlueprintsThatHaveBeenRecompiled;
		TSet<UBlueprint*> BlueprintsToRecompile;

		TArray<UUserDefinedStruct*> ChangedStructs; 

		if (FUserDefinedStructureCompilerInner::ShouldBeCompiled(Struct) || bForceRecompile)
		{
			ChangedStructs.Add(Struct);
		}

		for (int32 StructIdx = 0; StructIdx < ChangedStructs.Num(); ++StructIdx)
		{
			UUserDefinedStruct* ChangedStruct = ChangedStructs[StructIdx];
			if (ChangedStruct)
			{
				FStructureEditorUtils::BroadcastPreChange(ChangedStruct);
				FUserDefinedStructureCompilerInner::ReplaceStructWithTempDuplicate(ChangedStruct, BlueprintsToRecompile, ChangedStructs);
				ChangedStruct->Status = EUserDefinedStructureStatus::UDSS_Dirty;
			}
		}

		// COMPILE IN PROPER ORDER
		FUserDefinedStructureCompilerInner::BuildDependencyMapAndCompile(ChangedStructs, MessageLog);

		// UPDATE ALL THINGS DEPENDENT ON COMPILED STRUCTURES
		for (TObjectIterator<UK2Node> It(RF_Transient | RF_PendingKill | RF_ClassDefaultObject, true); It && ChangedStructs.Num(); ++It)
		{
			bool bReconstruct = false;

			UK2Node* Node = *It;

			if (Node && !Node->HasAnyFlags(RF_Transient | RF_PendingKill))
			{
				// If this is a struct operation node operation on the changed struct we must reconstruct
				if (UK2Node_StructOperation* StructOpNode = Cast<UK2Node_StructOperation>(Node))
				{
					UUserDefinedStruct* StructInNode = Cast<UUserDefinedStruct>(StructOpNode->StructType);
					if (StructInNode && ChangedStructs.Contains(StructInNode))
					{
						bReconstruct = true;
					}
				}
				if (!bReconstruct)
				{
					// Look through the nodes pins and if any of them are split and the type of the split pin is a user defined struct we need to reconstruct
					for (UEdGraphPin* Pin : Node->Pins)
					{
						if (Pin->SubPins.Num() > 0)
						{
							UUserDefinedStruct* StructType = Cast<UUserDefinedStruct>(Pin->PinType.PinSubCategoryObject.Get());
							if (StructType && ChangedStructs.Contains(StructType))
							{
								bReconstruct = true;
								break;
							}
						}

					}
				}
			}

			if (bReconstruct)
			{
				if (UBlueprint* FoundBlueprint = Node->GetBlueprint())
				{
					// The blueprint skeleton needs to be updated before we reconstruct the node
					// or else we may have member references that point to the old skeleton
					if (!BlueprintsThatHaveBeenRecompiled.Contains(FoundBlueprint))
					{
						BlueprintsThatHaveBeenRecompiled.Add(FoundBlueprint);
						BlueprintsToRecompile.Remove(FoundBlueprint);
						FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(FoundBlueprint);
					}
					Node->ReconstructNode();
				}
			}
		}

		for (auto BPIter = BlueprintsToRecompile.CreateIterator(); BPIter; ++BPIter)
		{
			FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(*BPIter);
		}

		for (auto ChangedStruct : ChangedStructs)
		{
			if (ChangedStruct)
			{
				FStructureEditorUtils::BroadcastPostChange(ChangedStruct);
				ChangedStruct->MarkPackageDirty();
			}
		}
	}
}

void FUserDefinedStructureCompilerUtils::DefaultUserDefinedStructs(UObject* Object, FCompilerResultsLog& MessageLog)
{
	if (Object && FStructureEditorUtils::UserDefinedStructEnabled())
	{
		for (TFieldIterator<UProperty> It(Object->GetClass()); It; ++It)
		{
			if (const UProperty* Property = (*It))
			{
				uint8* Mem = Property->ContainerPtrToValuePtr<uint8>(Object);
				if (!FStructureEditorUtils::Fill_MakeStructureDefaultValue(Property, Mem))
				{
					MessageLog.Warning(*FString::Printf(*LOCTEXT("MakeStructureDefaultValue_Error", "MakeStructureDefaultValue parsing error. Object: %s, Property: %s").ToString(),
						*Object->GetName(), *Property->GetName()));
				}
			}
		}
	}
}

#undef LOCTEXT_NAMESPACE