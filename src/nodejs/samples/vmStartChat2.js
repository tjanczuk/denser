var vm = require("vm");

if (process.argv.length != 4) {
    console.log("Usage: node vmStartChat2s.js <numberOfApplications> <startingPortNumber>");
    process.exit(1);
}

var chat2 = require("fs").readFileSync('./chat2.js', 'utf-8'); // this would come from an HTTP POST
var count = parseInt(process.argv[2]);
var port = parseInt(process.argv[3]);

while (count > 0) {

    // this is where we would define the "cloud profile" of node.js
    // as well as install some application-scope interception points (e.g. metering)
    var sandbox = { 
        portNumber: port, 
        require: require, // needs to be wrapped to only allow a whitelist of modules/APIs
        process: process, // needs to be cut; this is for a temporary test only
        console: console,
        setTimeout: setTimeout,
        setInterval: setInterval,
        clearTimeout: clearTimeout
    };

    vm.runInNewContext(chat2, sandbox, './chat2.js');
    port++;
    count--;
}
