/*
http.202

Demonstrates use of the metering module to get statistics of HTTP traffic.

*/

var http = require("http");
var scheduler = require("scheduler");
var tracing = require("tracing");
var meter = require("meter");

http.createServer({
    url: "http://*:8001/foo/bar",
    onNewRequest: function (request) {
        request.onEnd = function (event) {
            request.writeHeaders(200, "OK", { "Content-type": "text/plain" });
        };
        request.onWriteReady = function (event) {
            request.writeBody("To speed up your service, please pay your delinquent bills.", true);
        };
        tracing.write("Received request. Response will be sent in 3000ms.");
        scheduler.later(request.readMore, 3000); // finish reading HTTP request in 3 seconds
    }
}).start();

scheduler.later(function () {
    tracing.write("Current usage:");
    tracing.write(meter.getUsage()); 
}, 2000, true);

tracing.write("Listening at http://*:8001/foo/bar");
tracing.write("To test, navigate to http://localhost:8001/foo/bar (or any sub-path) with the browser.");
tracing.write("Press Ctrl-C to terminate server.");