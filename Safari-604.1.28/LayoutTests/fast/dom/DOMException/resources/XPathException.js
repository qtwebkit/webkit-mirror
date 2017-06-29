description("Tests the properties of the XPathException object.")

var e;
try {
    var evalulator = new XPathEvaluator;
    var nsResolver = evalulator.createNSResolver(document);
    var result = evalulator.evaluate("/body", document, nsResolver, 0, null);
    var num = result.numberValue;
   // raises a TYPE_ERR
} catch (err) {
    e = err;
}

shouldBeEqualToString("e.toString()", "TYPE_ERR (DOM XPath Exception 52): The expression could not be converted to return the specified type.");
shouldBeEqualToString("Object.prototype.toString.call(e)", "[object XPathException]");
shouldBeEqualToString("Object.prototype.toString.call(e.__proto__)", "[object XPathExceptionPrototype]");
shouldBeEqualToString("e.constructor.toString()", "function XPathException() {\n    [native code]\n}");
shouldBe("e.constructor", "window.XPathException");
shouldBe("e.TYPE_ERR", "e.constructor.TYPE_ERR");
shouldBe("e.INVALID_EXPRESSION_ERR", "51");
shouldBe("e.TYPE_ERR", "52");
