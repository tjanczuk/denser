(function () {

    var meter = require("meter");

    var programId = meter.getUsage().programId;
    var start = new Date();

    function hereDoc(f, programId) {
        return f.toString().replace(/^[^\/]+\/\*!?/, '').replace(/\*\/[^\/]+$/, '').replace(/##CONTEXTID##/g, programId);
    }

    function chatUi() {
        /*
<html>
<head>
    <title>Denser Web Chat</title>
    <script type="text/javascript" src="http://ajax.aspnetcdn.com/ajax/jQuery/jquery-1.6.min.js"></script>
    <script type="text/javascript">

        var retryCount = 0;
        var lastMessage = "0";

        function showMessage(message, contentType) {
            var now = new Date().toLocaleTimeString().replace(/ /, "&nbsp;");
            if (contentType === "text/html") {
                $("#stats").html('Last&nbsp;update:&nbsp;' + now + ' (updates received every 10s)' + message);
            }
            else {
                var entry = '<tr class="message-cell"><td>' + now + '</td><td>' +
                    $('<div/>').text(message).html() + '</td></tr>';
                $("#messages").prepend(entry);
                if ($("#messages tr").size() > 20) {
                    $("#messages tr").last().remove();
                }
            }
        }

        function ensurePolling() {
            var poll = $.ajax("/chat/##CONTEXTID##messages/?last=" + lastMessage, {
                success: function (data, status, xhr) {
                    if (data.length > 0) {
                        showMessage(data, poll.getResponseHeader("Content-Type"));
                    }
                    lastMessage = poll.getResponseHeader("Content-ID") || lastMessage;
                    retryCount = 0;
                    ensurePolling();
                },
                error: function (xhr, status, exception) {
                    if (retryCount++ < 5) {
                        ensurePolling();
                    }
                    else {
                        showMessage("Several attempts to communicate with the Denser chat server failed. Server may be down, but try refreshing the page to see if it helps.");
                    }
                },
                timeout: 20000
            });
        }

        $(function () {
            $("#message").val("");
            $("#message").keydown(function (event) {
                if (event.keyCode == 13) {
                    var v = $("#message").val();
                    if (v.length > 0) {
                        $.post("/chat/##CONTEXTID##messages/", v.substr(0, 500));
                        $("#message").val("");
                    }
                    event.preventDefault();
                }
            });
            ensurePolling();
        });
    </script>
    <style>
        .chat-table
        {
            align: center;
            width: 800px;
        }
        .chat-table-column
        {
            vertical-align: top;
            width: 50%;
            padding-right: 20px;
        }
        .message-cell
        {
            padding-top: 20px;
            vertical-align: top;
        }
        .usage-stats-container
        {
            width: 385px;
            vertical-align: top;
            font-size: 0.7em;
        }
        .usage-stats-table
        {
            font-size: 1em;
            width: 100%;
        }
        .usage-stats-column
        {
            vertical-align: top;
            width: 50%;
        }
        .usage-stats-header
        {
            text-align: center;
            background: grey;
            color: white;
            font-weight: bold;
        }
        .usage-stats-entry
        {
            font-size: 1em;
            text-align: right;
        }
    </style>
</head>
<body>
    <table class="chat-table">
        <tr>
            <td class="chat-table-column">
                <h2>
                    Denser Web Chat</h2>
                <p>
                    Type a message and press enter to send:</p>
                <textarea id="message" maxrows="3" style="width: 100%; resize: none"></textarea>
                <p>
                    Twenty most recent messages:</p>
                <table id="messages">
                </table>
            </td>
            <td class="chat-table-column">
                <h2>
                    Server Resource Consumption</h2>
                <div id="stats">
                </div>
            </td>
        </tr>
    </table>
</body>
</html>
        */
    };

    exports.html = hereDoc(chatUi, "");

    exports.htmlForMultitenancy = hereDoc(chatUi, programId.substring(0, 8) + "/");

    exports.getUsageUI = function () {
        var stats = meter.getUsage();
        var uptimeSeconds = (new Date() - start) / 1000;
        var result = '<p>Denser server uptime is ' + Math.floor(uptimeSeconds / 3600) + ' hours, ' + Math.floor((uptimeSeconds % 3600) / 60) + ' minutes, ' + Math.floor(uptimeSeconds % 60) + ' seconds.</p>' +
            '<p>Program ID:<br/>' + programId + '</p>' + 
            '<div class="usage-stats-container"><table class="usage-stats-table"><tr class="usage-stats-header"><td colspan="4">CPU usage</td></tr><tr class="usage-stats-entry"><th>Metric</th><th>Total</th><th>JavaScript&nbsp;only</th><th>JavaScript&nbsp;share&nbsp;[%]</th></tr>' +
            '<tr class="usage-stats-entry"><td>CPU&nbsp;cycles&nbsp;[#]</td><td>' + stats.cpu.threadCpuCycles + '</td><td>' + stats.cpu.applicationCpuCycles + '</td><td>' + (stats.cpu.applicationShareOfThreadTime * 100).toFixed(1) + '</td></tr>' +
            '</table></div>' +
            '<div class="usage-stats-container">' +
            '<table class="usage-stats-table"><tr><td class="usage-stats-column"><table class="usage-stats-table">' +
            '<tr class="usage-stats-header"><td colspan="2">Network usage (HTTP)</td></tr>' +
            '<tr class="usage-stats-entry"><th>Bytes&nbsp;received</th><td>' + stats.http.httpServerBytesReceived + '</td></tr>' +
            '<tr class="usage-stats-entry"><th>Bytes&nbsp;sent</th><td>' + stats.http.httpServerBytesSent + '</td></tr>' +
            '<tr class="usage-stats-entry"><th>Successful&nbsp;requests</th><td>' + stats.http.httpServerRequestsCompleted + '</td></tr>' +
            '<tr class="usage-stats-entry"><th>Aborted&nbsp;requests</th><td>' + stats.http.httpServerRequestsAborted + '</td></tr>' +
            '<tr class="usage-stats-entry"><th>Active&nbsp;requests</th><td>' + stats.http.httpServerRequestsActive + '</td></tr>' +
            '</table></td><td class="usage-stats-column">' +
            '<table class="usage-stats-table"><tr class="usage-stats-header"><td colspan="2">Memory usage</td></tr>' +
            '<tr class="usage-stats-entry"><th>Used heap size</th><td>' + stats.memory.usedHeapSize + '</td></tr>' +
            '<tr class="usage-stats-entry"><th>Total heap size</th><td>' + stats.memory.totalHeapSize + '</td></tr>' +
            '<tr class="usage-stats-entry"><th>Total heap size executable</th><td>' + stats.memory.totalHeapSizeExecutable + '</td></tr></table>' +
            '</td></tr></table></div>';

        return result;
    };
})();