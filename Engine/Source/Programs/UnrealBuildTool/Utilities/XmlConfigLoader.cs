// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections;
using System.Collections.Generic;
using System.Globalization;
using System.IO;
using System.Linq;
using System.Reflection;
using System.Text;
using System.Text.RegularExpressions;
using System.Threading;
using System.Xml;
using System.Xml.Linq;
using System.Xml.Schema;
using Tools.DotNETCommon;

namespace UnrealBuildTool
{
	/// <summary>
	/// Attribute to annotate fields in type that can be set using XML configuration system.
	/// </summary>
	[AttributeUsage(AttributeTargets.Field | AttributeTargets.Property)]
	public class XmlConfigAttribute : Attribute
	{

	}

	/// <summary>
	/// Loads data from disk XML files and stores it in memory.
	/// </summary>
	public static class XmlConfigLoader
	{
		/// <summary>
		/// Resets given config class.
		/// </summary>
		/// <typeparam name="ConfigClass">The class to reset.</typeparam>
		public static void Reset<ConfigClass>()
		{
			Load(typeof(ConfigClass));
		}

		/// <summary>
		/// Creates XML element for given config class.
		/// </summary>
		/// <param name="ConfigType">A type of the config class.</param>
		/// <returns>XML element representing config class.</returns>
		private static XElement CreateConfigTypeXMLElement(Type ConfigType)
		{
			XNamespace NS = XNamespace.Get("https://www.unrealengine.com/BuildConfiguration");

			return new XElement(NS + ConfigType.Name,
				from Field in GetConfigurableFields(ConfigType)
				where !(Field.FieldType == typeof(string) && Field.GetValue(null) == null)
				select CreateFieldXMLElement(Field));
		}

		/// <summary>
		/// Gets XML representing current config.
		/// </summary>
		/// <returns>XML representing current config.</returns>
		private static XDocument GetConfigXML()
		{
			XNamespace NS = XNamespace.Get("https://www.unrealengine.com/BuildConfiguration");
			return new XDocument(
					new XElement(NS + "Configuration",
						from ConfigType in GetAllConfigurationTypes()
						select CreateConfigTypeXMLElement(ConfigType)
					)
				);
		}

		/// <summary>
		/// Gets default config XML.
		/// </summary>
		/// <returns>Default config XML.</returns>
		public static string GetDefaultXML()
		{
			string[] Comment =
			{
				"	#########################################################################",
				"	#                                                                       #",
				"	#	This is an XML with default UBT configuration. If you want to		#",
				"	#	override it, create the same file in the locations c. or d.			#",
				"	#	(see below). DO NOT CHANGE THE CONTENTS OF THIS FILE!				#",
				"	#																		#",
				"	#########################################################################",
				"",
				"	The syntax of this file is:",
				"		<Configuration>",
				"			<ClassA>",
				"				<Field1>Value</Field2>",
				"				<ArrayField2>",
				"					<Item>First item</Item>",
				"					<Item>Second item</Item>",
				"					...",
				"					<Item>Last item</Item>",
				"				</ArrayField2>",
				"",
				"				...",
				"",
				"			</ClassA>",
				"			<ClassB>",
				"				...",
				"			</ClassB>",
				"",	
				"			...",
				"",
				"			<ClassN>",
				"				...",
				"			</ClassN>",
				"		</Configuration>",
				"",
				"	There are four possible location for this file:",
				"		a. UE4/Engine/Programs/UnrealBuildTool",
				"		b. UE4/Engine/Programs/NotForLicensees/UnrealBuildTool",
				"		c. UE4/Engine/Saved/UnrealBuildTool",
				"		d. AppData/Roaming/Unreal Engine/UnrealBuildTool",
				"		e. My Documents/Unreal Engine/UnrealBuildTool",
				"",
				"	UBT looks in each location in the order above, overriding previously read settings with data from subsequent files (hence e. has the highest priority).",
				"	Classes and fields which are not defined are ignored.",
				""
			};

			XDocument DefaultXml = GetConfigXML();

			DefaultXml.AddFirst(new XComment(String.Join("\n", Comment)));

			return WriteToBuffer(DefaultXml);
		}

