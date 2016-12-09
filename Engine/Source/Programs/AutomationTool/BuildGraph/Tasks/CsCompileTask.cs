using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.Xml;
using UnrealBuildTool;

namespace AutomationTool.Tasks
{
	/// <summary>
	/// Parameters for a task that compiles a C# project
	/// </summary>
	public class CsCompileTaskParameters
	{
		/// <summary>
		/// The C# project file to be compile. More than one project file can be specified by separating with semicolons.
		/// </summary>
		[TaskParameter]
		public string Project;

		/// <summary>
		/// The configuration to compile
		/// </summary>
		[TaskParameter(Optional = true)]
		public string Configuration;

		/// <summary>
		/// The platform to compile
		/// </summary>
		[TaskParameter(Optional = true)]
		public string Platform;

		/// <summary>
		/// Additional options to pass to the compiler
		/// </summary>
		[TaskParameter(Optional = true)]
		public string Arguments;

		/// <summary>
		/// Only enumerate build products; do not actually compile the projects.
		/// </summary>
		[TaskParameter(Optional = true)]
		public bool EnumerateOnly;

		/// <summary>
		/// Tag to be applied to build products of this task
		/// </summary>
		[TaskParameter(Optional = true, ValidationType = TaskParameterValidationType.TagList)]
		public string Tag;

		/// <summary>
		/// Tag to be applied to any non-private references the projects have
		/// (i.e. those that are external and not copied into the output dir)
		/// </summary>
		[TaskParameter(Optional = true, ValidationType = TaskParameterValidationType.TagList)]
		public string TagReferences;
	}

	/// <summary>
	/// Compiles C# project files, and their dependencies.
	/// </summary>
	[TaskElement("CsCompile", typeof(CsCompileTaskParameters))]
	public class CsCompileTask : CustomTask
	{
		/// <summary>
		/// Parameters for the task
		/// </summary>
		CsCompileTaskParameters Parameters;

		/// <summary>
		/// Constructor.
		/// </summary>
		/// <param name="InParameters">Parameters for this task</param>
		public CsCompileTask(CsCompileTaskParameters InParameters)
		{
			Parameters = InParameters;
		}

		/// <summary>
		/// Execute the task.
		/// </summary>
		/// <param name="Job">Information about the current job</param>
		/// <param name="BuildProducts">Set of build products produced by this node.</param>
		/// <param name="TagNameToFileSet">Mapping from tag names to the set of files they include</param>
		/// <returns>True if the task succeeded</returns>
		public override bool Execute(JobContext Job, HashSet<FileReference> BuildProducts, Dictionary<string, HashSet<FileReference>> TagNameToFileSet)
		{
			// Get the project file
			HashSet<FileReference> ProjectFiles = ResolveFilespec(CommandUtils.RootDirectory, Parameters.Project, TagNameToFileSet);
			foreach(FileReference ProjectFile in ProjectFiles)
			{
				if(!ProjectFile.Exists())
				{
					CommandUtils.LogError("Couldn't find project file '{0}'", ProjectFile.FullName);
					return false;
				}
				if(!ProjectFile.HasExtension(".csproj"))
				{
					CommandUtils.LogError("File '{0}' is not a C# project", ProjectFile.FullName);
					return false;
				}
			}

			// Get the default properties
			Dictionary<string, string> Properties = new Dictionary<string,string>(StringComparer.InvariantCultureIgnoreCase);
			if(!String.IsNullOrEmpty(Parameters.Platform))
			{
				Properties["Platform"] = Parameters.Platform;
			}
			if(!String.IsNullOrEmpty(Parameters.Configuration))
			{
				Properties["Configuration"] = Parameters.Configuration;
			}
	
			// Build the arguments and run the build
			if(!Parameters.EnumerateOnly)
			{
				List<string> Arguments = new List<string>();
				foreach(KeyValuePair<string, string> PropertyPair in Properties)
				{
					Arguments.Add(String.Format("/property:{0}={1}", CommandUtils.MakePathSafeToUseWithCommandLine(PropertyPair.Key), CommandUtils.MakePathSafeToUseWithCommandLine(PropertyPair.Value)));
				}
				if(!String.IsNullOrEmpty(Parameters.Arguments))
				{
					Arguments.Add(Parameters.Arguments);
				}
				Arguments.Add("/verbosity:minimal");
				Arguments.Add("/nologo");
				foreach(FileReference ProjectFile in ProjectFiles)
				{
					CommandUtils.MsBuild(CommandUtils.CmdEnv, ProjectFile.FullName, String.Join(" ", Arguments), null);
				}
			}

			// Try to figure out the output files
			HashSet<FileReference> ProjectBuildProducts;
			HashSet<FileReference> ProjectReferences;
			if(!FindBuildProducts(ProjectFiles, Properties, out ProjectBuildProducts, out ProjectReferences))
			{
				return false;
			}

			// Apply the optional tag to the produced archive
			foreach(string TagName in FindTagNamesFromList(Parameters.Tag))
			{
				FindOrAddTagSet(TagNameToFileSet, TagName).UnionWith(ProjectBuildProducts);
			}

			if (!String.IsNullOrEmpty(Parameters.TagReferences))
			{
				// Apply the optional tag to any references
				foreach (string TagName in FindTagNamesFromList(Parameters.TagReferences))
				{
					FindOrAddTagSet(TagNameToFileSet, TagName).UnionWith(ProjectReferences);
				}
			}

			// Merge them into the standard set of build products
			BuildProducts.UnionWith(ProjectBuildProducts);
			return true;
		}

