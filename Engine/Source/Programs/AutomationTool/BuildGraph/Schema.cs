using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Linq;
using System.Reflection;
using System.Text;
using System.Threading.Tasks;
using System.Xml;
using System.Xml.Schema;
using UnrealBuildTool;

namespace AutomationTool
{
	/// <summary>
	/// Information about a parameter to a task
	/// </summary>
	[DebuggerDisplay("{Name}")]
	class ScriptTaskParameter
	{
		/// <summary>
		/// Name of this parameter
		/// </summary>
		public string Name;

		/// <summary>
		/// Information about this field
		/// </summary>
		public FieldInfo FieldInfo;

		/// <summary>
		/// Validation type for this field
		/// </summary>
		public TaskParameterValidationType ValidationType;

		/// <summary>
		/// Whether this parameter is optional
		/// </summary>
		public bool bOptional;

		/// <summary>
		/// Constructor
		/// </summary>
		public ScriptTaskParameter(string InName, FieldInfo InFieldInfo, TaskParameterValidationType InValidationType, bool bInOptional)
		{
			Name = InName;
			FieldInfo = InFieldInfo;
			ValidationType = InValidationType;
			bOptional = bInOptional;
		}
	}

	/// <summary>
	/// Helper class to serialize a task from an xml element
	/// </summary>
	[DebuggerDisplay("{Name}")]
	class ScriptTask
	{
		/// <summary>
		/// Name of this task
		/// </summary>
		public string Name;

		/// <summary>
		/// Type of the task to construct with this info
		/// </summary>
		public Type TaskClass;

		/// <summary>
		/// Type to construct with the parsed parameters
		/// </summary>
		public Type ParametersClass;

		/// <summary>
		/// Mapping of attribute name to field
		/// </summary>
		public Dictionary<string, ScriptTaskParameter> NameToParameter = new Dictionary<string,ScriptTaskParameter>();

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="InName">Name of the task</param>
		/// <param name="InTaskClass">Task class to create</param>
		/// <param name="InParametersClass">Class type of an object to be constructed and passed as an argument to the task class constructor</param>
		public ScriptTask(string InName, Type InTaskClass, Type InParametersClass)
		{
			Name = InName;
			TaskClass = InTaskClass;
			ParametersClass = InParametersClass;

			// Find all the fields which are tagged as parameters in ParametersClass
			foreach(FieldInfo Field in ParametersClass.GetFields())
			{
				if(Field.MemberType == MemberTypes.Field)
				{
					TaskParameterAttribute ParameterAttribute = Field.GetCustomAttribute<TaskParameterAttribute>();
					if(ParameterAttribute != null)
					{
						NameToParameter.Add(Field.Name, new ScriptTaskParameter(Field.Name, Field, ParameterAttribute.ValidationType, ParameterAttribute.Optional));
					}
				}
			}
		}
	}

	/// <summary>
	/// Enumeration of standard types used in the schema. Avoids hard-coding names.
	/// </summary>
	enum ScriptSchemaStandardType
	{
		Graph,
		Trigger,
		Group,
		Node,
		Aggregate,
		Notify,
		Include,
		Property,
		Local,
		Name,
		NameList,
		Tag,
		TagList,
		QualifiedName,
		BalancedString,
		Boolean,
		Integer,
	}

	/// <summary>
	/// Schema for build graph definitions. Stores information about the supported tasks, and allows validating an XML document.
	/// </summary>
	class ScriptSchema
	{
		/// <summary>
		/// Name of the root element
		/// </summary>
		public const string RootElementName = "BuildGraph";

		/// <summary>
		/// Namespace for the schema
		/// </summary>
		public const string NamespaceURI = "http://www.epicgames.com/BuildGraph";

		/// <summary>
		/// List of all the loaded classes which derive from BuildGraph.Task
		/// </summary>
		Dictionary<string, ScriptTask> NameToTask = new Dictionary<string, ScriptTask>();

		/// <summary>
		/// Qualified name for the string type
		/// </summary>
		static readonly XmlQualifiedName StringTypeName = new XmlQualifiedName("string", "http://www.w3.org/2001/XMLSchema");

		/// <summary>
		/// The inner xml schema
		/// </summary>
		public readonly XmlSchema CompiledSchema;