		/// <summary>
		/// Loads data for a given config class.
		/// </summary>
		/// <param name="Class">A type of a config class.</param>
		public static void Load(Type Class)
		{
			InvokeIfExists(Class, "LoadDefaults");
			InvokeIfExists(Class, "Reset");

			XmlConfigLoaderClassData ClassData;
			if (Data.TryGetValue(Class, out ClassData))
			{
				ClassData.LoadXmlData();
			}

			InvokeIfExists(Class, "PostReset");
		}

		/// <summary>
		/// Builds xsd for BuildConfiguration.xml files.
		/// </summary>
		/// <returns>Content of the xsd file.</returns>
		public static string BuildXSD()
		{
			// all elements use this namespace
			XNamespace NS = XNamespace.Get("http://www.w3.org/2001/XMLSchema");

			return WriteToBuffer(new XDocument(
				// define the root element that declares the schema and target namespace it is validating.
				new XElement(NS + "schema",
					new XAttribute("targetNamespace", "https://www.unrealengine.com/BuildConfiguration"),
					new XAttribute("elementFormDefault", "qualified"),
					new XElement(NS + "element", new XAttribute("name", "Configuration"),
						new XElement(NS + "complexType",
							new XElement(NS + "all",
				// loop over all public types in UBT assembly
								from ConfigType in GetAllConfigurationTypes()
								// find all public static fields of intrinsic types (and a few extra)
								let PublicStaticFields = GetConfigurableFields(ConfigType).ToList()
								where PublicStaticFields.Count > 0
								select
								new XElement(NS + "element",
									new XAttribute("name", ConfigType.Name),
									new XAttribute("minOccurs", "0"),
									new XAttribute("maxOccurs", "1"),
									new XElement(NS + "complexType",
										new XElement(NS + "all",
											from Field in PublicStaticFields
											select CreateXSDElementForField(Field)
										)
									)
								)
							)
						)
					)
				)
			));
		}

		/// <summary>
		/// Initializing data and resets all found classes.
		/// </summary>
		public static void Init()
		{
			// No one should try to refereence configuration values until the config files are loaded.
			// The XmlConfig system itself will SET config values below, then anyone can read them.
			// But our static constructor checks can't differentiate this, so we just allow reads starting now,
			// right before the XML files are loaded.
			UnrealBuildTool.bIsSafeToReferenceConfigurationValues = true;

			OverwriteIfDifferent(GetXSDPath(), BuildXSD());

			LoadDefaults();
			CreateUserXmlConfigTemplate();

			LoadData();

			foreach (Type ConfClass in Data.Keys)
			{
				Load(ConfClass);
			}
		}

		/// <summary>
		/// Overwrites file at FilePath with the Content if the content was
		/// different. If the file doesn't exist it creates file with the
		/// Content.
		/// </summary>
		/// <param name="FilePath">File to check, overwrite or create.</param>
		/// <param name="Content">Content to fill the file with.</param>
		/// <param name="bReadOnlyFile">If file can and should be read-only?</param>
		private static void OverwriteIfDifferent(string FilePath, string Content, bool bReadOnlyFile = false)
		{
			if (FileDifferentThan(FilePath, Content))
			{
				if (bReadOnlyFile && File.Exists(FilePath))
				{
					FileAttributes Attributes = File.GetAttributes(FilePath);
					if (Attributes.HasFlag(FileAttributes.ReadOnly))
					{
						File.SetAttributes(FilePath, Attributes & ~FileAttributes.ReadOnly);
					}
				}

				Directory.CreateDirectory(Path.GetDirectoryName(FilePath));
				File.WriteAllText(FilePath, Content, Encoding.UTF8);

				if (bReadOnlyFile)
				{
					File.SetAttributes(FilePath, File.GetAttributes(FilePath) | FileAttributes.ReadOnly);
				}
			}
		}

		/// <summary>
		/// Tells if file at given path has different content than given.
		/// </summary>
		/// <param name="FilePath">Path of the file to check.</param>
		/// <param name="Content">Content to check.</param>
		/// <returns>True if file at FilePath has different content than Content. False otherwise.</returns>
		private static bool FileDifferentThan(string FilePath, string Content)
		{
			if (!File.Exists(FilePath))
			{
				return true;
			}

			return !File.ReadAllText(FilePath, Encoding.UTF8).Equals(Content, StringComparison.InvariantCulture);
		}