		/// <summary>
		/// Output this task out to an XML writer.
		/// </summary>
		public override void Write(XmlWriter Writer)
		{
			Write(Writer, Parameters);
		}

		/// <summary>
		/// Find all the tags which are used as inputs to this task
		/// </summary>
		/// <returns>The tag names which are read by this task</returns>
		public override IEnumerable<string> FindConsumedTagNames()
		{
			return FindTagNamesFromFilespec(Parameters.Project);
		}

		/// <summary>
		/// Find all the tags which are modified by this task
		/// </summary>
		/// <returns>The tag names which are modified by this task</returns>
		public override IEnumerable<string> FindProducedTagNames()
		{
			foreach (string TagName in FindTagNamesFromList(Parameters.Tag))
			{
				yield return TagName;
			}

			foreach (string TagName in FindTagNamesFromList(Parameters.TagReferences))
			{
				yield return TagName;
			}
		}

		/// <summary>
		/// Find all the build products created by compiling the given project file
		/// </summary>
		/// <param name="ProjectFiles">Initial project file to read. All referenced projects will also be read.</param>
		/// <param name="InitialProperties">Mapping of property name to value</param>
		/// <param name="OutBuildProducts">Receives a set of build products on success</param>
		/// <param name="OutReferences">Receives a set of non-private references on success</param>
		/// <returns>True if the build products were found, false otherwise.</returns>
		static bool FindBuildProducts(HashSet<FileReference> ProjectFiles, Dictionary<string, string> InitialProperties, out HashSet<FileReference> OutBuildProducts, out HashSet<FileReference> OutReferences)
		{
			// Read all the project information into a dictionary
			Dictionary<FileReference, CsProjectInfo> FileToProjectInfo = new Dictionary<FileReference,CsProjectInfo>();
			foreach(FileReference ProjectFile in ProjectFiles)
			{
				if(!ReadProjectsRecursively(ProjectFile, InitialProperties, FileToProjectInfo))
				{
					OutBuildProducts = null;
					OutReferences = null;
					return false;
				}
			}

			// Find all the build products and references
			HashSet<FileReference> BuildProducts = new HashSet<FileReference>();
			HashSet<FileReference> References = new HashSet<FileReference>();
			foreach(KeyValuePair<FileReference, CsProjectInfo> Pair in FileToProjectInfo)
			{
				CsProjectInfo ProjectInfo = Pair.Value;

				// Add the standard build products
				DirectoryReference OutputDir = ProjectInfo.GetOutputDir(Pair.Key.Directory);
				ProjectInfo.AddBuildProducts(OutputDir, BuildProducts);

				// Add the referenced assemblies
				foreach(KeyValuePair<FileReference, bool> Reference in ProjectInfo.References)
				{
					FileReference OtherAssembly = Reference.Key;
					if (Reference.Value)
					{
						// Add reference from the output dir
						FileReference OutputFile = FileReference.Combine(OutputDir, OtherAssembly.GetFileName());
						BuildProducts.Add(OutputFile);

						FileReference OutputSymbolFile = OutputFile.ChangeExtension(".pdb");
						if(OutputSymbolFile.Exists())
						{
							BuildProducts.Add(OutputSymbolFile);
						}
					}
					else
					{
						// Add reference directly
						References.Add(OtherAssembly);
						FileReference SymbolFile = OtherAssembly.ChangeExtension(".pdb");
						if(SymbolFile.Exists())
						{
							References.Add(SymbolFile);
						}
					}
				}

				// Add build products from all the referenced projects. MSBuild only copy the directly referenced build products, not recursive references or other assemblies.
				foreach(CsProjectInfo OtherProjectInfo in ProjectInfo.ProjectReferences.Where(x => x.Value).Select(x => FileToProjectInfo[x.Key]))
				{
					OtherProjectInfo.AddBuildProducts(OutputDir, BuildProducts);
				}
			}

			// Update the output set
			OutBuildProducts = BuildProducts;
			OutReferences = References;
			return true;
		}

