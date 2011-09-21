/*
scheduler.103

Demonstrates scheduling a repetitive callback and cancelling it after 3 repetitions.

*/

var tracing = require("tracing");
var scheduler = require("scheduler");

var counter = 0;
scheduler.later(
    function (ctx) {
        tracing.write("Executing callback");
        if (++counter === 3) {
            ctx.cancel();
        }
    }, 2000, true);

tracing.write("Callback is scheduled to execute 3 times at 2000ms interval.");