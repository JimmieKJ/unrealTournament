using Hangfire;
using Microsoft.Owin;
using Owin;
using Tools.CrashReporter.CrashReportWebSite;

[assembly: OwinStartup(typeof(Startup))]
namespace Tools.CrashReporter.CrashReportWebSite
{
    public class Startup
    {
        public void Configuration(IAppBuilder app)
        {
		    GlobalConfiguration.Configuration
			    .UseSqlServerStorage("Hangfire");
            
			app.UseHangfireDashboard();
			app.UseHangfireServer();
        }
    }
}