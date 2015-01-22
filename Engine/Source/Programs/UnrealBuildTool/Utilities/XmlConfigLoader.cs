// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

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

namespace UnrealBuildTool
{
	/// <summary>
	/// Attribute to annotate fields in type that can be set using XML configuration system.
	/// </summary>
	[AttributeUsage(AttributeTargets.Field)]
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
			var Type = typeof(ConfigClass);

			InvokeIfExists(Type, "LoadDefaults");

			// Load eventual XML configuration files if they exist to override default values.
			Load(Type);

			InvokeIfExists(Type, "PostReset");
		}

		/// <summary>
		/// Creates XML element for given config class.
		/// </summary>
		/// <param name="ConfigType">A type of the config class.</param>
		/// <returns>XML element representing config class.</returns>
		private static XElement CreateConfigTypeXMLElement(Type ConfigType)
		{
			var NS = XNamespace.Get("https://www.unrealengine.com/BuildConfiguration");

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
			var NS = XNamespace.Get("https://www.unrealengine.com/BuildConfiguration");
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
			foreach(var ConfigType in GetAllConfigurationTypes())
			{
				InvokeIfExists(ConfigType, "LoadDefaults");
			}

			var Comment = @"
	#########################################################################
	#																		#
	#	This is an XML with default UBT configuration. If you want to		#
	#	override it, create the same file in the locations c. or d.			#
	#	(see below). DONT'T CHANGE CONTENTS OF THIS FILE!					#	
	#																		#
	#########################################################################
	
	The syntax of this file is:
	<Configuration>
		<ClassA>
			<Field1>Value</Field2>
			<ArrayField2>
				<Item>First item</Item>
				<Item>Second item</Item>
				...
				<Item>Last item</Item>
			</ArrayField2>
			
			...
			
		</ClassA>
		<ClassB>
			...
		</ClassB>

		...

		<ClassN>
			...
		</ClassN>
	</Configuration>
	
	There are four possible location for this file:
		a. UE4/Engine/Programs/UnrealBuildTool
		b. UE4/Engine/Programs/NotForLicensees/UnrealBuildTool
		c. UE4/Engine/Saved/UnrealBuildTool
		d. My Documents/Unreal Engine/UnrealBuildTool
	
	The UBT is looking for it in all four places in the given order and
	overrides already read data with the loaded ones, hence d. has the
	priority. Not defined classes and fields are left alone.
";

			var DefaultXml = GetConfigXML();

			DefaultXml.AddFirst(new XComment(Comment));

			return WriteToBuffer(DefaultXml);
		}

		/// <summary>
		/// Loads data for a given config class.
		/// </summary>
		/// <param name="Class">A type of a config class.</param>
		public static void Load(Type Class)
		{
			XmlConfigLoaderClassData ClassData;

			if(Data.TryGetValue(Class, out ClassData))
			{
				ClassData.LoadXmlData();
			}
		}

		/// <summary>
		/// Builds xsd for BuildConfiguration.xml files.
		/// </summary>
		/// <returns>Content of the xsd file.</returns>
		public static string BuildXSD()
		{
			// all elements use this namespace
			var NS = XNamespace.Get("http://www.w3.org/2001/XMLSchema");

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
			OverwriteIfDifferent(GetXSDPath(), BuildXSD());

			LoadData();

			foreach(var ClassData in Data)
			{
				ClassData.Value.ResetData();

				InvokeIfExists(ClassData.Key, "PostReset");
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
				if(bReadOnlyFile && File.Exists(FilePath))
				{
					var Attributes = File.GetAttributes(FilePath);
					if(Attributes.HasFlag(FileAttributes.ReadOnly))
					{
						File.SetAttributes(FilePath, Attributes & ~FileAttributes.ReadOnly);
					}
				}

				Directory.CreateDirectory(Path.GetDirectoryName(FilePath));
				File.WriteAllText(FilePath, Content);

				if(bReadOnlyFile)
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

			return !File.ReadAllText(FilePath).Equals(Content, StringComparison.InvariantCulture);
		}

		/// <summary>
		/// Cache entry class to store loaded info for given class.
		/// </summary>
		class XmlConfigLoaderClassData
		{
			public XmlConfigLoaderClassData(Type ConfigClass)
			{
				// Adding empty types array to make sure this is parameterless Reset
				// in case of overloading.
				DefaultValuesLoader = ConfigClass.GetMethod("Reset", new Type[] { });
			}

			/// <summary>
			/// Resets previously stored data into class.
			/// </summary>
			public void ResetData()
			{
				bDoneLoading = false;

				if(DefaultValuesLoader != null)
				{
					DefaultValuesLoader.Invoke(null, new object[] { });
				}

				if (!bDoneLoading)
				{
					LoadXmlData();
				}
			}

			/// <summary>
			/// Loads previously stored data into class.
			/// </summary>
			public void LoadXmlData()
			{
				foreach (var DataPair in DataMap)
				{
					DataPair.Key.SetValue(null, DataPair.Value);
				}

				bDoneLoading = true;
			}

			/// <summary>
			/// Adds or overrides value in the cache.
			/// </summary>
			/// <param name="Field">The field info of the class.</param>
			/// <param name="Value">The value to store.</param>
			public void SetValue(FieldInfo Field, object Value)
			{
				if(DataMap.ContainsKey(Field))
				{
					DataMap[Field] = Value;
				}
				else
				{
					DataMap.Add(Field, Value);
				}
			}

			// A variable to indicate if loading was done during invoking of
			// default values loader.
			bool bDoneLoading = false;

			// Method info loader to invoke before overriding fields with XML files data.
			MethodInfo DefaultValuesLoader = null;

			// Loaded data map.
			Dictionary<FieldInfo, object> DataMap = new Dictionary<FieldInfo, object>();
		}

		/// <summary>
		/// Class that stores information about possible BuildConfiguration.xml
		/// location and its name that will be displayed in IDE.
		/// </summary>
		public class XmlConfigLocation
		{
			/// <summary>
			/// Returns location of the BuildConfiguration.xml.
			/// </summary>
			/// <returns>Location of the BuildConfiguration.xml.</returns>
			private static string GetConfigLocation(IEnumerable<string> PossibleLocations, out bool bExists)
			{
				if(PossibleLocations.Count() == 0)
				{
					throw new ArgumentException("Empty possible locations", "PossibleLocations");
				}

				const string ConfigXmlFileName = "BuildConfiguration.xml";

				// Filter out non-existing
				var ExistingLocations = new List<string>();

				foreach(var PossibleLocation in PossibleLocations)
				{
					var FilePath = Path.Combine(PossibleLocation, ConfigXmlFileName);

					if(File.Exists(FilePath))
					{
						ExistingLocations.Add(FilePath);
					}
				}

				if(ExistingLocations.Count == 0)
				{
					bExists = false;
					return Path.Combine(PossibleLocations.First(), ConfigXmlFileName);
				}

				bExists = true;

				if(ExistingLocations.Count == 1)
				{
					return ExistingLocations.First();
				}

				// Choose most recently used from existing.
				return ExistingLocations.OrderBy(Location => File.GetLastWriteTime(Location)).Last();
			}

			// Possible location of the config file in the file system.
			public string FSLocation { get; private set; }

			// IDE folder name that will contain this location if file will be found.
			public string IDEFolderName { get; private set; }

			// Tells if UBT has to create a template config file if it does not exist in the location.
			public bool bCreateIfDoesNotExist { get; private set; }

			// Tells if config file exists in this location.
			public bool bExists { get; protected set; }

			public XmlConfigLocation(string[] FSLocations, string IDEFolderName, bool bCreateIfDoesNotExist = false)
			{
				bool bExists;

				this.FSLocation = GetConfigLocation(FSLocations, out bExists);
				this.IDEFolderName = IDEFolderName;
				this.bCreateIfDoesNotExist = bCreateIfDoesNotExist;
				this.bExists = bExists;
			}

			public XmlConfigLocation(string FSLocation, string IDEFolderName, bool bCreateIfDoesNotExist = false)
				: this(new string[] { FSLocation }, IDEFolderName, bCreateIfDoesNotExist)
			{
			}

			/// <summary>
			/// Creates template file in the FS location.
			/// </summary>
			public virtual void CreateUserXmlConfigTemplate()
			{
				try
				{
					Directory.CreateDirectory(Path.GetDirectoryName(FSLocation));

					var FilePath = Path.Combine(FSLocation);

					const string TemplateContent =
						"<?xml version=\"1.0\" encoding=\"utf-8\" ?>" +
						"<Configuration xmlns=\"https://www.unrealengine.com/BuildConfiguration\">\n" +
						"	<BuildConfiguration>\n" +
						"	</BuildConfiguration>\n" +
						"</Configuration>\n";

					File.WriteAllText(FilePath, TemplateContent);
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

					var FilePath = Path.Combine(FSLocation);

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
			/*
			 *	There are four possible location for this file:
			 *		a. UE4/Engine/Programs/UnrealBuildTool
			 *		b. UE4/Engine/Programs/NotForLicensees/UnrealBuildTool
			 *		c. UE4/Engine/Saved/UnrealBuildTool
			 *		d. <AppData or My Documnets>/Unreal Engine/UnrealBuildTool -- the location is
			 *		   chosen by existence and if both exist most recently used.
			 *
			 *	The UBT is looking for it in all four places in the given order and
			 *	overrides already read data with the loaded ones, hence d. has the
			 *	priority. Not defined classes and fields are left alone.
			 */

			var UE4EnginePath = new FileInfo(Path.Combine(Utils.GetExecutingAssemblyDirectory(), "..", "..")).FullName;

			ConfigLocationHierarchy = new XmlConfigLocation[]
			{
				new XmlDefaultConfigLocation(Path.Combine(UE4EnginePath, "Programs", "UnrealBuildTool")),
				new XmlConfigLocation(Path.Combine(UE4EnginePath, "Programs", "NotForLicensees", "UnrealBuildTool"), "NotForLicensees"),
				new XmlConfigLocation(Path.Combine(UE4EnginePath, "Saved", "UnrealBuildTool"), "User", true),
				new XmlConfigLocation(new string[] {
					Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.ApplicationData), "Unreal Engine", "UnrealBuildTool"),
					Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.Personal), "Unreal Engine", "UnrealBuildTool")
				}, "Global", true)
			};
		}

		/// <summary>
		/// Loads BuildConfiguration from XML into memory.
		/// </summary>
		private static void LoadData()
		{
			foreach (var PossibleConfigLocation in ConfigLocationHierarchy)
			{
				if (!PossibleConfigLocation.IfShouldCreateFile())
				{
					continue;
				}

				PossibleConfigLocation.CreateUserXmlConfigTemplate();
			}

			foreach (var PossibleConfigLocation in ConfigLocationHierarchy)
			{
				if(!PossibleConfigLocation.bExists)
				{
					continue;
				}

				try
				{
					Load(PossibleConfigLocation.FSLocation);
				}
				catch(Exception Ex)
				{
					Console.WriteLine("Problem parsing {0}:\n   {1}", PossibleConfigLocation.FSLocation, Ex.ToString());
				}
			}
		}

		/// <summary>
		/// Sets values of this class with values from given XML file.
		/// </summary>
		/// <param name="ConfigurationXmlPath">The path to the file with values.</param>
		private static void Load(string ConfigurationXmlPath)
		{
			var ConfigDocument = new XmlDocument();
			var NS = new XmlNamespaceManager(ConfigDocument.NameTable);
			NS.AddNamespace("ns", "https://www.unrealengine.com/BuildConfiguration");
			ConfigDocument.Load(ConfigurationXmlPath);

			var XmlClasses = ConfigDocument.DocumentElement.SelectNodes("/ns:Configuration/*", NS);

			if(XmlClasses.Count == 0)
			{
				if(ConfigDocument.DocumentElement.Name == "Configuration")
				{
					ConfigDocument.DocumentElement.SetAttribute("xmlns", "https://www.unrealengine.com/BuildConfiguration");

					var NSDoc = new XmlDocument();

					NSDoc.LoadXml(ConfigDocument.OuterXml);

					try
					{
						File.WriteAllText(ConfigurationXmlPath, WriteToBuffer(NSDoc));
					}
					catch(Exception)
					{
						// Ignore gently.
					}

					XmlClasses = NSDoc.DocumentElement.SelectNodes("/ns:Configuration/*", NS);
				}
			}

			foreach (XmlNode XmlClass in XmlClasses)
			{
				var ClassType = Type.GetType("UnrealBuildTool." + XmlClass.Name);

				if(ClassType == null)
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
					ClassData = new XmlConfigLoaderClassData(ClassType);
					Data.Add(ClassType, ClassData);
				}

				var XmlFields = XmlClass.SelectNodes("*");

				foreach (XmlNode XmlField in XmlFields)
				{
					FieldInfo Field = ClassType.GetField(XmlField.Name);

					// allow settings in the .xml that don't exist, as another branch may have it, and can share this file from Documents
					if (Field == null)
					{
						continue;
					}
					
					if (!IsConfigurableField(Field))
					{
						throw new BuildException("BuildConfiguration Loading: field '{0}' is either non-public, non-static or not-xml-configurable.", XmlField.Name);
					}

					if(Field.FieldType.IsArray)
					{
						// If the type is an array type get items for it.
						var XmlItems = XmlField.SelectNodes("ns:Item", NS);

						// Get the C# type of the array.
						var ItemType = Field.FieldType.GetElementType();

						// Create the array according to the ItemType.
						var OutputArray = Array.CreateInstance(ItemType, XmlItems.Count);

						int Id = 0;
						foreach(XmlNode XmlItem in XmlItems)
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
			var ParseMethod = ParsingType.GetMethod("Parse", new Type[] { typeof(System.String), typeof(IFormatProvider) });

			if(ParseMethod == null)
			{
				ParseMethod = ParsingType.GetMethod("Parse", new Type[] { typeof(System.String) });
				bWithCulture = false;
			}

			if (ParseMethod == null)
			{
				throw new BuildException("BuildConfiguration Loading: Parsing of the type {0} is not supported.", ParsingType.Name);
			}

			var ParametersList = new List<object> { UnparsedValue };

			if(bWithCulture)
			{
				ParametersList.Add(CultureInfo.InvariantCulture);
			}

			try
			{
				ParsedValue = ParseMethod.Invoke(null, ParametersList.ToArray());
			}
			catch(Exception e)
			{
				if(e is TargetInvocationException &&
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
			return Class.GetFields().Any((Field) => IsConfigurableField(Field));
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
			var Method = ClassType.GetMethod(MethodName, new Type[]{}, new ParameterModifier[]{});

			if(Method != null && Method.IsPublic && Method.IsStatic)
			{
				Method.Invoke(null, new object[]{});
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
			var TypeMap = new Dictionary<Type, string>
			{
				{ typeof(string), "string" },
				{ typeof(bool), "boolean" },
				{ typeof(int), "int" },
				{ typeof(long), "long" },
				{ typeof(DateTime), "dateTime" }
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
			var NS = XNamespace.Get("http://www.w3.org/2001/XMLSchema");

			var Array = Field.GetValue(null) as IEnumerable;
			var ElementList = new List<XElement>();

			foreach(var Element in Array)
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
		private static string GetFieldXMLValue(FieldInfo Field)
		{
			return GetObjectXMLValue(Field.FieldType, Field.GetValue(null));
		}

		/// <summary>
		/// Gets custom C# object XML string representation.
		/// </summary>
		/// <param name="FieldType">The type of the field.</param>
		/// <param name="Obj">The value.</param>
		/// <returns>String representation.</returns>
		private static string GetObjectXMLValue(Type FieldType, object Obj)
		{
			if (Obj == null && FieldType == typeof(string))
			{
				return "";
			}

			if (FieldType == typeof(bool))
			{
				return (bool) Obj ? "true" : "false";
			}

			return Obj.ToString();
		}

		/// <summary>
		/// Gets standard BuildConfiguration.xml schema path.
		/// </summary>
		/// <returns>Standard BuildConfiguration.xml schema path.</returns>
		public static string GetXSDPath()
		{
			return new FileInfo(Path.Combine(Utils.GetExecutingAssemblyDirectory(), "..", "..", "Saved", "UnrealBuildTool", "BuildConfiguration.Schema.xsd")).FullName;
		}

		private static XElement CreateXSDElementForField(FieldInfo Field)
		{
			var NS = XNamespace.Get("http://www.w3.org/2001/XMLSchema");

			if (Field.FieldType.IsArray)
			{
				return new XElement(NS + "element",
					new XAttribute("name", Field.Name),
					new XAttribute("minOccurs", "0"),
					new XAttribute("maxOccurs", "1"),
					new XElement(NS + "complexType",
						new XElement(NS + "all",
							new XElement(NS + "element",
								new XAttribute("name", "Item"),
								new XAttribute("type", GetFieldXSDType(Field.FieldType.GetElementType())),
								new XAttribute("minOccurs", "0")
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
			var NS = XNamespace.Get("https://www.unrealengine.com/BuildConfiguration");

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
			using (var Reader = new XmlNodeReader(Document))
			{
				Reader.MoveToContent();
				return WriteToBuffer(XDocument.Load(Reader));
			}
		}

		private static string WriteToBuffer(XDocument Document)
		{
			var Settings = new XmlWriterSettings();
			Settings.Indent = true;
			Settings.IndentChars = "\t";
			Settings.CloseOutput = true;

			using (var MS = new MemoryStream())
			{
				using (var SW = new StreamWriter(MS, Encoding.UTF8))
				{
					using (var Writer = XmlWriter.Create(SW, Settings))
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
