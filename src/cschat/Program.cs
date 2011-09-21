using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.ServiceModel.Web;
using System.Threading;
using System.Reflection;

namespace cschat
{
    class Program
    {
        static void RunOneChat()
        {
            Guid programId = Guid.NewGuid();
            string address = "http://localhost/chat/" + programId.ToString().Substring(0, 8);
            ChatServiceUI.CustomizeUI(programId);
            WebServiceHost sh = new WebServiceHost(typeof(ChatService), new Uri(address));
            sh.Open();
            Console.WriteLine("Chat server started at " + address);            
        }

        static void Main(string[] args)
        {
            if (args.Length == 1 && args[0] == "worker")
            {
                RunOneChat();
            }
            else
            {
                if (args.Length != 1)
                {
                    Console.WriteLine("Usage: cschat.exe <numberOfPrograms>");
                    return;
                }

                for (int i = 0; i < Int32.Parse(args[0]); i++)
                {
                    //AppDomainSetup setup = new AppDomainSetup();
                    //setup.ApplicationBase = Assembly.GetExecutingAssembly().CodeBase;
                    Console.WriteLine("Creating app domain " + i.ToString());
                    AppDomain domain = AppDomain.CreateDomain(i.ToString());
                    domain.ExecuteAssembly("cschat.exe", new string[] { "worker" });
                }
                Console.WriteLine("Done creating app domains. Press Ctrl-C to exit.");
                new ManualResetEvent(false).WaitOne();
            }
        }
    }
}