		/// <summary>
		/// Pattern which matches any name; alphanumeric characters, with single embedded spaces.
		/// </summary>
		const string NamePattern = @"[A-Za-z0-9_]+( [A-Za-z0-9_]+)*";

		/// <summary>
		/// Pattern which matches a list of names, separated by semicolons.
		/// </summary>
		const string NameListPattern = NamePattern + "(;" + NamePattern + ")*";

		/// <summary>
		/// Pattern which matches any tag name; a name with optional leading '#' character
		/// </summary>
		const string TagPattern = "#?" + NamePattern;

		/// <summary>
		/// Pattern which matches a list of tag names, separated by semicolons;
		/// </summary>
		const string TagListPattern = TagPattern + "(;" + TagPattern + ")*";

		/// <summary>
		/// Pattern which matches a qualified name.
		/// </summary>
		const string QualifiedNamePattern = NamePattern + "(\\." + NamePattern + ")*";

		/// <summary>
		/// Pattern which matches a property name
		/// </summary>
		const string PropertyPattern = "\\$\\(" + NamePattern + "\\)";

		/// <summary>
		/// Pattern which matches balanced parentheses in a string
		/// </summary>
		const string StringWithPropertiesPattern = "[^\\$]*" + "(" + "(" + PropertyPattern + "|" + "\\$[^\\(]" + ")" + "[^\\$]*" + ")+" + "\\$?";

