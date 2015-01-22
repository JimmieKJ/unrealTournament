// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once


/** Enum for the components of a version string. */
enum class EVersionComponent
{
	Major,					///< Major version increments introduce breaking API changes.
	Minor,					///< Minor version increments add additional functionality without breaking existing APIs.
	Patch,					///< Patch version increments fix existing functionality without changing the API.
	Changelist,				///< The pre-release field adds additional versioning through a series of comparable dotted strings or numbers.
	Branch,					///<
};


/** Components of a version string. */
enum class EVersionComparison
{
	Neither,
	First,
	Second,
};


/** Utility functions. */
class CORE_API FEngineVersion
{
public:

	/** Empty constructor. Initializes the version to 0.0.0-0. */
	FEngineVersion();

	/** Constructs a version from the given components. */
	FEngineVersion(uint16 InMajor, uint16 InMinor, uint16 InPatch, uint32 InChangelist, const FString &InBranch);

	/** Sets the version to the given values. */
	void Set(uint16 InMajor, uint16 InMinor, uint16 InPatch, uint32 InChangelist, const FString &InBranch);

	/** Returns the changelist number corresponding to this version. */
	uint32 GetChangelist() const;	

	/** Checks if the changelist number represents licensee changelist number. */
	bool IsLicenseeVersion() const;

	/** Clears the object. */
	void Empty();

	/** Returns whether the current version is empty. */
	bool IsEmpty() const;

	/** Returns whether the engine version is for a promoted build. */
	bool IsPromotedBuild() const; 

	/** Checks compatibility with another version object. */
	bool IsCompatibleWith(const FEngineVersion &Other) const;

	/** Generates a version string */
	FString ToString(EVersionComponent LastComponent = EVersionComponent::Branch) const;

	/** Parses a version object from a string. Returns true on success. */
	static bool Parse(const FString &Text, FEngineVersion &OutVersion);
	
	/** Returns the newest of two versions, and the component at which they differ */
	static EVersionComparison GetNewest(const FEngineVersion &First, const FEngineVersion &Second, EVersionComponent *OutComponent);

	/** Serialization function */
	friend CORE_API void operator<<(FArchive &Ar, FEngineVersion &Version);

private:

	/** Major version number. */
	uint16 Major;

	/** Minor version number. */
	uint16 Minor;

	/** Patch version number. */
	uint16 Patch;

	/** Changelist number. This is used to arbitrate when Major/Minor/Patch version numbers match. Use GetChangelist() instead of using this member directly. */
	uint32 Changelist;

	/** Branch name. */
	FString Branch;
};


/** Global instance of the current engine version. */
CORE_API extern const FEngineVersion GEngineVersion;
