#pragma once

class ICustomSlashCommand
{
public:
	virtual void ExecuteCommand() = 0;
	virtual FString GetCommandToken() = 0;
	virtual bool IsEnabled() = 0;

	virtual ~ICustomSlashCommand(){};
};
