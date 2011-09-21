/*
tracing.102

Demonstrates tracing function values with and without showing source code.

*/

var tracing = require("tracing");

var anObject = {
    addNumbers: function (a, b) { return a + b; },
    anotherProperty: 102
};

// tracing function values without showing source code

tracing.write(anObject);

// tracing function values with showing function source code

tracing.write(anObject, true); 