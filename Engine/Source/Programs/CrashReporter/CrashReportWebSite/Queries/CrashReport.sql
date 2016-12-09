--MS SQL Maestro 12.9.0.7
------------------------------------------
--Host     : db-09
--Database : CrashReport


-- Dropping Table "dbo.Buggs"
IF EXISTS (SELECT name FROM sysobjects WHERE id = OBJECT_ID(N'dbo.Buggs') AND OBJECTPROPERTY(id, N'IsUserTable') = 1)
  DROP TABLE dbo.Buggs
GO

-- Dropping Table "dbo.Buggs_Crashes"
IF EXISTS (SELECT name FROM sysobjects WHERE id = OBJECT_ID(N'dbo.Buggs_Crashes') AND OBJECTPROPERTY(id, N'IsUserTable') = 1)
  DROP TABLE dbo.Buggs_Crashes
GO

-- Dropping Table "dbo.Buggs_UserGroups"
IF EXISTS (SELECT name FROM sysobjects WHERE id = OBJECT_ID(N'dbo.Buggs_UserGroups') AND OBJECTPROPERTY(id, N'IsUserTable') = 1)
  DROP TABLE dbo.Buggs_UserGroups
GO

-- Dropping Table "dbo.Buggs_Users"
IF EXISTS (SELECT name FROM sysobjects WHERE id = OBJECT_ID(N'dbo.Buggs_Users') AND OBJECTPROPERTY(id, N'IsUserTable') = 1)
  DROP TABLE dbo.Buggs_Users
GO

-- Dropping Table "dbo.CallStackPatterns"
IF EXISTS (SELECT name FROM sysobjects WHERE id = OBJECT_ID(N'dbo.CallStackPatterns') AND OBJECTPROPERTY(id, N'IsUserTable') = 1)
  DROP TABLE dbo.CallStackPatterns
GO

-- Dropping Table "dbo.Crashes"
IF EXISTS (SELECT name FROM sysobjects WHERE id = OBJECT_ID(N'dbo.Crashes') AND OBJECTPROPERTY(id, N'IsUserTable') = 1)
  DROP TABLE dbo.Crashes
GO

-- Dropping Table "dbo.CrashesHWM"
IF EXISTS (SELECT name FROM sysobjects WHERE id = OBJECT_ID(N'dbo.CrashesHWM') AND OBJECTPROPERTY(id, N'IsUserTable') = 1)
  DROP TABLE dbo.CrashesHWM
GO

-- Dropping Table "dbo.ErrorMessages"
IF EXISTS (SELECT name FROM sysobjects WHERE id = OBJECT_ID(N'dbo.ErrorMessages') AND OBJECTPROPERTY(id, N'IsUserTable') = 1)
  DROP TABLE dbo.ErrorMessages
GO

-- Dropping Table "dbo.FunctionCalls"
IF EXISTS (SELECT name FROM sysobjects WHERE id = OBJECT_ID(N'dbo.FunctionCalls') AND OBJECTPROPERTY(id, N'IsUserTable') = 1)
  DROP TABLE dbo.FunctionCalls
GO

-- Dropping Table "dbo.HistoricalCrashes"
IF EXISTS (SELECT name FROM sysobjects WHERE id = OBJECT_ID(N'dbo.HistoricalCrashes') AND OBJECTPROPERTY(id, N'IsUserTable') = 1)
  DROP TABLE dbo.HistoricalCrashes
GO

-- Dropping Table "dbo.sysdiagrams"
IF EXISTS (SELECT name FROM sysobjects WHERE id = OBJECT_ID(N'dbo.sysdiagrams') AND OBJECTPROPERTY(id, N'IsUserTable') = 1)
  DROP TABLE dbo.sysdiagrams
GO

-- Dropping Table "dbo.UserGroups"
IF EXISTS (SELECT name FROM sysobjects WHERE id = OBJECT_ID(N'dbo.UserGroups') AND OBJECTPROPERTY(id, N'IsUserTable') = 1)
  DROP TABLE dbo.UserGroups
GO

