// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.Xml;

namespace DoxygenLib
{
	public class DoxygenCompound
	{
		public string Id;
		public string Name;
		public string Kind;
		public XmlNode Node;

		public static DoxygenCompound FromXml(XmlNode Node)
		{
			DoxygenCompound Compound = new DoxygenCompound();
			Compound.Id = Node.Attributes["id"].Value;
			Compound.Kind = Node.Attributes["kind"].Value;
			Compound.Node = Node;

			XmlNode NameNode = Node.SelectSingleNode("compoundname");
			Compound.Name = (NameNode == null) ? null : NameNode.InnerText;

			return Compound;
		}
	}

	public class DoxygenEntity
	{
		public DoxygenModule Module;
		public DoxygenEntity Parent;
		public string Id;
		public string Name;
		public string Kind;
		public XmlNode Node;
		public XmlNode NamespaceNode;
		public string File;
		public int Line;
		public int Column;
		public string BodyFile;
		public int BodyStart;
		public int BodyEnd;

		public List<DoxygenEntity> Members = new List<DoxygenEntity>();

		public DoxygenEntity(DoxygenModule InModule)
		{
			Module = InModule;
			Line = Column = -1;
			BodyStart = BodyEnd = -1;
		}

		public static DoxygenEntity FromXml(DoxygenModule InModule, string Name, XmlNode Node, XmlNode NamespaceNode)
		{
			DoxygenEntity Entity = new DoxygenEntity(InModule);
			Entity.Id = Node.Attributes["id"].Value;
			Entity.Name = Name;
			Entity.Kind = Node.Attributes["kind"].Value;
			Entity.Node = Node;
			Entity.NamespaceNode = NamespaceNode;

			XmlNode LocationNode = Entity.Node.SelectSingleNode("location");
			if (LocationNode != null)
			{
				Entity.File = LocationNode.Attributes["file"].Value.Replace('/', '\\');

                // Fix Doxygen's inconsistent pathing.
                if (!Entity.File.StartsWith(InModule.BaseSrcDir))
                {
                    Entity.File = InModule.BaseSrcDir + "Source\\" + Entity.File;
                }

				XmlAttribute LineAttribute = LocationNode.Attributes["line"];
				if (LineAttribute != null)
				{
					Entity.Line = int.Parse(LineAttribute.Value);
					Entity.Column = int.Parse(LocationNode.Attributes["column"].Value);
				}

				XmlAttribute BodyFileAttribute = LocationNode.Attributes["bodyfile"];
				XmlAttribute BodyStartAttribute = LocationNode.Attributes["bodystart"];
				XmlAttribute BodyEndAttribute = LocationNode.Attributes["bodyend"];
				if (BodyFileAttribute != null && BodyStartAttribute != null && BodyEndAttribute != null)
				{
					Entity.BodyFile = BodyFileAttribute.Value.Replace('/', '\\');
                    // Fix Doxygen's inconsistent pathing.
                    if (!Entity.BodyFile.StartsWith(InModule.BaseSrcDir))
                    {
                        Entity.BodyFile = InModule.BaseSrcDir + "Source\\" + Entity.BodyFile;
                    }
                    Entity.BodyStart = Int32.Parse(BodyStartAttribute.Value);
					Entity.BodyEnd = Int32.Parse(BodyEndAttribute.Value);
				}
			}
			return Entity;
		}

		public string RelativeNameFrom(DoxygenEntity Scope)
		{
			string Result = Name;
			for (DoxygenEntity Next = Parent; Next != Scope; Next = Next.Parent)
			{
				Result = Next.Name + "::" + Result;
			}
			return Result;
		}

		public override string ToString()
		{
			return String.Format("{0}: {1}", Kind, Name);
		}
	}


	public class DoxygenModule
	{
		public readonly string Name;
		public readonly string BaseSrcDir;
		public List<DoxygenEntity> Entities = new List<DoxygenEntity>();

		public DoxygenModule(string InName, string InBaseSrcDir)
		{
			Name = InName;
			BaseSrcDir = InBaseSrcDir.Replace('/', '\\');
			if (!BaseSrcDir.EndsWith("\\")) BaseSrcDir += '\\';
		}

		public static DoxygenModule Read(string Name, string BaseSrcDir, string BaseXmlDir)
		{
			DoxygenModule Module = TryRead(Name, BaseSrcDir, BaseXmlDir);
			if (Module == null) throw new IOException("Couldn't read doxygen module");
			return Module;
		}

