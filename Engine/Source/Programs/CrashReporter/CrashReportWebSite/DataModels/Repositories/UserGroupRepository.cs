using System;
using System.Collections.Generic;
using System.Data.Entity;
using System.Linq;
using System.Linq.Expressions;

namespace Tools.CrashReporter.CrashReportWebSite.DataModels.Repositories
{
    public class UserGroupRepository : IUserGroupRepository
    {
        private readonly CrashReportEntities _entityContext;

        /// <summary>
        /// Constructor
        /// </summary>
        /// <param name="entityContext">A reference to the *single* instance of the data context.</param>
        public UserGroupRepository(CrashReportEntities entityContext)
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
        public IQueryable<UserGroup> ListAll()
        {
            return _entityContext.UserGroups;
        }

        /// <summary>
        /// Get a filtered list of UserGroups from data storage
        /// Calling this method returns the data directly. It will execute the data retrieval - in this case an sql transaction.
        /// </summary>
        /// <param name="filter">A linq expression used to filter the UserGroup table</param>
        /// <returns>Returns a fully filtered enumerated list object of UserGroupes.</returns>
        public IEnumerable<UserGroup> Get(Expression<Func<UserGroup, bool>> filter)
        {
            return _entityContext.UserGroups.Where(filter).ToList();
        }

        /// <summary>
        /// Get a filtered, ordered list of UserGroup
        /// </summary>
        /// <param name="filter">An expression used to filter the UserGroups</param>
        /// <param name="orderBy">An function delegate UserGroupd to order the results from the UserGroup table</param>
        /// <returns></returns>
        public IEnumerable<UserGroup> Get(Expression<Func<UserGroup, bool>> filter,
            Func<IQueryable<UserGroup>, IOrderedQueryable<UserGroup>> orderBy)
        {
            return orderBy(_entityContext.UserGroups.Where(filter)).ToList();
        }

        /// <summary>
        /// Return an ordered list of UserGroupes with data preloading
        /// </summary>
        /// <param name="filter">A linq expression used to filter the UserGroup table</param>
        /// <param name="orderBy">A linq expression used to order the results from the UserGroup table</param>
        /// <param name="includeProperties">A linq expression indicating properties to dynamically load.</param>
        /// <returns></returns>
        public IEnumerable<UserGroup> Get(Expression<Func<UserGroup, bool>> filter,
            Func<IQueryable<UserGroup>, IOrderedQueryable<UserGroup>> orderBy,
            params Expression<Func<UserGroup, object>>[] includeProperties)
        {
            var query = _entityContext.UserGroups.Where(filter);
            foreach (var includeProperty in includeProperties)
            {
                query.Include(includeProperty);
            }

            return orderBy == null ? query.ToList() : orderBy(query).ToList();
        }

        /// <summary>
        /// Get a UserGroup from it's id
        /// </summary>
        /// <param name="id">The id of the UserGroup to retrieve</param>
        /// <returns>UserGroup data model</returns>
        public UserGroup GetById(int id)
        {
            return _entityContext.UserGroups.FirstOrDefault(data => data.Id == id);
        }

        /// <summary>
        /// 
        /// </summary>
        /// <param name="UserGroupName"></param>
        /// <returns></returns>
        public UserGroup GetByUserGroupName(string UserGroupName)
        {
            return _entityContext.UserGroups.SqlQuery("SELECT TOP 1 * FROM UserGroups WHERE UserGroupName LIKE '" + UserGroupName + "'").FirstOrDefault();
        }

        /// <summary>
        /// Check if there're any UserGroups matching a specific filter.
        /// </summary>
        /// <param name="filter"></param>
        /// <returns></returns>
        public bool Any(Expression<Func<UserGroup, bool>> filter)
        {
            return _entityContext.UserGroups.Any(filter);
        }

        /// <summary>
        /// Get the first UserGroup matching a specific filter.
        /// </summary>
        /// <param name="filter"></param>
        /// <returns></returns>
        public UserGroup First(Expression<Func<UserGroup, bool>> filter)
        {
            return _entityContext.UserGroups.FirstOrDefault(filter);
        }

        /// <summary>
        /// Add a new UserGroup to the data store
        /// </summary>
        /// <param name="entity"></param>
        public void Save(UserGroup entity)
        {
            _entityContext.UserGroups.Add(entity);
        }

        /// <summary>
        /// Remove a UserGroup from the data store
        /// </summary>
        /// <param name="entity"></param>
        public void Delete(UserGroup entity)
        {
            _entityContext.UserGroups.Remove(entity);
        }

        /// <summary>
        /// Update an existing UserGroup
        /// </summary>
        /// <param name="entity"></param>
        public void Update(UserGroup entity)
        {
            var set = _entityContext.Set<UserGroup>();
            var entry = set.Local.SingleOrDefault(f => f.Id == entity.Id);

            if (entry != null)
            {
                var attachedFeature = _entityContext.Entry(entry);
                attachedFeature.CurrentValues.SetValues(entity);
                attachedFeature.State = EntityState.Modified;
            }
            else
            {
                _entityContext.UserGroups.Attach(entity);
                _entityContext.Entry(entity).State = EntityState.Modified;
            }
        }
    }
}