-- Dropping Table "dbo.Users"
IF EXISTS (SELECT name FROM sysobjects WHERE id = OBJECT_ID(N'dbo.Users') AND OBJECTPROPERTY(id, N'IsUserTable') = 1)
  DROP TABLE dbo.Users
GO

-- Dropping Table "dbo.Users"
IF EXISTS (SELECT name FROM sysobjects WHERE id = OBJECT_ID(N'dbo.Users') AND OBJECTPROPERTY(id, N'IsUserTable') = 1)
  DROP TABLE dbo.Users
GO

-- Creating Database "CrashReport"
CREATE DATABASE CrashReport
ON
(FILENAME = 'D:\MSSQL10_50.MSSQLSERVER\MSSQL\Data\CrashReport.mdf',
  NAME = [Buggr],
  SIZE = 65557MB,
  MAXSIZE = UNLIMITED,
  FILEGROWTH = 1MB)
LOG ON
(FILENAME = 'E:\MSSQL10_50.MSSQLSERVER\MSSQL\Data\CrashReport_1.LDF',
  NAME = [Buggr_log],
  SIZE = 3459MB,
  MAXSIZE = 4142MB,
  FILEGROWTH = 10%)
COLLATE SQL_Latin1_General_CP1_CI_AS
GO

USE master
GO

ALTER DATABASE [CrashReport]
SET
  MULTI_USER,
  ONLINE,
  READ_WRITE,
  CURSOR_DEFAULT GLOBAL,
  RECOVERY SIMPLE,
  PAGE_VERIFY CHECKSUM,
  PARAMETERIZATION SIMPLE,
  DISABLE_BROKER,
  CURSOR_CLOSE_ON_COMMIT OFF,
  AUTO_CLOSE OFF,
  AUTO_CREATE_STATISTICS ON,
  AUTO_SHRINK OFF,
  AUTO_UPDATE_STATISTICS ON,
  ANSI_NULL_DEFAULT OFF,
  ANSI_NULLS OFF,
  ANSI_PADDING OFF,
  ANSI_WARNINGS OFF,
  ARITHABORT OFF,
  CONCAT_NULL_YIELDS_NULL OFF,
  NUMERIC_ROUNDABORT OFF,
  QUOTED_IDENTIFIER OFF,
  RECURSIVE_TRIGGERS OFF,
  DB_CHAINING OFF,
  TRUSTWORTHY OFF,
  AUTO_UPDATE_STATISTICS_ASYNC OFF,
  SUPPLEMENTAL_LOGGING OFF,
  DATE_CORRELATION_OPTIMIZATION OFF,
  ENCRYPTION OFF
WITH NO_WAIT
GO

USE CrashReport
GO

USE CrashReport
GO

-- Creating Table "dbo.Buggs"
CREATE TABLE dbo.Buggs (
  Id                int IDENTITY(1, 1),
  TTPID             varchar(50),
  Pattern           varchar(800) NOT NULL,
  NumberOfCrashes   int,
  NumberOfUsers     int,
  TimeOfFirstCrash  datetime,
  TimeOfLastCrash   datetime,
  Status            varchar(64),
  FixedChangeList   varchar(50),
  CrashType         smallint,
  BuildVersion      varchar(64),
  PatternId         int,
  ErrorMessageId    int,
  /* Keys */
  CONSTRAINT PK_Buggs PRIMARY KEY (Id)
)
GO

CREATE INDEX IX_Pattern
  ON dbo.Buggs
  (Pattern)
GO

-- Creating Table "dbo.Buggs_Crashes"
CREATE TABLE dbo.Buggs_Crashes (
  BuggId   int,
  CrashId  int,
  /* Keys */
  CONSTRAINT PK_Buggs_Crashes PRIMARY KEY (BuggId, CrashId)
)
GO

-- Creating Table "dbo.Buggs_UserGroups"
CREATE TABLE dbo.Buggs_UserGroups (
  BuggId       int,
  UserGroupId  int,
  /* Keys */
  CONSTRAINT PK_Buggs_UserGroups PRIMARY KEY (BuggId, UserGroupId)
)
GO

