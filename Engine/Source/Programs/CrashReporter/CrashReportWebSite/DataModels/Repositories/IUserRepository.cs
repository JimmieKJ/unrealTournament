namespace Tools.CrashReporter.CrashReportWebSite.DataModels.Repositories
{
    public interface IUserRepository: IDataRepository<User>
    {
        User GetByUserName(string userName);
    }
}