		/// <summary>
		/// Read a project file, plus all the project files it references.
		/// </summary>
		/// <param name="File">Project file to read</param>
		/// <param name="InitialProperties">Mapping of property name to value for the initial project</param>
		/// <param name="FileToProjectInfo"></param>
		/// <returns>True if the projects were read correctly, false (and prints an error to the log) if not</returns>
		static bool ReadProjectsRecursively(FileReference File, Dictionary<string, string> InitialProperties, Dictionary<FileReference, CsProjectInfo> FileToProjectInfo)
		{
			// Early out if we've already read this project, return success
			if(FileToProjectInfo.ContainsKey(File))
			{
				return true;
			}

			// Try to read this project
			CsProjectInfo ProjectInfo;
			if(!CsProjectInfo.TryRead(File, InitialProperties, out ProjectInfo))
			{
				CommandUtils.LogError("Couldn't read project '{0}'", File.FullName);
				return false;
			}

			// Add it to the project lookup, and try to read all the projects it references
			FileToProjectInfo.Add(File, ProjectInfo);
			return ProjectInfo.ProjectReferences.Keys.All(x => ReadProjectsRecursively(x, InitialProperties, FileToProjectInfo));
		}
	}

	/// <summary>
	/// Basic information from a preprocessed C# project file. Supports reading a project file, expanding simple conditions in it, parsing property values, assembly references and references to other projects.
	/// </summary>
	class CsProjectInfo
	{
		/// <summary>
		/// Evaluated properties from the project file
		/// </summary>
		public Dictionary<string, string> Properties;

		/// <summary>
		/// Mapping of referenced assemblies to their 'CopyLocal' (aka 'Private') setting.
		/// </summary>
		public Dictionary<FileReference, bool> References = new Dictionary<FileReference,bool>();

		/// <summary>
		/// Mapping of referenced projects to their 'CopyLocal' (aka 'Private') setting.
		/// </summary>
		public Dictionary<FileReference, bool> ProjectReferences = new Dictionary<FileReference,bool>();

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="InProperties">Initial mapping of property names to values</param>
		CsProjectInfo(Dictionary<string, string> InProperties)
		{
			Properties = new Dictionary<string,string>(InProperties);
		}

		/// <summary>
		/// Resolve the project's output directory
		/// </summary>
		/// <param name="BaseDirectory">Base directory to resolve relative paths to</param>
		/// <returns>The configured output directory</returns>
		public DirectoryReference GetOutputDir(DirectoryReference BaseDirectory)
		{
			string OutputPath;
			if(Properties.TryGetValue("OutputPath", out OutputPath))
			{
				return DirectoryReference.Combine(BaseDirectory, OutputPath);
			}
			else
			{
				return BaseDirectory;
			}
		}

