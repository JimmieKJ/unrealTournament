using System;
using System.Collections.Generic;
using System.Data.Entity;
using System.Linq;
using System.Linq.Expressions;

namespace Tools.CrashReporter.CrashReportWebSite.DataModels.Repositories
{
    public class DataRepository<T> : IDataRepository<T> where T : class
    {
        private readonly CrashReportEntities _entityContext;

        /// <summary>
        /// Constructor
        /// </summary>
        /// <param name="entityContext">A reference to the *single* instance of the data context.</param>
        public DataRepository(CrashReportEntities entityContext)
        {
            _entityContext = entityContext;
        }

        /// <summary>
        /// Return a queryable string for query construction.
        /// 
        /// NOTE - This is bad. Replace this method with proper expression tree construction and strictly return
        /// enumerated lisSet<T>(). All data handling should happen within the repository. 
        /// </summary>
        /// <returns></returns>
        public IQueryable<T> ListAll()
        {
            return _entityContext.Set<T>();
        }

        /// <summary>
        /// Get a filtered list of type T from data storage
        /// Calling this method returns the data directly. It will execute the data retrieval - in this case an sql transaction.
        /// </summary>
        /// <param name="filter">A linq expression used to filter the T table</param>
        /// <returns>Returns a fully filtered enumerated list object of Tes.</returns>
        public IEnumerable<T> Get(Expression<Func<T, bool>> filter)
        {
            return _entityContext.Set<T>().Where(filter).ToList();
        }

        /// <summary>
        /// Get a filtered, ordered list of T
        /// </summary>
        /// <param name="filter">An expression used to filter the Set<T>()</param>
        /// <param name="orderBy">An function delegate userd to order the resulSet<T>() from the T table</param>
        /// <returns></returns>
        public IEnumerable<T> Get(Expression<Func<T, bool>> filter,
            Func<IQueryable<T>, IOrderedQueryable<T>> orderBy)
        {
            return orderBy(_entityContext.Set<T>().Where(filter)).ToList();
        }

        /// <summary>
        /// Return an ordered list of type T with data preloading
        /// </summary>
        /// <param name="filter">A linq expression used to filter the T table</param>
        /// <param name="orderBy">A linq expression used to order the resulSet<T>() from the T table</param>
        /// <param name="includeProperties">A linq expression indicating properties to dynamically load.</param>
        /// <returns></returns>
        public IEnumerable<T> Get(Expression<Func<T, bool>> filter,
            Func<IQueryable<T>, IOrderedQueryable<T>> orderBy,
            params Expression<Func<T, object>>[] includeProperties)
        {
            var query = _entityContext.Set<T>().Where(filter);
            foreach (var includeProperty in includeProperties)
            {
                query.Include(includeProperty);
            }

            return orderBy == null ? query.ToList() : orderBy(query).ToList();
        }

        /// <summary>
        /// Get a T from it's id
        /// </summary>
        /// <param name="id">The id of the T to retrieve</param>
        /// <returns>T data model</returns>
        public T GetById(int id)
        {
            //return _entityContext.Set<T>().FirstOrDefault(data => data.Id == id);
            return _entityContext.Set<T>().FirstOrDefault();
        }

        /// <summary>
        /// Check if there're any Set<T>() matching a specific filter.
        /// </summary>
        /// <param name="filter"></param>
        /// <returns></returns>
        public bool Any(Expression<Func<T, bool>> filter)
        {
            return _entityContext.Set<T>().Any(filter);
        }

        /// <summary>
        /// Get the first T matching a specific filter.
        /// </summary>
        /// <param name="filter"></param>
        /// <returns></returns>
        public T First(Expression<Func<T, bool>> filter)
        {
            return _entityContext.Set<T>().FirstOrDefault(filter);
        }

        /// <summary>
        /// Add a new T to the data store
        /// </summary>
        /// <param name="entity"></param>
        public void Save(T entity)
        {
            _entityContext.Set<T>().Add(entity);
        }

        /// <summary>
        /// Remove a T from the data store
        /// </summary>
        /// <param name="entity"></param>
        public void Delete(T entity)
        {
            _entityContext.Set<T>().Remove(entity);
        }

        /// <summary>
        /// Update an existing T
        /// </summary>
        /// <param name="entity"></param>
        public void Update(T entity)
        {
            var set = _entityContext.Set<T>();
            var entry = set.Local.SingleOrDefault(f => f == entity);

            if (entry != null)
            {
                var attachedFeature = _entityContext.Entry(entry);
                attachedFeature.CurrentValues.SetValues(entity);
                attachedFeature.State = EntityState.Modified;
            }
            else
            {
                _entityContext.Set<T>().Attach(entity);
                _entityContext.Entry(entity).State = EntityState.Modified;
            }
        }
    }
}