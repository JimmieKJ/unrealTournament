using System;
using System.Collections.Generic;
using System.Linq;
using System.Linq.Expressions;

namespace Tools.CrashReporter.CrashReportWebSite.DataModels.Repositories
{
    /// <summary>
    /// Interface to our
    /// </summary>
    /// <typeparam name="T"></typeparam>
    public interface IDataRepository<T>
    {
        /// <summary>
        /// Awful hideous IQuerable return that allows for construction of own queries! Hiss!
        /// </summary>
        IQueryable<T> ListAll();

        /// <summary>
        /// Return a list of data objects of type T matching our search expression
        /// </summary>
        /// <param name="filter">An expression tree defining a search filter.</param>
        /// <returns></returns>
        IEnumerable<T> Get(Expression<Func<T, bool>> filter);

        /// <summary>
        /// Return a list of data objects of type T matching our search expression and ordered by our ordering function
        /// </summary>
        /// <param name="filter">An expression tree defining a search filter.</param>
        /// <param name="orderBy">Templated ordering function</param>
        /// <returns></returns>
        IEnumerable<T> Get(Expression<Func<T, bool>> filter, Func<IQueryable<T>, IOrderedQueryable<T>> orderBy);

        /// <summary>
        /// Return a list of data objects of type T matching our search expression and ordered by our ordering function
        /// with eager loading of defined properties
        /// </summary>
        /// <param name="filter">An expression tree defining a search filter.</param>
        /// <param name="orderBy">Templated ordering function</param>
        /// <param name="includeProperties">Parameter list of expressions defining navigation properties to eager load</param>
        /// <returns></returns>
        IEnumerable<T> Get(Expression<Func<T, bool>> filter, Func<IQueryable<T>, IOrderedQueryable<T>> orderBy = null, params Expression<Func<T, object>>[] includeProperties);

        /// <summary>
        /// Check to see if any objects match our filter
        /// </summary>
        /// <param name="filter">An expression tree defining a search filter.</param>
        /// <returns>A bool indicating if any objects match our filter.</returns>
        bool Any(Expression<Func<T, bool>> filter);

        /// <summary>
        /// Return the first object matching the filter.
        /// 
        /// Consider changing this to return a bool for success with an 'out' parameter to return the value. 
        /// </summary>
        /// <param name="filter">An expression tree defining a search filter.</param>
        /// <returns>An object of type T OR NULL IF NO OBJECT IS FOUND!</returns>
        T First(Expression<Func<T, bool>> filter);

        /// <summary>
        /// Return the first object matching the id.
        /// </summary>
        /// <param name="id">The id of the object</param>
        /// <returns>An object of type T or NULL if no object is found</returns>
        T GetById(int id);

        /// <summary>
        /// Add a new object of type T to our data-model
        /// </summary>
        /// <param name="entity">The objec to be added.</param>
        void Save(T entity);

        /// <summary>
        /// Remove an object of type T from our data model
        /// </summary>
        /// <param name="entity">he Object to remove</param>
        void Delete(T entity);

        /// <summary>
        /// Update an existing object of type T
        /// </summary>
        /// <param name="entity">The object to update</param>
        void Update(T entity);
    }
}
