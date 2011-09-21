(function () {

    var postEventDelayed = imports.postEventDelayed;
    var postEventAsap = imports.postEventAsap;

    exports.later = function (callback, dueTime, repeat) {
        if (typeof (callback) != "function") {
            throw "Callback parameter must be specified as a function.";
        }
        if (dueTime && typeof (dueTime) != "number") {
            throw "Invalid value of 'dueTime' parameter. Due time must be a numeric value indicating the number of miliseconds.";
        }
        if (typeof (repeat) != "undefined" && typeof (repeat) != "boolean") {
            throw "Invalid value of 'repeat' parameter. The 'repeat' is must be a boolean indicating whether the callback is to be invoked periodically instead of once.";
        }

        if (!dueTime) {
            postEventAsap(callback);
        }
        else {
            var timerState = { cancelled: false };

            var timerControl = {
                cancel: function () { timerState.cancelled = true; },
                isActive: function () { return !timerState.cancelled; }
            };

            var callbackThunk = function () {
                if (timerControl.isActive()) {
                    callback(timerControl);
                    if (repeat && timerControl.isActive()) {
                        postEventDelayed(callbackThunk, dueTime);
                    }
                    else {
                        timerControl.cancel();
                    }
                }
            };
            postEventDelayed(callbackThunk, dueTime);

            return timerControl;
        }        
    };

})();