-- Creating Table "dbo.Buggs_Users"
CREATE TABLE dbo.Buggs_Users (
  BuggId      int,
  UserName    varchar(64),
  UserNameId  int,
  /* Keys */
  CONSTRAINT PK_Buggs_Users PRIMARY KEY (BuggId, UserName)
)
GO

-- Creating Table "dbo.CallStackPatterns"
CREATE TABLE dbo.CallStackPatterns (
  id       int IDENTITY(1, 1),
  Pattern  varchar(800),
  /* Keys */
  CONSTRAINT PK_CallstackPatterns_PrimaryKey PRIMARY KEY (id)
)
GO

CREATE UNIQUE CLUSTERED INDEX CallStackPatterns_ClusteredIndex
  ON dbo.CallStackPatterns
  (Pattern)
GO

-- Creating Table "dbo.Crashes"
CREATE TABLE dbo.Crashes (
  Id                  int IDENTITY(1, 1),
  Title               nchar(20),
  Summary             varchar(512),
  GameName            varchar(64),
  Status              varchar(64),
  TimeOfCrash         datetime,
  ChangeListVersion   varchar(64),
  PlatformName        varchar(64),
  EngineMode          varchar(64),
  [Description]       varchar(1024),
  RawCallStack        varchar(max),
  Pattern             varchar(800),
  CommandLine         varchar(512),
  ComputerName        varchar(64),
  Selected            bit,
  FixedChangeList     varchar(64),
  LanguageExt         varchar(64),
  Module              varchar(128),
  BuildVersion        varchar(128),
  BaseDir             varchar(512),
  Version             int,
  UserName            varchar(64),
  TTPID               varchar(64),
  AutoReporterID      int,
  Processed           bit CONSTRAINT DF_Crashes_Processed DEFAULT 0,
  HasLogFile          bit,
  HasMiniDumpFile     bit,
  HasVideoFile        bit,
  HasDiagnosticsFile  bit CONSTRAINT DF_Crashes_HasDiagnosticsFile DEFAULT 0,
  HasNewLogFile       bit CONSTRAINT DF_Crashes_HasNewLogFile DEFAULT 0,
  Branch              varchar(32),
  CrashType           smallint NOT NULL,
  UserNameId          int,
  HasMetaData         bit,
  SourceContext       varchar(max),
  EpicAccountId       varchar(64),
  EngineVersion       varchar(64),
  UserActivityHint    varchar(512),
  InsertTS            datetime CONSTRAINT DF_Crashes_InsertTS DEFAULT getdate(),
  PatternId           int,
  ErrorMessageId      int,
  BuggId              int,
  ProcessFailed       bit,
  /* Keys */
  CONSTRAINT PK_Crashes PRIMARY KEY (Id)
)
GO

CREATE INDEX IX_Crashes_TimeOfCrash
  ON dbo.Crashes
  (TimeOfCrash)
GO

CREATE INDEX IX_Crashes_InsertTS
  ON dbo.Crashes
  (InsertTS)
GO

CREATE INDEX IX_PatternId
  ON dbo.Crashes
  (PatternId)
GO

CREATE INDEX IX_Buggs
  ON dbo.Crashes
  (BuggId DESC)
GO

CREATE INDEX IX_Crashes_CrashType
  ON dbo.Crashes
  (CrashType)
GO

CREATE INDEX IX_Crashes_UserNameId
  ON dbo.Crashes
  (UserNameId)
GO

CREATE TRIGGER dbo.CrashesTestTrigger ON dbo.Crashes
AFTER INSERT
AS
BEGIN
	-- SET NOCOUNT ON added to prevent extra result sets from
	-- interfering with SELECT statements.
	SET NOCOUNT ON;

    -- Insert statements for trigger here
               
     
	INSERT INTO [Buggr].[dbo].[Notes]
           (Text
           )
	Values 
	( 'new note')


END
GO

/* Changing trigger state */
DISABLE TRIGGER dbo.CrashesTestTrigger
  ON dbo.Crashes
