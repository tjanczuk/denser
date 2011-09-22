/*
chat

HTTP long polling based web chat.

*/

var http = require("http");
var tracing = require("tracing");
var scheduler = require("scheduler");
var chatui = require("chat.ui");
var dante = require("dante");

var programId = require("meter").getUsage().programId.substring(0,8);
var messageId = 0;
var messages = [];
var polls = {};

function releaseAllPolls() {
    for (var requestSlot in polls) {
        polls[requestSlot].timer.cancel();
        releasePoll(polls[requestSlot].request);
    }
    polls = {};
}

function releasePoll(request) {

    // parse query string to determine the last messageId the client received

    if (!request.last) {
        // TODO, tjanczuk, this is fragile, use regex instead
        request.last = parseInt(request.query.substring(6)); // "?last=xyz"
    }

    // determine the index of the message that should be returned in response to the poll

    var indexOfNextMessage = messages.length > 0 ? request.last - messages[0].messageId + 1 : 0;
    if (indexOfNextMessage < 0) {
        indexOfNextMessage = 0;
    }

    // return the message if it has been received already, otherwise indicate the poll needs to be parked

    if (indexOfNextMessage < messages.length) {
        try {
            request.onWriteReady = function () { request.writeBody(messages[indexOfNextMessage].message, true); };
            request.writeHeaders(200, "OK", {
                "Content-type": messages[indexOfNextMessage].contentType,
                "Content-length": messages[indexOfNextMessage].message.length.toString(),
                "Content-ID": messages[indexOfNextMessage].messageId.toString(),
                "Cache-Control": "no-cache"
            }, false);
        } catch (e) {
        }
        return true;
    }

    return false;
}

function timeoutPoll(requestSlot) {
    var poll = polls[requestSlot];
    delete polls[requestSlot];
    poll.request.writeHeaders(200, "OK", { "Content-length": "0", "Cache-Control": "no-cache" }, true);
}

function publishMessage(message, contentType) {
    messages.push({ messageId: ++messageId, message: message, contentType: contentType });
    if (messages.length > 50) { // cap the size of the message buffer to 50 most recent messages
        messages.shift();
    }    
    releaseAllPolls();
}

function scheduleDanteQuote() {
    scheduler.later(
        function () {
            publishMessage(dante.getNextLine(), "text/plain");
            scheduleDanteQuote();
        },
        200);
        //5000 + Math.floor(4000 * Math.random()));
}

function publishUsageUpdate() {
    publishMessage(chatui.getUsageUI(), "text/html"); 
}

function chatHttpHandler(request) {
    if (request.path === "/chat/" + programId + "/messages/") { // handle messaging logic
        var bufferedBody = "";
        request.onData = function (event) {
            bufferedBody += event.data;
            request.readMore();
        };
        request.onEnd = function (event) {
            if ("POST" === request.verb) { // publish message   
                publishMessage(bufferedBody, "text/plain");
                request.writeHeaders(200, "OK", {}, true);
            }
            else { // HTTP long poll for messages
                if (!releasePoll(request)) {
                    polls[request.requestSlot] = {
                        request: request,
                        timer: scheduler.later(function () { timeoutPoll(request.requestSlot); }, 15000)
                    };
                }
            }
        };
    }
    else { // serve HTML UI 
        request.onEnd = function () {
            request.onWriteReady = function () { request.writeBody(chatui.htmlForMultitenancy, true); };
            request.writeHeaders(200, "OK", { "Content-type": "text/html", "Content-length": chatui.htmlForMultitenancy.length.toString(), "Cache-Control": "no-cache" }, false);
        }
    }
    request.readMore();
}

http.createServer({ url: "http://*:80/chat/" + programId, onNewRequest: chatHttpHandler }).start(); // start message and UI server

scheduler.later(publishUsageUpdate, 10000, true); // publish usage statistics every 10 seconds

scheduleDanteQuote(); // schedule a post of Dante's Divine Comedy (Cante I, The Inferno)

publishUsageUpdate(); // publish once on startup to ensure every new client receives an immediate update

tracing.write("Chat started at http://localhost/chat/" + programId);