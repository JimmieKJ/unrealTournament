using System;
using System.Collections.Generic;
using System.Data.Entity;
using System.Linq;
using System.Linq.Expressions;
using System.Web.Mvc;

namespace Tools.CrashReporter.CrashReportWebSite.DataModels.Repositories
{
    /// <summary>
    /// 
    /// </summary>
    public class CrashRepository : ICrashRepository
    {
        private readonly CrashReportEntities _entityContext;

        /// <summary>
        /// Constructor
        /// </summary>
        /// <param name="entityContext">A reference to the *single* instance of the data context.</param>
        public CrashRepository(CrashReportEntities entityContext)
        {
            _entityContext = entityContext;
            _entityContext.Database.CommandTimeout = 1200;
        }

        /// <summary>
        /// Return a queryable string for query construction.
        /// 
        /// NOTE - This is bad. Replace this method with proper expression tree construction and strictly return
        /// enumerated lists. All data handling should happen within the repository. 
        /// </summary>
        /// <returns></returns>
        public IQueryable<Crash> ListAll()
        {
            return _entityContext.Crashes;
        }

        /// <summary>
        /// Get a filtered list of crashes from data storage
        /// Calling this method returns the data directly. It will execute the data retrieval - in this case an sql transaction.
        /// </summary>
        /// <param name="filter">A linq expression used to filter the crash table</param>
        /// <returns>Returns a fully filtered enumerated list object of crashes.</returns>
        public IEnumerable<Crash> Get(Expression<Func<Crash, bool>> filter)
        {
            return _entityContext.Crashes.Where(filter).ToList();
        }

        /// <summary>
        /// Get a filtered, ordered list of Crash
        /// </summary>
        /// <param name="filter">An expression used to filter the Crashes</param>
        /// <param name="orderBy">An function delegate userd to order the results from the Crash table</param>
        /// <returns></returns>
        public IEnumerable<Crash> Get(Expression<Func<Crash, bool>> filter,
            Func<IQueryable<Crash>, IOrderedQueryable<Crash>> orderBy)
        {
            return orderBy(_entityContext.Crashes.Where(filter)).ToList();
        }

        /// <summary>
        /// Return an ordered list of Crashes with data preloading
        /// </summary>
        /// <param name="filter">A linq expression used to filter the crash table</param>
        /// <param name="orderBy">A linq expression used to order the results from the crash table</param>
        /// <param name="includeProperties">A linq expression indicating properties to dynamically load.</param>
        /// <returns></returns>
        public IEnumerable<Crash> Get(Expression<Func<Crash, bool>> filter,
            Func<IQueryable<Crash>, IOrderedQueryable<Crash>> orderBy,
            params Expression<Func<Crash, object>>[] includeProperties)
        {
            var query = _entityContext.Crashes.Where(filter);
            foreach (var includeProperty in includeProperties)
            {
                query.Include(includeProperty);
            }

            return orderBy == null ? query.ToList() : orderBy(query).ToList();
        }

        /// <summary>
        /// Get a crash from it's id
        /// </summary>
        /// <param name="id">The id of the crash to retrieve</param>
        /// <returns>Crash data model</returns>
        public Crash GetById(int id)
        {
            var result = _entityContext.Crashes.First(data => data.Id == id);
            result.ReadCrashContextIfAvailable();
            return result;
        }

        /// <summary>
        /// Check if there are any crashes matching a specific filter.
        /// </summary>
        /// <param name="filter"></param>
        /// <returns></returns>
        public bool Any(Expression<Func<Crash, bool>> filter)
        {
            return _entityContext.Crashes.Any(filter);
        }

        /// <summary>
        /// Get the first crash matching a specific filter.
        /// </summary>
        /// <param name="filter"></param>
        /// <returns></returns>
        public Crash First(Expression<Func<Crash, bool>> filter)
        {
            return _entityContext.Crashes.FirstOrDefault(filter);
        }

        /// <summary>
        /// Add a new crash to the data store
        /// </summary>
        /// <param name="entity"></param>
        public void Save(Crash entity)
        {
            _entityContext.Crashes.Add(entity);
        }

        /// <summary>
        /// Remove a crash from the data store
        /// </summary>
        /// <param name="entity"></param>
        public void Delete(Crash entity)
        {
            _entityContext.Crashes.Remove(entity);
        }

        /// <summary>
        /// Update an existing crash
        /// </summary>
        /// <param name="entity"></param>
        public void Update(Crash entity)
        {
            var set = _entityContext.Set<Crash>();
            var entry = set.Local.SingleOrDefault(f => f.Id == entity.Id);

            if (entry != null)
            {
                var attachedFeature = _entityContext.Entry(entry);
                attachedFeature.CurrentValues.SetValues(entity);
                attachedFeature.State = EntityState.Modified;
            }
            else
            {
                _entityContext.Crashes.Attach(entity);
                _entityContext.Entry(entity).State = EntityState.Modified;
            }
        }

