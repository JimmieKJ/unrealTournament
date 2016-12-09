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
    public class BuggRepository : IBuggRepository
    {
        private readonly CrashReportEntities _entityContext;

        /// <summary>
        /// Constructor
        /// </summary>
        /// <param name="entityContext">A reference to the *single* instance of the data context.</param>
        public BuggRepository(CrashReportEntities entityContext)
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
        public IQueryable<Bugg> ListAll()
        {
            return _entityContext.Buggs;
        }

        /// <summary>
        /// Get a filtered list of Buggs from data storage
        /// Calling this method returns the data directly. It will execute the data retrieval - in this case an sql transaction.
        /// </summary>
        /// <param name="filter">A linq expression used to filter the Bugg table</param>
        /// <returns>Returns a fully filtered enumerated list object of Bugges.</returns>
        public IEnumerable<Bugg> Get(Expression<Func<Bugg, bool>> filter)
        {
            return _entityContext.Buggs.Where(filter).ToList();
        }

        /// <summary>
        /// Get a filtered, ordered list of Bugg
        /// </summary>
        /// <param name="filter">An expression used to filter the Buggs</param>
        /// <param name="orderBy">An function delegate userd to order the results from the Bugg table</param>
        /// <returns></returns>
        public IEnumerable<Bugg> Get(Expression<Func<Bugg, bool>> filter,
            Func<IQueryable<Bugg>, IOrderedQueryable<Bugg>> orderBy)
        {
            return orderBy(_entityContext.Buggs.Where(filter)).ToList();
        }

        /// <summary>
        /// Return an ordered list of Bugges with data preloading
        /// </summary>
        /// <param name="filter">A linq expression used to filter the Bugg table</param>
        /// <param name="orderBy">A linq expression used to order the results from the Bugg table</param>
        /// <param name="includeProperties">A linq expression indicating properties to dynamically load.</param>
        /// <returns></returns>
        public IEnumerable<Bugg> Get(Expression<Func<Bugg, bool>> filter,
            Func<IQueryable<Bugg>, IOrderedQueryable<Bugg>> orderBy,
            params Expression<Func<Bugg, object>>[] includeProperties)
        {
            var query = _entityContext.Buggs.Where(filter);
            foreach (var includeProperty in includeProperties)
            {
                query.Include(includeProperty);
            }

            return orderBy == null ? query.ToList() : orderBy(query).ToList();
        }

        /// <summary>
        /// Get a Bugg from it's id
        /// </summary>
        /// <param name="id">The id of the Bugg to retrieve</param>
        /// <returns>Bugg data model</returns>
        public Bugg GetById(int id)
        {
            return _entityContext.Buggs.FirstOrDefault(data => data.Id == id);
        }

        /// <summary>
        /// Check if there're any buggs matching a specific filter.
        /// </summary>
        /// <param name="filter"></param>
        /// <returns></returns>
        public bool Any(Expression<Func<Bugg, bool>> filter)
        {
            return _entityContext.Buggs.Any(filter);
        }

        /// <summary>
        /// Get the first Bugg matching a specific filter.
        /// </summary>
        /// <param name="filter"></param>
        /// <returns></returns>
        public Bugg First(Expression<Func<Bugg, bool>> filter)
        {
            return _entityContext.Buggs.FirstOrDefault(filter);
        }

        /// <summary>
        /// Add a new Bugg to the data store
        /// </summary>
        /// <param name="entity"></param>
        public void Save(Bugg entity)
        {
            _entityContext.Buggs.Add(entity);
        }

        /// <summary>
        /// Remove a Bugg from the data store
        /// </summary>
        /// <param name="entity"></param>
        public void Delete(Bugg entity)
        {
            _entityContext.Buggs.Remove(entity);
        }

        /// <summary>
        /// Update an existing Bugg
        /// </summary>
        /// <param name="entity"></param>
        public void Update(Bugg entity)
        {
            var set = _entityContext.Set<Bugg>();
            var entry = set.Local.SingleOrDefault(f => f.Id == entity.Id);

            if (entry != null)
            {
                var attachedFeature = _entityContext.Entry(entry);
                attachedFeature.CurrentValues.SetValues(entity);
                attachedFeature.State = EntityState.Modified;
            }
            else
            {
                _entityContext.Buggs.Attach(entity);
                _entityContext.Entry(entity).State = EntityState.Modified;
            }
        }
    }
}