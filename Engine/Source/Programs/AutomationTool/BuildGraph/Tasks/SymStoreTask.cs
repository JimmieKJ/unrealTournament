
using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text;
using System.Threading;
using System.Xml;
using UnrealBuildTool;

namespace AutomationTool
{
    /// <summary>
    /// Parameters for a task that uploads symbols to a symbol server
    /// </summary>
    public class SymStoreTaskParameters
    {
        /// <summary>
        /// The platform toolchain required to handle symbol files.
        /// </summary>
        [TaskParameter]
        public UnrealTargetPlatform Platform;

        /// <summary>
        /// List of output files. PDBs will be extracted from this list.
        /// </summary>
        [TaskParameter]
        public string Files;

        /// <summary>
        /// Output directory for the compressed symbols.
        /// </summary>
        [TaskParameter]
        public string StoreDir;

        /// <summary>
        /// Name of the product for the symbol store records.
        /// </summary>
        [TaskParameter]
        public string Product;
    }

    /// <summary>
    /// Task which strips symbols from a set of files
    /// </summary>
    [TaskElement("SymStore", typeof(SymStoreTaskParameters))]
    public class SymStoreTask : CustomTask
    {
        /// <summary>
        /// Parameters for this task
        /// </summary>
        SymStoreTaskParameters Parameters;

        /// <summary>
        /// Construct a spawn task
        /// </summary>
        /// <param name="InParameters">Parameters for the task</param>
        public SymStoreTask(SymStoreTaskParameters InParameters)
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
            bool bSuccess = false;

            UEBuildPlatform Platform = UEBuildPlatform.GetBuildPlatform(Parameters.Platform);
            UEToolChain ToolChain = Platform.CreateContext(null).CreateToolChainForDefaultCppPlatform();
            
            // Find the matching files
            List<FileReference> Files = ResolveFilespec(CommandUtils.RootDirectory, Parameters.Files, TagNameToFileSet).ToList();
            
            // Get the symbol store directory
            DirectoryReference StoreDir = ResolveDirectory(Parameters.StoreDir);

            // Take the lock before accessing the symbol server
            LockFile.TakeLock(StoreDir, TimeSpan.FromMinutes(15), () =>
            {
                bSuccess = ToolChain.PublishSymbols(StoreDir, Files, Parameters.Product);
            });

            if (!bSuccess)
                CommandUtils.LogError("Failure publishing symbol files.");

            return bSuccess;
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
            return FindTagNamesFromFilespec(Parameters.Files);
        }

        /// <summary>
        /// Find all the tags which are modified by this task
        /// </summary>
        /// <returns>The tag names which are modified by this task</returns>
        public override IEnumerable<string> FindProducedTagNames()
        {
            yield break;
        }
    }
}
