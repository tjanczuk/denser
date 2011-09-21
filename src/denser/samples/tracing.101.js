/*
tracing.101

Demonstrates importing the tracing module and using its functionlity to output a string,
a number, and a simple object.

*/

var tracing = require("tracing");

// a string

tracing.write("This is a simple string");

// a number

tracing.write(101);

// a complex object

var anObject = { 
    property1: "A string value",
    property2: 101,
    property3: {
        property3_1: "Another string value",
        property3_2: 2
    }
};
tracing.write(anObject);

// an array

var anArray = ["tracing", 101, { property1: "A string property", property2: 2}];
tracing.write(anArray);