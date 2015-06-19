using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text;

namespace UnrealBuildTool
{
	public class JsonParseException : Exception
	{
		public JsonParseException(string Format, params string[] Args) : base(String.Format(Format, Args))
		{
		}
	}

	public class JsonObject
	{
		Dictionary<string, object> RawObject;

		public JsonObject(Dictionary<string, object> InRawObject)
		{
			RawObject = new Dictionary<string,object>(InRawObject, StringComparer.InvariantCultureIgnoreCase);
		}

		public static JsonObject FromFile(string FileName)
		{
			string Text = File.ReadAllText(FileName);
			Dictionary<string, object> CaseSensitiveRawObject = fastJSON.JSON.Instance.ToObject<Dictionary<string, object>>(Text);
			return new JsonObject(CaseSensitiveRawObject);
		}

		public string GetStringField(string FieldName)
		{
			string StringValue;
			if(!TryGetStringField(FieldName, out StringValue))
			{
				throw new JsonParseException("Missing or invalid '{0}' field", FieldName);
			}
			return StringValue;
		}

		public bool TryGetStringField(string FieldName, out string Result)
		{
			object RawValue;
			if(RawObject.TryGetValue(FieldName, out RawValue) && (RawValue is string))
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
			if(!TryGetStringArrayField(FieldName, out StringValues))
			{
				throw new JsonParseException("Missing or invalid '{0}' field", FieldName);
			}
			return StringValues;
		}

		public bool TryGetStringArrayField(string FieldName, out string[] Result)
		{
			object RawValue;
			if(RawObject.TryGetValue(FieldName, out RawValue) && (RawValue is object[]) && ((object[])RawValue).All(x => x is string))
			{
				Result = Array.ConvertAll((object[])RawValue, x => (string)x);
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
			if(!TryGetBoolField(FieldName, out BoolValue))
			{
				throw new JsonParseException("Missing or invalid '{0}' field", FieldName);
			}
			return BoolValue;
		}

		public bool TryGetBoolField(string FieldName, out bool Result)
		{
			object RawValue;
			if(RawObject.TryGetValue(FieldName, out RawValue) && (RawValue is Boolean))
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

		public bool TryGetIntegerField(string FieldName, out int Result)
		{
			object RawValue;
			if(!RawObject.TryGetValue(FieldName, out RawValue) || !int.TryParse(RawValue.ToString(), out Result))
			{
				Result = 0;
				return false;
			}
			return true;
		}

		public bool TryGetUnsignedIntegerField(string FieldName, out uint Result)
		{
			object RawValue;
			if(!RawObject.TryGetValue(FieldName, out RawValue) || !uint.TryParse(RawValue.ToString(), out Result))
			{
				Result = 0;
				return false;
			}
			return true;
		}

		public T GetEnumField<T>(string FieldName) where T : struct
		{
			T EnumValue;
			if(!TryGetEnumField(FieldName, out EnumValue))
			{
				throw new JsonParseException("Missing or invalid '{0}' field", FieldName);
			}
			return EnumValue;
		}

		public bool TryGetEnumField<T>(string FieldName, out T Result) where T : struct
		{
			string StringValue;
			if(!TryGetStringField(FieldName, out StringValue) || !Enum.TryParse<T>(StringValue, true, out Result))
			{
				Result = default(T);
				return false;
			}
			return true;
		}

		public bool TryGetEnumArrayField<T>(string FieldName, out T[] Result) where T : struct
		{
			string[] StringValues;
			if(!TryGetStringArrayField(FieldName, out StringValues))
			{
				Result = null;
				return false;
			}

			T[] EnumValues = new T[StringValues.Length];
			for(int Idx = 0; Idx < StringValues.Length; Idx++)
			{
				if(!Enum.TryParse<T>(StringValues[Idx], true, out EnumValues[Idx]))
				{
					Result = null;
					return false;
				}
			}

			Result = EnumValues;
			return true;
		}

		public bool TryGetObjectArrayField(string FieldName, out JsonObject[] Result)
		{
			object RawValue;
			if(RawObject.TryGetValue(FieldName, out RawValue) && (RawValue is object[]) && ((object[])RawValue).All(x => x is Dictionary<string, object>))
			{
				Result = Array.ConvertAll((object[])RawValue, x => new JsonObject((Dictionary<string, object>)x));
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

			Writer.Write("{0}\"{1}\"", Indent, Value);

			bRequiresComma = true;
		}

		public void WriteValue(string Name, string Value)
		{
			WriteValueInternal(Name, '"' + Value + '"');
		}

		public void WriteValue(string Name, int Value)
		{
			WriteValueInternal(Name, Value.ToString());
		}

		public void WriteValue(string Name, bool Value)
		{
			WriteValueInternal(Name, Value? "true" : "false");
		}

		void WriteCommaNewline()
		{
			if(bRequiresComma)
			{
				Writer.WriteLine(",");
			}
			else if(Indent.Length > 0)
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
	}
}