		/// <summary>
		/// Pattern which matches balanced parentheses in a string
		/// </summary>
		const string BalancedStringPattern = "[^\\$]*" + "(" + "(" + PropertyPattern + "|" + "\\$[^\\(]" + ")" + "[^\\$]*" + ")*" + "\\$?";

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="InTaskNameToReflectionInfo">Mapping of task name to information about how to construct it</param>
		public ScriptSchema(Dictionary<string, ScriptTask> InNameToTask)
		{
			NameToTask = InNameToTask;

			// Create a lookup from standard types to their qualified names
			Dictionary<Type, XmlQualifiedName> TypeToSchemaTypeName = new Dictionary<Type,XmlQualifiedName>();
			TypeToSchemaTypeName.Add(typeof(String), GetQualifiedTypeName(ScriptSchemaStandardType.BalancedString));
			TypeToSchemaTypeName.Add(typeof(Boolean), GetQualifiedTypeName(ScriptSchemaStandardType.Boolean));
			TypeToSchemaTypeName.Add(typeof(Int32), GetQualifiedTypeName(ScriptSchemaStandardType.Integer));
			
			// Create all the custom user types, and add them to the qualified name lookup
			List<XmlSchemaType> UserTypes = new List<XmlSchemaType>();
			foreach(Type Type in NameToTask.Values.SelectMany(x => x.NameToParameter.Values).Select(x => x.FieldInfo.FieldType))
			{
				if(!TypeToSchemaTypeName.ContainsKey(Type))
				{
					string Name = Type.Name + "UserType";
					XmlSchemaType SchemaType = CreateUserType(Name, Type);
					UserTypes.Add(SchemaType);
					TypeToSchemaTypeName.Add(Type, new XmlQualifiedName(Name, NamespaceURI));
				}
			}

			// Create all the task types
			Dictionary<string, XmlSchemaComplexType> TaskNameToType = new Dictionary<string,XmlSchemaComplexType>();
			foreach(ScriptTask Task in NameToTask.Values)
			{
				XmlSchemaComplexType TaskType = new XmlSchemaComplexType();
				TaskType.Name = Task.Name + "TaskType";
				foreach(ScriptTaskParameter Parameter in Task.NameToParameter.Values)
				{
					XmlQualifiedName SchemaTypeName;
					if(Parameter.ValidationType == TaskParameterValidationType.Default)
					{
						SchemaTypeName = TypeToSchemaTypeName[Parameter.FieldInfo.FieldType];
					}
					else
					{
						SchemaTypeName = GetQualifiedTypeName(Parameter.ValidationType);
					}
					TaskType.Attributes.Add(CreateSchemaAttribute(Parameter.Name, SchemaTypeName, Parameter.bOptional? XmlSchemaUse.Optional : XmlSchemaUse.Required));
				}
				TaskType.Attributes.Add(CreateSchemaAttribute("If", ScriptSchemaStandardType.BalancedString, XmlSchemaUse.Optional));
				TaskNameToType.Add(Task.Name, TaskType);
			}

			// Create the schema object
			XmlSchema NewSchema = new XmlSchema();
			NewSchema.TargetNamespace = NamespaceURI;
			NewSchema.ElementFormDefault = XmlSchemaForm.Qualified;
			NewSchema.Items.Add(CreateSchemaElement(RootElementName, ScriptSchemaStandardType.Graph));
			NewSchema.Items.Add(CreateGraphType());
			NewSchema.Items.Add(CreateTriggerType());
			NewSchema.Items.Add(CreateGroupType());
			NewSchema.Items.Add(CreateNodeType(TaskNameToType));
			NewSchema.Items.Add(CreateAggregateType());
			NewSchema.Items.Add(CreateNotifyType());
			NewSchema.Items.Add(CreateIncludeType());
			NewSchema.Items.Add(CreatePropertyType(ScriptSchemaStandardType.Property));
			NewSchema.Items.Add(CreatePropertyType(ScriptSchemaStandardType.Local));
			NewSchema.Items.Add(CreateSimpleTypeFromRegex(ScriptSchemaStandardType.Name, "(" + NamePattern + "|" + StringWithPropertiesPattern + ")"));
			NewSchema.Items.Add(CreateSimpleTypeFromRegex(ScriptSchemaStandardType.NameList, "(" + NameListPattern + "|" + StringWithPropertiesPattern + ")"));
			NewSchema.Items.Add(CreateSimpleTypeFromRegex(ScriptSchemaStandardType.Tag, "(" + TagPattern + "|" + StringWithPropertiesPattern + ")"));
			NewSchema.Items.Add(CreateSimpleTypeFromRegex(ScriptSchemaStandardType.TagList, "(" + TagListPattern + "|" + StringWithPropertiesPattern + ")"));
			NewSchema.Items.Add(CreateSimpleTypeFromRegex(ScriptSchemaStandardType.QualifiedName, "(" + QualifiedNamePattern + "|" + StringWithPropertiesPattern + ")"));
			NewSchema.Items.Add(CreateSimpleTypeFromRegex(ScriptSchemaStandardType.BalancedString, BalancedStringPattern));
			NewSchema.Items.Add(CreateSimpleTypeFromRegex(ScriptSchemaStandardType.Boolean, "(" + "true" + "|" + "false" + "|" + StringWithPropertiesPattern + ")"));
			NewSchema.Items.Add(CreateSimpleTypeFromRegex(ScriptSchemaStandardType.Integer, "(" + "(-?[1-9][0-9]*|0)" + "|" + StringWithPropertiesPattern + ")"));
			foreach(XmlSchemaComplexType Type in TaskNameToType.Values)
			{
				NewSchema.Items.Add(Type);
			}
			foreach(XmlSchemaSimpleType Type in UserTypes)
			{
				NewSchema.Items.Add(Type);
			}

			// Now that we've finished, compile it and store it to the class
			XmlSchemaSet NewSchemaSet = new XmlSchemaSet();
			NewSchemaSet.Add(NewSchema);
			NewSchemaSet.Compile();
			foreach(XmlSchema NewCompiledSchema in NewSchemaSet.Schemas())
			{
				CompiledSchema = NewCompiledSchema;
			}
		}

		/// <summary>
		/// Gets information about the task with the given name
		/// </summary>
		/// <param name="TaskName">Name of the task</param>
		/// <param name="Task">Receives task info for the named task</param>
		/// <returns>True if the task name was found and Task is set, false otherwise.</returns>
		public bool TryGetTask(string TaskName, out ScriptTask Task)
		{
			return NameToTask.TryGetValue(TaskName, out Task);
		}

		/// <summary>
		/// Export the schema to a file
		/// </summary>
		/// <param name="File"></param>
		public void Export(FileReference File)
		{
			XmlWriterSettings Settings = new XmlWriterSettings();
			Settings.Indent = true;
			Settings.IndentChars = "  ";
			Settings.NewLineChars = "\n";

			using(XmlWriter Writer = XmlWriter.Create(File.FullName, Settings))
			{
				CompiledSchema.Write(Writer);
			}
		}