        public static DoxygenModule TryRead(string Name, string BaseSrcDir, string BaseXmlDir)
		{
			// Load the index
			XmlDocument Document;
			if (!TryReadXmlDocument(Path.Combine(BaseXmlDir, "index.xml"), out Document)) return null;

			// Create all the compound entities
			List<string> CompoundIdList = new List<string>();
			using (XmlNodeList NodeList = Document.SelectNodes("doxygenindex/compound"))
			{
				foreach (XmlNode Node in NodeList)
				{
					CompoundIdList.Add(Node.Attributes["refid"].Value);
				}
			}

			// Read all the compound id nodes
			List<DoxygenCompound> Compounds = new List<DoxygenCompound>();
			foreach (string CompoundId in CompoundIdList)
			{
				string EntityPath = Path.Combine(BaseXmlDir, CompoundId + ".xml");
				XmlDocument EntityDocument;
				if(TryReadXmlDocument(EntityPath, out EntityDocument))
				{
					Compounds.Add(DoxygenCompound.FromXml(EntityDocument.SelectSingleNode("doxygen/compounddef")));
				}
				else
				{
					Console.WriteLine("Couldn't read entity document: '{0}'", EntityPath);
				}
			}

			// Create the module
			DoxygenModule Module = new DoxygenModule(Name, BaseSrcDir);

			// Create all the other namespaces
			Dictionary<string, DoxygenEntity> Scopes = new Dictionary<string, DoxygenEntity>();
			foreach (DoxygenCompound Compound in Compounds)
			{
				if (Compound.Kind == "namespace")
				{
					ReadMembers(Module, Compound, Compound.Name + "::", null, Compound.Node, Module.Entities);
				}
				else if (Compound.Kind == "class" || Compound.Kind == "struct" || Compound.Kind == "union")
				{
					DoxygenEntity Entity = DoxygenEntity.FromXml(Module, Compound.Name, Compound.Node, null);
					ReadMembers(Module, Compound, "", Entity, null, Entity.Members);
					Scopes.Add(Entity.Name, Entity);
				}
			}

			// Go back over all the scopes and fixup their parents
			foreach (KeyValuePair<string, DoxygenEntity> Scope in Scopes)
			{
				int ScopeIdx = Scope.Key.LastIndexOf("::");
				if (ScopeIdx != -1 && Scopes.TryGetValue(Scope.Key.Substring(0, ScopeIdx), out Scope.Value.Parent))
				{
					Scope.Value.Parent.Members.Add(Scope.Value);
				}
				else
				{
					Module.Entities.Add(Scope.Value);
				}
			}

			// Create the module
			return Module;
		}

		protected static bool TryReadXmlDocument(string InputPath, out XmlDocument Document)
		{
			Document = null;
			try
			{
				XmlDocument NewDocument = new XmlDocument();
				NewDocument.Load(InputPath);
				Document = NewDocument;
				return true;
			}
			catch(FileNotFoundException)
			{
				Console.WriteLine("Couldn't find file '{0}'", InputPath);
				return false;
			}
			catch (XmlException Ex)
			{
				Console.WriteLine("Failed to read '{0}':\n{1}", InputPath, Ex);
				return false;
			}
		}

		protected static void ReadMembers(DoxygenModule Module, DoxygenCompound Compound, string NamePrefix, DoxygenEntity Parent, XmlNode NamespaceNode, List<DoxygenEntity> Entities)
		{
			using (XmlNodeList NodeList = Compound.Node.SelectNodes("sectiondef/memberdef"))
			{
				foreach (XmlNode Node in NodeList)
				{
					XmlNode NameNode = Node.SelectSingleNode("name");
					string Name = NamePrefix + NameNode.InnerText;
					DoxygenEntity Entity = DoxygenEntity.FromXml(Module, Name, Node, NamespaceNode);
					Entity.Parent = Parent;
					Entities.Add(Entity);
				}
			}
		}

		/*
		public DoxygenSourceFile FindSourceFile(string InFileName)
		{
			string NormalizedFileName = DoxygenSourceFile.NormalizeFileName(InFileName);
			foreach(DoxygenSourceFile SourceFile in SourceFiles)
			{
				if(SourceFile.NormalizedFileName == NormalizedFileName)
				{
					return SourceFile;
				}
			}
			return null;
		}
		 * */

		public override string ToString()
		{
			return Name;
		}
	}
}
