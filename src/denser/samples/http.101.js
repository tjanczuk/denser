/*
http.101

Demonstrates a simple HTTP server reponsing with a "Hello, World!" string to all requests.

*/

var http = require("http");
var tracing = require("tracing");

http.createServer({
    url: "http://*:8001/foo/bar",
    onNewRequest: function (request) {
        tracing.write("Received request")
        tracing.write(request);
        request.onEnd = function (event) {
            request.writeHeaders(200, "OK", { "Content-type": "text/plain" });
        };
        request.onWriteReady = function (event) {
            request.writeBody("Hello, world!", true);
        };
        request.readMore();
    }
}).start();

tracing.write("Listening at http://*:8001/foo/bar");
tracing.write("To test, navigate to http://localhost:8001/foo/bar (or any sub-path) with the browser.");
tracing.write("Press Ctrl-C to terminate server.");