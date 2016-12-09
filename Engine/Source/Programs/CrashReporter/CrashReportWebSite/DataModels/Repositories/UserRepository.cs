using System;
using System.Collections.Generic;
using System.Data.Entity;
using System.Linq;
using System.Linq.Expressions;

namespace Tools.CrashReporter.CrashReportWebSite.DataModels.Repositories
{
    public class UserRepository: IUserRepository
    {
        private readonly CrashReportEntities _entityContext;

        /// <summary>
        /// Constructor
        /// </summary>
        /// <param name="entityContext">A reference to the *single* instance of the data context.</param>
        public UserRepository(CrashReportEntities entityContext)
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
        public IQueryable<User> ListAll()
        {
            return _entityContext.Users;
        }

        /// <summary>
        /// Get a filtered list of Users from data storage
        /// Calling this method returns the data directly. It will execute the data retrieval - in this case an sql transaction.
        /// </summary>
        /// <param name="filter">A linq expression used to filter the User table</param>
        /// <returns>Returns a fully filtered enumerated list object of Useres.</returns>
        public IEnumerable<User> Get(Expression<Func<User, bool>> filter)
        {
            return _entityContext.Users.Where(filter).ToList();
        }

        /// <summary>
        /// Get a filtered, ordered list of User
        /// </summary>
        /// <param name="filter">An expression used to filter the Users</param>
        /// <param name="orderBy">An function delegate userd to order the results from the User table</param>
        /// <returns></returns>
        public IEnumerable<User> Get(Expression<Func<User, bool>> filter,
            Func<IQueryable<User>, IOrderedQueryable<User>> orderBy)
        {
            return orderBy(_entityContext.Users.Where(filter)).ToList();
        }

        /// <summary>
        /// Return an ordered list of Useres with data preloading
        /// </summary>
        /// <param name="filter">A linq expression used to filter the User table</param>
        /// <param name="orderBy">A linq expression used to order the results from the User table</param>
        /// <param name="includeProperties">A linq expression indicating properties to dynamically load.</param>
        /// <returns></returns>
        public IEnumerable<User> Get(Expression<Func<User, bool>> filter,
            Func<IQueryable<User>, IOrderedQueryable<User>> orderBy,
            params Expression<Func<User, object>>[] includeProperties)
        {
            var query = _entityContext.Users.Where(filter);
            foreach (var includeProperty in includeProperties)
            {
                query.Include(includeProperty);
            }

            return orderBy == null ? query.ToList() : orderBy(query).ToList();
        }

        /// <summary>
        /// Get a User from it's id
        /// </summary>
        /// <param name="id">The id of the User to retrieve</param>
        /// <returns>User data model</returns>
        public User GetById(int id)
        {
            return _entityContext.Users.FirstOrDefault(data => data.Id == id);
        }

        /// <summary>
        /// 
        /// </summary>
        /// <param name="userName"></param>
        /// <returns></returns>
        public User GetByUserName(string userName)
        {
            return _entityContext.Users.SqlQuery("SELECT TOP 1 * FROM Users WHERE UserName LIKE '" + userName + "'").FirstOrDefault();
        }

        /// <summary>
        /// Check if there're any Users matching a specific filter.
        /// </summary>
        /// <param name="filter"></param>
        /// <returns></returns>
        public bool Any(Expression<Func<User, bool>> filter)
        {
            return _entityContext.Users.Any(filter);
        }

        /// <summary>
        /// Get the first User matching a specific filter.
        /// </summary>
        /// <param name="filter"></param>
        /// <returns></returns>
        public User First(Expression<Func<User, bool>> filter)
        {
            return _entityContext.Users.FirstOrDefault(filter);
        }

        /// <summary>
        /// Add a new User to the data store
        /// </summary>
        /// <param name="entity"></param>
        public void Save(User entity)
        {
            _entityContext.Users.Add(entity);
        }

        /// <summary>
        /// Remove a User from the data store
        /// </summary>
        /// <param name="entity"></param>
        public void Delete(User entity)
        {
            _entityContext.Users.Remove(entity);
        }

        /// <summary>
        /// Update an existing User
        /// </summary>
        /// <param name="entity"></param>
        public void Update(User entity)
        {
            var set = _entityContext.Set<User>();
            var entry = set.Local.SingleOrDefault(f => f.Id == entity.Id);

            if (entry != null)
            {
                var attachedFeature = _entityContext.Entry(entry);
                attachedFeature.CurrentValues.SetValues(entity);
                attachedFeature.State = EntityState.Modified;
            }
            else
            {
                _entityContext.Users.Attach(entity);
                _entityContext.Entry(entity).State = EntityState.Modified;
            }
        }
    }
}