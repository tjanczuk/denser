/*
meter.102

Demonstrates use of the metering module to get resource usage statistics 
for programs of different event frequency in multi-tenant situations.

*/

var scheduler = require("scheduler");
var tracing = require("tracing");
var meter = require("meter");
var programId = meter.getUsage().programId;
var eventFrequency = Math.round(Math.random() * 1000 + 1000);
var eventCount = 0;

var doSomeWork = function () {
    var result = 1;
    for (var i = 0; i < 10000000; i++) {
        result += i;
    }    
    eventCount++;
};

scheduler.later(doSomeWork, eventFrequency, true);

scheduler.later(function () {
    var usage = meter.getUsage();
    tracing.write("Program ID " + programId + ": CPU cycles: " + usage.cpu.applicationCpuCycles + ", Cycles/event: " + Math.round(usage.cpu.applicationCpuCycles / eventCount));
}, 5000, true);

tracing.write("Program ID " + programId + " is running with one event per " + eventFrequency + " ms. Press Ctrl-C to terminate.");