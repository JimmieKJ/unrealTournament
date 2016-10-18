using System;
using System.Collections.Generic;
using System.Linq;
using System.Web;

using Hangfire;
using Microsoft.Owin;
using Owin;
using Tools.CrashReporter.CrashReportCommon;
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