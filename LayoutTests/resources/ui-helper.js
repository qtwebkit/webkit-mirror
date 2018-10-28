
window.UIHelper = class UIHelper {
    static isIOS()
    {
        return navigator.userAgent.includes('iPhone') || navigator.userAgent.includes('iPad');
    }

    static isWebKit2()
    {
        return window.testRunner.isWebKit2;
    }

    static tapAt(x, y)
    {
        console.assert(this.isIOS());

        if (!this.isWebKit2()) {
            eventSender.addTouchPoint(x, y);
            eventSender.touchStart();
            eventSender.releaseTouchPoint(0);
            eventSender.touchEnd();
            return Promise.resolve();
        }

        return new Promise((resolve) => {
            testRunner.runUIScript(`
                uiController.singleTapAtPoint(${x}, ${y}, function() {
                    uiController.uiScriptComplete('Done');
                });`, resolve);
        });
    }

    static activateAt(x, y)
    {
        if (!this.isWebKit2() || !this.isIOS()) {
            eventSender.mouseMoveTo(x, y);
            eventSender.mouseDown();
            eventSender.mouseUp();
            return Promise.resolve();
        }

        return new Promise((resolve) => {
            testRunner.runUIScript(`
                uiController.singleTapAtPoint(${x}, ${y}, function() {
                    uiController.uiScriptComplete('Done');
                });`, resolve);
        });
    }

    static activateElement(element)
    {
        const x = element.offsetLeft + element.offsetWidth / 2;
        const y = element.offsetTop + element.offsetHeight / 2;
        return UIHelper.activateAt(x, y);
    }

    static keyDown(key)
    {
        if (!this.isWebKit2() || !this.isIOS()) {
            eventSender.keyDown(key);
            return Promise.resolve();
        }

        return new Promise((resolve) => {
            testRunner.runUIScript(`
                uiController.keyDownUsingHardwareKeyboard("downArrow", function() {
                    uiController.uiScriptComplete("Done");
                });`, resolve);
        });
    }

    static ensurePresentationUpdate()
    {
        if (!this.isWebKit2()) {
            testRunner.display();
            return Promise.resolve();
        }

        return new Promise(resolve => {
            testRunner.runUIScript(`
                uiController.doAfterPresentationUpdate(function() {
                    uiController.uiScriptComplete('Done');
                });`, resolve);
        });
    }

    static ensureVisibleContentRectUpdate()
    {
        if (!this.isWebKit2())
            return Promise.resolve();

        return new Promise(resolve => {
            const visibleContentRectUpdateScript = "uiController.doAfterVisibleContentRectUpdate(() => uiController.uiScriptComplete())";
            testRunner.runUIScript(visibleContentRectUpdateScript, resolve);
        });
    }

    static activateAndWaitForInputSessionAt(x, y)
    {
        if (!this.isWebKit2() || !this.isIOS())
            return this.activateAt(x, y);

        return new Promise(resolve => {
            testRunner.runUIScript(`
                (function() {
                    uiController.didShowKeyboardCallback = function() {
                        uiController.uiScriptComplete("Done");
                    };
                    uiController.singleTapAtPoint(${x}, ${y}, function() { });
                })()`, resolve);
        });
    }

    static waitForKeyboardToHide()
    {
        return new Promise(resolve => {
            testRunner.runUIScript(`
                (function() {
                    if (uiController.isShowingKeyboard)
                        uiController.didHideKeyboardCallback = () => uiController.uiScriptComplete();
                    else
                        uiController.uiScriptComplete();
                })()`, resolve);
        });
    }

    static getUICaretRect()
    {
        if (!this.isWebKit2() || !this.isIOS())
            return Promise.resolve();

        return new Promise(resolve => {
            testRunner.runUIScript(`(function() {
                uiController.doAfterNextStablePresentationUpdate(function() {
                    uiController.uiScriptComplete(JSON.stringify(uiController.textSelectionCaretRect));
                });
            })()`, jsonString => {
                resolve(JSON.parse(jsonString));
            });
        });
    }

    static getUISelectionRects()
    {
        if (!this.isWebKit2() || !this.isIOS())
            return Promise.resolve();

        return new Promise(resolve => {
            testRunner.runUIScript(`(function() {
                uiController.doAfterNextStablePresentationUpdate(function() {
                    uiController.uiScriptComplete(JSON.stringify(uiController.selectionRangeViewRects));
                });
            })()`, jsonString => {
                resolve(JSON.parse(jsonString));
            });
        });
    }

    static replaceTextAtRange(text, location, length) {
        return new Promise(resolve => {
            testRunner.runUIScript(`(() => {
                uiController.replaceTextAtRange("${text}", ${location}, ${length});
                uiController.uiScriptComplete('Done');
            })()`, resolve);
        });
    }

    static wait(promise)
    {
        testRunner.waitUntilDone();
        if (window.finishJSTest)
            window.jsTestIsAsync = true;

        let finish = () => {
            if (window.finishJSTest)
                finishJSTest();
            else
                testRunner.notifyDone();
        }

        return promise.then(finish, finish);
    }

    static withUserGesture(callback)
    {
        internals.withUserGesture(callback);
    }

    static selectFormAccessoryPickerRow(rowIndex)
    {
        const selectRowScript = `(() => uiController.selectFormAccessoryPickerRow(${rowIndex}))()`;
        return new Promise(resolve => testRunner.runUIScript(selectRowScript, resolve));
    }

    static selectFormPopoverTitle()
    {
        return new Promise(resolve => {
            testRunner.runUIScript(`(() => {
                uiController.uiScriptComplete(uiController.selectFormPopoverTitle);
            })()`, resolve);
        });
    }

    static enterText(text)
    {
        const escapedText = text.replace(/`/g, "\\`");
        const enterTextScript = `(() => uiController.enterText(\`${escapedText}\`))()`;
        return new Promise(resolve => testRunner.runUIScript(enterTextScript, resolve));
    }

    static setTimePickerValue(hours, minutes)
    {
        const setValueScript = `(() => uiController.setTimePickerValue(${hours}, ${minutes}))()`;
        return new Promise(resolve => testRunner.runUIScript(setValueScript, resolve));
    }

    static textContentType()
    {
        return new Promise(resolve => {
            testRunner.runUIScript(`(() => {
                uiController.uiScriptComplete(uiController.textContentType);
            })()`, resolve);
        });
    }

    static formInputLabel()
    {
        return new Promise(resolve => {
            testRunner.runUIScript(`(() => {
                uiController.uiScriptComplete(uiController.formInputLabel);
            })()`, resolve);
        });
    }

    static zoomScale()
    {
        return new Promise(resolve => {
            testRunner.runUIScript(`(() => {
                uiController.uiScriptComplete(uiController.zoomScale);
            })()`, resolve);
        });
    }
}