		/// <summary>
		/// Adds build products from the project to the given set.
		/// </summary>
		/// <param name="OutputDir">Output directory for the build products. May be different to the project's output directory in the case that we're copying local to another project.</param>
		/// <param name="BuildProducts">Set to receive the list of build products</param>
		public bool AddBuildProducts(DirectoryReference OutputDir, HashSet<FileReference> BuildProducts)
		{
			string OutputType, AssemblyName;
			if(Properties.TryGetValue("OutputType", out OutputType) && Properties.TryGetValue("AssemblyName", out AssemblyName))
			{
				switch(OutputType)
				{
					case "Exe":
					case "WinExe":
						BuildProducts.Add(FileReference.Combine(OutputDir, AssemblyName + ".exe"));
						AddOptionalBuildProduct(FileReference.Combine(OutputDir, AssemblyName + ".pdb"), BuildProducts);
						AddOptionalBuildProduct(FileReference.Combine(OutputDir, AssemblyName + ".exe.config"), BuildProducts);
						AddOptionalBuildProduct(FileReference.Combine(OutputDir, AssemblyName + ".exe.mdb"), BuildProducts);
						return true;
					case "Library":
						BuildProducts.Add(FileReference.Combine(OutputDir, AssemblyName + ".dll"));
						AddOptionalBuildProduct(FileReference.Combine(OutputDir, AssemblyName + ".pdb"), BuildProducts);
						AddOptionalBuildProduct(FileReference.Combine(OutputDir, AssemblyName + ".dll.config"), BuildProducts);
						AddOptionalBuildProduct(FileReference.Combine(OutputDir, AssemblyName + ".dll.mdb"), BuildProducts);
						return true;
				}
			}
			return false;
		}

		/// <summary>
		/// Adds a build product to the output list if it exists
		/// </summary>
		/// <param name="BuildProduct">The build product to add</param>
		/// <param name="BuildProducts">List of output build products</param>
		public static void AddOptionalBuildProduct(FileReference BuildProduct, HashSet<FileReference> BuildProducts)
		{
			if(BuildProduct.Exists())
			{
				BuildProducts.Add(BuildProduct);
			}
		}

		/// <summary>
		/// Attempts to read project information for the given file.
		/// </summary>
		/// <param name="File">The project file to read</param>
		/// <param name="Properties">Initial set of property values</param>
		/// <param name="OutProjectInfo">If successful, the parsed project info</param>
		/// <returns>True if the project was read successfully, false otherwise</returns>
		public static bool TryRead(FileReference File, Dictionary<string, string> Properties, out CsProjectInfo OutProjectInfo)
		{
			// Read the project file
			XmlDocument Document = new XmlDocument();
			Document.Load(File.FullName);

			// Check the root element is the right type
//			HashSet<FileReference> ProjectBuildProducts = new HashSet<FileReference>();
			if(Document.DocumentElement.Name != "Project")
			{
				OutProjectInfo = null;
				return false;
			}

			// Parse the basic structure of the document, updating properties and recursing into other referenced projects as we go
			CsProjectInfo ProjectInfo = new CsProjectInfo(Properties);
			foreach(XmlElement Element in Document.DocumentElement.ChildNodes.OfType<XmlElement>())
			{
				switch(Element.Name)
				{
					case "PropertyGroup":
						if(EvaluateCondition(Element, ProjectInfo.Properties))
						{
							ParsePropertyGroup(Element, ProjectInfo.Properties);
						}
						break;
					case "ItemGroup":
						if(EvaluateCondition(Element, ProjectInfo.Properties))
						{
							ParseItemGroup(File.Directory, Element, ProjectInfo);
						}
						break;
				}
			}

			// Return the complete project
			OutProjectInfo = ProjectInfo;
			return true;
		}

		/// <summary>
		/// Parses a 'PropertyGroup' element.
		/// </summary>
		/// <param name="ParentElement">The parent 'PropertyGroup' element</param>
		/// <param name="Properties">Dictionary mapping property names to values</param>
		static void ParsePropertyGroup(XmlElement ParentElement, Dictionary<string, string> Properties)
		{
			// We need to know the overridden output type and output path for the selected configuration.
			foreach(XmlElement Element in ParentElement.ChildNodes.OfType<XmlElement>())
			{
				if(EvaluateCondition(Element, Properties))
				{
					Properties[Element.Name] = ExpandProperties(Element.InnerText, Properties);
				}
			}
		}

