// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text;

namespace UnrealBuildTool
{
	public class JsonParseException : Exception
	{
		public JsonParseException(string Format, params object[] Args)
			: base(String.Format(Format, Args))
		{
		}
	}

	public class JsonObject
	{
		Dictionary<string, object> RawObject;

		public JsonObject(Dictionary<string, object> InRawObject)
		{
			RawObject = new Dictionary<string, object>(InRawObject, StringComparer.InvariantCultureIgnoreCase);
		}

		public static JsonObject Read(string FileName)
		{
			string Text = File.ReadAllText(FileName);
			Dictionary<string, object> CaseSensitiveRawObject = (Dictionary<string, object>)fastJSON.JSON.Instance.Parse(Text);
			return new JsonObject(CaseSensitiveRawObject);
		}

		public static bool TryRead(string FileName, out JsonObject Result)
		{
			if (!File.Exists(FileName))
			{
				Result = null;
				return false;
			}

			try
			{
				Result = Read(FileName);
				return true;
			}
			catch (Exception)
			{
				Result = null;
				return false;
			}
		}

		public IEnumerable<string> KeyNames
		{
			get { return RawObject.Keys; }
		}

		public string GetStringField(string FieldName)
		{
			string StringValue;
			if (!TryGetStringField(FieldName, out StringValue))
			{
				throw new JsonParseException("Missing or invalid '{0}' field", FieldName);
			}
			return StringValue;
		}

		public bool TryGetStringField(string FieldName, out string Result)
		{
			object RawValue;
			if (RawObject.TryGetValue(FieldName, out RawValue) && (RawValue is string))
			{
				Result = (string)RawValue;
				return true;
			}
			else
			{
				Result = null;
				return false;
			}
		}

		public string[] GetStringArrayField(string FieldName)
		{
			string[] StringValues;
			if (!TryGetStringArrayField(FieldName, out StringValues))
			{
				throw new JsonParseException("Missing or invalid '{0}' field", FieldName);
			}
			return StringValues;
		}

		public bool TryGetStringArrayField(string FieldName, out string[] Result)
		{
			object RawValue;
			if (RawObject.TryGetValue(FieldName, out RawValue) && (RawValue is IEnumerable<object>) && ((IEnumerable<object>)RawValue).All(x => x is string))
			{
				Result = ((IEnumerable<object>)RawValue).Select(x => (string)x).ToArray();
				return true;
			}
			else
			{
				Result = null;
				return false;
			}
		}

		public bool GetBoolField(string FieldName)
		{
			bool BoolValue;
			if (!TryGetBoolField(FieldName, out BoolValue))
			{
				throw new JsonParseException("Missing or invalid '{0}' field", FieldName);
			}
			return BoolValue;
		}

		public bool TryGetBoolField(string FieldName, out bool Result)
		{
			object RawValue;
			if (RawObject.TryGetValue(FieldName, out RawValue) && (RawValue is Boolean))
			{
				Result = (bool)RawValue;
				return true;
			}
			else
			{
				Result = false;
				return false;
			}
		}

		public int GetIntegerField(string FieldName)
		{
			int IntegerValue;
			if (!TryGetIntegerField(FieldName, out IntegerValue))
			{
				throw new JsonParseException("Missing or invalid '{0}' field", FieldName);
			}
			return IntegerValue;
		}

		public bool TryGetIntegerField(string FieldName, out int Result)
		{
			object RawValue;
			if (!RawObject.TryGetValue(FieldName, out RawValue) || !int.TryParse(RawValue.ToString(), out Result))
			{
				Result = 0;
				return false;
			}
			return true;
		}

		public bool TryGetUnsignedIntegerField(string FieldName, out uint Result)
		{
			object RawValue;
			if (!RawObject.TryGetValue(FieldName, out RawValue) || !uint.TryParse(RawValue.ToString(), out Result))
			{
				Result = 0;
				return false;
			}
			return true;
		}

		public double GetDoubleField(string FieldName)
		{
			double DoubleValue;
			if (!TryGetDoubleField(FieldName, out DoubleValue))
			{
				throw new JsonParseException("Missing or invalid '{0}' field", FieldName);
			}
			return DoubleValue;
		}

		public bool TryGetDoubleField(string FieldName, out double Result)
		{
			object RawValue;
			if (!RawObject.TryGetValue(FieldName, out RawValue) || !double.TryParse(RawValue.ToString(), out Result))
			{
				Result = 0.0;
				return false;
			}
			return true;
		}

		public T GetEnumField<T>(string FieldName) where T : struct
		{
			T EnumValue;
			if (!TryGetEnumField(FieldName, out EnumValue))
			{
				throw new JsonParseException("Missing or invalid '{0}' field", FieldName);
			}
			return EnumValue;
		}

		public bool TryGetEnumField<T>(string FieldName, out T Result) where T : struct
		{
			string StringValue;
			if (!TryGetStringField(FieldName, out StringValue) || !Enum.TryParse<T>(StringValue, true, out Result))
			{
				Result = default(T);
				return false;
			}
			return true;
		}

