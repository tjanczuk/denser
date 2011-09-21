using System;
using System.Collections.Generic;
using System.Linq;
using System.Web;
using System.Diagnostics;
using System.Configuration;
using System.Threading;

namespace denseradmin.Models
{
    public static class DenserProcess
    {
        static Process denser = null;

        static ProcessStartInfo denserStartInfo = new ProcessStartInfo 
        {
            Arguments = ConfigurationManager.AppSettings["DenserArguments"],
            FileName = ConfigurationManager.AppSettings["DenserPath"],
            WorkingDirectory = ConfigurationManager.AppSettings["DenserPath"].Substring(0, ConfigurationManager.AppSettings["DenserPath"].LastIndexOf('\\'))
        };

        public static void EnsureDenserRunning()
        {
            // poor man's activation

            if (denser == null || denser.HasExited)
            {
                Process[] list = Process.GetProcessesByName("denser");
                if (list.Length == 0)
                {
                    denser = Process.Start(denserStartInfo);
                    Thread.Sleep(1000);
                }
                else if (list.Length == 1)
                {
                    denser = list[0];
                }
                else
                {
                    throw new InvalidOperationException("More than one denser.exe process is running.");
                }
            }
        }
    }
}