using System;
using System.Collections.Generic;
using System.Linq;
using System.Web;
using System.Web.Mvc;
using denseradmin.Models;
using System.Configuration;

namespace denseradmin.Controllers
{
    public class DenserController : Controller
    {
        //
        // GET: /Denser/

        public ActionResult Programs()
        {
            DenserProcess.EnsureDenserRunning();
            return View(new ProgramsModel { PublicEndpoint = ConfigurationManager.AppSettings["DenserPublicEndpoint"] });
        }

        public ActionResult About()
        {
            return View();
        }

        public ActionResult NewProgram()
        {
            return View(new ProgramsModel { PublicEndpoint = ConfigurationManager.AppSettings["DenserPublicEndpoint"] });
        }
    }
}