		public bool TryGetEnumArrayField<T>(string FieldName, out T[] Result) where T : struct
		{
			string[] StringValues;
			if (!TryGetStringArrayField(FieldName, out StringValues))
			{
				Result = null;
				return false;
			}

			T[] EnumValues = new T[StringValues.Length];
			for (int Idx = 0; Idx < StringValues.Length; Idx++)
			{
				if (!Enum.TryParse<T>(StringValues[Idx], true, out EnumValues[Idx]))
				{
					Result = null;
					return false;
				}
			}

			Result = EnumValues;
			return true;
		}

		public JsonObject GetObjectField(string FieldName)
		{
			JsonObject Result;
			if (!TryGetObjectField(FieldName, out Result))
			{
				throw new JsonParseException("Missing or invalid '{0}' field", FieldName);
			}
			return Result;
		}

		public bool TryGetObjectField(string FieldName, out JsonObject Result)
		{
			object RawValue;
			if (RawObject.TryGetValue(FieldName, out RawValue) && (RawValue is Dictionary<string, object>))
			{
				Result = new JsonObject((Dictionary<string, object>)RawValue);
				return true;
			}
			else
			{
				Result = null;
				return false;
			}
		}

		public bool TryGetObjectArrayField(string FieldName, out JsonObject[] Result)
		{
			object RawValue;
			if (RawObject.TryGetValue(FieldName, out RawValue) && (RawValue is IEnumerable<object>) && ((IEnumerable<object>)RawValue).All(x => x is Dictionary<string, object>))
			{
				Result = ((IEnumerable<object>)RawValue).Select(x => new JsonObject((Dictionary<string, object>)x)).ToArray();
				return true;
			}
			else
			{
				Result = null;
				return false;
			}
		}
	}

	public class JsonWriter : IDisposable
	{
		StreamWriter Writer;
		bool bRequiresComma;
		string Indent;

		public JsonWriter(string FileName)
		{
			Writer = new StreamWriter(FileName);
			Indent = "";
		}

		public void Dispose()
		{
			Writer.Dispose();
		}

		public void WriteObjectStart()
		{
			WriteCommaNewline();

			Writer.Write(Indent);
			Writer.Write("{");

			Indent += "\t";
			bRequiresComma = false;
		}

		public void WriteObjectStart(string ObjectName)
		{
			WriteCommaNewline();

			Writer.Write("{0}\"{1}\" : ", Indent, ObjectName);

			bRequiresComma = false;

			WriteObjectStart();
		}

		public void WriteObjectEnd()
		{
			Indent = Indent.Substring(0, Indent.Length - 1);

			Writer.WriteLine();
			Writer.Write(Indent);
			Writer.Write("}");

			bRequiresComma = true;
		}

		public void WriteArrayStart(string ArrayName)
		{
			WriteCommaNewline();

			Writer.WriteLine("{0}\"{1}\" :", Indent, ArrayName);
			Writer.Write("{0}[", Indent);

			Indent += "\t";
			bRequiresComma = false;
		}

		public void WriteArrayEnd()
		{
			Indent = Indent.Substring(0, Indent.Length - 1);

			Writer.WriteLine();
			Writer.Write("{0}]", Indent);

			bRequiresComma = true;
		}

		public void WriteValue(string Value)
		{
			WriteCommaNewline();

			Writer.Write(Indent);
			WriteEscapedString(Value);

			bRequiresComma = true;
		}

		public void WriteValue(string Name, string Value)
		{
			WriteCommaNewline();

			Writer.Write("{0}\"{1}\" : ", Indent, Name);
			WriteEscapedString(Value);

			bRequiresComma = true;
		}

		public void WriteValue(string Name, int Value)
		{
			WriteValueInternal(Name, Value.ToString());
		}

		public void WriteValue(string Name, double Value)
		{
			WriteValueInternal(Name, Value.ToString());
		}

		public void WriteValue(string Name, bool Value)
		{
			WriteValueInternal(Name, Value ? "true" : "false");
		}

		void WriteCommaNewline()
		{
			if (bRequiresComma)
			{
				Writer.WriteLine(",");
			}
			else if (Indent.Length > 0)
			{
				Writer.WriteLine();
			}
		}

		void WriteValueInternal(string Name, string Value)
		{
			WriteCommaNewline();

			Writer.Write("{0}\"{1}\" : {2}", Indent, Name, Value);

			bRequiresComma = true;
		}

		void WriteEscapedString(string Value)
		{
			// Escape any characters which may not appear in a JSON string (see http://www.json.org).
			Writer.Write("\"");
			if(Value != null)
			{
				for(int Idx = 0; Idx < Value.Length; Idx++)
				{
					switch(Value[Idx])
					{
						case '\"':
							Writer.Write("\\\"");
							break;
						case '\\':
							Writer.Write("\\\\");
							break;
						case '\b':
							Writer.Write("\\b");
							break;
						case '\f':
							Writer.Write("\\f");
							break;
						case '\n':
							Writer.Write("\\n");
							break;
						case '\r':
							Writer.Write("\\r");
							break;
						case '\t':
							Writer.Write("\\t");
							break;
						default:
							if(Char.IsControl(Value[Idx]))
							{
								Writer.Write("\\u{0:X4}", (int)Value[Idx]);
							}
							else
							{
								Writer.Write(Value[Idx]);
							}
							break;
					}
				}
			}
			Writer.Write("\"");
		}
	}
}
