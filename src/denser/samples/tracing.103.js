/*
tracing.103

Demonstrates tracing cyclic graphs of objects.

*/

var tracing = require("tracing");

var anObject = {
    atomProperty: 103
};
anObject.recustiveProperty = anObject;

// tracing cyclic graphs with default depth

tracing.write(anObject);

// tracing cyclic graphs with custom depth

tracing.write(anObject, false, 3); 