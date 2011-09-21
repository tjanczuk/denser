(function () {

    var getUsage = imports.getUsage;

    exports.getUsage = function () {
        var stats = getUsage();

        stats.cpu.applicationShareOfThreadTime = stats.cpu.applicationCpuCycles / stats.cpu.threadCpuCycles;

        return stats;
    };

})();