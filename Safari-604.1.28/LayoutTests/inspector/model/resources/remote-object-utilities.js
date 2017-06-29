function runInBrowserTest() {
    if (window.testRunner)
        return;
 
    test(); // Populate window.steps.

    for (let step of steps) {
        console.info("EXPRESSION", step.expression);
        try {
            console.log(eval(step.expression));
        } catch (e) {
            console.log("EXCEPTION: " + e);
        }
    }
}

TestPage.registerInitializer(() => {
    function remoteObjectJSONFilter(key, value) {
        if (key === "_target" || key === "_hasChildren" || key === "_listeners")
            return undefined;
        if (key === "_objectId" || key === "_stackTrace")
            return "<filtered>";
        return value;
    }

    window.runSteps = function(steps) {
        let currentStepIndex = 0;

        function checkComplete() {
            if (++currentStepIndex >= steps.length)
                InspectorTest.completeTest();
        }

        for (let {expression, browserOnly} of steps) {
            if (browserOnly) {
                checkComplete();
                continue;
            }

            WebInspector.runtimeManager.evaluateInInspectedWindow(expression, {objectGroup: "test", doNotPauseOnExceptionsAndMuteConsole: true, generatePreview: true}, (remoteObject, wasThrown) => {
                InspectorTest.log("");
                InspectorTest.log("-----------------------------------------------------");
                InspectorTest.log("EXPRESSION: " + expression);
                InspectorTest.assert(remoteObject instanceof WebInspector.RemoteObject);
                InspectorTest.log(JSON.stringify(remoteObject, remoteObjectJSONFilter, 2));
                checkComplete();
            });
        }
    };
});
