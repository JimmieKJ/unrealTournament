// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "EnginePrivate.h"

#include "Engine/MemberReference.h"

#if WITH_EDITOR
#include "Kismet2/BlueprintEditorUtils.h"
#endif

//////////////////////////////////////////////////////////////////////////
// FMemberReference

void FMemberReference::SetExternalMember(FName InMemberName, TSubclassOf<class UObject> InMemberParentClass)
{
	MemberName = InMemberName;
#if WITH_EDITOR
	MemberParent = (InMemberParentClass != nullptr) ? InMemberParentClass->GetAuthoritativeClass() : nullptr;
#else
	MemberParent = *InMemberParentClass;
#endif
	MemberScope.Empty();
	bSelfContext = false;
	bWasDeprecated = false;
}

void FMemberReference::SetGlobalField(FName InFieldName, UPackage* InParentPackage)
{
	MemberName = InFieldName;
	MemberParent = InParentPackage;
	MemberScope.Empty();
	bSelfContext = false;
	bWasDeprecated = false;
}

void FMemberReference::SetExternalDelegateMember(FName InMemberName)
{
	SetExternalMember(InMemberName, nullptr);
}

void FMemberReference::SetSelfMember(FName InMemberName)
{
	MemberName = InMemberName;
	MemberParent = NULL;
	MemberScope.Empty();
	bSelfContext = true;
	bWasDeprecated = false;
}

void FMemberReference::SetDirect(const FName InMemberName, const FGuid InMemberGuid, TSubclassOf<class UObject> InMemberParentClass, bool bIsConsideredSelfContext)
{
	MemberName = InMemberName;
	MemberGuid = InMemberGuid;
	bSelfContext = bIsConsideredSelfContext;
	bWasDeprecated = false;
	MemberParent = InMemberParentClass;
	MemberScope.Empty();
}

void FMemberReference::SetGivenSelfScope(const FName InMemberName, const FGuid InMemberGuid, TSubclassOf<class UObject> InMemberParentClass, TSubclassOf<class UObject> SelfScope) const
{
	MemberName = InMemberName;
	MemberGuid = InMemberGuid;
	MemberParent = (InMemberParentClass != nullptr) ? InMemberParentClass->GetAuthoritativeClass() : nullptr;
	MemberScope.Empty();

	// SelfScope should always be valid, but if it's not ensure and move on, the node will be treated as if it's not self scoped.
	ensure(SelfScope);
	bSelfContext = SelfScope && ((SelfScope->IsChildOf(InMemberParentClass)) || (SelfScope->ClassGeneratedBy == InMemberParentClass->ClassGeneratedBy));
	bWasDeprecated = false;

	if (bSelfContext)
	{
		MemberParent = NULL;
	}
}

void FMemberReference::SetLocalMember(FName InMemberName, UStruct* InScope, const FGuid InMemberGuid)
{
	SetLocalMember(InMemberName, InScope->GetName(), InMemberGuid);
}

void FMemberReference::SetLocalMember(FName InMemberName, FString InScopeName, const FGuid InMemberGuid)
{
	MemberName = InMemberName;
	MemberScope = InScopeName;
	MemberGuid = InMemberGuid;
	bSelfContext = false;
}

void FMemberReference::InvalidateScope()
{
	if( IsSelfContext() )
	{
		MemberParent = NULL;
	}
	else if(IsLocalScope())
	{
		MemberScope.Empty();

		// Make it into a member reference since we are clearing the local context
		bSelfContext = true;
	}
}

#if WITH_EDITOR
FName FMemberReference::RefreshLocalVariableName(UClass* InSelfScope) const
{
	TArray<UBlueprint*> Blueprints;
	UBlueprint::GetBlueprintHierarchyFromClass(InSelfScope, Blueprints);

	FName RenamedMemberName = NAME_None;
	for (int32 BPIndex = 0; BPIndex < Blueprints.Num(); ++BPIndex)
	{
		RenamedMemberName = FBlueprintEditorUtils::FindLocalVariableNameByGuid(Blueprints[BPIndex], MemberGuid);
		if (RenamedMemberName != NAME_None)
		{
			MemberName = RenamedMemberName;
			break;
		}
	}
	return RenamedMemberName;
}

TMap<FFieldRemapInfo, FFieldRemapInfo> FMemberReference::FieldRedirectMap;
TMultiMap<UClass*, FParamRemapInfo> FMemberReference::ParamRedirectMap;

bool FMemberReference::bFieldRedirectMapInitialized = false;

