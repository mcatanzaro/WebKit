function shouldBe(expected, actual, msg = "") {
    if (msg)
        msg = " for " + msg;
    if (actual !== expected)
        throw new Error("bad value" + msg + ": " + actual + ". Expected " + expected);
}
var AsyncFunctionPrototype = async function(){}.__proto__;

function call(o) { o.x = 3; }
noInline(call);

function sink (p, q) {
    async function f() { };
    if (p) {
        call(f); // Force allocation of f
        if (q) {
            OSRExit();
        }
        return f;
    }
    return { 'x': 2 };
}
noInline(sink);

for (var i = 0; i < testLoopCount; ++i) {
    var o = sink(true, false);
    shouldBe(o.__proto__, AsyncFunctionPrototype);
    if (o.x != 3)
        throw "Error: expected o.x to be 2 but is " + result;
}

// At this point, the function should be compiled down to the FTL

// Check that the function is properly allocated on OSR exit
var f = sink(true, true);
shouldBe(f.__proto__, AsyncFunctionPrototype);
if (f.x != 3)
    throw "Error: expected o.x to be 3 but is " + result;