		/// <summary>
		/// Loads default values for all configuration classes in assembly.
		/// </summary>
		private static void LoadDefaults()
		{
			foreach (Type ConfigType in GetAllConfigurationTypes())
			{
				InvokeIfExists(ConfigType, "LoadDefaults");
			}
		}

		/// <summary>
		/// Cache entry class to store loaded info for given class.
		/// </summary>
		class XmlConfigLoaderClassData
		{
			/// <summary>
			/// Loads previously stored data into class.
			/// </summary>
			public void LoadXmlData()
			{
				foreach (KeyValuePair<FieldInfo, object> DataPair in DataMap)
				{
					DataPair.Key.SetValue(null, DataPair.Value);
				}

				foreach (KeyValuePair<PropertyInfo, object> PropertyPair in PropertyMap)
				{
					PropertyPair.Key.SetValue(null, PropertyPair.Value, null);
				}
			}

			/// <summary>
			/// Adds or overrides value in the cache.
			/// </summary>
			/// <param name="Field">The field info of the class.</param>
			/// <param name="Value">The value to store.</param>
			public void SetValue(FieldInfo Field, object Value)
			{
				if (DataMap.ContainsKey(Field))
				{
					DataMap[Field] = Value;
				}
				else
				{
					DataMap.Add(Field, Value);
				}
			}

			/// <summary>
			/// Adds or overrides value in the cache.
			/// </summary>
			/// <param name="Property">The property info of the class.</param>
			/// <param name="Value">The value to store.</param>
			public void SetValue(PropertyInfo Property, object Value)
			{
				if (PropertyMap.ContainsKey(Property))
				{
					PropertyMap[Property] = Value;
				}
				else
				{
					PropertyMap.Add(Property, Value);
				}
			}

			// Loaded data map.
			Dictionary<FieldInfo, object> DataMap = new Dictionary<FieldInfo, object>();

			// Loaded data map.
			Dictionary<PropertyInfo, object> PropertyMap = new Dictionary<PropertyInfo, object>();
		}

		/// <summary>
		/// Class that stores information about possible BuildConfiguration.xml
		/// location and its name that will be displayed in IDE.
		/// </summary>
		public class XmlConfigLocation
		{
			// Possible location of the config file in the file system.
			public string FSLocation { get; private set; }

			// IDE folder name that will contain this location if file will be found.
			public string IDEFolderName { get; private set; }

			// Tells if UBT has to create a template config file if it does not exist in the location.
			public bool bCreateIfDoesNotExist { get; private set; }

			// Tells if config file exists in this location.
			public bool bExists { get; protected set; }

			// Timestamp of file.
			public DateTime Timestamp { get; protected set; }

			public XmlConfigLocation(string DirectoryLocation, string IDEFolderName, bool bCreateIfDoesNotExist = false)
			{
				this.FSLocation = Path.Combine(DirectoryLocation, "BuildConfiguration.xml");
				this.IDEFolderName = IDEFolderName;
				this.bCreateIfDoesNotExist = bCreateIfDoesNotExist;
				this.bExists = File.Exists(FSLocation);
				if (bExists)
				{
					try
					{
						Timestamp = new FileInfo(FSLocation).LastWriteTime;
					}
					catch (Exception)
					{
						Timestamp = DateTime.MaxValue;
					}
				}
			}

			/// <summary>
			/// Creates template file in the FS location.
			/// </summary>
			public virtual void CreateUserXmlConfigTemplate()
			{
				try
				{
					Directory.CreateDirectory(Path.GetDirectoryName(FSLocation));

					string FilePath = Path.Combine(FSLocation);

					string[] TemplateContent =
					{
						"<?xml version=\"1.0\" encoding=\"utf-8\" ?>",
						"<Configuration xmlns=\"https://www.unrealengine.com/BuildConfiguration\">",
						"	<BuildConfiguration>",
						"	</BuildConfiguration>",
						"</Configuration>"
					};

					File.WriteAllLines(FilePath, TemplateContent);
					bExists = true;
				}
				catch (Exception)
				{
					// Ignore quietly.
				}
			}

