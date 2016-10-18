// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "CoreUObjectPrivate.h"

#include "StringAssetReference.h"
#include "PropertyTag.h"

FStringAssetReference::FStringAssetReference(const UObject* InObject)
{
	if (InObject)
	{
		SetPath(InObject->GetPathName());
	}
}

PRAGMA_DISABLE_DEPRECATION_WARNINGS
FStringAssetReference::~FStringAssetReference()
{

}

const FString& FStringAssetReference::ToString() const
{
	return AssetLongPathname;
}

void FStringAssetReference::SetPath(FString Path)
{
	if (Path.IsEmpty())
	{
		// Empty path, just empty the pathname.
		AssetLongPathname.Empty();
	}
	else if (FPackageName::IsShortPackageName(Path))
	{
		// Short package name. Try to expand it.
		AssetLongPathname = FPackageName::GetNormalizedObjectPath(Path); 
	}
	else if (Path[0] != '/')
	{
		// Possibly an ExportText path. Trim the ClassName.
		AssetLongPathname = FPackageName::ExportTextPathToObjectPath(Path);
	}
	else
	{
		// Normal object path. Fast path set directly.
		AssetLongPathname = MoveTemp(Path);
	}
}
PRAGMA_POP

bool FStringAssetReference::Serialize(FArchive& Ar)
{
#if WITH_EDITOR
	if (Ar.IsSaving() && Ar.IsPersistent() && FCoreUObjectDelegates::StringAssetReferenceSaving.IsBound())
	{
		SetPath(FCoreUObjectDelegates::StringAssetReferenceSaving.Execute(ToString()));
	}
#endif // WITH_EDITOR
	Ar << *this;
#if WITH_EDITOR
	if (Ar.IsLoading() && Ar.IsPersistent() && FCoreUObjectDelegates::StringAssetReferenceLoaded.IsBound())
	{
		FCoreUObjectDelegates::StringAssetReferenceLoaded.Execute(ToString());
	}
#endif // WITH_EDITOR

	if (Ar.IsLoading() && (Ar.GetPortFlags()&PPF_DuplicateForPIE))
	{
		// Remap unique ID if necessary
		// only for fixing up cross-level references, inter-level references handled in FDuplicateDataReader
		FixupForPIE();
	}

	return true;
}
bool FStringAssetReference::operator==(FStringAssetReference const& Other) const
{
	return ToString() == Other.ToString();
}
void FStringAssetReference::operator=(FStringAssetReference const& Other)
{
	SetPath(Other.ToString());
}
bool FStringAssetReference::ExportTextItem(FString& ValueStr, FStringAssetReference const& DefaultValue, UObject* Parent, int32 PortFlags, UObject* ExportRootScope) const
{
	if (0 != (PortFlags & EPropertyPortFlags::PPF_ExportCpp))
	{
		return false;
	}

	if (IsValid())
	{
		ValueStr += ToString();
	}
	else
	{
		ValueStr += TEXT("None");
	}
	return true;
}
bool FStringAssetReference::ImportTextItem(const TCHAR*& Buffer, int32 PortFlags, UObject* Parent, FOutputDevice* ErrorText)
{
	FString ImportedPath = TEXT("");
	const TCHAR* NewBuffer = UPropertyHelpers::ReadToken(Buffer, ImportedPath, 1);
	if (!NewBuffer)
	{
		return false;
	}
	Buffer = NewBuffer;
	if (ImportedPath == TEXT("None"))
	{
		ImportedPath = TEXT("");
	}
	else
	{
		if (*Buffer == TCHAR('\''))
		{
			NewBuffer = UPropertyHelpers::ReadToken(Buffer, ImportedPath, 1);
			if (!NewBuffer)
			{
				return false;
			}
			Buffer = NewBuffer;
			if (*Buffer++ != TCHAR('\''))
			{
				return false;
			}
		}
	}

	SetPath(MoveTemp(ImportedPath));

#if WITH_EDITOR
	// Consider this a load, so Config string asset references get cooked
	if (FCoreUObjectDelegates::StringAssetReferenceLoaded.IsBound())
	{
		FCoreUObjectDelegates::StringAssetReferenceLoaded.Execute(ToString());
	}
#endif // WITH_EDITOR

	return true;
}