bool FMemberReference::FindReplacementFieldName(UClass* Class, FName FieldName, FFieldRemapInfo& RemapInfo)
{
	check(Class != NULL);
	InitFieldRedirectMap();

	// Reset the property remap info
	RemapInfo = FFieldRemapInfo();

	FFieldRemapInfo OldField;
	OldField.FieldClass = Class->GetFName();
	OldField.FieldName = FieldName;

	FFieldRemapInfo* NewFieldInfoPtr = FieldRedirectMap.Find(OldField);
	if (NewFieldInfoPtr != NULL)
	{
		RemapInfo = *NewFieldInfoPtr;
		return true;
	}
	else
	{
		return false;
	}
}

void FMemberReference::InitFieldRedirectMap()
{
	if (!bFieldRedirectMapInitialized)
	{
		if (GConfig)
		{
			FConfigSection* PackageRedirects = GConfig->GetSectionPrivate( TEXT("/Script/Engine.Engine"), false, true, GEngineIni );
			for (FConfigSection::TIterator It(*PackageRedirects); It; ++It)
			{
				if (It.Key() == TEXT("K2FieldRedirects"))
				{
					FString OldFieldPathString;
					FString NewFieldPathString;

					FParse::Value( *It.Value(), TEXT("OldFieldName="), OldFieldPathString );
					FParse::Value( *It.Value(), TEXT("NewFieldName="), NewFieldPathString );

					// Handle both cases of just a field being renamed (just one FName), as well as a class and field name (ClassName.FieldName)
					FFieldRemapInfo OldFieldRemap;
					{
						TArray<FString> OldFieldPath;
						OldFieldPathString.ParseIntoArray(OldFieldPath, TEXT("."), true);

						if (OldFieldPath.Num() == 1)
						{
							// Only the new property name is specified
							OldFieldRemap.FieldName = FName(*OldFieldPath[0]);
						}
						else if (OldFieldPath.Num() == 2)
						{
							// Property name and new class are specified
							OldFieldRemap.FieldClass = FName(*OldFieldPath[0]);
							OldFieldRemap.FieldName = FName(*OldFieldPath[1]);
						}
					}

					// Handle both cases of just a field being renamed (just one FName), as well as a class and field name (ClassName.FieldName)
					FFieldRemapInfo NewFieldRemap;
					{
						TArray<FString> NewFieldPath;
						NewFieldPathString.ParseIntoArray(NewFieldPath, TEXT("."), true);

						if( NewFieldPath.Num() == 1 )
						{
							// Only the new property name is specified
							NewFieldRemap.FieldName = FName(*NewFieldPath[0]);
						}
						else if( NewFieldPath.Num() == 2 )
						{
							// Property name and new class are specified
							NewFieldRemap.FieldClass = FName(*NewFieldPath[0]);
							NewFieldRemap.FieldName = FName(*NewFieldPath[1]);
						}
					}

					FieldRedirectMap.Add(OldFieldRemap, NewFieldRemap);
				}			
				if (It.Key() == TEXT("K2ParamRedirects"))
				{
					FName NodeName = NAME_None;
					FName OldParam = NAME_None;
					FName NewParam = NAME_None;
					FName NodeTitle = NAME_None;

					FString OldParamValues;
					FString NewParamValues;
					FString CustomValueMapping;

					FParse::Value( *It.Value(), TEXT("NodeName="), NodeName );
					FParse::Value( *It.Value(), TEXT("NodeTitle="), NodeTitle );
					FParse::Value( *It.Value(), TEXT("OldParamName="), OldParam );
					FParse::Value( *It.Value(), TEXT("NewParamName="), NewParam );
					FParse::Value( *It.Value(), TEXT("OldParamValues="), OldParamValues );
					FParse::Value( *It.Value(), TEXT("NewParamValues="), NewParamValues );
					FParse::Value( *It.Value(), TEXT("CustomValueMapping="), CustomValueMapping );

					FParamRemapInfo ParamRemap;
					ParamRemap.NewParam = NewParam;
					ParamRemap.OldParam = OldParam;
					ParamRemap.NodeTitle = NodeTitle;
					ParamRemap.bCustomValueMapping = (FCString::Stricmp(*CustomValueMapping,TEXT("true")) == 0);

					if (CustomValueMapping.Len() > 0 && !ParamRemap.bCustomValueMapping)
					{
						UE_LOG(LogBlueprint, Warning, TEXT("Value other than true specified for CustomValueMapping for '%s' node param redirect '%s' to '%s'."), *(NodeName.ToString()), *(OldParam.ToString()), *(NewParam.ToString()));
					}

					TArray<FString> OldParamValuesList;
					TArray<FString> NewParamValuesList;
					OldParamValues.ParseIntoArray(OldParamValuesList, TEXT(";"), false);
					NewParamValues.ParseIntoArray(NewParamValuesList, TEXT(";"), false);

					if (OldParamValuesList.Num() != NewParamValuesList.Num())
					{
						UE_LOG(LogBlueprint, Warning, TEXT("Unequal lengths for old and new param values for '%s' node param redirect '%s' to '%s'."), *(NodeName.ToString()), *(OldParam.ToString()), *(NewParam.ToString()));
					}

					if (CustomValueMapping.Len() > 0 && (OldParamValuesList.Num() > 0 || NewParamValuesList.Num() > 0))
					{
						UE_LOG(LogBlueprint, Warning, TEXT("Both Custom and Automatic param value remapping specified for '%s' node param redirect '%s' to '%s'.  Only Custom will be applied."), *(NodeName.ToString()), *(OldParam.ToString()), *(NewParam.ToString()));
					}

					for (int32 i = FMath::Min(OldParamValuesList.Num(), NewParamValuesList.Num()) - 1; i >= 0; --i)
					{
						int32 CurSize = ParamRemap.ParamValueMap.Num();
						ParamRemap.ParamValueMap.Add(OldParamValuesList[i], NewParamValuesList[i]);
						if (CurSize == ParamRemap.ParamValueMap.Num())
						{
							UE_LOG(LogBlueprint, Warning, TEXT("Duplicate old param value '%s' for '%s' node param redirect '%s' to '%s'."), *(OldParamValuesList[i]), *(NodeName.ToString()), *(OldParam.ToString()), *(NewParam.ToString()));
						}
					}

					// load class
					static UClass* K2NodeClass = StaticLoadClass(UObject::StaticClass(), nullptr, TEXT("BlueprintGraph.K2Node"));
					if (K2NodeClass)
					{
						UClass* NodeClass = StaticLoadClass(K2NodeClass, nullptr, *NodeName.ToString());
						if (NodeClass )
						{
							ParamRedirectMap.Add(NodeClass, ParamRemap);
						}
					}
				}			
			}

			bFieldRedirectMapInitialized = true;
		}
	}
}

