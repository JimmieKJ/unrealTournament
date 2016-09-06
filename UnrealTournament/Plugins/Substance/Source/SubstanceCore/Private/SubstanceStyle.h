#pragma once

class FSubstanceStyle
{
public:
	static void Initialize();
	static void Shutdown();
	static FName GetStyleSetName();
	static TSharedPtr<class ISlateStyle> Get() { return StyleSet; }
private:
	static FString InContent(const FString& RelativePath, const ANSICHAR* Extension);
private:
	static TSharedPtr<class FSlateStyleSet> StyleSet;
};

