/*
chat

HTTP long polling based web chat.

*/

var http = require("http");
var chatui = require("./chat.ui");
var dante = require("./dante");
var url = require("url");

var requestSlot = 1;

if (process.argv.length != 3 && !portNumber) {
    console.log("Usage: node chat2.js <portNumber>");
    process.exit(1);
}

var port = portNumber || parseInt(process.argv[2]);

console.log("PID: " + process.pid + ", port: " + port);

var messageId = 0;
var messages = [];
var polls = {};

function releaseAllPolls() {
    for (var requestSlot in polls) {
        clearTimeout(polls[requestSlot].timer);
        releasePoll(polls[requestSlot].request, polls[requestSlot].response);
    }
    polls = {};
}

function releasePoll(request, response) {

    // parse query string to determine the last messageId the client received

    if (!request.last) {
        request.last = parseInt(request.url2.query["last"]); 
    }

    // determine the index of the message that should be returned in response to the poll

    var indexOfNextMessage = messages.length > 0 ? request.last - messages[0].messageId + 1 : 0;
    if (indexOfNextMessage < 0) {
        indexOfNextMessage = 0;
    }

    // return the message if it has been received already, otherwise indicate the poll needs to be parked

    if (indexOfNextMessage < messages.length) {
        try {            
            response.writeHead(200, "OK", {
                "Content-Type": messages[indexOfNextMessage].contentType,
                //"Content-length": messages[indexOfNextMessage].message.length.toString(),
                "Content-ID": messages[indexOfNextMessage].messageId.toString(),
                "Cache-Control": "no-cache"
            });
            response.end(messages[indexOfNextMessage].message);
        } catch (e) {
        }
        return true;
    }

    return false;
}

function timeoutPoll(requestSlot) {
    var poll = polls[requestSlot];
    delete polls[requestSlot];
    poll.response.writeHead(200, "OK", { "Content-length": "0", "Cache-Control": "no-cache" });
    poll.response.end();
}

function publishMessage(message, contentType) {
    messages.push({ messageId: ++messageId, message: message, contentType: contentType });
    if (messages.length > 50) { // cap the size of the message buffer to 50 most recent messages
        messages.shift();
    }    
    releaseAllPolls();
}

function scheduleDanteQuote() {
    setTimeout(
        function () {
            publishMessage(dante.getNextLine(), "text/plain");
            scheduleDanteQuote();
        },
        200);
        //5000 + Math.floor(4000 * Math.random()));
}

function publishUsageUpdate() {
    publishMessage(chatui.getUsageUI(), "text/html"); 
    setTimeout(publishUsageUpdate, 10000); // publish usage statistics every 10 seconds
}

function chatHttpHandler(request, response) {
    request.url2 = url.parse(request.url, true);
    if (request.url2.pathname === "/messages") { // handle messaging logic
        var bufferedBody = "";
        request.on('data', function (chunk) {
            bufferedBody += chunk;
        });
        request.on('end', function () {
            if ("POST" === request.method) { // publish message   
                publishMessage(bufferedBody, "text/plain");
                response.writeHead(200, "OK");
                response.end();
            }
            else { // HTTP long poll for messages
                if (!releasePoll(request, response)) {
                    request.requestSlot = requestSlot++;                   
                    polls[request.requestSlot] = {
                        request: request,
                        response: response,
                        timer: setTimeout(function () { timeoutPoll(request.requestSlot); }, 15000)
                    };
                }
            }
        });
    }
    else { // serve HTML UI 
        response.writeHead(200, "OK", { "Content-Type": "text/html", "Cache-Control": "no-cache" });
        response.end(chatui.htmlForMultitenancy);
    }
}

http.createServer(chatHttpHandler).listen(port); // start message and UI server

//scheduleDanteQuote(); // schedule a post of Dante's Divine Comedy (Cante I, The Inferno)

//publishUsageUpdate(); // publish once on startup to ensure every new client receives an immediate update

console.log("Chat server running. Navigate to http://localhost:" + port + "/ with the browser. Press Ctrl-C to terminate the server.");

//setInterval(function () { console.log(process.memoryUsage()) }, 5000);