		/// <summary>
		/// Parses an 'ItemGroup' element.
		/// </summary>
		/// <param name="BaseDirectory">Base directory to resolve relative paths against</param>
		/// <param name="ParentElement">The parent 'ItemGroup' element</param>
		/// <param name="ProjectInfo">Project info object to be updated</param>
		static void ParseItemGroup(DirectoryReference BaseDirectory, XmlElement ParentElement, CsProjectInfo ProjectInfo)
		{
			// Parse any external assembly references
			foreach(XmlElement ItemElement in ParentElement.ChildNodes.OfType<XmlElement>())
			{
				switch(ItemElement.Name)
				{
					case "Reference":
						// Reference to an external assembly
						if(EvaluateCondition(ItemElement, ProjectInfo.Properties))
						{
							ParseReference(BaseDirectory, ItemElement, ProjectInfo.References);
						}
						break;
					case "ProjectReference":
						// Reference to another project
						if(EvaluateCondition(ItemElement, ProjectInfo.Properties))
						{
							ParseProjectReference(BaseDirectory, ItemElement, ProjectInfo.ProjectReferences);
						}
						break;
				}
			}
		}

		/// <summary>
		/// Parses an assembly reference from a given 'Reference' element
		/// </summary>
		/// <param name="BaseDirectory">Directory to resolve relative paths against</param>
		/// <param name="ParentElement">The parent 'Reference' element</param>
		/// <param name="References">Dictionary of project files to a bool indicating whether the assembly should be copied locally to the referencing project.</param>
		static void ParseReference(DirectoryReference BaseDirectory, XmlElement ParentElement, Dictionary<FileReference, bool> References)
		{
			string HintPath = GetChildElementString(ParentElement, "HintPath", null);
			if(!String.IsNullOrEmpty(HintPath))
			{
				FileReference AssemblyFile = FileReference.Combine(BaseDirectory, HintPath);
				bool bPrivate = GetChildElementBoolean(ParentElement, "Private", true);
				References.Add(AssemblyFile, bPrivate);
			}
		}

		/// <summary>
		/// Parses a project reference from a given 'ProjectReference' element
		/// </summary>
		/// <param name="BaseDirectory">Directory to resolve relative paths against</param>
		/// <param name="ParentElement">The parent 'ProjectReference' element</param>
		/// <param name="ProjectReferences">Dictionary of project files to a bool indicating whether the outputs of the project should be copied locally to the referencing project.</param>
		static void ParseProjectReference(DirectoryReference BaseDirectory, XmlElement ParentElement, Dictionary<FileReference, bool> ProjectReferences)
		{
			string IncludePath = ParentElement.GetAttribute("Include");
			if(!String.IsNullOrEmpty(IncludePath))
			{
				FileReference ProjectFile = FileReference.Combine(BaseDirectory, IncludePath);
				bool bPrivate = GetChildElementBoolean(ParentElement, "Private", true);
				ProjectReferences[ProjectFile] = bPrivate;
			}
		}

		/// <summary>
		/// Reads the inner text of a child XML element
		/// </summary>
		/// <param name="ParentElement">The parent element to check</param>
		/// <param name="Name">Name of the child element</param>
		/// <param name="DefaultValue">Default value to return if the child element is missing</param>
		/// <returns>The contents of the child element, or default value if it's not present</returns>
		static string GetChildElementString(XmlElement ParentElement, string Name, string DefaultValue)
		{
			XmlElement ChildElement = ParentElement.ChildNodes.OfType<XmlElement>().FirstOrDefault(x => x.Name == Name);
			if(ChildElement == null)
			{
				return DefaultValue;
			}
			else
			{
				return ChildElement.InnerText ?? DefaultValue;
			}
		}

		/// <summary>
		/// Read a child XML element with the given name, and parse it as a boolean.
		/// </summary>
		/// <param name="ParentElement">Parent element to check</param>
		/// <param name="Name">Name of the child element to look for</param>
		/// <param name="DefaultValue">Default value to return if the element is missing or not a valid bool</param>
		/// <returns>The parsed boolean, or the default value</returns>
		static bool GetChildElementBoolean(XmlElement ParentElement, string Name, bool DefaultValue)
		{
			string Value = GetChildElementString(ParentElement, Name, null);
			if(Value == null)
			{
				return DefaultValue;
			}
			else if(Value.Equals("True", StringComparison.InvariantCultureIgnoreCase))
			{
				return true;
			}
			else if(Value.Equals("False", StringComparison.InvariantCultureIgnoreCase))
			{
				return false;
			}
			else
			{
				return DefaultValue;
			}
		}

