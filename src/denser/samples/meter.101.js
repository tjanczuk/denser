/*
meter.101

Demonstrates use of the metering module to get resource usage statistics. 
For demonstration of HTTP traffic measurements, see http.202.js. 

*/

var scheduler = require("scheduler");
var tracing = require("tracing");
var meter = require("meter");

var doSomeWork = function() {
    var result = 1;
    for (var i = 0; i < 50; i++) {
        result *= i;
    }
};

scheduler.later(doSomeWork, 500, true);

scheduler.later(function () {
    tracing.write("Current usage:");
    tracing.write(meter.getUsage());
}, 2000, true);

tracing.write("Press Ctrl-C to terminate.");