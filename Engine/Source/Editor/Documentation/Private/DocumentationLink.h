// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#pragma once

class FDocumentationLink
{
public: 

	static FString GetUrlRoot()
	{
		FString Url;
		FUnrealEdMisc::Get().GetURL( TEXT("UDNDocsURL"), Url, true );

		if ( !Url.EndsWith( TEXT( "/" ) ) )
		{
			Url += TEXT( "/" );
		}

		return Url;
	}

	static FString GetHomeUrl()
	{
		return GetHomeUrl(FInternationalization::Get().GetCurrentCulture());
	}

	static FString GetHomeUrl(const FCultureRef& Culture)
	{
		FString Url;
		FUnrealEdMisc::Get().GetURL( TEXT("UDNURL"), Url, true );

		Url.ReplaceInline(TEXT("/INT/"), *FString::Printf(TEXT("/%s/"), *(Culture->GetUnrealLegacyThreeLetterISOLanguageName())));
		return Url;
	}

	static FString ToUrl(const FString& Link)
	{
		return ToUrl(Link, FInternationalization::Get().GetCurrentCulture());
	}

	static FString ToUrl(const FString& Link, const FCultureRef& Culture)
	{
		FString Path;
		FString Anchor;
		SplitLink( Link, Path, Anchor );

		const FString PartialPath = FString::Printf(TEXT("%s%s/index.html"), *(Culture->GetUnrealLegacyThreeLetterISOLanguageName()), *Path);
		
		return GetUrlRoot() + PartialPath + Anchor;
	}

	static FString ToFilePath( const FString& Link )
	{
		FInternationalization& I18N = FInternationalization::Get();

		FString FilePath = ToFilePath(Link, I18N.GetCurrentCulture());

		if (!FPaths::FileExists(FilePath))
		{
			const FCulturePtr FallbackCulture = I18N.GetCulture(TEXT("en"));
			if (FallbackCulture.IsValid())
			{
				const FString FallbackFilePath = ToFilePath(Link, FallbackCulture.ToSharedRef());
				if (FPaths::FileExists(FallbackFilePath))
				{
					FilePath = FallbackFilePath;
				}
			}
		}

		return FilePath;
	}

	static FString ToFilePath(const FString& Link, const FCultureRef& Culture)
	{
		FString Path;
		FString Anchor;
		SplitLink(Link, Path, Anchor);

		const FString PartialPath = FString::Printf(TEXT("%s%s/index.html"), *(Culture->GetUnrealLegacyThreeLetterISOLanguageName()), *Path);
		return FString::Printf(TEXT("%sDocumentation/HTML/%s"), *FPaths::ConvertRelativePathToFull(FPaths::EngineDir()), *PartialPath);
	}

	static FString ToFileUrl( const FString& Link )
	{
		FInternationalization& I18N = FInternationalization::Get();

		FCultureRef Culture = I18N.GetCurrentCulture();
		FString FilePath = ToFilePath(Link, Culture);

		if (!FPaths::FileExists(FilePath))
		{
			const FCulturePtr FallbackCulture = I18N.GetCulture(TEXT("en"));
			if (FallbackCulture.IsValid())
			{
				const FString FallbackFilePath = ToFilePath(Link, FallbackCulture.ToSharedRef());
				if (FPaths::FileExists(FallbackFilePath))
				{
					Culture = FallbackCulture.ToSharedRef();
				}
			}
		}

		return ToFileUrl(Link, Culture);
	}

	static FString ToFileUrl(const FString& Link, const FCultureRef& Culture)
	{
		FString Path;
		FString Anchor;
		SplitLink(Link, Path, Anchor);

		return FString::Printf(TEXT("file:///%s%s"), *ToFilePath(Link, Culture), *Anchor);
	}

	static FString ToSourcePath(const FString& Link)
	{
		FInternationalization& I18N = FInternationalization::Get();

		FString SourcePath = ToSourcePath(Link, I18N.GetCurrentCulture());

		if (!FPaths::FileExists(SourcePath))
		{
			const FCulturePtr FallbackCulture = I18N.GetCulture(TEXT("en"));
			if (FallbackCulture.IsValid())
			{
				const FString FallbackSourcePath = ToSourcePath(Link, FallbackCulture.ToSharedRef());
				if (FPaths::FileExists(FallbackSourcePath))
				{
					SourcePath = FallbackSourcePath;
				}
			}
		}

		return SourcePath;
	}

	static FString ToSourcePath(const FString& Link, const FCultureRef& Culture)
	{
		FString Path;
		FString Anchor;
		SplitLink( Link, Path, Anchor );

		const FString FullDirectoryPath = FPaths::EngineDir() + TEXT( "Documentation/Source" ) + Path + "/";

		const FString WildCard = FString::Printf(TEXT("%s*.%s.udn"), *FullDirectoryPath, *(Culture->GetUnrealLegacyThreeLetterISOLanguageName()));

		TArray<FString> Filenames;
		IFileManager::Get().FindFiles(Filenames, *WildCard, true, false);

		if (Filenames.Num() > 0)
		{
			return FullDirectoryPath + Filenames[0];
		}

		// Since the source file doesn't exist already make up a valid name for a new one
		FString Category = FPaths::GetBaseFilename(Link);
		return FString::Printf(TEXT("%s%s.%s.udn"), *FullDirectoryPath, *Category, *(Culture->GetUnrealLegacyThreeLetterISOLanguageName()));
	}

	static void SplitLink( const FString& Link, /*OUT*/ FString& Path, /*OUT*/ FString& Anchor )
	{
		FString CleanedLink = Link;
		CleanedLink.Trim();
		CleanedLink.TrimTrailing();

		if ( CleanedLink == TEXT("%ROOT%") )
		{
			Path.Empty();
			Anchor.Empty();
		}
		else
		{
			if ( !CleanedLink.Split( TEXT("#"), &Path, &Anchor ) )
			{
				Path = CleanedLink;
			}
			else if ( !Anchor.IsEmpty() )
			{
				Anchor = FString( TEXT("#") ) + Anchor;
			}

			if ( Anchor.EndsWith( TEXT("/") ) )
			{
				Anchor = Anchor.Left( Anchor.Len() - 1 );
			}

			if ( Path.EndsWith( TEXT("/") ) )
			{
				Path = Path.Left( Path.Len() - 1 );
			}

			if ( !Path.IsEmpty() && !Path.StartsWith( TEXT("/") ) )
			{
				Path = FString( TEXT("/") ) + Path;
			}
		}
	}
};
