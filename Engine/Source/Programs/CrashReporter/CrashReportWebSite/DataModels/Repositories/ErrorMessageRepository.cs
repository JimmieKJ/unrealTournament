using System;
using System.Collections.Generic;
using System.Data.Entity;
using System.Linq;
using System.Linq.Expressions;

namespace Tools.CrashReporter.CrashReportWebSite.DataModels.Repositories
{
    public class ErrorMessageRepository : IErrorMessageRepository
    {
        private readonly CrashReportEntities _entityContext;

        /// <summary>
        /// Constructor
        /// </summary>
        /// <param name="entityContext">A reference to the *single* instance of the data context.</param>
        public ErrorMessageRepository(CrashReportEntities entityContext)
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
        public IQueryable<ErrorMessage> ListAll()
        {
            return _entityContext.ErrorMessages;
        }

        /// <summary>
        /// Get a filtered list of ErrorMessages from data storage
        /// Calling this method returns the data directly. It will execute the data retrieval - in this case an sql transaction.
        /// </summary>
        /// <param name="filter">A linq expression used to filter the ErrorMessage table</param>
        /// <returns>Returns a fully filtered enumerated list object of ErrorMessagees.</returns>
        public IEnumerable<ErrorMessage> Get(Expression<Func<ErrorMessage, bool>> filter)
        {
            return _entityContext.ErrorMessages.Where(filter).ToList();
        }

        /// <summary>
        /// Get a filtered, ordered list of ErrorMessage
        /// </summary>
        /// <param name="filter">An expression used to filter the ErrorMessages</param>
        /// <param name="orderBy">An function delegate ErrorMessaged to order the results from the ErrorMessage table</param>
        /// <returns></returns>
        public IEnumerable<ErrorMessage> Get(Expression<Func<ErrorMessage, bool>> filter,
            Func<IQueryable<ErrorMessage>, IOrderedQueryable<ErrorMessage>> orderBy)
        {
            return orderBy(_entityContext.ErrorMessages.Where(filter)).ToList();
        }

        /// <summary>
        /// Return an ordered list of ErrorMessagees with data pre-loading
        /// </summary>
        /// <param name="filter">A linq expression used to filter the ErrorMessage table</param>
        /// <param name="orderBy">A linq expression used to order the results from the ErrorMessage table</param>
        /// <param name="includeProperties">A linq expression indicating properties to dynamically load.</param>
        /// <returns></returns>
        public IEnumerable<ErrorMessage> Get(Expression<Func<ErrorMessage, bool>> filter,
            Func<IQueryable<ErrorMessage>, IOrderedQueryable<ErrorMessage>> orderBy,
            params Expression<Func<ErrorMessage, object>>[] includeProperties)
        {
            var query = _entityContext.ErrorMessages.Where(filter);
            foreach (var includeProperty in includeProperties)
            {
                query.Include(includeProperty);
            }

            return orderBy == null ? query.ToList() : orderBy(query).ToList();
        }

        /// <summary>
        /// Get a ErrorMessage from it's id
        /// </summary>
        /// <param name="id">The id of the ErrorMessage to retrieve</param>
        /// <returns>ErrorMessage data model</returns>
        public ErrorMessage GetById(int id)
        {
            return _entityContext.ErrorMessages.FirstOrDefault(data => data.Id == id);
        }

        /// <summary>
        /// 
        /// </summary>
        /// <param name="errorMessageName"></param>
        /// <returns></returns>
        public ErrorMessage GetByErrorMessageName(string errorMessageName)
        {
            return _entityContext.ErrorMessages.SqlQuery("SELECT TOP 1 * FROM ErrorMessages WHERE ErrorMessageName LIKE '" + errorMessageName + "'").FirstOrDefault();
        }

        /// <summary>
        /// Check if there are any ErrorMessages matching a specific filter.
        /// </summary>
        /// <param name="filter"></param>
        /// <returns></returns>
        public bool Any(Expression<Func<ErrorMessage, bool>> filter)
        {
            return _entityContext.ErrorMessages.Any(filter);
        }

        /// <summary>
        /// Get the first ErrorMessage matching a specific filter.
        /// </summary>
        /// <param name="filter"></param>
        /// <returns></returns>
        public ErrorMessage First(Expression<Func<ErrorMessage, bool>> filter)
        {
            return _entityContext.ErrorMessages.FirstOrDefault(filter);
        }

        /// <summary>
        /// Add a new ErrorMessage to the data store
        /// </summary>
        /// <param name="entity"></param>
        public void Save(ErrorMessage entity)
        {
            _entityContext.ErrorMessages.Add(entity);
        }

        /// <summary>
        /// Remove a ErrorMessage from the data store
        /// </summary>
        /// <param name="entity"></param>
        public void Delete(ErrorMessage entity)
        {
            _entityContext.ErrorMessages.Remove(entity);
        }

        /// <summary>
        /// Update an existing ErrorMessage
        /// </summary>
        /// <param name="entity"></param>
        public void Update(ErrorMessage entity)
        {
            var set = _entityContext.Set<ErrorMessage>();
            var entry = set.Local.SingleOrDefault(f => f.Id == entity.Id);

            if (entry != null)
            {
                var attachedFeature = _entityContext.Entry(entry);
                attachedFeature.CurrentValues.SetValues(entity);
                attachedFeature.State = EntityState.Modified;
            }
            else
            {
                _entityContext.ErrorMessages.Attach(entity);
                _entityContext.Entry(entity).State = EntityState.Modified;
            }
        }
    }
}