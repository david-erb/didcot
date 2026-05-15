export class ScanTrendChartRenderer {
    constructor(divId, options = {}) {
        this.divId = divId;
        this.windowNs = options.windowNs ?? 10_000_000_000; // 10 seconds
        this.channelCount = options.channelCount ?? 4;

        this.chart = null;
        this.canvas = null;

        this.lastTimestampNs = null;
        this.seenKeys = new Set();

        this.yMin = null;
        this.yMax = null;

        this.seriesColors = options.seriesColors ?? [
            "rgb(255, 80, 80)",
            "rgb(80, 160, 255)",
            "rgb(80, 200, 120)",
            "rgb(255, 180, 60)"
        ];
    }

    render(model) {
        if (!model || !Array.isArray(model.scans) || model.scans.length === 0) {
            return;
        }

        this.ensureChart();

        for (const scan of model.scans) {
            const timestampNs = Number(scan.timestampNs);

            if (!Number.isFinite(timestampNs)) {
                continue;
            }

            const values = Array.isArray(scan.values) ? scan.values : [];
            const key = this.makeScanKey(scan, timestampNs);

            if (this.seenKeys.has(key)) {
                continue;
            }

            this.seenKeys.add(key);
            this.lastTimestampNs = timestampNs;

            for (let channelIndex = 0; channelIndex < this.channelCount; channelIndex++) {
                const value = Number(values[channelIndex]);

                if (!Number.isFinite(value)) {
                    continue;
                }

                this.expandYRange(value);

                this.chart.data.datasets[channelIndex].data.push({
                    x: timestampNs,
                    y: value
                });
            }
        }

        this.pruneOldData();
        this.updateAxes();
        this.chart.update("none");
    }

    makeScanKey(scan, timestampNs) {
        if (scan.sequenceNumber !== null && scan.sequenceNumber !== undefined) {
            return `${timestampNs}:${scan.sequenceNumber}`;
        }

        if (scan.index !== null && scan.index !== undefined) {
            return `${timestampNs}:${scan.index}`;
        }

        return `${timestampNs}:${Math.random()}`;
    }

    ensureChart() {
        if (this.chart) {
            return;
        }

        const host = document.getElementById(this.divId);
        if (!host) {
            throw new Error(`ScanTrendChartRenderer: div '${this.divId}' not found.`);
        }

        host.innerHTML = "";

        this.canvas = document.createElement("canvas");
        this.canvas.style.width = "100%";
        this.canvas.style.height = "100%";
        host.appendChild(this.canvas);

        const datasets = [];
        for (let i = 0; i < this.channelCount; i++) {
            datasets.push({
                label: `Ch ${i + 1}`,
                data: [],
                borderColor: this.seriesColors[i % this.seriesColors.length],
                borderWidth: 2,
                fill: false,
                pointRadius: 0,
                pointHoverRadius: 0,
                tension: 0
            });
        }

        this.chart = new Chart(this.canvas.getContext("2d"), {
            type: "line",
            data: {
                datasets
            },
            options: {
                animation: false,
                responsive: true,
                maintainAspectRatio: false,
                normalized: false,
                parsing: false,
                plugins: {
                    legend: {
                        display: true
                    }
                },
                scales: {
                    x: {
                        type: "linear",
                        min: 0,
                        max: this.windowNs,
                        title: {
                            display: true,
                            text: "Time (s)"
                        },
                        ticks: {
                            callback: (value) => {
                                return (Number(value) / 1_000_000_000).toFixed(1);
                            }
                        }
                    },
                    y: {
                        title: {
                            display: true,
                            text: "Sensor value"
                        }
                    }
                }
            }
        });
    }

    expandYRange(value) {
        if (this.yMin === null || value < this.yMin) {
            this.yMin = value;
        }

        if (this.yMax === null || value > this.yMax) {
            this.yMax = value;
        }

        if (this.yMin === this.yMax) {
            this.yMin -= 1;
            this.yMax += 1;
        }
    }

    pruneOldData() {
        if (!this.chart || this.lastTimestampNs === null) {
            return;
        }

        const minX = this.lastTimestampNs - this.windowNs;

        for (const dataset of this.chart.data.datasets) {
            const data = dataset.data;
            let firstKeepIndex = 0;

            while (firstKeepIndex < data.length && data[firstKeepIndex].x < minX) {
                firstKeepIndex++;
            }

            if (firstKeepIndex > 0) {
                data.splice(0, firstKeepIndex);
            }
        }

        if (this.seenKeys.size > 50000) {
            this.rebuildSeenKeys();
        }
    }

    rebuildSeenKeys() {
        this.seenKeys.clear();

        for (const dataset of this.chart.data.datasets) {
            for (const point of dataset.data) {
                this.seenKeys.add(`${point.x}`);
            }
        }
    }

    updateAxes() {
        if (!this.chart || this.lastTimestampNs === null) {
            return;
        }

        const maxX = this.lastTimestampNs;
        const minX = maxX - this.windowNs;

        this.chart.options.scales.x.min = minX;
        this.chart.options.scales.x.max = maxX;

        if (this.yMin !== null && this.yMax !== null) {
            this.chart.options.scales.y.min = this.yMin;
            this.chart.options.scales.y.max = this.yMax;
        }
    }
}