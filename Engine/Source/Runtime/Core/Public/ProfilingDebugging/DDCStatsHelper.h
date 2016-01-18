#pragma once


class FDDCScopeStatHelper
{
private:
	FName TransactionGuid;
	double StartTime;
	bool bHasParent;
public:
	CORE_API FDDCScopeStatHelper(const TCHAR* CacheKey, const FName& FunctionName);

	CORE_API ~FDDCScopeStatHelper();

	CORE_API void AddTag(const FName& Tag, const FString& Value);

	CORE_API bool HasParent() const { return bHasParent; }
};