			/// <summary>
			/// Tells if procedure should try to create file for this location.
			/// </summary>
			/// <returns>True if procedure should try to create file for this location. False otherwise.</returns>
			public virtual bool IfShouldCreateFile()
			{
				return bCreateIfDoesNotExist && !bExists;
			}
		}

		/// <summary>
		/// Class that stores information about possible default BuildConfiguration.xml.
		/// </summary>
		public class XmlDefaultConfigLocation : XmlConfigLocation
		{
			public XmlDefaultConfigLocation(string FSLocation)
				: base(FSLocation, "Default", true)
			{

			}

			/// <summary>
			/// Creates template file in the FS location.
			/// </summary>
			public override void CreateUserXmlConfigTemplate()
			{
				try
				{
					Directory.CreateDirectory(Path.GetDirectoryName(FSLocation));

					string FilePath = Path.Combine(FSLocation);

					OverwriteIfDifferent(FilePath, GetDefaultXML(), true);
					bExists = true;
				}
				catch (Exception)
				{
					// Ignore quietly.
				}
			}

			/// <summary>
			/// Tells if procedure should try to create file for this location.
			/// </summary>
			/// <returns>True if procedure should try to create file for this location. False otherwise.</returns>
			public override bool IfShouldCreateFile()
			{
				return true;
			}
		}

		public static readonly XmlConfigLocation[] ConfigLocationHierarchy;