UField* FMemberReference::FindRemappedField(UClass* InitialScope, FName InitialName, bool bInitialScopeMustBeOwnerOfField)
{
	FFieldRemapInfo NewFieldInfo;

	bool bFoundReplacement = false;
	
	// Step up the class chain to check if us or any of our parents specify a redirect
	UClass* TestRemapClass = InitialScope;
	while( TestRemapClass != NULL )
	{
		if( FindReplacementFieldName(TestRemapClass, InitialName, NewFieldInfo) )
		{
			// Found it, stop our search
			bFoundReplacement = true;
			break;
		}

		TestRemapClass = TestRemapClass->GetSuperClass();
	}

	// In the case of a bifurcation of a variable (e.g. moved from a parent into certain children), verify that we don't also define the variable in the current scope first
	if( bFoundReplacement && (FindField<UField>(InitialScope, InitialName) != nullptr))
	{
		bFoundReplacement = false;		
	}

	if( bFoundReplacement )
	{
		const FName NewFieldName = NewFieldInfo.FieldName;
		UClass* SearchClass = (NewFieldInfo.FieldClass != NAME_None) ? (UClass*)StaticFindObject(UClass::StaticClass(), ANY_PACKAGE, *NewFieldInfo.FieldClass.ToString()) : (UClass*)TestRemapClass;

		// Find the actual field specified by the redirector, so we can return it and update the node that uses it
		UField* NewField = FindField<UField>(SearchClass, NewFieldInfo.FieldName);
		if( NewField != NULL )
		{
			if (bInitialScopeMustBeOwnerOfField && !InitialScope->IsChildOf(SearchClass))
			{
				UE_LOG(LogBlueprint, Log, TEXT("UK2Node:  Unable to update field. Remapped field '%s' in not owned by given scope. Scope: '%s', Owner: '%s'."), *InitialName.ToString(), *InitialScope->GetName(), *NewFieldInfo.FieldClass.ToString());
			}
			else
			{
				UE_LOG(LogBlueprint, Log, TEXT("UK2Node:  Fixed up old field '%s' to new name '%s' on class '%s'."), *InitialName.ToString(), *NewFieldInfo.FieldName.ToString(), *SearchClass->GetName());
				return NewField;
			}
		}
		else if (SearchClass != NULL)
		{
			UE_LOG(LogBlueprint, Log, TEXT("UK2Node:  Unable to find updated field name for '%s' on class '%s'."), *InitialName.ToString(), *SearchClass->GetName());
		}
		else
		{
			UE_LOG(LogBlueprint, Log, TEXT("UK2Node:  Unable to find updated field name for '%s' on unknown class '%s'."), *InitialName.ToString(), *NewFieldInfo.FieldClass.ToString());
		}
	}

	return NULL;
}

#endif