SELECT [EngineMode]
FROM [CrashReport].[dbo].[Crashes]
where charindex('Game', [EngineMode]) = 0 and
	  charindex('Commandlet', [EngineMode]) = 0 and
	  charindex('Editor', [EngineMode]) = 0 and
	  charindex('Tool', [EngineMode]) = 0 and
	  charindex('Server', [EngineMode]) = 0 and
	  charindex('Unknown', [EngineMode]) = 0 
go


UPDATE [CrashReport].[dbo].[Crashes]
SET [EngineMode] = 'Unknown'
FROM [CrashReport].[dbo].[Crashes]
where charindex('Game', [EngineMode]) = 0 and
	  charindex('Commandlet', [EngineMode]) = 0 and
	  charindex('Editor', [EngineMode]) = 0 and
	  charindex('Tool', [EngineMode]) = 0 and
	  charindex('Server', [EngineMode]) = 0 and
	  charindex('Unknown', [EngineMode]) = 0 