		/// <summary>
		/// Gets the bare name for the given script type
		/// </summary>
		/// <param name="Type">Script type to find the name of</param>
		/// <returns>Name of the schema type that matches the given script type</returns>
		static string GetTypeName(ScriptSchemaStandardType Type)
		{
			return Type.ToString() + "Type";
		}
		
		/// <summary>
		/// Gets the qualified name for the given script type
		/// </summary>
		/// <param name="Type">Script type to find the qualified name for</param>
		/// <returns>Qualified name of the schema type that matches the given script type</returns>
		static XmlQualifiedName GetQualifiedTypeName(ScriptSchemaStandardType Type)
		{
			return new XmlQualifiedName(GetTypeName(Type), NamespaceURI);
		}

		/// <summary>
		/// Gets the qualified name of the schema type for the given type of validation
		/// </summary>
		/// <returns>Qualified name for the corresponding schema type</returns>
		static XmlQualifiedName GetQualifiedTypeName(TaskParameterValidationType Type)
		{
			switch(Type)
			{
				case TaskParameterValidationType.Name:
					return GetQualifiedTypeName(ScriptSchemaStandardType.Name);
				case TaskParameterValidationType.NameList:
					return GetQualifiedTypeName(ScriptSchemaStandardType.NameList);
				case TaskParameterValidationType.Tag:
					return GetQualifiedTypeName(ScriptSchemaStandardType.Tag);
				case TaskParameterValidationType.TagList:
					return GetQualifiedTypeName(ScriptSchemaStandardType.TagList);
			}
			return null;
		}

		/// <summary>
		/// Creates the schema type representing the graph type
		/// </summary>
		/// <returns>Type definition for a graph</returns>
		static XmlSchemaType CreateGraphType()
		{
			XmlSchemaChoice GraphChoice = new XmlSchemaChoice();
			GraphChoice.MinOccurs = 0;
			GraphChoice.MaxOccursString = "unbounded";
			GraphChoice.Items.Add(CreateSchemaElement("Include", ScriptSchemaStandardType.Include));
			GraphChoice.Items.Add(CreateSchemaElement("Property", ScriptSchemaStandardType.Property));
			GraphChoice.Items.Add(CreateSchemaElement("Local", ScriptSchemaStandardType.Local));
			GraphChoice.Items.Add(CreateSchemaElement("Group", ScriptSchemaStandardType.Group));
			GraphChoice.Items.Add(CreateSchemaElement("Trigger", ScriptSchemaStandardType.Trigger));
			GraphChoice.Items.Add(CreateSchemaElement("Aggregate", ScriptSchemaStandardType.Aggregate));
			GraphChoice.Items.Add(CreateSchemaElement("Notify", ScriptSchemaStandardType.Notify));

			XmlSchemaComplexType GraphType = new XmlSchemaComplexType();
			GraphType.Name = GetTypeName(ScriptSchemaStandardType.Graph);
			GraphType.Particle = GraphChoice;
			return GraphType;
		}
		/// <summary>
		/// Creates the schema type representing the trigger type
		/// </summary>
		/// <returns>Type definition for a trigger</returns>
		static XmlSchemaType CreateTriggerType()
		{
			XmlSchemaChoice TriggerChoice = new XmlSchemaChoice();
			TriggerChoice.MinOccurs = 0;
			TriggerChoice.MaxOccursString = "unbounded";
			TriggerChoice.Items.Add(CreateSchemaElement("Property", ScriptSchemaStandardType.Property));
			TriggerChoice.Items.Add(CreateSchemaElement("Local", ScriptSchemaStandardType.Local));
			TriggerChoice.Items.Add(CreateSchemaElement("Group", ScriptSchemaStandardType.Group));
			TriggerChoice.Items.Add(CreateSchemaElement("Aggregate", ScriptSchemaStandardType.Aggregate));

			XmlSchemaComplexType TriggerType = new XmlSchemaComplexType();
			TriggerType.Name = GetTypeName(ScriptSchemaStandardType.Trigger);
			TriggerType.Particle = TriggerChoice;
			TriggerType.Attributes.Add(CreateSchemaAttribute("Name", ScriptSchemaStandardType.QualifiedName, XmlSchemaUse.Required));
			TriggerType.Attributes.Add(CreateSchemaAttribute("If", ScriptSchemaStandardType.BalancedString, XmlSchemaUse.Optional));
			return TriggerType;
		}

