//@ skip if $model == "Apple Watch Series 3" # added by mark-jsc-stress-test.py
function foo(o) {
    var count = 0;
    for (var p in o) {
        if (o[p])
            count ++;
    }
    return count;
}
noInline(foo);

var total = 0;
for (let j = 0; j < 100000; ++j)
    total += foo(new Error);

