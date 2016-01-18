// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "BlueprintCompilerCppBackendModulePrivatePCH.h"
#include "BlueprintCompilerCppBackendUtils.h"
#include "Animation/AnimBlueprintGeneratedClass.h"
#include "Animation/AnimClassData.h"

FString FBackendHelperAnim::AddHeaders(UClass* GeneratedClass)
{
	auto AnimClass = Cast<UAnimBlueprintGeneratedClass>(GeneratedClass);
	return AnimClass ? FString(TEXT("#include \"Animation/AnimClassData.h\"")) : FString();
}

void FBackendHelperAnim::CreateAnimClassData(FEmitterLocalContext& Context)
{
	if (auto AnimClass = Cast<UAnimBlueprintGeneratedClass>(Context.GetCurrentlyGeneratedClass()))
	{
		const FString LocalNativeName = Context.GenerateUniqueLocalName();
		Context.AddLine(FString::Printf(TEXT("auto %s = NewObject<UAnimClassData>(GetClass(), TEXT(\"AnimClassData\"));"), *LocalNativeName));

		auto AnimClassData = NewObject<UAnimClassData>(GetTransientPackage(), TEXT("AnimClassData"));
		AnimClassData->CopyFrom(AnimClass);

		auto ObjectArchetype = AnimClassData->GetArchetype();
		for (auto Property : TFieldRange<const UProperty>(UAnimClassData::StaticClass()))
		{
			FEmitDefaultValueHelper::OuterGenerate(Context, Property, LocalNativeName
				, reinterpret_cast<const uint8*>(AnimClassData)
				, reinterpret_cast<const uint8*>(ObjectArchetype)
				, FEmitDefaultValueHelper::EPropertyAccessOperator::Pointer);
		}

		Context.AddLine(FString::Printf(TEXT("CastChecked<UDynamicClass>(GetClass())->AnimClassImplementation = %s;"), *LocalNativeName));
	}
}

