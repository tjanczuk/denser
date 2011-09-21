(function () {
    var tracingLevel = 0; // 0 (none) through 2 (verbose)

    var importsTmp = imports; // This is to allow require("tracing") to create its own "imports" global var    
    var tracing = require("tracing");
    imports = importsTmp;

    var serverHeader = "Denser/v0.2";

    var addUrl = imports.addUrl;
    var removeUrl = imports.removeUrl;
    var sendHttpResponseHeaders = imports.sendHttpResponseHeaders;
    var sendHttpResponseBodyChunk = imports.sendHttpResponseBodyChunk;
    var pumpRequest = imports.pumpRequest;
    var isShuttingDown = false;

    var servers = {};
    var requests = {};

    function trace(arg, level) {
        if ((typeof (level) == "undefined" && tracingLevel > 0) || tracingLevel >= level) {
            tracing.write(arg);
        }
    }

    function getNormalizedAbsolutePath(url) {
        trace("http::getNormalizedAbsolutePath for " + url);
        if (url.charAt(url.length - 1) !== '/') {
            url += "/";
        }
        return url.toLowerCase();
    }

    function getNormalizedRelativePath(url) {
        trace("http::getNormalizedRelativePath for " + url);
        url = getNormalizedAbsolutePath(url);
        var fwdSlashCount = 0;
        var result = null;
        for (var i = 0; i < url.length; i++) {
            if (url.charAt(i) === '/') {
                fwdSlashCount++;
                if (fwdSlashCount === 3) {
                    var result = url.substring(i);
                    break;
                }
            }
        }
        trace("http::getNormalizedRelativePath result " + result);
        return result;
    }

    var dispatcher = function (event) {

        trace("http::dipatcher event", 2);
        trace(event, 2);

        if (isShuttingDown) {
            trace("http::dipatcher is in shutdown mode and skips event processing", 2);
            return;
        }

        var requestImpl = requests[event.requestSlot];

        if (event.state == 2) { // ProcessingRequestHeaders
            trace("http::newRequest " + event.requestSlot);

            // TODO, tjanczuk, match requests using scheme, host, port, path prefix

            var normalizedEventPath = event.path.toLowerCase();
            if (normalizedEventPath.charAt(normalizedEventPath.length - 1) != '/') {
                normalizedEventPath += "/";
            }
            var server = null;
            for (var pathPrefix in servers) {
                if (normalizedEventPath.indexOf(pathPrefix) === 0) {
                    server = servers[pathPrefix];
                    break;
                }
            }
            if (!server) {
                throw "Server matching url '" + event.path + "' was not found.";
            }

            requestImpl = {};
            requests[event.requestSlot] = requestImpl;
            requestImpl.requestSlot = event.requestSlot;
            requestImpl.api = event;

            requestImpl.api.readMore = function () {
                trace("http::readMore called for " + requestImpl.requestSlot);
                pumpRequest(requestImpl.requestSlot);
            };

            requestImpl.api.writeHeaders = function (statusCode, reason, headers, closeResponse) {
                trace("http::readMore called for " + requestImpl.requestSlot);

                if (isShuttingDown) {
                    throw "Http module is shutting down and cannot respond to HTTP requests at this time.";
                }

                if (typeof (statusCode) != "number") {
                    throw "StatusCode must be a number";
                }
                if (typeof (reason) != "string") {
                    throw "Reason must be a string";
                }
                if (typeof (headers) != "undefined" && typeof (headers) != "object") {
                    throw "Optional headers parameter must be an object if present.";
                }
                if (typeof (closeResponse) != "undefined" && typeof (closeResponse) != "boolean") {
                    throw "Optional closeResponse parameter must be a boolean if present.";
                }

                var headersImpl = headers || {};
                headersImpl["Server"] = typeof (headersImpl["Server"]) == "string" ? headersImpl["Server"] + " " + serverHeader : serverHeader;

                sendHttpResponseHeaders(
                    requestImpl.requestSlot,
                    statusCode,
                    reason,
                    typeof (closeResponse) == "boolean" ? closeResponse : false,
                    headersImpl
                );
            }

            requestImpl.api.writeBody = function (chunk, closeResponse) {
                trace("http::writeBody called for " + requestImpl.requestSlot);

                if (isShuttingDown) {
                    throw "Http module is shutting down and cannot respond to HTTP requests at this time.";
                }

                if (chunk == null || typeof (chunk) != "string") {
                    throw "Chunk must be a string (last chunk is indicated by an empty string)";
                }
                if (typeof (closeResponse) != undefined && typeof (closeResponse) != "boolean") {
                    throw "Optional closeResponse parameter must be a boolean if present.";
                }
                sendHttpResponseBodyChunk(
                    requestImpl.requestSlot,
                    chunk || "",
                    typeof (closeResponse) == "boolean" ? closeResponse : (typeof (chunk) == string ? chunk : true)
                );
            }

            server.args.onNewRequest(requestImpl.api);
        }
        else if (event.error || event.state == 12) { // Aborted or otherwise error
            trace("http::error event for " + requestImpl.requestSlot);
            if (typeof (requestImpl.api.onError) == "function") {
                requestImpl.api.onError(event);
            }
            delete requests[event.requestSlot];
        }
        else if (event.state == 4) { // ProcessingRequestBody
            trace("http::ProcessingRequestBody " + requestImpl.requestSlot);
            try {
                if (typeof (requestImpl.api.onData) == "function") {
                    requestImpl.api.onData(event);
                }
                else {
                    // JS user code is not interested in processing the body of the request
                    pumpRequest(event.requestSlot);
                }
            } catch (e) {
                tracing.write(e);
            }
        }
        else if (event.state == 5) { // WaitingForResponseHeaders
            trace("http::WaitingForResponseHeaders " + requestImpl.requestSlot);
            if (typeof (requestImpl.api.onEnd) == "function") {
                requestImpl.api.onEnd(event);
            }
            else {
                // JS user code did not register a handler to react to the end of the request; this request is beyond rescue
                sendHttpResponseHeaders(event.requestSlot, 500, "Internal Server Error", true, { "Server": serverHeader });
            }
        }
        else if (event.state == 7) { // WaitingForResponseBody
            trace("http::WaitingForResponseBody " + requestImpl.requestSlot);
            if (typeof (requestImpl.api.onWriteReady) == "function") {
                requestImpl.api.onWriteReady(event);
            }
            else {
                // JS user code did not register a handler to write the response body; interpret that as lack of response body
                sendHttpResponseBodyChunk(event.requestSlot, "", true);
            }
        }
        else if (event.state == 10) { // Complete
            trace("http::Complete " + requestImpl.requestSlot);
            if (typeof (requestImpl.api.onCompleted) == "function") {
                requestImpl.api.onCompleted(event);
            }
            delete requests[event.requestSlot];
        }
        else {
            trace("http::unexpected request state " + event.state + " for " + requestImpl.requestSlot);
            throw "Unexpected request state: " + event.state;
        }
    }

    var shutdown = function () {
        isShuttingDown = true;
        for (var path in servers) {
            var server = servers[path];
            try {
                removeUrl(server.normalizedAbsolutePath);
            }
            catch (e) {
                trace("http::shutdown failed unregistering URL " + server.normalizedAbsolutePath + " with exception " + e);
            }
        }
        servers = {};
    }

    imports.setHttpCallbacks(dispatcher, shutdown);

    exports.createServer = function (args) {
        trace("http::createServer");
        if (isShuttingDown) {
            throw "Http module is shutting down and cannot create new servers at this time.";
        }
        if (typeof (args) != "object") {
            throw "An object with configuration parameters must be provided to create an HTTP server.";
        }
        if (typeof (args.onNewRequest) != "function") {
            throw "args.onNewReqeust property must be specified as a function.";
        }
        if (typeof (args.url) != "string") {
            throw "args.url must be a string value representing the full listen URL.";
        }
        var normalizedRelativePath = getNormalizedRelativePath(args.url);
        if (normalizedRelativePath == null) {
            throw "args.url must specify absolute listen URL with optional wildcard patterns supported by HTTP.SYS, e.g. http://*:8001/myapp/";
        }

        var serverImpl = {};
        serverImpl.normalizedRelativePath = normalizedRelativePath;
        serverImpl.normalizedAbsolutePath = getNormalizedAbsolutePath(args.url);
        serverImpl.args = args;
        serverImpl.api = {};

        serverImpl.api.start = function () {
            if (isShuttingDown) {
                throw "Http module is shutting down and cannot start servers at this time.";
            }
            trace("http::start " + serverImpl.normalizedRelativePath);
            servers[serverImpl.normalizedRelativePath] = serverImpl;
            try {
                addUrl(serverImpl.normalizedAbsolutePath);
            } catch (e) {
                delete servers[serverImpl.normalizedRelativePath];
                throw e;
            }
        };

        serverImpl.api.stop = function () {
            if (isShuttingDown) {
                throw "Http module is shutting down and cannot stop servers at this time.";
            }
            trace("http::stop " + serverImpl.normalizedRelativePath);
            removeUrl(serverImpl.args.url);
            delete servers[serverImpl.normalizedRelativePath];
        }

        return serverImpl.api;
    };

})();