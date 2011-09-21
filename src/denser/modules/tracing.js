(function () {

    var write = imports.write;

    var defaultMaxDepth = 10;
    var newline = "\n";
    var indentStep = "  ";

    function format(indent, item, expandFunctions, depth, topLevel) {
        if (typeof (item) == "undefined") {
            return "<undefined>";
        }
        else if (depth == 0) {
            return "...";
        }
        else if (typeof (item) == "object") {
            var result = "{";
            for (var prop in item) {
                if (item.hasOwnProperty(prop)) {
                    result += (result.length > 1 ? "," : "")
                        + newline + indent + indentStep + '"' + prop + '" : ' + format(indent + indentStep, item[prop], expandFunctions, depth - 1);
                }
            }
            if (result.length == 1) {
                result += "}";
            }
            else {
                result += newline + indent + "}";
            }
            return result;
        }
        else if (typeof (item) == "function" && !expandFunctions) {
            return "<function>";
        }
        else if (typeof (item) == "string") {            
            return topLevel ? item : '"' + item + '"';
        }
        else {
            return item.toString();
        }
    }

    exports.write = function (item, expandFunctions, depth) {
        write(format("", item, typeof (expandFunctions) != "undefined" ? expandFunctions : false, depth || defaultMaxDepth, true));
    };

})();