		/// <summary>
		/// Evaluate whether the optional MSBuild condition on an XML element evaluates to true. Currently only supports 'ABC' == 'DEF' style expressions, but can be expanded as needed.
		/// </summary>
		/// <param name="Element">The XML element to check</param>
		/// <param name="Properties">Dictionary mapping from property names to values.</param>
		/// <returns></returns>
		static bool EvaluateCondition(XmlElement Element, Dictionary<string, string> Properties)
		{
			// Read the condition attribute. If it's not present, assume it evaluates to true.
			string Condition = Element.GetAttribute("Condition");
			if(String.IsNullOrEmpty(Condition))
			{
				return true;
			}

			// Expand all the properties
			Condition = ExpandProperties(Condition, Properties);

			// Tokenize the condition
			string[] Tokens = Tokenize(Condition);

			// Try to evaluate it. We only support a very limited class of condition expressions at the moment, but it's enough to parse standard projects
			bool bResult;
			if(Tokens.Length == 3 && Tokens[0].StartsWith("'") && Tokens[1] == "==" && Tokens[2].StartsWith("'"))
			{
				bResult = String.Compare(Tokens[0], Tokens[2], StringComparison.InvariantCultureIgnoreCase) == 0; 
			}
			else
			{
				throw new AutomationException("Couldn't parse condition in project file");
			}
			return bResult;
		}

		/// <summary>
		/// Expand MSBuild properties within a string. If referenced properties are not in this dictionary, the process' environment variables are expanded. Unknown properties are expanded to an empty string.
		/// </summary>
		/// <param name="Text">The input string to expand</param>
		/// <param name="Properties">Dictionary mapping from property names to values.</param>
		/// <returns>String with all properties expanded.</returns>
		static string ExpandProperties(string Text, Dictionary<string, string> Properties)
		{
			string NewText = Text;
			for (int Idx = NewText.IndexOf("$("); Idx != -1; Idx = NewText.IndexOf("$(", Idx))
			{
				// Find the end of the variable name
				int EndIdx = NewText.IndexOf(')', Idx + 2);
				if (EndIdx != -1)
				{
					// Extract the variable name from the string
					string Name = NewText.Substring(Idx + 2, EndIdx - (Idx + 2));

					// Find the value for it, either from the dictionary or the environment block
					string Value;
					if(!Properties.TryGetValue(Name, out Value))
					{
						Value = Environment.GetEnvironmentVariable(Name) ?? "";
					}

					// Replace the variable, or skip past it
					NewText = NewText.Substring(0, Idx) + Value + NewText.Substring(EndIdx + 1);

					// Make sure we skip over the expanded variable; we don't want to recurse on it.
					Idx += Value.Length;
				}
			}
			return NewText;
		}

		/// <summary>
		/// Split an MSBuild condition into tokens
		/// </summary>
		/// <param name="Condition">The condition expression</param>
		/// <returns>Array of the parsed tokens</returns>
		static string[] Tokenize(string Condition)
		{
			List<string> Tokens = new List<string>();
			for(int Idx = 0; Idx < Condition.Length; Idx++)
			{
				if(Idx + 1 < Condition.Length && Condition[Idx] == '=' && Condition[Idx + 1] == '=')
				{
					// "==" operator
					Idx++;
					Tokens.Add("==");
				}
				else if(Condition[Idx] == '\'')
				{
					// Quoted string
					int StartIdx = Idx++;
					while(Idx + 1 < Condition.Length && Condition[Idx] != '\'')
					{
						Idx++;
					}
					Tokens.Add(Condition.Substring(StartIdx, Idx - StartIdx));
				}
				else if(!Char.IsWhiteSpace(Condition[Idx]))
				{
					// Other token; assume a single character.
					string Token = Condition.Substring(Idx, 1);
					Tokens.Add(Token);
				}
			}
			return Tokens.ToArray();
		}
	}
}