#include "StringReferenceTemplates.h"

bool FStringAssetReference::SerializeFromMismatchedTag(struct FPropertyTag const& Tag, FArchive& Ar)
{
	struct UObjectTypePolicy
	{
		typedef UObject Type;
		static const FName FORCEINLINE GetTypeName() { return NAME_ObjectProperty; }
	};

	FString Path = ToString();

	bool bReturn = SerializeFromMismatchedTagTemplate<UObjectTypePolicy>(Path, Tag, Ar);

	if (Ar.IsLoading())
	{
		SetPath(MoveTemp(Path));
	}

	return bReturn;
}

UObject* FStringAssetReference::TryLoad() const
{
	UObject* LoadedObject = nullptr;

	if ( IsValid() )
	{
		LoadedObject = LoadObject<UObject>(nullptr, *ToString());
		while (UObjectRedirector* Redirector = Cast<UObjectRedirector>(LoadedObject))
		{
			LoadedObject = Redirector->DestinationObject;
		}
	}

	return LoadedObject;
}

UObject* FStringAssetReference::ResolveObject() const
{
	// Don't try to resolve if we're saving a package because StaticFindObject can't be used here
	// and we usually don't want to force references to weak pointers while saving.
	if (!IsValid() || GIsSavingPackage)
	{
		return nullptr;
	}

	UObject* FoundObject = FindObject<UObject>(nullptr, *ToString());
	while (UObjectRedirector* Redirector = Cast<UObjectRedirector>(FoundObject))
	{
		FoundObject = Redirector->DestinationObject;
	}

	return FoundObject;
}

FStringAssetReference FStringAssetReference::GetOrCreateIDForObject(const class UObject *Object)
{
	check(Object);
	return FStringAssetReference(Object);
}

void FStringAssetReference::SetPackageNamesBeingDuplicatedForPIE(const TArray<FString>& InPackageNamesBeingDuplicatedForPIE)
{
	PackageNamesBeingDuplicatedForPIE = InPackageNamesBeingDuplicatedForPIE;
}

void FStringAssetReference::ClearPackageNamesBeingDuplicatedForPIE()
{
	PackageNamesBeingDuplicatedForPIE.Empty();
}

void FStringAssetReference::FixupForPIE()
{
	if (FPlatformProperties::HasEditorOnlyData() && IsValid())
	{
		FString Path = ToString();

		// Determine if this reference has already been fixed up for PIE
		const FString ShortPackageOuterAndName = FPackageName::GetLongPackageAssetName(Path);
		if (!ShortPackageOuterAndName.StartsWith(PLAYWORLD_PACKAGE_PREFIX))
		{
			check(GPlayInEditorID != -1);

			FString PIEPath = FString::Printf(TEXT("%s/%s_%d_%s"), *FPackageName::GetLongPackagePath(Path), PLAYWORLD_PACKAGE_PREFIX, GPlayInEditorID, *ShortPackageOuterAndName);

			// Determine if this refers to a package that is being duplicated for PIE
			for (auto PackageNameIt = PackageNamesBeingDuplicatedForPIE.CreateConstIterator(); PackageNameIt; ++PackageNameIt)
			{
				const FString PathPrefix = (*PackageNameIt) + TEXT(".");
				if (PIEPath.StartsWith(PathPrefix))
				{
					// Need to prepend PIE prefix, as we're in PIE and this refers to an object in a PIE package
					SetPath(MoveTemp(PIEPath));

					break;
				}
			}
		}
	}
}

FThreadSafeCounter FStringAssetReference::CurrentTag(1);
TArray<FString> FStringAssetReference::PackageNamesBeingDuplicatedForPIE;