		static XmlConfigLoader()
		{
			// There are five possible location for this file:
			//      a. UE4/Engine/Programs/UnrealBuildTool
			//      b. UE4/Engine/Programs/NotForLicensees/UnrealBuildTool
			//      c. UE4/Engine/Saved/UnrealBuildTool
			//      d. AppData/Unreal Engine/UnrealBuildTool
			//      e. My Documents/Unreal Engine/UnrealBuildTool
			//
			// UBT looks in each location in the order above, overriding previously read settings with data from subsequent files (hence e. has the highest priority).
			// Classes and fields which are not defined are left alone.

			string UE4EnginePath = new FileInfo(Path.Combine(Path.GetDirectoryName(Assembly.GetEntryAssembly().GetOriginalLocation()), "..", "..")).FullName;

			ConfigLocationHierarchy = new XmlConfigLocation[]
			{
				new XmlDefaultConfigLocation(Path.Combine(UE4EnginePath, "Programs", "UnrealBuildTool")),
				new XmlConfigLocation(Path.Combine(UE4EnginePath, "Programs", "NotForLicensees", "UnrealBuildTool"), "NotForLicensees"),
				new XmlConfigLocation(Path.Combine(UE4EnginePath, "Saved", "UnrealBuildTool"), "User", true),
				new XmlConfigLocation(Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.ApplicationData), "Unreal Engine", "UnrealBuildTool"), "Global (AppData)", true),
				new XmlConfigLocation(Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.Personal), "Unreal Engine", "UnrealBuildTool"), "Global (Documents)", false)
			};
		}

		public static DateTime NewestXmlTimestamp
		{
			get
			{
				return ConfigLocationHierarchy.Max(x => x.Timestamp);
			}
		}

		/// <summary>
		/// Loads BuildConfiguration from XML into memory.
		/// </summary>
		private static void LoadData()
		{
			foreach (XmlConfigLocation PossibleConfigLocation in ConfigLocationHierarchy)
			{
				if (!PossibleConfigLocation.bExists)
				{
					continue;
				}

				try
				{
					LoadDataFromFile(PossibleConfigLocation.FSLocation);
				}
				catch (Exception Ex)
				{
					Console.WriteLine("Problem parsing {0}:\n   {1}", PossibleConfigLocation.FSLocation, Ex.ToString());
				}
			}
		}

		/// <summary>
		/// Creates XML files for all known XmlConfigLocations which return IfShouldCreateFile()==true. Files
		/// will be filled with default content chosen by the XmlConfigLocation implementation.
		/// </summary>
		private static void CreateUserXmlConfigTemplate()
		{
			foreach (XmlConfigLocation PossibleConfigLocation in ConfigLocationHierarchy)
			{
				if (!PossibleConfigLocation.IfShouldCreateFile())
				{
					continue;
				}

				PossibleConfigLocation.CreateUserXmlConfigTemplate();
			}
		}

		/// <summary>
		/// Reads config schema from XSD file.
		/// </summary>
		private static XmlSchema ReadConfigSchema()
		{
			XmlReaderSettings Settings = new XmlReaderSettings();
			Settings.ValidationType = ValidationType.DTD;

			using (StringReader SR = new StringReader(File.ReadAllText(GetXSDPath(), Encoding.UTF8)))
			{
				using (XmlReader XR = XmlReader.Create(SR, Settings))
				{
					return XmlSchema.Read(XR, (object Sender, System.Xml.Schema.ValidationEventArgs EventArgs) =>
					{
						throw new BuildException("XmlConfigLoader: Reading config XSD failed:\n{0}({1}): {2}",
							new Uri(EventArgs.Exception.SourceUri).LocalPath, EventArgs.Exception.LineNumber,
							EventArgs.Message);
					});
				}
			}
		}

		// Stores read XSD schema for config files.
		private static XmlSchema ConfigSchemaCache = null;

		/// <summary>
		/// Gets config XSD schema.
		/// </summary>
		private static XmlSchema GetConfigSchema()
		{
			if (ConfigSchemaCache == null)
			{
				ConfigSchemaCache = ReadConfigSchema();
			}

			return ConfigSchemaCache;
		}

		/// <summary>
		/// Sets values of this class with values from given XML file.
		/// </summary>
		/// <param name="ConfigurationXmlPath">The path to the file with values.</param>
		private static void LoadDataFromFile(string ConfigurationXmlPath)
		{
			XmlDocument ConfigDocument = new XmlDocument();
			XmlNamespaceManager NS = new XmlNamespaceManager(ConfigDocument.NameTable);
			NS.AddNamespace("ns", "https://www.unrealengine.com/BuildConfiguration");
			XmlReaderSettings ReaderSettings = new XmlReaderSettings();

			ReaderSettings.ValidationEventHandler += (object Sender, System.Xml.Schema.ValidationEventArgs EventArgs) =>
			{
				throw new BuildException("XmlConfigLoader: Reading config XML failed:\n{0}({1}): {2}",
					ConfigurationXmlPath, EventArgs.Exception.LineNumber,
					EventArgs.Message);
			};
			ReaderSettings.ValidationType = ValidationType.Schema;
			ReaderSettings.Schemas.Add(GetConfigSchema());

			using (StringReader SR = new StringReader(File.ReadAllText(ConfigurationXmlPath, Encoding.UTF8)))
			{
				XmlReader Reader = XmlReader.Create(SR, ReaderSettings);
				ConfigDocument.Load(Reader);
			}

			XmlNodeList XmlClasses = ConfigDocument.DocumentElement.SelectNodes("/ns:Configuration/*", NS);

			if (XmlClasses.Count == 0)
			{
				if (ConfigDocument.DocumentElement.Name == "Configuration")
				{
					ConfigDocument.DocumentElement.SetAttribute("xmlns", "https://www.unrealengine.com/BuildConfiguration");

					XmlDocument NSDoc = new XmlDocument();

					NSDoc.LoadXml(ConfigDocument.OuterXml);

					try
					{
						File.WriteAllText(ConfigurationXmlPath, WriteToBuffer(NSDoc));
					}
					catch (Exception)
					{
						// Ignore gently.
					}

					XmlClasses = NSDoc.DocumentElement.SelectNodes("/ns:Configuration/*", NS);
				}
			}

			foreach (XmlNode XmlClass in XmlClasses)
			{
				Type ClassType = Type.GetType("UnrealBuildTool." + XmlClass.Name);

				if (ClassType == null)
				{
					Log.TraceVerbose("XmlConfig Loading: class '{0}' doesn't exist.", XmlClass.Name);
					continue;
				}

				if (!IsConfigurableClass(ClassType))
				{
					Log.TraceVerbose("XmlConfig Loading: class '{0}' is not allowed to be configured using XML system.", XmlClass.Name);
					continue;
				}

				XmlConfigLoaderClassData ClassData;

				if (!Data.TryGetValue(ClassType, out ClassData))
				{
					ClassData = new XmlConfigLoaderClassData();
					Data.Add(ClassType, ClassData);
				}

				XmlNodeList XmlFields = XmlClass.SelectNodes("*");

				foreach (XmlNode XmlField in XmlFields)
				{
					FieldInfo Field = ClassType.GetField(XmlField.Name);

					// allow settings in the .xml that don't exist, as another branch may have it, and can share this file from Documents
					if (Field == null)
					{
						PropertyInfo Property = ClassType.GetProperty(XmlField.Name);
						if (Property != null)
						{
							if (!IsConfigurable(Property))
							{
								throw new BuildException("BuildConfiguration Loading: property '{0}' is either non-public, non-static or not-xml-configurable.", XmlField.Name);
							}

							ClassData.SetValue(Property, ParseFieldData(Property.PropertyType, XmlField.InnerText));
						}

						continue;
					}

					if (!IsConfigurableField(Field))
					{
						throw new BuildException("BuildConfiguration Loading: field '{0}' is either non-public, non-static or not-xml-configurable.", XmlField.Name);
					}

					if (Field.FieldType.IsArray)
					{
						// If the type is an array type get items for it.
						XmlNodeList XmlItems = XmlField.SelectNodes("ns:Item", NS);

						// Get the C# type of the array.
						Type ItemType = Field.FieldType.GetElementType();

						// Create the array according to the ItemType.
						Array OutputArray = Array.CreateInstance(ItemType, XmlItems.Count);

						int Id = 0;
						foreach (XmlNode XmlItem in XmlItems)
						{
							// Append values to the OutputArray.
							OutputArray.SetValue(ParseFieldData(ItemType, XmlItem.InnerText), Id++);
						}

						ClassData.SetValue(Field, OutputArray);
					}
					else
					{
						ClassData.SetValue(Field, ParseFieldData(Field.FieldType, XmlField.InnerText));
					}
				}
			}
		}

		private static object ParseFieldData(Type FieldType, string Text)
		{
			if (FieldType.Equals(typeof(System.String)))
			{
				return Text;
			}
			else
			{
				// Declaring parameters array used by TryParse method.
				// Second parameter is "out", so you have to just
				// assign placeholder null to it.
				object ParsedValue;
				if (!TryParse(FieldType, Text, out ParsedValue))
				{
					throw new BuildException("BuildConfiguration Loading: Parsing {0} value from \"{1}\" failed.", FieldType.Name, Text);
				}

				// If Invoke returned true, the second object of the
				// parameters array is set to the parsed value.
				return ParsedValue;
			}
		}

		/// <summary>
		/// Emulates TryParse behavior on custom type. If the type implements
		/// Parse(string, IFormatProvider) or Parse(string) static method uses
		/// one of them to parse with preference of the one with format
		/// provider (but passes invariant culture).
		/// </summary>
		/// <param name="ParsingType">Type to parse.</param>
		/// <param name="UnparsedValue">String representation of the value.</param>
		/// <param name="ParsedValue">Output parsed value.</param>
		/// <returns>True if parsing succeeded. False otherwise.</returns>
		private static bool TryParse(Type ParsingType, string UnparsedValue, out object ParsedValue)
		{
			// Getting Parse method for FieldType which is required,
			// if it doesn't exists for complex type, author should add
			// one. The signature should be one of:
			//     static T Parse(string Input, IFormatProvider Provider) or
			//     static T Parse(string Input)
			// where T is containing type.
			// The one with format provider is preferred and invoked with
			// InvariantCulture.

			bool bWithCulture = true;
			MethodInfo ParseMethod = ParsingType.GetMethod("Parse", new Type[] { typeof(System.String), typeof(IFormatProvider) });

			if (ParseMethod == null)
			{
				ParseMethod = ParsingType.GetMethod("Parse", new Type[] { typeof(System.String) });
				bWithCulture = false;
			}

			if (ParseMethod == null)
			{
				throw new BuildException("BuildConfiguration Loading: Parsing of the type {0} is not supported.", ParsingType.Name);
			}

			List<object> ParametersList = new List<object> { UnparsedValue };

			if (bWithCulture)
			{
				ParametersList.Add(CultureInfo.InvariantCulture);
			}

			try
			{
				ParsedValue = ParseMethod.Invoke(null, ParametersList.ToArray());
			}
			catch (Exception e)
			{
				if (e is TargetInvocationException &&
					(
						e.InnerException is ArgumentNullException ||
						e.InnerException is FormatException ||
						e.InnerException is OverflowException
					)
				)
				{
					ParsedValue = null;
					return false;
				}

				throw;
			}

			return true;
		}

		/// <summary>
		/// Tells if given class is XML configurable.
		/// </summary>
		/// <param name="Class">Class to check.</param>
		/// <returns>True if the class is configurable using XML system. Otherwise false.</returns>
		private static bool IsConfigurableClass(Type Class)
		{
			return
				Class.GetFields().Any((Field) => IsConfigurableField(Field)) ||
				Class.GetProperties().Any(prop => IsConfigurable(prop));

		}

		/// <summary>
		/// Tells if given property is XML configurable.
		/// </summary>
		/// <param name="Property">Property to check.</param>
		/// <returns>True if the property is configurable using XML system. Otherwise false.</returns>
		private static bool IsConfigurable(PropertyInfo Property)
		{
			return Property.GetCustomAttributes(typeof(XmlConfigAttribute), false).Length > 0 && Property.CanWrite && Property.GetSetMethod().IsStatic && Property.GetSetMethod().IsPublic;
		}

		/// <summary>
		/// Tells if given field is XML configurable.
		/// </summary>
		/// <param name="Field">field to check.</param>
		/// <returns>True if the field is configurable using XML system. Otherwise false.</returns>
		private static bool IsConfigurableField(FieldInfo Field)
		{
			return Field.IsStatic && Field.IsPublic && Field.GetCustomAttributes(typeof(XmlConfigAttribute), false).Length > 0;
		}

		/// <summary>
		/// Gets all types that can be configured using this XML system.
		/// </summary>
		/// <returns>Enumerable of types that can be configured using this xml system.</returns>
		private static IEnumerable<Type> GetAllConfigurationTypes()
		{
			return Assembly.GetExecutingAssembly().GetTypes().Where((Class) => IsConfigurableClass(Class));
		}

		/// <summary>
		/// Invokes a public static parameterless method from type if it exists.
		/// </summary>
		/// <param name="ClassType">Class type to look the method in.</param>
		/// <param name="MethodName">Name of the method.</param>
		private static void InvokeIfExists(Type ClassType, string MethodName)
		{
			MethodInfo Method = ClassType.GetMethod(MethodName, new Type[] { }, new ParameterModifier[] { });

			if (Method != null && Method.IsPublic && Method.IsStatic)
			{
				Method.Invoke(null, new object[] { });
			}
		}

		/// <summary>
		/// Enumerates all fields that are configurable using this system.
		/// </summary>
		/// <param name="ClassType">Type of the class to enumrate field for.</param>
		/// <returns>Enumerable fields that are configurable using this system.</returns>
		private static IEnumerable<FieldInfo> GetConfigurableFields(Type ClassType)
		{
			return ClassType.GetFields().Where((Field) => IsConfigurableField(Field));
		}

		/// <summary>
		/// Gets xsd type string for the field type.
		/// </summary>
		/// <param name="FieldType">Field type to get xsd string for.</param>
		/// <returns>String representation of xsd type corresponding given field type.</returns>
		private static string GetFieldXSDType(Type FieldType)
		{
			Dictionary<Type, string> TypeMap = new Dictionary<Type, string>
			{
				{ typeof(string), "string" },
				{ typeof(bool), "boolean" },
				{ typeof(int), "int" },
				{ typeof(long), "long" },
				{ typeof(DateTime), "dateTime" },
				{ typeof(float), "float" },
				{ typeof(double), "double" }
			};

			return TypeMap.ContainsKey(FieldType)
				? TypeMap[FieldType]
				: FieldType.Name.ToLowerInvariant();
		}

		/// <summary>
		/// Gets xml representation of an array field value.
		/// </summary>
		/// <param name="Field">Array field to represent.</param>
		/// <returns>Array field value as XML elements.</returns>
		private static XElement[] GetArrayFieldXMLValue(FieldInfo Field)
		{
			XNamespace NS = XNamespace.Get("http://www.w3.org/2001/XMLSchema");

			IEnumerable Array = Field.GetValue(null) as IEnumerable;
			List<XElement> ElementList = new List<XElement>();

			foreach (object Element in Array)
			{
				ElementList.Add(new XElement(NS + "Item", GetObjectXMLValue(Field.FieldType.GetElementType(), Element)));
			}

			return ElementList.ToArray();
		}

		/// <summary>
		/// XML representation of a regular field value.
		/// </summary>
		/// <param name="Field">Field to represent value.</param>
		/// <returns>Field value as XML.</returns>
		private static object GetFieldXMLValue(FieldInfo Field)
		{
			return GetObjectXMLValue(Field.FieldType, Field.GetValue(null));
		}

		/// <summary>
		/// Gets custom C# object XML string representation.
		/// </summary>
		/// <param name="FieldType">The type of the field.</param>
		/// <param name="Obj">The value.</param>
		/// <returns>String representation.</returns>
		private static object GetObjectXMLValue(Type FieldType, object Obj)
		{
			if (Obj == null && FieldType == typeof(string))
			{
				return "";
			}

			if (FieldType == typeof(bool))
			{
				return (bool)Obj ? "true" : "false";
			}

			return Obj;
		}

		/// <summary>
		/// Gets standard BuildConfiguration.xml schema path.
		/// </summary>
		/// <returns>Standard BuildConfiguration.xml schema path.</returns>
		public static string GetXSDPath()
		{
			return new FileInfo(Path.Combine(Path.GetDirectoryName(Assembly.GetEntryAssembly().GetOriginalLocation()), "..", "..", "Saved", "UnrealBuildTool", "BuildConfiguration.Schema.xsd")).FullName;
		}

		private static XElement CreateXSDElementForField(FieldInfo Field)
		{
			XNamespace NS = XNamespace.Get("http://www.w3.org/2001/XMLSchema");

			if (Field.FieldType.IsArray)
			{
				return new XElement(NS + "element",
					new XAttribute("name", Field.Name),
					new XAttribute("minOccurs", "0"),
					new XAttribute("maxOccurs", "1"),
					new XElement(NS + "complexType",
						new XElement(NS + "sequence",
							new XElement(NS + "element",
								new XAttribute("name", "Item"),
								new XAttribute("type", GetFieldXSDType(Field.FieldType.GetElementType())),
								new XAttribute("minOccurs", "0"),
								new XAttribute("maxOccurs", "unbounded")
							)
						)
					)
				);
			}

			return new XElement(NS + "element",
				new XAttribute("name", Field.Name),
				new XAttribute("type", GetFieldXSDType(Field.FieldType)),
				new XAttribute("minOccurs", "0"),
				new XAttribute("maxOccurs", "1"));
		}

		private static XElement CreateFieldXMLElement(FieldInfo Field)
		{
			XNamespace NS = XNamespace.Get("https://www.unrealengine.com/BuildConfiguration");

			if (Field.FieldType.IsArray)
			{
				return new XElement(NS + Field.Name, GetArrayFieldXMLValue(Field));
			}
			else
			{
				return new XElement(NS + Field.Name, GetFieldXMLValue(Field));
			}
		}

		private static string WriteToBuffer(XmlDocument Document)
		{
			using (XmlNodeReader Reader = new XmlNodeReader(Document))
			{
				Reader.MoveToContent();
				return WriteToBuffer(XDocument.Load(Reader));
			}
		}

		private static string WriteToBuffer(XDocument Document)
		{
			XmlWriterSettings Settings = new XmlWriterSettings();
			Settings.Indent = true;
			Settings.IndentChars = "\t";
			Settings.CloseOutput = true;

			using (MemoryStream MS = new MemoryStream())
			{
				using (StreamWriter SW = new StreamWriter(MS, Encoding.UTF8))
				{
					using (XmlWriter Writer = XmlWriter.Create(SW, Settings))
					{
						Document.WriteTo(Writer);
					}
				}

				// Skipping first 3 bytes, that informs about encoding.
				return Encoding.UTF8.GetString(MS.ToArray().Skip(3).ToArray());
			}
		}

		// Map to store class data in.
		private static readonly Dictionary<Type, XmlConfigLoaderClassData> Data = new Dictionary<Type, XmlConfigLoaderClassData>();
	}
}
