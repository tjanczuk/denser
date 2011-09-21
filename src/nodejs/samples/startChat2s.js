var spawn = require("child_process").spawn;
var cwd = require("path").dirname(process.argv[1]);

if (process.argv.length != 3) {
    console.log("Usage: node startChat2s.js <numberOfProcesses>");
    process.exit(1);
}

var count = parseInt(process.argv[2]);
var port = 8001;
while (count > 0) {
    var child = spawn(process.argv[0], [ '--max_old_space_size=3', '--max_new_space_size=50', cwd + "/chat2.js", port ]);
    console.log("PID: " + child.pid + ", URL: http://localhost:" + port + "/");
    child.on('exit', function (code) { console.log('child process exited with code ' + code); });
    child.stdin.end();
    count--;
    port++;
}
console.log("All processes started");
