/*
scheduler.102

Demonstrates scheduling a repetitive callback. 

*/

var tracing = require("tracing");
var scheduler = require("scheduler");

scheduler.later(function () { tracing.write("Executing callback"); }, 2000, true);

tracing.write("Callback is scheduled to execute every 2000ms. Press Ctrl-C to terminate.");