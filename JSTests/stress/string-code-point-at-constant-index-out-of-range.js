function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

function test(string) {
    return string.codePointAt(0);
}
noInline(test);

for (var i = 0; i < testLoopCount; ++i)
    shouldBe(test(""), undefined);
