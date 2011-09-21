using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;

namespace cschat
{
    static class ChatServiceUI
    {
        public static string UITemplate = @"<html>
<head>
    <title>Denser Web Chat</title>
    <script type=""text/javascript"" src=""http://ajax.aspnetcdn.com/ajax/jQuery/jquery-1.6.min.js""></script>
    <script type=""text/javascript"">

        var retryCount = 0;
        var lastMessage = ""0"";

        function showMessage(message, contentType) {
            var now = new Date().toLocaleTimeString().replace(/ /, ""&nbsp;"");
            if (contentType === ""text/html"") {
                $(""#stats"").html('Last&nbsp;update:&nbsp;' + now + ' (updates received every 10s)' + message);
            }
            else {
                var entry = '<tr class=""message-cell""><td>' + now + '</td><td>' +
                    $('<div/>').text(message).html() + '</td></tr>';
                $(""#messages"").prepend(entry);
                if ($(""#messages tr"").size() > 20) {
                    $(""#messages tr"").last().remove();
                }
            }
        }

        function ensurePolling() {
            var poll = $.ajax(""/chat/##CONTEXTID##messages/?last="" + lastMessage, {
                success: function (data, status, xhr) {
                    if (data.length > 0) {
                        showMessage(data, poll.getResponseHeader(""Content-Type""));
                    }
                    lastMessage = poll.getResponseHeader(""Content-ID"") || lastMessage;
                    retryCount = 0;
                    ensurePolling();
                },
                error: function (xhr, status, exception) {
                    if (retryCount++ < 5) {
                        ensurePolling();
                    }
                    else {
                        showMessage(""Several attempts to communicate with the Denser chat server failed. Server may be down, but try refreshing the page to see if it helps."");
                    }
                },
                timeout: 20000
            });
        }

        $(function () {
            $(""#message"").val("""");
            $(""#message"").keydown(function (event) {
                if (event.keyCode == 13) {
                    var v = $(""#message"").val();
                    if (v.length > 0) {
                        $.post(""/chat/##CONTEXTID##messages/"", v.substr(0, 500));
                        $(""#message"").val("""");
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
    <table class=""chat-table"">
        <tr>
            <td class=""chat-table-column"">
                <h2>
                    Denser Web Chat</h2>
                <p>
                    This application is built using the <a href=""http://tjanczuk-hp/denser/denser.pdf"">Denser prototype</a>.</p>
                <p>
                    Denser is a Win32, server side JavaScript host based on V8 engine and HTTP.SYS.
                    Denser is an incubation effort in the AppFabric team with the goal of lowering the
                    cost of running cloud applications by enabling high application density. Read more
                    about Denser <a href=""http://tjanczuk-hp/denser/denser.pdf"">here</a>. Get the Denser
                    binaries and samples <a href=""http://tjanczuk-hp/denser/"">here</a>. View source
                    code of the web chat server <a href=""http://tjanczuk-hp/denser/chat.js"">here</a>.
                    Contact <a href=""mailto:tjanczuk@microsoft.com"">tjanczuk</a> with any questions.</p>
                <p>
                    Type a message and press enter to send:</p>
                <textarea id=""message"" maxrows=""3"" style=""width: 100%; resize: none""></textarea>
                <p>
                    Twenty most recent messages:</p>
                <table id=""messages"">
                </table>
            </td>
            <td class=""chat-table-column"">
                <h2>
                    Server Resource Consumption</h2>
                <div id=""stats"">
                </div>
            </td>
        </tr>
    </table>
</body>
</html>
";

        public static byte[] UI { get; private set; }

        public static void CustomizeUI(Guid programId)
        {
            UI = Encoding.UTF8.GetBytes(UITemplate.Replace("##CONTEXTID##", programId.ToString().Substring(0, 8) + "/"));
        }
    }
}