        /// <summary>
        /// 
        /// </summary>
        /// <returns></returns>
        public List<SelectListItem> GetBranchesAsListItems()
        {
            var branchesAsSelectList = new List<SelectListItem>();
            using (FAutoScopedLogTimer LogTimer = new FAutoScopedLogTimer("CrashRepository.GetBranches"))
            {

                var date = DateTime.Now.AddDays(-14);

                var BranchList = _entityContext.Crashes
                    .Where(n => n.TimeOfCrash > date)
                    .Where(c => c.CrashType != 3) // Ignore ensures
                        // Depot - //depot/UE4* || Stream //UE4, //Something etc.
                    .Where(n => n.Branch.StartsWith("UE4") || n.Branch.StartsWith("//"))
                    .Select(n => n.Branch)
                    .Distinct()
                    .ToList();
                    branchesAsSelectList = BranchList
                        .Select(listitem => new SelectListItem { Selected = false, Text = listitem, Value = listitem })
                        .ToList();


                branchesAsSelectList.Insert(0, new SelectListItem { Selected = true, Text = "", Value = "" });
                return branchesAsSelectList;
            }
        }

        /// <summary>
        /// Static list of platforms for filtering
        /// </summary>
        public List<SelectListItem> GetPlatformsAsListItems()
        {
            var PlatformsAsListItems = new List<SelectListItem>();
            
            string[] PlatformNames = { "Win64", "Win32", "Mac", "Linux", "PS4", "XboxOne" };

            PlatformsAsListItems = PlatformNames
                .Select(listitem => new SelectListItem { Selected = false, Text = listitem, Value = listitem })
                .ToList();
                PlatformsAsListItems.Insert(0, new SelectListItem { Selected = true, Text = "", Value = "" });

            return PlatformsAsListItems;
        }

        /// <summary>
        /// Static list of Engine Modes for filtering
        /// </summary>
        public List<SelectListItem> GetEngineModesAsListItems()
        {
            string[] engineModes = { "Commandlet", "Editor", "Game", "Server" };

            List<SelectListItem> engineModesAsListItems = engineModes
                .Select(listitem => new SelectListItem { Selected = false, Text = listitem, Value = listitem })
                .ToList();
            engineModesAsListItems.Insert(0, new SelectListItem { Selected = true, Text = "", Value = "" });

            return engineModesAsListItems;
        }

        private static DateTime LastVersionDate = DateTime.UtcNow.AddDays(-1);
        private static List<SelectListItem> VersionsAsSelectList = null;
        private static HashSet<string> DistinctBuildVersions = null;
        /// <summary>
        /// Retrieves a list of distinct UE4 Versions from the CrashRepository
        /// </summary>
        public List<SelectListItem> GetVersionsAsListItems()
        {
            DateTime Now = DateTime.UtcNow;
            var date = DateTime.Now.AddDays(-14);
            if (Now - LastVersionDate > TimeSpan.FromHours(1))
            {
                var BuildVersions = _entityContext.Crashes
                .Where(c => c.TimeOfCrash > date)
                .Where(c => c.CrashType != 3) // Ignore ensures
                .Select(c => c.BuildVersion)
                .Distinct()
                .ToList();

                DistinctBuildVersions = new HashSet<string>();

                foreach (var BuildVersion in BuildVersions)
                {
                    var BVParts = BuildVersion.Split(new char[] { '.' }, StringSplitOptions.RemoveEmptyEntries);
                    if (BVParts.Length > 2 && BVParts[0] != "0")
                    {
                        string CleanBV = string.Format("{0}.{1}.{2}", BVParts[0], BVParts[1], BVParts[2]);
                        DistinctBuildVersions.Add(CleanBV);
                    }
                }

                VersionsAsSelectList = DistinctBuildVersions
                    .Select(listitem => new SelectListItem { Selected = false, Text = listitem, Value = listitem })
                    .ToList();
                VersionsAsSelectList.Insert(0, new SelectListItem { Selected = true, Text = "", Value = "" });

                LastVersionDate = Now;
            }

            return VersionsAsSelectList;
        }

        private static DateTime LastEngineVersionDate = DateTime.UtcNow.AddDays(-1);
        private static List<SelectListItem> EngineVersionsAsSelectList = null;
        private static HashSet<string> DistinctEngineVersions = null;
        public List<SelectListItem> GetEngineVersionsAsListItems()
        {
            DateTime Now = DateTime.UtcNow;
            var date = DateTime.Now.AddDays(-14);
            if (Now - LastEngineVersionDate > TimeSpan.FromHours(1))
            {
                var engineVersions = _entityContext.Crashes
                .Where(c => c.TimeOfCrash > date)
                .Where(c => c.CrashType != 3) // Ignore ensures
                .Select(c => c.EngineVersion)
                .Distinct()
                .ToList();

                DistinctEngineVersions = new HashSet<string>();

                foreach (var engineVersion in engineVersions)
                {
                    if (string.IsNullOrWhiteSpace(engineVersion))
                        continue;

                    var BVParts = engineVersion.Split(new char[] { '.' }, StringSplitOptions.RemoveEmptyEntries);
                    if (BVParts.Length > 2 && BVParts[0] != "0")
                    {
                        string CleanBV = string.Format("{0}.{1}.{2}", BVParts[0], BVParts[1], BVParts[2]);
                        DistinctEngineVersions.Add(CleanBV);
                    }
                }

                EngineVersionsAsSelectList = DistinctEngineVersions
                    .Select(listitem => new SelectListItem { Selected = false, Text = listitem, Value = listitem })
                    .ToList();
                EngineVersionsAsSelectList.Insert(0, new SelectListItem { Selected = true, Text = "", Value = "" });

                LastVersionDate = Now;
            }

            return EngineVersionsAsSelectList;
        } 


        /// <summary>
        /// Retrieves a list of distinct UE4 Versions from the CrashRepository
        /// </summary>
        public HashSet<string> GetVersions()
        {
            GetVersionsAsListItems();
            return DistinctBuildVersions;
        }
    }
}