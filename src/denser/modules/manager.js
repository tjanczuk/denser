var http = require("http");
var tracing = require("tracing");

var config;
try {
    config = JSON.parse(host.config);
} catch (e) {
    tracing.write("Error parsing Denser configuration.");
    throw e;
}

var programs = {};

programs[host.managerProgram.programId] = host.managerProgram; // add admin program to the list of running programs

// Start HTTP admin service if desired

if (config.admin) {
    if (typeof (config.admin.url) !== "string") {
        throw "Configuration file must specify admin.url property as the listed URL for administration service if the admin property itself is present.";
    }

    // normalize the listed URL
    if (config.admin.url.charAt(config.admin.url.length - 1) !== '/') {
        config.admin.url += "/";
    }
    config.admin.url = config.admin.url.toLowerCase();

    config.pathPrefix = /^[^\/]+\/\/[^\/]+(\/.+)/.exec(config.admin.url)[1];
    config.pathPrefixSansSlash = config.pathPrefix.substring(0, config.pathPrefix.length - 1);
    config.pathRegEx = new RegExp("^" + config.pathPrefix + "programs(?:/((?:[A-F0-9]{8}\-[A-F0-9]{4}\-[A-F0-9]{4}\-[A-F0-9]{4}\-[A-F0-9]{12})|manager))?(?:/(stats|log|code))?/?$", "i");

    function sendResponse(request, statusCode, reason, body, contentType) {
        request.onEnd = function () {
            if (body) {
                request.onWriteReady = function () { request.writeBody(body, true); };
                request.writeHeaders(statusCode, reason, { "Content-length": body.length.toString(), "Content-Type": contentType, "Cache-Control": "no-cache" }, false);
            }
            else {
                request.writeHeaders(statusCode, reason, { "Content-length": "0", "Cache-Control": "no-cache" }, true);
            }
        };
        request.readMore();
    }

    function getListOfPrograms(request) {
        var response = '[';
        for (var programId in programs) 
            response += (response !== '[') ? (',"' + programId.toString() + '"') : ('"' + programId.toString() + '"');
        response += ']';
        sendResponse(request, 200, "OK", response, "application/json");
    }

    function getProgramCode(request, program) {        
        var script = host.getProgramSource(program);
        sendResponse(request, 200, "OK", script, "text/plain");
    }

    function getProgramStatus(request, program) {
        var status = host.getProgramStatus(program);
        sendResponse(request, 200, "OK", status, "application/json");
    }

    function getProgramStats(request, program) {
        var stats = host.getProgramStats(program);
        var statsResponse = JSON.stringify(stats);
        sendResponse(request, 200, "OK", statsResponse, "application/json");
    }

    function getProgramLog(request, program) {
        var log = host.getProgramLog(program);
        var logResponse;
        if (request.query && request.query === "?format=text") {
            logResponse = "";
            for (var index in log) {
                var item = log[index];
                var time = new Date();
                time.setUTCFullYear(item.time.y);
                time.setUTCMonth(item.time.m);
                time.setUTCDate(item.time.d);
                time.setUTCHours(item.time.h);
                time.setUTCMinutes(item.time.mi);
                time.setUTCSeconds(item.time.s);
                time.setUTCMilliseconds(item.time.ms);
                logResponse += '>>>>>>>>>> ' + time.toUTCString() + ' ' + time.getUTCMilliseconds() + 'ms\n' + item.entry + '\n';
            }
            sendResponse(request, 200, "OK", logResponse, "text/plain");
        }
        else {
            // return JSON by default
            logResponse = JSON.stringify(log);
            sendResponse(request, 200, "OK", logResponse, "application/json");
        }
    }

    function postProgram(request) {
        var contentType = request.headers["Content-Type"];
        if (contentType !== "application/json" && contentType !== "application/javascript") {
            return sendResponse(request, 415, "Unsupported Media Type",
                "Accepted request formats are application/json (JSON object with program specification) or application/javascript (plaintext JavaScript program).", 
                "text/plain");
        }
        var body = "";
        request.onData = function (event) { body += event.data; request.readMore(); }
        request.onEnd = function () {
            var program;
            try {
                var programSource;
                if (contentType === "application/json") {
                    programSource = JSON.parse(body);
                    if (typeof (programSource.code) !== "string")
                        throw "The JSON object submitted in the request must have a 'code' string property with the code of the program to run.";
                    programSource = programSource.code;
                }
                else { // "application/javascript"
                    programSource = body;
                }
                program = host.startProgram(programSource);
                programs[program.programId] = program;
                tracing.write("POST /programs: created a new program " + program.programId);
            } catch (e) {
                tracing.write("POST /programs: unable to create a new program:\n" + e.toString());
                try {
                    body = e.ToString();
                    request.onWriteReady = function () { request.writeBody(body, true); };
                    request.writeHeaders(400, "Bad Request", { "Content-length": body.length.toString(), "Content-Type": "text/plain", "Cache-Control": "no-cache" }, false);
                } catch (e1) {
                    tracing.write("POST /programs: unable to send back a 400 response:\n" + e1.toString());
                }
            }
            try {
                // TODO, tjanczuk, change Location to the full URL
                request.writeHeaders(201, "Created", { "Content-length": "0", "Location": program.programId.toString() }, true);
            } catch (e) {
                tracing.write("POST /programs: unable to send back 201 response:\n" + e.toString());
            }
        }
        request.readMore();
    }

    function deleteProgram(request, program) {
        host.stopProgram(program);
        sendResponse(request, 204, "No Content");
    }

    function authorize(request) {
        if (config.admin.authorizationKey && request.headers["Authorization"] !== config.admin.authorizationKey) {
            sendResponse(request, 403, "Forbidden");
            return false;
        }
        return true;
    }

    var adminService = function (request) {
        try {
            if (request.path === config.pathPrefix || request.path === config.pathPrefixSansSlash) {
                return sendResponse(request, 200, "OK", helpContent, "text/html");
            }
            else {
                var match = config.pathRegEx.exec(request.path);
                if (match) {

                    if (!authorize(request))
                        return;

                    var id = match["1"].length > 0 ? match["1"] : undefined;
                    if (id === "manager") id = "00000000-0000-0000-0000-000000000000";
                    var resource = match["2"].length > 0 ? match["2"].toLowerCase() : undefined;
                    var program = id ? programs[id] : undefined;

                    if (id && !program)
                        return sendResponse(request, 404, "Not Found");

                    if (request.verb === "GET") {
                        if (!id && !resource)
                            return getListOfPrograms(request);
                        else if (id && !resource)
                            return getProgramStatus(request, program);
                        else if (id && resource === "stats")
                            return getProgramStats(request, program);
                        else if (id && resource === "log")
                            return getProgramLog(request, program);
                        else if (id && resource === "code")
                            return getProgramCode(request, program);
                    }
                    else if (request.verb === "POST") {
                        if (!id && !resource)
                            return postProgram(request);
                    }
                    else if (request.verb === "DELETE") {
                        if (id && !resource)
                            return deleteProgram(request, program);
                    }
                }
            }

            sendResponse(request, 404, "Not Found");
        } catch (e) {
            tracing.write("Error processing Denser administration request: " + e.toString());
            try {
                sendResponse(request, 500, "Internal server error");
            }
            catch (e1) { }
        }
    };

    http.createServer({ url: config.admin.url, onNewRequest: adminService }).start();

    tracing.write("Denser management service is running at " + config.admin.url + (config.admin.authorizationKey ? ". Authentication is required." : ". Anonymous access is allowed.") + " Press Ctrl-C to terminate.");
}

