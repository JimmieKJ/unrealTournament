// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

// Generates single if scope. It's condition checks context of given term.
struct FSafeContextScopedEmmitter
{
private:
	FStringOutputDevice& Body;
	bool bSafeContextUsed;
	const TCHAR* CurrentIndent;
public:
	FString GetAdditionalIndent() const
	{
		return bSafeContextUsed ? TEXT("\t") : FString();
	}

	bool IsSafeContextUsed() const
	{
		return bSafeContextUsed;
	}

	FSafeContextScopedEmmitter(FStringOutputDevice& InBody, const FBPTerminal* Term, FBlueprintCompilerCppBackend& CppBackend, const TCHAR* InCurrentIndent)
		: Body(InBody), bSafeContextUsed(false), CurrentIndent(InCurrentIndent)
	{
		TArray<FString> SafetyConditions;
		for (; Term; Term = Term->Context)
		{
			if (!Term->bIsStructContext && (Term->Type.PinSubCategory != TEXT("self")))
			{
				ensure(!Term->bIsLiteral);
				SafetyConditions.Add(CppBackend.TermToText(Term));
			}
		}

		if (SafetyConditions.Num())
		{
			bSafeContextUsed = true;
			Body += CurrentIndent;
			Body += TEXT("if (");
			for (int32 Iter = SafetyConditions.Num() - 1; Iter >= 0; --Iter)
			{
				Body += FString(TEXT("IsValid("));
				Body += SafetyConditions[Iter];
				Body += FString(TEXT(")"));
				if (Iter)
				{
					Body += FString(TEXT(" && "));
				}
			}
			Body += TEXT(")\n");
			Body += CurrentIndent;
			Body += TEXT("{\n");
		}
	}

	~FSafeContextScopedEmmitter()
	{
		if (bSafeContextUsed)
		{
			Body += CurrentIndent;
			Body += TEXT("}\n");
		}
	}
};

struct FEmitHelper
{
	static void ArrayToString(const TArray<FString>& Array, FString& String, const TCHAR* Separator)
	{
		if (Array.Num())
		{
			String += Array[0];
		}
		for (int32 i = 1; i < Array.Num(); ++i)
		{
			String += TEXT(", ");
			String += Array[i];
		}
	}

	static bool HasAllFlags(uint64 Flags, uint64 FlagsToCheck)
	{
		return FlagsToCheck == (Flags & FlagsToCheck);
	}

	static FString HandleRepNotifyFunc(const UProperty* Property)
	{
		check(Property);
		if (HasAllFlags(Property->PropertyFlags, CPF_Net | CPF_RepNotify))
		{
			return FString::Printf(TEXT("ReplicatedUsing=%s"), *Property->RepNotifyFunc.ToString());
		}
		else if (HasAllFlags(Property->PropertyFlags, CPF_Net))
		{
			return TEXT("Replicated");
		}
		return FString();
	}

	static bool MetaDataCanBeNative(const FName MetaDataName)
	{
		if (MetaDataName == TEXT("ModuleRelativePath"))
		{
			return false;
		}
		return true;
	}

	static FString HandleMetaData(const UField* Field, bool AddCategory = true)
	{
		FString MetaDataStr;

		check(Field);
		UPackage* Package = Field->GetOutermost();
		check(Package);
		const UMetaData* MetaData = Package->GetMetaData();
		check(MetaData);
		const TMap<FName, FString>* ValuesMap = MetaData->ObjectMetaDataMap.Find(Field);
		TArray<FString> MetaDataStrings;
		if (ValuesMap && ValuesMap->Num())
		{
			for (auto& Pair : *ValuesMap)
			{
				if (!MetaDataCanBeNative(Pair.Key))
				{
					continue;
				}
				if (!Pair.Value.IsEmpty())
				{
					FString Value = Pair.Value.Replace(TEXT("\n"), TEXT(""));
					MetaDataStrings.Emplace(FString::Printf(TEXT("%s=\"%s\""), *Pair.Key.ToString(), *Pair.Value));
				}
				else
				{
					MetaDataStrings.Emplace(Pair.Key.ToString());
				}
			}
		}
		if (AddCategory && (!ValuesMap || !ValuesMap->Find(TEXT("Category"))))
		{
			MetaDataStrings.Emplace(TEXT("Category"));
		}
		MetaDataStrings.Remove(FString());
		if (MetaDataStrings.Num())
		{
			MetaDataStr += TEXT("meta=(");
			ArrayToString(MetaDataStrings, MetaDataStr, TEXT(", "));
			MetaDataStr += TEXT(")");
		}
		return MetaDataStr;
	}

#ifdef HANDLE_CPF_TAG
	static_assert(false, "Macro HANDLE_CPF_TAG redefinition.");
#endif
#define HANDLE_CPF_TAG(TagName, CheckedFlags) if (HasAllFlags(Flags, (CheckedFlags))) { Tags.Emplace(TagName); }

