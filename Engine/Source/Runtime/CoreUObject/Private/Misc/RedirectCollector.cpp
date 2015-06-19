// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.


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
	FString ContainingPackage;
	if (ThreadContext.SerializedObject)
	{
		auto Linker = ThreadContext.SerializedObject->GetLinker();
		if (Linker)
		{
			ContainingPackage = Linker->Filename;
		}
	}
	StringAssetReferences.Add(InString, ContainingPackage);
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

void FRedirectCollector::ResolveStringAssetReference()
{
	while (StringAssetReferences.Num())
	{
		TMultiMap<FString, FString>::TIterator First(StringAssetReferences);
		FString ToLoad = First.Key();
		FString RefFilename = First.Value();
		First.RemoveCurrent();

		if (ToLoad.Len() > 0)
		{
			UE_LOG(LogRedirectors, Log, TEXT("String Asset Reference '%s'"), *ToLoad);
			StringAssetRefFilenameStack.Push(RefFilename);

			UObject *Loaded = LoadObject<UObject>(NULL, *ToLoad, NULL, LOAD_None, NULL);
			StringAssetRefFilenameStack.Pop();

			UObjectRedirector* Redirector = dynamic_cast<UObjectRedirector*>(Loaded);
			if (Redirector)
			{
				UE_LOG(LogRedirectors, Log, TEXT("    Found redir '%s'"), *Redirector->GetFullName());
				FRedirection Redir;
				Redir.PackageFilename = RefFilename;
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
				UE_LOG(LogRedirectors, Log, TEXT("    Resolved to '%s'"), *Dest);
				if (Dest != ToLoad)
				{
					StringAssetRemap.Add(ToLoad, Dest);
				}
			}
			else
			{
				UE_LOG(LogRedirectors, Log, TEXT("    Not Found!"));
			}
		}
	}
}


FRedirectCollector GRedirectCollector;


