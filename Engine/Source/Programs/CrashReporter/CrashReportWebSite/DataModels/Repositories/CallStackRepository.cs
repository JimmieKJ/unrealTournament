using System;
using System.Collections.Generic;
using System.Data.Entity;
using System.Linq;
using System.Linq.Expressions;

namespace Tools.CrashReporter.CrashReportWebSite.DataModels.Repositories
{
    /// <summary>
    /// 
    /// </summary>
    public class CallStackRepository : ICallstackRepository
    {
        private readonly CrashReportEntities _entityContext;

        /// <summary>
        /// Constructor
        /// </summary>
        /// <param name="entityContext">A reference to the *single* instance of the data context.</param>
        public CallStackRepository(CrashReportEntities entityContext)
        {
            _entityContext = entityContext;
        }

        /// <summary>
        /// Return a queryable string for query construction.
        /// 
        /// NOTE - This is bad. Replace this method with proper expression tree construction and strictly return
        /// enumerated lists. All data handling should happen within the repository. 
        /// </summary>
        /// <returns></returns>
        public IQueryable<CallStackPattern> ListAll()
        {
            return _entityContext.CallStackPatterns;
        }

        /// <summary>
        /// Get a filtered list of CallStackPatterns from data storage
        /// Calling this method returns the data directly. It will execute the data retrieval - in this case an sql transaction.
        /// </summary>
        /// <param name="filter">A linq expression used to filter the CallStackPattern table</param>
        /// <returns>Returns a fully filtered enumerated list object of CallStackPatternes.</returns>
        public IEnumerable<CallStackPattern> Get(Expression<Func<CallStackPattern, bool>> filter)
        {
            return _entityContext.CallStackPatterns.Where(filter).ToList();
        }

        /// <summary>
        /// Get a filtered, ordered list of CallStackPattern
        /// </summary>
        /// <param name="filter">An expression used to filter the CallStackPatterns</param>
        /// <param name="orderBy">An function delegate CallStackPatternd to order the results from the CallStackPattern table</param>
        /// <returns></returns>
        public IEnumerable<CallStackPattern> Get(Expression<Func<CallStackPattern, bool>> filter,
            Func<IQueryable<CallStackPattern>, IOrderedQueryable<CallStackPattern>> orderBy)
        {
            return orderBy(_entityContext.CallStackPatterns.Where(filter)).ToList();
        }

        /// <summary>
        /// Return an ordered list of CallStackPatternes with data pre-loading
        /// </summary>
        /// <param name="filter">A linq expression used to filter the CallStackPattern table</param>
        /// <param name="orderBy">A linq expression used to order the results from the CallStackPattern table</param>
        /// <param name="includeProperties">A linq expression indicating properties to dynamically load.</param>
        /// <returns></returns>
        public IEnumerable<CallStackPattern> Get(Expression<Func<CallStackPattern, bool>> filter,
            Func<IQueryable<CallStackPattern>, IOrderedQueryable<CallStackPattern>> orderBy,
            params Expression<Func<CallStackPattern, object>>[] includeProperties)
        {
            var query = _entityContext.CallStackPatterns.Where(filter);
            foreach (var includeProperty in includeProperties)
            {
                query.Include(includeProperty);
            }

            return orderBy == null ? query.ToList() : orderBy(query).ToList();
        }

        /// <summary>
        /// Get a CallStackPattern from it's id
        /// </summary>
        /// <param name="id">The id of the CallStackPattern to retrieve</param>
        /// <returns>CallStackPattern data model</returns>
        public CallStackPattern GetById(int id)
        {
            return _entityContext.CallStackPatterns.FirstOrDefault(data => data.id == id);
        }

        /// <summary>
        /// 
        /// </summary>
        /// <param name="CallStackPatternName"></param>
        /// <returns></returns>
        public CallStackPattern GetByCallStackPatternName(string CallStackPatternName)
        {
            return _entityContext.CallStackPatterns.SqlQuery("SELECT TOP 1 * FROM CallStackPatterns WHERE CallStackPatternName LIKE '" + CallStackPatternName + "'").FirstOrDefault();
        }

        /// <summary>
        /// Check if there are any CallStackPatterns matching a specific filter.
        /// </summary>
        /// <param name="filter"></param>
        /// <returns></returns>
        public bool Any(Expression<Func<CallStackPattern, bool>> filter)
        {
            return _entityContext.CallStackPatterns.Any(filter);
        }

        /// <summary>
        /// Get the first CallStackPattern matching a specific filter.
        /// </summary>
        /// <param name="filter"></param>
        /// <returns></returns>
        public CallStackPattern First(Expression<Func<CallStackPattern, bool>> filter)
        {
            return _entityContext.CallStackPatterns.FirstOrDefault(filter);
        }

        /// <summary>
        /// Add a new CallStackPattern to the data store
        /// </summary>
        /// <param name="entity"></param>
        public void Save(CallStackPattern entity)
        {
            _entityContext.CallStackPatterns.Add(entity);
        }

        /// <summary>
        /// Remove a CallStackPattern from the data store
        /// </summary>
        /// <param name="entity"></param>
        public void Delete(CallStackPattern entity)
        {
            _entityContext.CallStackPatterns.Remove(entity);
        }

        /// <summary>
        /// Update an existing CallStackPattern
        /// </summary>
        /// <param name="entity"></param>
        public void Update(CallStackPattern entity)
        {
            var set = _entityContext.Set<CallStackPattern>();
            var entry = set.Local.SingleOrDefault(f => f.id == entity.id);

            if (entry != null)
            {
                var attachedFeature = _entityContext.Entry(entry);
                attachedFeature.CurrentValues.SetValues(entity);
                attachedFeature.State = EntityState.Modified;
            }
            else
            {
                _entityContext.CallStackPatterns.Attach(entity);
                _entityContext.Entry(entity).State = EntityState.Modified;
            }
        }
    }
}