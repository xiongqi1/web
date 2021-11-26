var logfile = PageTableObj("logfile", "system log", {
    readOnly: true,

    tableAttribs: { class: "no-border" },

    customLua: {
        lockRdb: false,

        get: function() {
            return [
                "local h = io.popen('/sbin/logcat.sh -a')\n",
                "o = h:read('*a')\n",
                "h:close()\n",
            ];
        },
    },

    /**
     * The log_str contains entire syslog as string blob. This function breaks it into lines and further breaks each
     * line as date, machine, level, process and message items. No further sub-parsing is performed.
     *
     * E.g.
     * Jan  5 22:24:41 Aurus-DD35 user.err kernel: [    0.233541] msm_pcie_probe: PCIe: Driver probe failed for RC0:-517
     *
     * @returns     An array of objects, each object representing a syslog line.
     */
    decodeRdb: function(log_str) {
        var lines = log_str.match(/[^\r\n]+/g);
        var rows = [];

        lines.forEach((line) => {
            var idx;
            var row = {};
            /* Split the line at white spaces. */
            var parts = line.split(/\s+/);

            try {
                row['date']    = parts[0] + ' ' + parts[1] + ' ' + parts[2];
                row['machine'] = parts[3];
                row['level']   = parts[4];
                row['process'] = parts[5].replace(':', ' ').trim();
                row['message'] = parts.slice(6).join(' ')

                rows.push(row);
            }
            catch (err) {
                /* Log it and skip this line. */
                console.log('Parse failed: ' + err.message);
                console.log('Actual text: ' + line);
            }
        });

        return rows;
    },

    members: [
        staticTextVariable('date', 'date'),
        staticTextVariable('machine', 'machine'),
        staticTextVariable('level', 'level'),
        staticTextVariable('process', 'process'),
        staticTextVariable('message', 'message')
    ]
});

var pageData : PageDef = {
#if !defined V_WEBIF_SPEC_saturn
    onDevice : false,
#endif

    title: "System Logs",

    authenticatedOnly: true,

    menuPos: ["System", "LOGFILE"],
    pageObjects: [ logfile ],
}