		/// <summary>
		/// Creates the schema type representing the group type
		/// </summary>
		/// <returns>Type definition for a group</returns>
		static XmlSchemaType CreateGroupType()
		{
			XmlSchemaChoice GroupChoice = new XmlSchemaChoice();
			GroupChoice.MinOccurs = 0;
			GroupChoice.MaxOccursString = "unbounded";
			GroupChoice.Items.Add(CreateSchemaElement("Property", ScriptSchemaStandardType.Property));
			GroupChoice.Items.Add(CreateSchemaElement("Local", ScriptSchemaStandardType.Local));
			GroupChoice.Items.Add(CreateSchemaElement("Node", ScriptSchemaStandardType.Node));

			XmlSchemaComplexType GroupType = new XmlSchemaComplexType();
			GroupType.Name = GetTypeName(ScriptSchemaStandardType.Group);
			GroupType.Particle = GroupChoice;
			GroupType.Attributes.Add(CreateSchemaAttribute("Name", StringTypeName, XmlSchemaUse.Required));
			GroupType.Attributes.Add(CreateSchemaAttribute("Agent", ScriptSchemaStandardType.NameList, XmlSchemaUse.Optional));
			GroupType.Attributes.Add(CreateSchemaAttribute("If", ScriptSchemaStandardType.BalancedString, XmlSchemaUse.Optional));
			return GroupType;
		}

		/// <summary>
		/// Creates the schema type representing the node type
		/// </summary>
		/// <returns>Type definition for a node</returns>
		static XmlSchemaType CreateNodeType(Dictionary<string, XmlSchemaComplexType> TaskNameToType)
		{
			XmlSchemaChoice NodeChoice = new XmlSchemaChoice();
			NodeChoice.MinOccurs = 0;
			NodeChoice.MaxOccursString = "unbounded";
			NodeChoice.Items.Add(CreateSchemaElement("Property", ScriptSchemaStandardType.Property));
			NodeChoice.Items.Add(CreateSchemaElement("Local", ScriptSchemaStandardType.Local));
			foreach(KeyValuePair<string, XmlSchemaComplexType> Pair in TaskNameToType.OrderBy(x => x.Key))
			{
				NodeChoice.Items.Add(CreateSchemaElement(Pair.Key, new XmlQualifiedName(Pair.Value.Name, NamespaceURI)));
			}

			XmlSchemaComplexType NodeType = new XmlSchemaComplexType();
			NodeType.Name = GetTypeName(ScriptSchemaStandardType.Node);
			NodeType.Particle = NodeChoice;
			NodeType.Attributes.Add(CreateSchemaAttribute("Name", ScriptSchemaStandardType.Name, XmlSchemaUse.Required));
			NodeType.Attributes.Add(CreateSchemaAttribute("Requires", ScriptSchemaStandardType.TagList, XmlSchemaUse.Optional));
			NodeType.Attributes.Add(CreateSchemaAttribute("Produces", ScriptSchemaStandardType.TagList, XmlSchemaUse.Optional));
			NodeType.Attributes.Add(CreateSchemaAttribute("After", ScriptSchemaStandardType.TagList, XmlSchemaUse.Optional));
			NodeType.Attributes.Add(CreateSchemaAttribute("If", ScriptSchemaStandardType.BalancedString, XmlSchemaUse.Optional));
			return NodeType;
		}

		/// <summary>
		/// Creates the schema type representing the aggregate type
		/// </summary>
		/// <returns>Type definition for an aggregate</returns>
		static XmlSchemaType CreateAggregateType()
		{
			XmlSchemaComplexType AggregateType = new XmlSchemaComplexType();
			AggregateType.Name = GetTypeName(ScriptSchemaStandardType.Aggregate);
			AggregateType.Attributes.Add(CreateSchemaAttribute("Name", ScriptSchemaStandardType.Name, XmlSchemaUse.Required));
			AggregateType.Attributes.Add(CreateSchemaAttribute("Requires", ScriptSchemaStandardType.TagList, XmlSchemaUse.Required));
			AggregateType.Attributes.Add(CreateSchemaAttribute("If", ScriptSchemaStandardType.BalancedString, XmlSchemaUse.Optional));
			return AggregateType;
		}

