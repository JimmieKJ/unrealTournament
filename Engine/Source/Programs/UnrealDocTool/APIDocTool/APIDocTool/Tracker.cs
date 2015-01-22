// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace APIDocTool
{
	class Tracker : IDisposable
	{
		string Message;
		int Progress;
		int Total;
		DateTime NextUpdateTime;
		bool bShowFinished;

		public Tracker(string InMessage, int InTotal)
		{
			Message = InMessage;
			Console.WriteLine(Message);
			Total = InTotal;
			NextUpdateTime = DateTime.UtcNow + TimeSpan.FromSeconds(2);
		}

		public IEnumerable<int> Indices
		{
			get { for (int Idx = 0; Idx < Total; Idx++) { Update(Idx); yield return Idx; } }
		}

		public void Dispose()
		{
			if (bShowFinished)
			{
				Console.WriteLine("{0} ({1})", Message, Total);
			}
		}

		public void Update(int InProgress)
		{
			Progress = InProgress;
			if (DateTime.UtcNow > NextUpdateTime)
			{
				Console.WriteLine("{0} ({1}/{2})", Message, Progress + 1, Total);
				NextUpdateTime = DateTime.UtcNow + TimeSpan.FromSeconds(5);
				bShowFinished = true;
			}
		}
	}
}
