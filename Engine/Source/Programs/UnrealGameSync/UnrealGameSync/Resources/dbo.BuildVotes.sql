CREATE TABLE [dbo].[BuildVotes]
(
	[Id] INT NOT NULL PRIMARY KEY, 
    [Changelist] INT NOT NULL, 
    [User] NVARCHAR(128) NOT NULL, 
    [Vote] NCHAR(10) NOT NULL
)
