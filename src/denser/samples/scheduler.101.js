/*
scheduler.101

Demonstrates scheduling a simple, one time, delayed callback. 

*/

var tracing = require("tracing");
var scheduler = require("scheduler");

scheduler.later(function () { tracing.write("Executing callback"); }, 3000);

tracing.write("Callback is scheduled to execute in 3000ms.");