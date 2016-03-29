// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Data.SqlClient;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace PostBuildInfoTool
{
	partial class Program
	{
		/// <summary>
		/// Update this string with a valid setting
		/// </summary>
		static readonly string SqlConnectionString;

		static int Main(string[] Args)
		{
			if(SqlConnectionString == null)
			{
				Console.WriteLine("PostBuildInfoTool was not compiled with a valid SqlConnectionString.");
				return 1;
			}

			if(Args.Length != 5 && Args.Length != 6)
			{
				Console.WriteLine("Syntax:");
				Console.WriteLine("  PostBuildInfoTool <ChangeNumber> <BuildType> <Success/Fail/Warning> <URL> [<ArchiveDepotPath>]");
				return 1;
			}

			using(SqlConnection Connection = new SqlConnection(SqlConnectionString))
			{
				Connection.Open();
				using (SqlCommand Command = new SqlCommand("INSERT INTO dbo.CIS (ChangeNumber, BuildType, Result, URL, Project, ArchivePath) VALUES (@ChangeNumber, @BuildType, @Result, @URL, @Project, @ArchivePath)", Connection))
				{
					Command.Parameters.AddWithValue("@ChangeNumber", Args[0]);
					Command.Parameters.AddWithValue("@BuildType", Args[1]);
					Command.Parameters.AddWithValue("@Result", Args[2]);
					Command.Parameters.AddWithValue("@URL", Args[3]);
					Command.Parameters.AddWithValue("@Project", Args[4]);
					Command.Parameters.AddWithValue("@ArchivePath", (Args.Length >= 6)? Args[5] : "");
					Command.ExecuteNonQuery();
				}
			}

			return 0;
		}
	}
}