	static TArray<FString> ProperyFlagsToTags(uint64 Flags)
	{
		TArray<FString> Tags;

		// EDIT FLAGS
		if (HasAllFlags(Flags, (CPF_Edit | CPF_EditConst | CPF_DisableEditOnInstance)))
		{
			Tags.Emplace(TEXT("VisibleDefaultsOnly"));
		}
		else if (HasAllFlags(Flags, (CPF_Edit | CPF_EditConst | CPF_DisableEditOnTemplate)))
		{
			Tags.Emplace(TEXT("VisibleInstanceOnly"));
		}
		else if (HasAllFlags(Flags, (CPF_Edit | CPF_EditConst)))
		{
			Tags.Emplace(TEXT("VisibleAnywhere"));
		}
		else if (HasAllFlags(Flags, (CPF_Edit | CPF_DisableEditOnInstance)))
		{
			Tags.Emplace(TEXT("EditDefaultsOnly"));
		}
		else if (HasAllFlags(Flags, (CPF_Edit | CPF_DisableEditOnTemplate)))
		{
			Tags.Emplace(TEXT("EditInstanceOnly"));
		}
		else if (HasAllFlags(Flags, (CPF_Edit)))
		{
			Tags.Emplace(TEXT("EditAnywhere"));
		}

		// BLUEPRINT EDIT
		if (HasAllFlags(Flags, (CPF_BlueprintVisible | CPF_BlueprintReadOnly)))
		{
			Tags.Emplace(TEXT("BlueprintReadOnly"));
		}
		else if (HasAllFlags(Flags, (CPF_BlueprintVisible)))
		{
			Tags.Emplace(TEXT("BlueprintReadWrite"));
		}

		// CONFIG
		if (HasAllFlags(Flags, (CPF_GlobalConfig | CPF_Config)))
		{
			Tags.Emplace(TEXT("GlobalConfig"));
		}
		else if (HasAllFlags(Flags, (CPF_Config)))
		{
			Tags.Emplace(TEXT("Config"));
		}

		// OTHER
		HANDLE_CPF_TAG(TEXT("Transient"), CPF_Transient)
			HANDLE_CPF_TAG(TEXT("DuplicateTransient"), CPF_DuplicateTransient)
			HANDLE_CPF_TAG(TEXT("TextExportTransient"), CPF_TextExportTransient)
			HANDLE_CPF_TAG(TEXT("NonPIEDuplicateTransient"), CPF_NonPIEDuplicateTransient)
			HANDLE_CPF_TAG(TEXT("Export"), CPF_ExportObject)
			HANDLE_CPF_TAG(TEXT("NoClear"), CPF_NoClear)
			HANDLE_CPF_TAG(TEXT("EditFixedSize"), CPF_EditFixedSize)
			HANDLE_CPF_TAG(TEXT("NotReplicated"), CPF_RepSkip)
			HANDLE_CPF_TAG(TEXT("Interp"), CPF_Edit | CPF_BlueprintVisible | CPF_Interp)
			HANDLE_CPF_TAG(TEXT("NonTransactional"), CPF_NonTransactional)
			HANDLE_CPF_TAG(TEXT("BlueprintAssignable"), CPF_BlueprintAssignable)
			HANDLE_CPF_TAG(TEXT("BlueprintCallable"), CPF_BlueprintCallable)
			HANDLE_CPF_TAG(TEXT("BlueprintAuthorityOnly"), CPF_BlueprintAuthorityOnly)
			HANDLE_CPF_TAG(TEXT("AssetRegistrySearchable"), CPF_AssetRegistrySearchable)
			HANDLE_CPF_TAG(TEXT("SimpleDisplay"), CPF_SimpleDisplay)
			HANDLE_CPF_TAG(TEXT("AdvancedDisplay"), CPF_AdvancedDisplay)
			HANDLE_CPF_TAG(TEXT("SaveGame"), CPF_SaveGame)

			//TODO:
			//HANDLE_CPF_TAG(TEXT("Instanced"), CPF_PersistentInstance | CPF_ExportObject | CPF_InstancedReference)

			return Tags;
	}