		/// <summary>
		/// Creates the schema type representing a notifier
		/// </summary>
		/// <returns>Type definition for a notifier</returns>
		static XmlSchemaType CreateNotifyType()
		{
			XmlSchemaComplexType AggregateType = new XmlSchemaComplexType();
			AggregateType.Name = GetTypeName(ScriptSchemaStandardType.Notify);
			AggregateType.Attributes.Add(CreateSchemaAttribute("Targets", ScriptSchemaStandardType.TagList, XmlSchemaUse.Optional));
			AggregateType.Attributes.Add(CreateSchemaAttribute("Except", ScriptSchemaStandardType.TagList, XmlSchemaUse.Optional));
			AggregateType.Attributes.Add(CreateSchemaAttribute("Nodes", ScriptSchemaStandardType.TagList, XmlSchemaUse.Optional));
			AggregateType.Attributes.Add(CreateSchemaAttribute("Triggers", ScriptSchemaStandardType.NameList, XmlSchemaUse.Optional));
			AggregateType.Attributes.Add(CreateSchemaAttribute("Users", ScriptSchemaStandardType.BalancedString, XmlSchemaUse.Optional));
			AggregateType.Attributes.Add(CreateSchemaAttribute("Submitters", ScriptSchemaStandardType.BalancedString, XmlSchemaUse.Optional));
			AggregateType.Attributes.Add(CreateSchemaAttribute("Warnings", ScriptSchemaStandardType.Boolean, XmlSchemaUse.Optional));
			AggregateType.Attributes.Add(CreateSchemaAttribute("If", ScriptSchemaStandardType.BalancedString, XmlSchemaUse.Optional));
			return AggregateType;
		}

		/// <summary>
		/// Creates the schema type representing an include type
		/// </summary>
		/// <returns>Type definition for an include directive</returns>
		static XmlSchemaType CreateIncludeType()
		{
			XmlSchemaComplexType PropertyType = new XmlSchemaComplexType();
			PropertyType.Name = GetTypeName(ScriptSchemaStandardType.Include);
			PropertyType.Attributes.Add(CreateSchemaAttribute("Script", ScriptSchemaStandardType.BalancedString, XmlSchemaUse.Required));
			PropertyType.Attributes.Add(CreateSchemaAttribute("If", ScriptSchemaStandardType.BalancedString, XmlSchemaUse.Optional));
			return PropertyType;
		}

		/// <summary>
		/// Creates the schema type representing a property type
		/// </summary>
		/// <returns>Type definition for a property</returns>
		static XmlSchemaType CreatePropertyType(ScriptSchemaStandardType StandardType)
		{
			XmlSchemaComplexType PropertyType = new XmlSchemaComplexType();
			PropertyType.Name = GetTypeName(StandardType);
			PropertyType.Attributes.Add(CreateSchemaAttribute("Name", ScriptSchemaStandardType.Name, XmlSchemaUse.Required));
			PropertyType.Attributes.Add(CreateSchemaAttribute("Value", StringTypeName, XmlSchemaUse.Required));
			PropertyType.Attributes.Add(CreateSchemaAttribute("If", ScriptSchemaStandardType.BalancedString, XmlSchemaUse.Optional));
			return PropertyType;
		}

		/// <summary>
		/// Constructs an XmlSchemaElement and initializes it with the given parameters
		/// </summary>
		/// <param name="Name">Element name</param>
		/// <param name="SchemaType">Type enumeration for the attribute</param>
		/// <returns>A new XmlSchemaElement object</returns>
		static XmlSchemaElement CreateSchemaElement(string Name, ScriptSchemaStandardType SchemaType)
		{
			return CreateSchemaElement(Name, GetQualifiedTypeName(SchemaType));
		}

