RunUAT buildcookrun -project=unrealtournament -cook -pak -stage -platform=win64 -AllMaps
xcopy /s Engine\Content\Localization\ICU UnrealTournament\Saved\StagedBuilds\WindowsNoEditor\Engine\Content\Localization\ICU\
xcopy /s Engine\Binaries\ThirdParty\ICU\icu4c-53_1\Win64\VS2013\*.dll UnrealTournament\Saved\StagedBuilds\WindowsNoEditor\Engine\Binaries\ThirdParty\ICU\icu4c-53_1\Win64\VS2013\
xcopy Engine\Binaries\Win64\UE4.exe UnrealTournament\Saved\StagedBuilds\WindowsNoEditor\Engine\Binaries\Win64\
xcopy Engine\Binaries\Win64\UE4-*.dll UnrealTournament\Saved\StagedBuilds\WindowsNoEditor\Engine\Binaries\Win64\
xcopy UnrealTournament\Binaries\Win64\UE4-UnrealTournament.dll UnrealTournament\Saved\StagedBuilds\WindowsNoEditor\UnrealTournament\Binaries\Win64\
xcopy /s UnrealTournament\Plugins\*UE4-*.dll UnrealTournament\Saved\StagedBuilds\WindowsNoEditor\UnrealTournament\Plugins\
xcopy /s UnrealTournament\Plugins\*.uplugin UnrealTournament\Saved\StagedBuilds\WindowsNoEditor\UnrealTournament\Plugins\
xcopy /s UnrealTournament\Plugins\*.uasset UnrealTournament\Saved\StagedBuilds\WindowsNoEditor\UnrealTournament\Plugins\
pause