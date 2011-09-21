(function () {

    var createConnectionImpl = imports.createConnection;
    var setCallbacksImpl = imports.setCallbacks;
    var executeImpl = imports.execute;

    var executeStreaming = function (connection, query, callbacks) {

        var connectionStringKeywords = ['Driver={SQL Server}'];
        if (connection.server != undefined) {
            connectionStringKeywords.push('Server=' + connection.server);
        }
        if (connection.database != undefined) {
            connectionStringKeywords.push('Database=' + connection.database);
        }
        if (connection.user != undefined) {
            connectionStringKeywords.push('User Id=' + connection.user);
        }
        if (connection.password != undefined) {
            connectionStringKeywords.push('Password=' + connection.password);
        }
        if (connection.windowsauth != undefined) {
            connectionStringKeywords.push('Trusted_Connection=' + (connection.windowsauth ? "Yes" : "No"));
        }
        var connectionString = connectionStringKeywords.join(';');

        var slot = createConnectionImpl(connectionString, query, callbacks);
        return {};
    };

    var execute = function (connection, query, callbacks) {
        var rowValues = [];
        var meta = [];
        var ctx = {
            onMeta: function (column) {
                meta[column.index] = column;
            },
            onRow: function () {
                rowValues = [];
            },
            onColumn: function (index, value) {
                rowValues[index] = value;
            },
            onEndColumns: function () {
                if (callbacks.onRow) {
                    var row = {};
                    for (var idx = 0; idx < rowValues.length; idx++) {
                        row[meta[idx].name] = rowValues[idx];
                    }
                    callbacks.onRow(row);
                }
            },
            onEndRows: function () {
                if (callbacks.onDone) {
                    callbacks.onDone();
                }
            }
        };
        return executeStreaming(connection, query, ctx);
    };

    exports.executeStreaming = executeStreaming;
    exports.execute = execute;

})();

