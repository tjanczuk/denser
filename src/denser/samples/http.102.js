/*
http.102

Demonstrates an HTTP server that buffers HTTP POST request body and responds with 
a "Hello, World!" string.

*/

var http = require("http");
var tracing = require("tracing");

http.createServer({
    url: "http://*:8001/foo/bar",
    onNewRequest: function (request) {
        var bufferedBody = "";
        request.onData = function (event) {
            bufferedBody += event.data;
            request.readMore();
        };
        request.onEnd = function (event) {
            tracing.write("Received request with body: " + bufferedBody);
            request.writeHeaders(200, "OK", { "Content-type": "text/plain" });
        };
        request.onWriteReady = function (event) {
            request.writeBody("Hello, world!", true);
        };
        request.readMore();
    }
}).start();

tracing.write("Listening at http://*:8001/foo/bar");
tracing.write("To test, send a simple string with HTTP POST to http://localhost:8001/foo/bar (or any sub-path) using Fiddler.");
tracing.write("Press Ctrl-C to terminate server.");