// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using System;
using System.IO;
using System.Text;
using System.Collections.Generic;


namespace AutoReporter
{

    /**
     * ReportFileData - a container representing the data that a crash dump contains and must be sent to the ReportService
     */
    class ReportFileData
    {
        public int Version;
        public string ComputerName;
        public string UserName;
        public string GameName;
        public string PlatformName;
        public string LanguageExt;
        public string TimeOfCrash;
        public string BuildVer;
        public string ChangelistVer;
        public string CommandLine;
        public string BaseDir;
        public string CallStack;
        public string EngineMode;

        /**
         * IsValid - checks if this is valid to be sent to ReportService
         * 
         * @return bool - true if successful
         */
        public bool IsValid(OutputLogFile logFile) 
        {
            if (Version != 0 && !ComputerName.Equals("") && !UserName.Equals("") && !GameName.Equals("") 
                && !LanguageExt.Equals("") && !TimeOfCrash.Equals("") && !BuildVer.Equals("") && !ChangelistVer.Equals("") 
                && !BaseDir.Equals("") && !CallStack.Equals("") && !EngineMode.Equals(""))
            {
                return true;
            }

            logFile.WriteLine("Errors detected reading report dump!");
            return false;
        }
    }

    class ReportFile
    {
		private static string currentDumpVersion = "3";
		private static bool _isMono = Environment.OSVersion.Platform == PlatformID.Unix;
		private static int _charLength = ReportFile._isMono ? 4 : 2;

		private char ReadCharacter(MemoryStream ms)
		{
			byte[] bytes = new byte[ReportFile._charLength];
			int count = 0;
			while (count < ReportFile._charLength)
			{
				int num = ms.ReadByte();
				bytes[count++] = (byte) num;
				if (count == ReportFile._charLength)
					break;
			}
			if (bytes.Length == 0)
				return char.MinValue;
			char[] chArray = ReportFile._charLength == 4 ? Encoding.UTF32.GetChars(bytes, 0, count) : Encoding.Unicode.GetChars(bytes, 0, count);
			return chArray.Length == 1 ? chArray[0] : char.MinValue;
		}
		
		private string GetNextString(MemoryStream ms, OutputLogFile logFile)
		{
			StringBuilder stringBuilder = new StringBuilder();
			try
			{
				while (true)
				{
					char ch = this.ReadCharacter(ms);
					if ((int) ch != 0)
						stringBuilder.Append(ch);
					else
						break;
				}
				string line = ((object) stringBuilder).ToString();
				logFile.WriteLine(line);
				return line;
			}
			catch (Exception ex)
			{
				logFile.WriteLine(ex.Message);
			}
			return string.Empty;
		}

        /**
         * ParseReportFile - extracts a ReportFileData from a crash dump file
         * 
         * @param filename - the crash dump
         * @param reportData - the container to fill
         * 
         * @return bool - true if successful
         */
        public bool ParseReportFile(string filename, ReportFileData reportData, OutputLogFile logFile)
        {
			logFile.WriteLine("");
			logFile.WriteLine("Parsing report dump " + filename);
			logFile.WriteLine("\n\n");
			MemoryStream ms = new MemoryStream();
			try
			{
				using (BinaryReader binaryReader = new BinaryReader((Stream) File.OpenRead(filename)))
				{
					while (true)
					{
						byte[] buffer = binaryReader.ReadBytes(4096);
						if (buffer != null && buffer.Length != 0)
							ms.Write(buffer, 0, buffer.Length);
						else
							break;
					}
				}
			}
			catch (Exception ex)
			{
				logFile.WriteLine(ex.Message);
				logFile.WriteLine("Failed to read report dump!");
				return false;
			}
			ms.Seek(0L, SeekOrigin.Begin);
			string nextString = this.GetNextString(ms, logFile);
			logFile.WriteLine("Version = " + nextString);
			if (nextString.Equals(ReportFile.currentDumpVersion))
			{
				logFile.WriteLine("Dump is current version: " + ReportFile.currentDumpVersion);
				reportData.Version = int.Parse(nextString);
				reportData.ComputerName = this.GetNextString(ms, logFile);
				reportData.UserName = this.GetNextString(ms, logFile);
				reportData.GameName = this.GetNextString(ms, logFile);
				reportData.PlatformName = this.GetNextString(ms, logFile);
				reportData.LanguageExt = this.GetNextString(ms, logFile);
				reportData.TimeOfCrash = this.GetNextString(ms, logFile);
				reportData.BuildVer = this.GetNextString(ms, logFile);
				reportData.ChangelistVer = this.GetNextString(ms, logFile);
				reportData.CommandLine = this.GetNextString(ms, logFile);
				reportData.BaseDir = this.GetNextString(ms, logFile);
				reportData.CallStack = this.GetNextString(ms, logFile);
				reportData.EngineMode = this.GetNextString(ms, logFile);
			}
			else if (nextString.Equals("2"))
			{
				logFile.WriteLine("Dump is old but supported. DumpVer=" + nextString + " CurVer=" + ReportFile.currentDumpVersion);
				reportData.Version = int.Parse(nextString);
				reportData.ComputerName = this.GetNextString(ms, logFile);
				reportData.UserName = this.GetNextString(ms, logFile);
				reportData.GameName = this.GetNextString(ms, logFile);
				reportData.LanguageExt = this.GetNextString(ms, logFile);
				reportData.TimeOfCrash = this.GetNextString(ms, logFile);
				reportData.BuildVer = this.GetNextString(ms, logFile);
				reportData.ChangelistVer = this.GetNextString(ms, logFile);
				reportData.CommandLine = this.GetNextString(ms, logFile);
				reportData.BaseDir = this.GetNextString(ms, logFile);
				reportData.CallStack = this.GetNextString(ms, logFile);
				reportData.EngineMode = this.GetNextString(ms, logFile);
			}
			else
			{
				logFile.WriteLine("Outdated dump version " + nextString + "! Current Version is " + ReportFile.currentDumpVersion);
				return false;
			}
			logFile.WriteLine("\n\n");
			return reportData.IsValid(logFile);
		}
    }
}
