var spawn = require("child_process").spawn;
var cwd = require("path").dirname(process.argv[1]);

if (process.argv.length != 4) {
    console.log("Usage: node startChat2Farm.js <numberOfProcesses> <numberOfAppsPerProcess>");
    process.exit(1);
}

var count = parseInt(process.argv[2]);
var apps = parseInt(process.argv[3]);
var port = 8001;
while (count > 0) {
    var args = [ cwd + "/vmStartChat2.js", apps, port ];
    console.log('Starting process ' + count);
    var child = spawn(process.argv[0], args);
    child.on('exit', function (code) { console.log('child process exited with code ' + code); });
    //child.stdout.on('data', function (data) { console.log('' + data); });
    child.stdin.end();
    count--;
    port += apps;
}
console.log("All processes started");