GO

CREATE TRIGGER dbo.crashtest ON dbo.Crashes
AFTER INSERT, UPDATE, DELETE
AS
BEGIN
	-- SET NOCOUNT ON added to prevent extra result sets from
	-- interfering with SELECT statements.
	SET NOCOUNT ON;

    -- Insert statements for trigger here

END
GO

/* Changing trigger state */
DISABLE TRIGGER dbo.crashtest
  ON dbo.Crashes
GO

-- Creating Table "dbo.CrashesHWM"
CREATE TABLE dbo.CrashesHWM (
  ID           smallint NOT NULL IDENTITY(1, 1),
  LastRunTime  datetime NOT NULL
)
GO

-- Creating Table "dbo.ErrorMessages"
CREATE TABLE dbo.ErrorMessages (
  Id            int IDENTITY(1, 1),
  ErrorMessage  nvarchar(1600),
  /* Keys */
  PRIMARY KEY (Id)
)
GO

CREATE UNIQUE CLUSTERED INDEX ErrorMessages_Index01
  ON dbo.ErrorMessages
  (ErrorMessage)
GO

-- Creating Table "dbo.FunctionCalls"
CREATE TABLE dbo.FunctionCalls (
  Id    int IDENTITY(1, 1),
  Call  varchar(max),
  /* Keys */
  CONSTRAINT PK_FunctionCalls PRIMARY KEY (Id)
)
GO

-- Creating Table "dbo.HistoricalCrashes"
CREATE TABLE dbo.HistoricalCrashes (
  Id                  int IDENTITY(1, 1),
  Title               nchar(20),
  Summary             varchar(512),
  GameName            varchar(64),
  Status              varchar(64),
  TimeOfCrash         datetime,
  ChangeListVersion   varchar(64),
  PlatformName        varchar(64),
  EngineMode          varchar(64),
  [Description]       varchar(512),
  RawCallStack        varchar(max),
  Pattern             varchar(800),
  CommandLine         varchar(512),
  ComputerName        varchar(64),
  Selected            bit,
  FixedChangeList     varchar(64),
  LanguageExt         varchar(64),
  Module              varchar(128),
  BuildVersion        varchar(128),
  BaseDir             varchar(512),
  Version             int,
  UserName            varchar(64),
  TTPID               varchar(64),
  AutoReporterID      int,
  Processed           bit,
  HasLogFile          bit,
  HasMiniDumpFile     bit,
  HasVideoFile        bit,
  HasDiagnosticsFile  bit,
  HasNewLogFile       bit,
  Branch              varchar(32),
  CrashType           smallint NOT NULL,
  UserNameId          int,
  HasMetaData         bit,
  SourceContext       varchar(max),
  EpicAccountId       varchar(64),
  EngineVersion       varchar(64),
  UserActivityHint    varchar(512),
  InsertTS            datetime,
  /* Keys */
  CONSTRAINT PK_HistoricalCrashes PRIMARY KEY (Id)
)
GO

-- Creating Table "dbo.sysdiagrams"
CREATE TABLE dbo.sysdiagrams (
  [name]        sys.sysname NOT NULL,
  principal_id  int NOT NULL,
  diagram_id    int IDENTITY(1, 1),
  version       int,
  [definition]  varbinary(max),
  /* Keys */
  PRIMARY KEY (diagram_id), 
  CONSTRAINT UK_principal_name UNIQUE ([name], principal_id)
)
GO

EXEC sp_addextendedproperty
  N'microsoft_database_tools_support', N'1',
  N'SCHEMA', N'dbo',
  N'TABLE', N'sysdiagrams'
GO

-- Creating Table "dbo.UserGroups"
CREATE TABLE dbo.UserGroups (
  Id      int IDENTITY(1, 1),
  [Name]  varchar(64),
  /* Keys */
  CONSTRAINT PK_UserGroups PRIMARY KEY (Id)
)
GO

