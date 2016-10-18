using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace Tools.CrashReporter.CrashReportProcess
{
	static class ReportIndex
	{
		public static void WriteToFile()
		{
			try
			{
				if (File.Exists(Filepath))
				{
					File.Delete(Filepath + ".backup");
					File.Move(Filepath, Filepath + ".backup");
				}

				using (var Writer = File.CreateText(Filepath))
				{
					var Today = DateTime.UtcNow.Date;

					foreach (var Item in Index)
					{
						if (Today - Item.Value.Date <= Retention)
						{
							Writer.WriteLine(ItemToString(Item));
						}
					}
				}

				LastFlush = DateTime.UtcNow;
			}
			catch (Exception ex)
			{
				CrashReporterProcessServicer.WriteException(string.Format("Failed to write ReportIndex to {0}. Exception was: {1}", Filepath, ex));
				CrashReporterProcessServicer.StatusReporter.Alert(string.Format("Failed to write ReportIndex to {0}", Filepath));
			}
		}

		public static void ReadFromFile()
		{
			try
			{
				Index = new Dictionary<string, DateTime>();

				if (!File.Exists(Filepath))
				{
					if (File.Exists(Filepath + ".backup"))
					{
						CrashReporterProcessServicer.WriteFailure(string.Format("Failed to read ReportIndex from {0}. Attempting to read from {1}", Filepath, Filepath + ".backup"));
						CrashReporterProcessServicer.StatusReporter.Alert(string.Format("Failed to read ReportIndex from {0}. Using backup.", Filepath));
						File.Move(Filepath + ".backup", Filepath);
					}
					else
					{
						CrashReporterProcessServicer.WriteFailure(string.Format("Failed to read ReportIndex from {0}. Generating new one.", Filepath));
						File.Create(Filepath).Close();
					}
				}

				using (var Reader = File.OpenText(Filepath))
				{
					string ItemStringRaw;
					while ((ItemStringRaw = Reader.ReadLine()) != null)
					{
						string ItemString = ItemStringRaw.Trim();
						if (!string.IsNullOrWhiteSpace(ItemString))
						{
							KeyValuePair<string, DateTime> NewItem;
							if (!TryParseItemString(ItemString, out NewItem))
							{
								CrashReporterProcessServicer.WriteFailure(string.Format("Failed to read line from ReportIndex: {0}.", ItemString));
								continue;
							}
							Index.Add(NewItem.Key, NewItem.Value);
						}
					}
				}

				LastFlush = DateTime.UtcNow;
			}
			catch (Exception ex)
			{
				CrashReporterProcessServicer.WriteException(string.Format("Failed to read ReportIndex from {0}. Exception was: {1}", Filepath, ex));
				CrashReporterProcessServicer.StatusReporter.Alert(string.Format("Failed to read ReportIndex from {0}", Filepath));
			}
		}

		public static bool TryAddNewReport(string ReportKey)
		{
			if (ContainsReport(ReportKey))
			{
				return false;
			}

			Index.Add(ReportKey, DateTime.UtcNow);

			if (LastFlush.Date < DateTime.UtcNow.Date)
			{
				WriteToFile();
			}

			return true;
		}

		public static bool ContainsReport(string ReportKey)
		{
			return Index.ContainsKey(ReportKey);
		}

		public static bool TryRemoveReport(string ReportKey)
		{
			return Index.Remove(ReportKey);
		}

		private static string ItemToString(KeyValuePair<string, DateTime> Item)
		{
			return string.Format("\"{0}\",{1}", Item.Key, Item.Value.ToString("yyyy-MM-dd"));
		}

		private static bool TryParseItemString(string ItemString, out KeyValuePair<string, DateTime> OutItem)
		{
			OutItem = new KeyValuePair<string, DateTime>();

			var ItemStringParts = ItemString.Split(',');
			if (ItemStringParts.Length != 2) return false;

			var KeyString = ItemStringParts[0].Trim('\"');
			if (KeyString.Length != ItemStringParts[0].Length - 2) return false;

			var DateStringParts = ItemStringParts[1].Split('-');
			if (DateStringParts.Length != 3) return false;

			int Year, Month, Day;
			if (!int.TryParse(DateStringParts[0], out Year) ||
			    !int.TryParse(DateStringParts[1], out Month) ||
			    !int.TryParse(DateStringParts[2], out Day))
			{
				return false;
			}

			DateTime ValueDate = new DateTime(Year, Month, Day, 0, 0, 0, DateTimeKind.Utc);

			OutItem = new KeyValuePair<string, DateTime>(KeyString, ValueDate);
			return true;
		}

		public static string Filepath { get; set; }
		public static TimeSpan Retention { get; set; }

		private static Dictionary<string, DateTime> Index = new Dictionary<string, DateTime>();
		private static DateTime LastFlush = DateTime.MinValue;
	}
}