// Start programs specified in configuration

if (typeof (config.programs) === "object") {
    for (var i in config.programs) {
        var item = config.programs[i];
        if (typeof (item) != "object" || typeof(item.file) != "string") {
            throw "Configuration file must contain program specifications as objects with a 'file' property containing a string file name.";
        }
        var programSource = host.loadProgram(item.file);
        var program = host.startProgram(programSource);
        programs[program.programId] = program;
    }
}

var help = function () {
    /*
<html>
<head>
    <title>Denser Managment Service</title>
</head>
<style type="text/css">
    #content
    {
        font-size: 0.7em;
        padding-bottom: 2em;
        margin-left: 30px;
    }
    body
    {
        margin-top: 0px;
        margin-left: 0px;
        color: #000000;
        font-family: Verdana;
        background-color: white;
    }
    p
    {
        margin-top: 0px;
        margin-bottom: 12px;
        color: #000000;
        font-family: Verdana;
    }
    pre
    {
        border-right: #f0f0e0 1px solid;
        padding-right: 5px;
        border-top: #f0f0e0 1px solid;
        margin-top: -5px;
        padding-left: 5px;
        font-size: 1.2em;
        padding-bottom: 5px;
        border-left: #f0f0e0 1px solid;
        padding-top: 5px;
        border-bottom: #f0f0e0 1px solid;
        font-family: Courier New;
        background-color: #e5e5cc;
    }
    .heading1
    {
        margin-top: 0px;
        padding-left: 15px;
        font-weight: normal;
        font-size: 26px;
        margin-bottom: 0px;
        padding-bottom: 3px;
        margin-left: -30px;
        width: 100%;
        color: #ffffff;
        padding-top: 10px;
        font-family: Tahoma;
        background-color: #003366;
    }
    .intro
    {
        margin-left: -15px;
    }
</style>
<body>
    <div id="content">
        <p class="heading1">
            Denser Management Service</p>
        <br />
        <p class="intro">
            Welcome to the Denser Management Service.</p>
        <p class="intro">
            You can start, terminate, and interrogate server side JavaScript programs managed
            by this instance of the Denser server using a collection of HTTP APIs documented
            below.
        </p>
        <p class="intro" style="display: ##anonymous##">
            <font color="red">This instance of Denser Management Service allows calls from anonymous
                clients.</font>
        </p>
        <p class="intro" style="display: ##authenticated##">
            <font color="red">This instance of the Denser Management Service requires all calls
                to be authenticated.</font> Callers can authenticate by adding the Authorization
            HTTP request header with the value that is a literal match of the authorizationKey
            parameter specified in the configuration of this Denser server instance.
        </p>
        <p class="intro">
            <b>GET ##ROOT##/programs/</b></p>
        <p>
            Get the list of programs managed by the Denser server instance. The result is a
            JSON array with a list of GUID program identifiers. The list contains programs that
            are still running as well as those that have already terminated.</p>
        <div style="display: ##anonymous##">
            <p>
                <a href="##ROOT##/programs/">Try it now!</a></p>
        </div>
        <p>
            Sample request:</p>
        <pre>GET http://tjanczuk-e6400/denser/programs/ HTTP/1.1
User-Agent: Fiddler
Host: tjanczuk-e6400</pre>
        <p>
            Sample response:</p>
        <pre>HTTP/1.1 200 OK
Cache-Control: no-cache
Content-Length: 3940
Content-Type: application/json
Server: Denser/v0.2 Microsoft-HTTPAPI/2.0
Date: Mon, 23 May 2011 07:03:31 GMT

["00000000-0000-0000-0000-000000000000","4B336297-FEC4-44F2-BE07-0271CD159FE1","81B8EF69-49B2-42A1-B98A-80AF1CA3302E"]</pre>
        <p class="intro">
            <b>GET ##ROOT##/programs/{programId}</b></p>
        <p>
            Get the status of the program identified with the programId GUID. The result is
            a JSON object that indicates the status of the program (running or finished). For
            finished programs, the result code is also provided (zero means successful completion).</p>
        <div style="display: ##anonymous##">
            <p>
                <a href="##ROOT##/programs/00000000-0000-0000-0000-000000000000/" style="display: ##anonymous##">
                    Try it now!</a></p>
        </div>
        <p>
            Sample request:</p>
        <pre>GET http://tjanczuk-e6400/denser/programs/4B336297-FEC4-44F2-BE07-0271CD159FE1/ HTTP/1.1
User-Agent: Fiddler
Host: tjanczuk-e6400</pre>
        <p>
            Sample response:</p>
        <pre>HTTP/1.1 200 OK
Cache-Control: no-cache
Content-Length: 19
Content-Type: application/json
Server: Denser/v0.2 Microsoft-HTTPAPI/2.0
Date: Mon, 23 May 2011 07:08:41 GMT

{"state":"running"}</pre>
        <p class="intro">
            <b>GET ##ROOT##/programs/{programId}/code/</b></p>
        <p>
            Get the JavaScript source code of the program identified with the programId GUID.
            The result is the JavaScript source code of the program.</p>
        <div style="display: ##anonymous##">
            <p>
                <a href="##ROOT##/programs/00000000-0000-0000-0000-000000000000/code/" style="display: ##anonymous##">
                    Try it now!</a></p>
        </div>
        <p>
            Sample request:</p>
        <pre>GET http://tjanczuk-e6400/denser/programs/252FE948-BFE2-480F-A55F-5CA3F9AE7288/code HTTP/1.1
User-Agent: Fiddler
Host: tjanczuk-e6400</pre>
        <p>
            Sample response:</p>
        <pre>HTTP/1.1 200 OK
Cache-Control: no-cache
Content-Length: 42
Content-Type: text/plain
Server: Denser/v0.2 Microsoft-HTTPAPI/2.0
Date: Mon, 23 May 2011 07:15:56 GMT

require("tracing").write("Hello, world!");</pre>
        <p class="intro">
            <b>GET ##ROOT##/programs/{programId}/stats/</b></p>
        <p>
            Get the server resource usage statistics of the program identified with the programId
            GUID. The result is a JSON object with the current metrics of resource consumption.
            For programs that have completed, this represents the total resource consumption
            throghout their lifetime.</p>
        <div style="display: ##anonymous##">
            <p>
                <a href="##ROOT##/programs/00000000-0000-0000-0000-000000000000/stats/" style="display: ##anonymous##">
                    Try it now!</a></p>
        </div>
        <p>
            Sample request:</p>
        <pre>GET http://tjanczuk-e6400/denser/programs/252FE948-BFE2-480F-A55F-5CA3F9AE7288/stats HTTP/1.1
User-Agent: Fiddler
Host: tjanczuk-e6400</pre>
        <p>
            Sample response:</p>
        <pre>HTTP/1.1 200 OK
Cache-Control: no-cache
Content-Length: 270
Content-Type: application/json
Server: Denser/v0.2 Microsoft-HTTPAPI/2.0
Date: Mon, 23 May 2011 07:17:38 GMT

{"programId":"252FE948-BFE2-480F-A55F-5CA3F9AE7288",
 "cpu":{"applicationCpuCycles":9368384,
        "threadCpuCycles":23934740},
 "http":{"httpServerRequestsCompleted":0,
         "httpServerRequestsAborted":0,
         "httpServerRequestsActive":0,
         "httpServerBytesReceived":0,
         "httpServerBytesSent":0}}</pre>
        <p class="intro">
            <b>GET ##ROOT##/programs/{programId}/log/</b></p>
        <p>
            Get the trace log of the program identified with the programId GUID. The result
            is a JSON array with a server-side configured number of the most recent trace entires.
            Each entry is a JSON object containing the timestamp and the entry content.</p>
        <div style="display: ##anonymous##">
            <p>
                <a href="##ROOT##/programs/00000000-0000-0000-0000-000000000000/log/" style="display: ##anonymous##">
                    Try it now!</a></p>
        </div>
        <p>
            Sample request:</p>
        <pre>GET http://tjanczuk-e6400/denser/programs/252FE948-BFE2-480F-A55F-5CA3F9AE7288/log HTTP/1.1
User-Agent: Fiddler
Host: tjanczuk-e6400</pre>
        <p>
            Sample response:</p>
        <pre>HTTP/1.1 200 OK
Cache-Control: no-cache
Content-Length: 88
Content-Type: application/json
Server: Denser/v0.2 Microsoft-HTTPAPI/2.0
Date: Mon, 23 May 2011 07:21:53 GMT

[{"time":{"y":2011,"m":5,"d":23,"h":7,"mi":13,"s":41,"ms":436},"entry":"Hello, world!"}]</pre>
        <p class="intro">
            <b>GET ##ROOT##/programs/{programId}/log?format=text</b></p>
        <p>
            The same as above, just in a more human-readable text format.</p>
        <div style="display: ##anonymous##">
            <p>
                <a href="##ROOT##/programs/00000000-0000-0000-0000-000000000000/log?format=text"
                    style="display: ##anonymous##">Try it now!</a></p>
        </div>
        <p>
            Sample request:</p>
        <pre>GET http://tjanczuk-e6400/denser/programs/252FE948-BFE2-480F-A55F-5CA3F9AE7288/log?format=text HTTP/1.1
User-Agent: Fiddler
Host: tjanczuk-e6400</pre>
        <p>
            Sample response:</p>
        <pre>HTTP/1.1 200 OK
Cache-Control: no-cache
Content-Length: 61
Content-Type: text/plain
Server: Denser/v0.2 Microsoft-HTTPAPI/2.0
Date: Mon, 23 May 2011 07:19:56 GMT

>>>>>>>>>> Thu, 23 Jun 2011 07:13:41 UTC 436ms
Hello, world!</pre>
        <p class="intro">
            <b>POST ##ROOT##/programs/</b></p>
        <p>
            Starts a new program on the server. The code of the program must be included in
            the body of the request.
        </p>
        <p>
            If the request content type is application/javascript, the entire content of the
            body is treated literally as the code of the JavaScript program to run.
        </p>
        <p>
            If the request content type is application/json, the body of the request must contain
            a JSON object that specifies the literal content of the program to run in the 'code'
            property. This format is supported to allow for supporting additional caller-specified
            parameters in the future.
        </p>
        <p>
            Sample request:</p>
        <pre>POST http://tjanczuk-e6400/denser/programs HTTP/1.1
User-Agent: Fiddler
Host: tjanczuk-e6400
Content-Type: application/javascript
Content-Length: 42

require("tracing").write("Hello, world!");</pre>
        <p>
            Sample response:</p>
        <pre>HTTP/1.1 201 Created
Content-Length: 0
Location: 252FE948-BFE2-480F-A55F-5CA3F9AE7288
Server: Denser/v0.2 Microsoft-HTTPAPI/2.0
Date: Mon, 23 May 2011 07:13:41 GMT</pre>
        <p class="intro">
            <b>DELETE ##ROOT##/programs/{programId}</b></p>
        <p>
            Initiates shutdown of the program identified by the programId GUID. After the
            shutdown has completed, the program can still be interrogated using the program-scope
            GET APIs described above.
        </p>
        <p>
            Sample request:</p>
        <pre>DELETE http://tjanczuk-e6400/denser/programs/252FE948-BFE2-480F-A55F-5CA3F9AE7288 HTTP/1.1
User-Agent: Fiddler
Host: tjanczuk-e6400</pre>
        <p>
            Sample response:</p>
        <pre>HTTP/1.1 204 No Content
Cache-Control: no-cache
Content-Length: 0
Server: Denser/v0.2 Microsoft-HTTPAPI/2.0
Date: Mon, 23 May 2011 07:23:16 GMT</pre>
    </div>
</body>
</html>
    */
};

function hereDoc(f) {
    var result = f.toString().replace(/^[^\/]+\/\*!?/, '').replace(/\*\/[^\/]+$/, '');
    if (config.admin && config.admin.authorizationKey) {
        result = result.replace(/##anonymous##/g, "none").replace(/##authenticated##/g, "block");
    }
    else {
        result = result.replace(/##anonymous##/g, "block").replace(/##authenticated##/g, "none");
    }
    result = result.replace(/##ROOT##/g, config.pathPrefixSansSlash);
    return result;
}

var helpContent = hereDoc(help);