-- Creating Table "dbo.Users"
CREATE TABLE dbo.Users (
  Id           int IDENTITY(1, 1),
  UserName     nvarchar(64),
  UserGroupId  int NOT NULL,
  /* Keys */
  CONSTRAINT PK_Users PRIMARY KEY (Id)
)
GO



ALTER TABLE dbo.Buggs
  ADD CONSTRAINT Buggs_ErrorMessages_FK
  FOREIGN KEY (ErrorMessageId)
    REFERENCES dbo.ErrorMessages(Id)
    ON DELETE NO ACTION
    ON UPDATE NO ACTION
GO

ALTER TABLE dbo.Buggs
  ADD CONSTRAINT FK_Buggs_CallStackPatterns
  FOREIGN KEY (PatternId)
    REFERENCES dbo.CallStackPatterns(id)
    ON DELETE NO ACTION
    ON UPDATE NO ACTION
GO

ALTER TABLE dbo.Buggs_Crashes
  ADD CONSTRAINT FK_Buggs_Crashes_Buggs
  FOREIGN KEY (BuggId)
    REFERENCES dbo.Buggs(Id)
    ON DELETE NO ACTION
    ON UPDATE NO ACTION
GO

ALTER TABLE dbo.Buggs_Crashes
  ADD CONSTRAINT FK_Buggs_Crashes_Crashes
  FOREIGN KEY (CrashId)
    REFERENCES dbo.Crashes(Id)
    ON DELETE NO ACTION
    ON UPDATE NO ACTION
GO

ALTER TABLE dbo.Buggs_UserGroups
  ADD CONSTRAINT FK_Buggs_UserGroups_Buggs
  FOREIGN KEY (BuggId)
    REFERENCES dbo.Buggs(Id)
    ON DELETE NO ACTION
    ON UPDATE NO ACTION
GO

ALTER TABLE dbo.Buggs_UserGroups
  ADD CONSTRAINT FK_Buggs_UserGroups_UserGroups
  FOREIGN KEY (UserGroupId)
    REFERENCES dbo.UserGroups(Id)
    ON DELETE NO ACTION
    ON UPDATE NO ACTION
GO

ALTER TABLE dbo.Buggs_Users
  ADD CONSTRAINT FK_Buggs_Users_Users
  FOREIGN KEY (UserNameId)
    REFERENCES dbo.Users(Id)
    ON DELETE NO ACTION
    ON UPDATE NO ACTION
GO

ALTER TABLE dbo.Buggs_Users
  ADD CONSTRAINT FK_Buggs_Users_Users_ByName
  FOREIGN KEY (UserNameId)
    REFERENCES dbo.Users(Id)
    ON DELETE NO ACTION
    ON UPDATE NO ACTION
GO

ALTER TABLE dbo.Crashes
  ADD CONSTRAINT FK_Crashes_Buggs
  FOREIGN KEY (BuggId)
    REFERENCES dbo.Buggs(Id)
    ON DELETE NO ACTION
    ON UPDATE NO ACTION
GO

ALTER TABLE dbo.Crashes
  ADD CONSTRAINT FK_Crashes_CallStackPattern
  FOREIGN KEY (PatternId)
    REFERENCES dbo.CallStackPatterns(id)
    ON DELETE NO ACTION
    ON UPDATE NO ACTION
GO

ALTER TABLE dbo.Crashes
  ADD CONSTRAINT FK_Crashes_ErrorMessages
  FOREIGN KEY (ErrorMessageId)
    REFERENCES dbo.ErrorMessages(Id)
    ON DELETE NO ACTION
    ON UPDATE NO ACTION
GO

ALTER TABLE dbo.Crashes
  ADD CONSTRAINT FK_Crashes_Users
  FOREIGN KEY (UserNameId)
    REFERENCES dbo.Users(Id)
    ON DELETE NO ACTION
    ON UPDATE NO ACTION
GO

ALTER TABLE dbo.Users
  ADD CONSTRAINT FK_Users_UserGroups
  FOREIGN KEY (UserGroupId)
    REFERENCES dbo.UserGroups(Id)
    ON DELETE NO ACTION
    ON UPDATE NO ACTION
GO

