// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.


// Core includes.
#include "CoreUObjectPrivate.h"
#include "UObject/UObjectThreadContext.h"

DEFINE_LOG_CATEGORY_STATIC(LogRedirectors, Log, All);

static TArray<FString> StringAssetRefFilenameStack;

void FRedirectCollector::OnRedirectorFollowed(const FString& InString, UObject* InObject)
{
	check(InObject);

	// the object had better be a redir
	UObjectRedirector& Redirector = dynamic_cast<UObjectRedirector&>(*InObject);

	// save the info if it matches the parameters we need to match on:
	// if the string matches the package we were loading
	// AND
	// we aren't matching a particular package || the string matches the package
	if (!FileToFixup.Len() || FileToFixup == FPackageName::FilenameToLongPackageName(Redirector.GetLinker()->Filename))
	{
		FRedirection Redir;
		Redir.PackageFilename = InString;
		Redir.RedirectorName = Redirector.GetFullName();
		Redir.RedirectorPackageFilename = Redirector.GetLinker()->Filename;
		Redir.DestinationObjectName = Redirector.DestinationObject->GetFullName();
		// we only want one of each redirection reported
		Redirections.AddUnique(Redir);

		// and add a fake reference caused by a string redirectors
		if (StringAssetRefFilenameStack.Num())
		{
			Redir.PackageFilename = StringAssetRefFilenameStack[StringAssetRefFilenameStack.Num() - 1];
			UE_LOG(LogRedirectors, Verbose, TEXT("String Asset Reference fake redir %s(%s) -> %s"), *Redir.RedirectorName, *Redir.PackageFilename, *Redir.DestinationObjectName);
			if (!Redir.PackageFilename.IsEmpty())
			{
				Redirections.AddUnique(Redir);
			}
		}
	}
}

void FRedirectCollector::OnStringAssetReferenceLoaded(const FString& InString)
{
	FUObjectThreadContext& ThreadContext = FUObjectThreadContext::Get();
	FPackagePropertyPair ContainingPackageAndProperty;

	if (InString.IsEmpty())
	{
		// No need to track empty strings
		return;
	}

	if (ThreadContext.SerializedObject)
	{
		FLinkerLoad* Linker = ThreadContext.SerializedObject->GetLinker();
		if (Linker)
		{
			ContainingPackageAndProperty.Package = Linker->Filename;
			if (Linker->GetSerializedProperty())
			{
				ContainingPackageAndProperty.Property = FString::Printf(TEXT("%s:%s"), *ThreadContext.SerializedObject->GetPathName(), *Linker->GetSerializedProperty()->GetName());
			}
#if WITH_EDITORONLY_DATA
			ContainingPackageAndProperty.bReferencedByEditorOnlyProperty = Linker->IsEditorOnlyPropertyOnTheStack();
#endif
		}
	}
	StringAssetReferences.AddUnique(InString, ContainingPackageAndProperty);
}

FString FRedirectCollector::OnStringAssetReferenceSaved(const FString& InString)
{
	FString* Found = StringAssetRemap.Find(InString);
	if (Found)
	{
		return *Found;
	}
	return InString;
}

void FRedirectCollector::ResolveStringAssetReference(FString FilterPackage)
{
	TMultiMap<FString, FPackagePropertyPair> SkippedReferences;
	while (StringAssetReferences.Num())
	{
		TMultiMap<FString, FPackagePropertyPair>::TIterator First(StringAssetReferences);
		FString ToLoad = First.Key();
		FPackagePropertyPair RefFilenameAndProperty = First.Value();
		First.RemoveCurrent();
		
		if (FCoreDelegates::LoadStringAssetReferenceInCook.IsBound())
		{
			if (FCoreDelegates::LoadStringAssetReferenceInCook.Execute(ToLoad) == false)
			{
				// Skip this reference
				continue;
			}
		}

		if (!FilterPackage.IsEmpty() && FilterPackage != RefFilenameAndProperty.Package)
		{
			// If we have a valid filter and it doesn't match, skip this reference
			SkippedReferences.Add(ToLoad, RefFilenameAndProperty);
			continue;
		}

		if (ToLoad.Len() > 0)
		{
			UE_LOG(LogRedirectors, Verbose, TEXT("String Asset Reference '%s'"), *ToLoad);
			UE_CLOG(RefFilenameAndProperty.Property.Len(), LogRedirectors, Verbose, TEXT("    Referenced by '%s'"), *RefFilenameAndProperty.Property);

			StringAssetRefFilenameStack.Push(RefFilenameAndProperty.Package);

			UObject *Loaded = LoadObject<UObject>(NULL, *ToLoad, NULL, RefFilenameAndProperty.bReferencedByEditorOnlyProperty ? LOAD_EditorOnly : LOAD_None, NULL);
			StringAssetRefFilenameStack.Pop();

			UObjectRedirector* Redirector = dynamic_cast<UObjectRedirector*>(Loaded);
			if (Redirector)
			{
				UE_LOG(LogRedirectors, Verbose, TEXT("    Found redir '%s'"), *Redirector->GetFullName());
				FRedirection Redir;
				Redir.PackageFilename = RefFilenameAndProperty.Package;
				Redir.RedirectorName = Redirector->GetFullName();
				Redir.RedirectorPackageFilename = Redirector->GetLinker()->Filename;
				CA_SUPPRESS(28182)
				Redir.DestinationObjectName = Redirector->DestinationObject->GetFullName();
				Redirections.AddUnique(Redir);
				Loaded = Redirector->DestinationObject;
			}
			if (Loaded)
			{
				FString Dest = Loaded->GetPathName();
				UE_LOG(LogRedirectors, Verbose, TEXT("    Resolved to '%s'"), *Dest);
				if (Dest != ToLoad)
				{
					StringAssetRemap.Add(ToLoad, Dest);
				}
			}
			else
			{
				const FString Referencer = RefFilenameAndProperty.Property.Len() ? RefFilenameAndProperty.Property : TEXT("Unknown");
				UE_LOG(LogRedirectors, Warning, TEXT("String Asset Reference '%s' was not found! (Referencer '%s')"), *ToLoad, *Referencer);
			}
		}
	}

	// Add any skipped references back into the map for the next time this is called
	StringAssetReferences = SkippedReferences;
}


FRedirectCollector GRedirectCollector;