		/// <summary>
		/// Constructs an XmlSchemaElement and initializes it with the given parameters
		/// </summary>
		/// <param name="Name">Element name</param>
		/// <param name="SchemaTypeName">Qualified name of the type for this element</param>
		/// <returns>A new XmlSchemaElement object</returns>
		static XmlSchemaElement CreateSchemaElement(string Name, XmlQualifiedName SchemaTypeName)
		{
			XmlSchemaElement Element = new XmlSchemaElement();
			Element.Name = Name;
			Element.SchemaTypeName = SchemaTypeName;
			return Element;
		}

		/// <summary>
		/// Constructs an XmlSchemaAttribute and initialize it with the given parameters
		/// </summary>
		/// <param name="Name">The attribute name</param>
		/// <param name="SchemaType">Type enumeration for the attribute</param>
		/// <param name="Use">Whether the attribute is required or optional</param>
		/// <returns>A new XmlSchemaAttribute object</returns>
		static XmlSchemaAttribute CreateSchemaAttribute(string Name, ScriptSchemaStandardType SchemaType, XmlSchemaUse Use)
		{
			return CreateSchemaAttribute(Name, GetQualifiedTypeName(SchemaType), Use);
		}

		/// <summary>
		/// Constructs an XmlSchemaAttribute and initialize it with the given parameters
		/// </summary>
		/// <param name="Name">The attribute name</param>
		/// <param name="SchemaTypeName">Qualified name of the type for this attribute</param>
		/// <param name="Use">Whether the attribute is required or optional</param>
		/// <returns>The new attribute</returns>
		static XmlSchemaAttribute CreateSchemaAttribute(string Name, XmlQualifiedName SchemaTypeName, XmlSchemaUse Use)
		{
			XmlSchemaAttribute Attribute = new XmlSchemaAttribute();
			Attribute.Name = Name;
			Attribute.SchemaTypeName = SchemaTypeName;
			Attribute.Use = Use;
			return Attribute;
		}

		/// <summary>
		/// Creates a simple type that matches a regex
		/// </summary>
		/// <param name="Type">The type enumeration to define</param>
		/// <param name="Pattern">Regex pattern to match</param>
		/// <returns>A simple type which will match the given pattern</returns>
		static XmlSchemaSimpleType CreateSimpleTypeFromRegex(ScriptSchemaStandardType Type, string Pattern)
		{
			XmlSchemaPatternFacet PatternFacet = new XmlSchemaPatternFacet();
			PatternFacet.Value = Pattern;

			XmlSchemaSimpleTypeRestriction Restriction = new XmlSchemaSimpleTypeRestriction();
			Restriction.BaseTypeName = StringTypeName;
			Restriction.Facets.Add(PatternFacet);

			XmlSchemaSimpleType SimpleType = new XmlSchemaSimpleType();
			SimpleType.Name = GetTypeName(Type);
			SimpleType.Content = Restriction;
			return SimpleType;
		}

		/// <summary>
		/// Create a schema type for the given user type. Currently only handles enumerations.
		/// </summary>
		/// <param name="Name">Name for the new type</param>
		/// <param name="Type">CLR type information to create a schema type for</param>
		static XmlSchemaType CreateUserType(string Name, Type Type)
		{
			if(Type.IsEnum)
			{
				return CreateEnumType(Name, Type);
			}
			else
			{
				throw new AutomationException("Cannot create custom type in schema for '{0}'", Type.Name);
			}
		}

		/// <summary>
		/// Create a schema type for the given enum.
		/// </summary>
		/// <param name="Name">Name for the new type</param>
		/// <param name="Type">CLR type information to create a schema type for</param>
		static XmlSchemaType CreateEnumType(string Name, Type Type)
		{
			XmlSchemaSimpleTypeRestriction Restriction = new XmlSchemaSimpleTypeRestriction();
			Restriction.BaseTypeName = StringTypeName;

			foreach(string EnumName in Enum.GetNames(Type))
			{
				XmlSchemaEnumerationFacet Facet = new XmlSchemaEnumerationFacet();
				Facet.Value = EnumName;
				Restriction.Facets.Add(Facet);
			}

			XmlSchemaSimpleType SchemaType = new XmlSchemaSimpleType();
			SchemaType.Name = Name;
			SchemaType.Content = Restriction;
			return SchemaType;
		}
	}
}