	static TArray<FString> FunctionFlagsToTags(uint64 Flags)
	{
		TArray<FString> Tags;

		//Pointless: BlueprintNativeEvent, BlueprintImplementableEvent
		//Pointless: CustomThunk ?

		//TODO: SealedEvent
		//TODO: Unreliable
		//TODO: ServiceRequest, ServiceResponse

		HANDLE_CPF_TAG(TEXT("Exec"), FUNC_Exec)
			HANDLE_CPF_TAG(TEXT("Server"), FUNC_Net | FUNC_NetServer)
			HANDLE_CPF_TAG(TEXT("Client"), FUNC_Net | FUNC_NetClient)
			HANDLE_CPF_TAG(TEXT("NetMulticast"), FUNC_Net | FUNC_NetMulticast)
			HANDLE_CPF_TAG(TEXT("Reliable"), FUNC_NetReliable)
			HANDLE_CPF_TAG(TEXT("BlueprintCallable"), FUNC_BlueprintCallable)
			HANDLE_CPF_TAG(TEXT("BlueprintPure"), FUNC_BlueprintCallable | FUNC_BlueprintPure)
			HANDLE_CPF_TAG(TEXT("BlueprintAuthorityOnly"), FUNC_BlueprintAuthorityOnly)
			HANDLE_CPF_TAG(TEXT("BlueprintCosmetic"), FUNC_BlueprintCosmetic)
			HANDLE_CPF_TAG(TEXT("WithValidation"), FUNC_NetValidate)

			return Tags;
	}

#undef HANDLE_CPF_TAG

	static bool IsBlueprintNativeEvent(uint64 FunctionFlags)
	{
		return HasAllFlags(FunctionFlags, FUNC_Event | FUNC_BlueprintEvent | FUNC_Native);
	}

	static bool IsBlueprintImplementableEvent(uint64 FunctionFlags)
	{
		return HasAllFlags(FunctionFlags, FUNC_Event | FUNC_BlueprintEvent) && !HasAllFlags(FunctionFlags, FUNC_Native);
	}

	static FString EmitUFuntion(UFunction* Function)
	{
		TArray<FString> Tags = FEmitHelper::FunctionFlagsToTags(Function->FunctionFlags);
		const bool bMustHaveCategory = (Function->FunctionFlags & (FUNC_BlueprintCallable | FUNC_BlueprintPure)) != 0;
		Tags.Emplace(FEmitHelper::HandleMetaData(Function, bMustHaveCategory));
		Tags.Remove(FString());

		FString AllTags;
		FEmitHelper::ArrayToString(Tags, AllTags, TEXT(", "));

		return FString::Printf(TEXT("UFUNCTION(%s)"), *AllTags);
	}

	static int32 ParseDelegateDetails(UFunction* Signature, FString& OutParametersMacro, FString& OutParamNumberStr)
	{
		int32 ParameterNum = 0;
		FStringOutputDevice Parameters;
		for (TFieldIterator<UProperty> PropIt(Signature); PropIt && (PropIt->PropertyFlags & CPF_Parm); ++PropIt)
		{
			Parameters += ", ";
			PropIt->ExportCppDeclaration(Parameters, EExportedDeclaration::MacroParameter, NULL, EPropertyExportCPPFlags::CPPF_CustomTypeName);
			++ParameterNum;
		}

		FString ParamNumberStr;
		switch (ParameterNum)
		{
		case 0: ParamNumberStr = TEXT("");				break;
		case 1: ParamNumberStr = TEXT("_OneParam");		break;
		case 2: ParamNumberStr = TEXT("_TwoParams");	break;
		case 3: ParamNumberStr = TEXT("_ThreeParams");	break;
		case 4: ParamNumberStr = TEXT("_FourParams");	break;
		case 5: ParamNumberStr = TEXT("_FiveParams");	break;
		case 6: ParamNumberStr = TEXT("_SixParams");	break;
		case 7: ParamNumberStr = TEXT("_SevenParams");	break;
		case 8: ParamNumberStr = TEXT("_EightParams");	break;
		default: ParamNumberStr = TEXT("_TooMany");		break;
		}

		OutParametersMacro = Parameters;
		OutParamNumberStr = ParamNumberStr;
		return ParameterNum;
	}

