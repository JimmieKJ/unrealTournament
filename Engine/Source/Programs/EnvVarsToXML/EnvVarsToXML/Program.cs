using System;
using System.Collections;
using System.Collections.Generic;
using System.IO;
using System.Xml;
using System.Xml.Serialization;
using System.Text;

namespace EnvVarsToXML
{
	[Serializable]
	public struct EnvVar
	{
		[XmlAttribute("Key")]
		public string Key;

		[XmlAttribute("Value")]
		public string Value;
	}

	class Program
	{
		static void Main(string[] args)
		{
			if (args.Length != 1)
			{
				Console.WriteLine("Dumps the environment variables to an XML file.");
				Console.WriteLine("");
				Console.WriteLine("EnvVarsToXML <filename>");
				return;
			}

			var OutputFilename = args[0];

			var ConvertedEnvVars = new List<EnvVar>();
			var EnvVars = Environment.GetEnvironmentVariables();
			foreach (DictionaryEntry  Var in EnvVars)
			{
				ConvertedEnvVars.Add(new EnvVar{ Key = (string)Var.Key, Value = (string)Var.Value });
			}

			var Serializer = new XmlSerializer(typeof(List<EnvVar>));
			using (var Stream = new StreamWriter(OutputFilename, false, Encoding.Unicode))
			{
				Serializer.Serialize(Stream, ConvertedEnvVars);
			}
		}
	}
}