	static TArray<FString> EmitSinglecastDelegateDeclarations(const TArray<UDelegateProperty*>& Delegates)
	{
		TArray<FString> Results;
		for (auto It : Delegates)
		{
			check(It);
			auto Signature = It->SignatureFunction;
			check(Signature);

			FString ParamNumberStr, Parameters;
			ParseDelegateDetails(Signature, Parameters, ParamNumberStr);

			Results.Add(*FString::Printf(TEXT("DECLARE_DYNAMIC_DELEGATE%s(%s%s)"),
				*ParamNumberStr, *It->GetCPPType(NULL, EPropertyExportCPPFlags::CPPF_CustomTypeName), *Parameters));
		}
		return Results;
	}

	static TArray<FString> EmitMulticastDelegateDeclarations(UClass* SourceClass)
	{
		TArray<FString> Results;
		for (TFieldIterator<UMulticastDelegateProperty> It(SourceClass, EFieldIteratorFlags::ExcludeSuper); It; ++It)
		{
			auto Signature = It->SignatureFunction;
			check(Signature);

			FString ParamNumberStr, Parameters;
			ParseDelegateDetails(Signature, Parameters, ParamNumberStr);

			Results.Add(*FString::Printf(TEXT("DECLARE_DYNAMIC_MULTICAST_DELEGATE%s(%s%s)"),
				*ParamNumberStr, *It->GetCPPType(NULL, EPropertyExportCPPFlags::CPPF_CustomTypeName), *Parameters));
		}

		return Results;
	}

	static FString EmitLifetimeReplicatedPropsImpl(UClass* SourceClass, const FString& CppClassName, const TCHAR* InCurrentIndent)
	{
		FString Result;
		bool bFunctionInitilzed = false;
		for (TFieldIterator<UProperty> It(SourceClass, EFieldIteratorFlags::ExcludeSuper); It; ++It)
		{
			if ((It->PropertyFlags & CPF_Net) != 0)
			{
				if (!bFunctionInitilzed)
				{
					Result += FString::Printf(TEXT("%svoid %s::GetLifetimeReplicatedProps(TArray< FLifetimeProperty > & OutLifetimeProps) const\n"), InCurrentIndent, *CppClassName);
					Result += FString::Printf(TEXT("%s{\n"), InCurrentIndent);
					Result += FString::Printf(TEXT("%s\tSuper::GetLifetimeReplicatedProps(OutLifetimeProps);\n"), InCurrentIndent);
					bFunctionInitilzed = true;
				}
				Result += FString::Printf(TEXT("%s\tDOREPLIFETIME( %s, %s);\n"), InCurrentIndent, *CppClassName, *It->GetNameCPP());
			}
		}
		if (bFunctionInitilzed)
		{
			Result += FString::Printf(TEXT("%s}\n"), InCurrentIndent);
		}
		return Result;
	}

	static FString GatherHeadersToInclude(UClass* SourceClass, const TArray<FString>& PersistentHeaders)
	{
		TSet<FString> HeaderFiles;
		HeaderFiles.Append(PersistentHeaders);
		{
			TArray<UObject*> ReferencedObjects;
			{
				FReferenceFinder ReferenceFinder(ReferencedObjects, NULL, false, false, false, true);
				TArray<UObject*> ObjectsToCheck;
				GetObjectsWithOuter(SourceClass, ObjectsToCheck, true);
				//CDO?
				for (auto Obj : ObjectsToCheck)
				{
					ReferenceFinder.FindReferences(Obj);
				}
			}

			auto EngineSourceDir = FPaths::EngineSourceDir();
			auto GameSourceDir = FPaths::GameSourceDir();
			auto CurrentPackage = SourceClass->GetTypedOuter<UPackage>();
			for (auto Obj : ReferencedObjects)
			{
				auto Field = Cast<UField>(Obj);
				FString PackPath;
				if (Field
					&& (Field->GetTypedOuter<UPackage>() != CurrentPackage)
					&& FSourceCodeNavigation::FindClassHeaderPath(Field, PackPath))
				{
					if (!PackPath.RemoveFromStart(EngineSourceDir))
					{
						if (!PackPath.RemoveFromStart(GameSourceDir))
						{
							PackPath = FPaths::GetCleanFilename(PackPath);
						}
					}
					HeaderFiles.Add(PackPath);
				}
			}
		}

		FString Result;
		for (auto HeaderFile : HeaderFiles)
		{
			Result += FString::Printf(TEXT("#include \"%s\"\n"), *HeaderFile);
		}

		return Result;
	}
};

