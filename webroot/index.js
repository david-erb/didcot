import { WebSocketClient } from "./web-socket-client.js";
import { ScanlistParser } from "./scanlist-parser.js";
import { ScanlistManager } from "./scanlist-manager.js";
import { MultiRenderer } from "./multi-renderer.js";
import { ScanTrendChartRenderer } from "./scan-trend-chart-renderer.js";
import { TableRenderer } from "./table-renderer.js";

Chart.defaults.color = "#506078";
Chart.defaults.borderColor = "#202840";

document.addEventListener("DOMContentLoaded", () => {
    const statusEl = document.getElementById("status");

    const parser = new ScanlistParser({ littleEndian: true });

    const chartRenderer = new ScanTrendChartRenderer("trendChart");
    const tableRenderer = new TableRenderer(document.getElementById("scanlist-root"));

    const manager = new ScanlistManager({
        parser,
        renderer: new MultiRenderer([chartRenderer, tableRenderer]),
        renderIntervalMs: 100
    });

    const client = new WebSocketClient({
        wsUrl: getWebSocketUrl(),
        manager,
        reconnectDelayMs: 1000,
        onConnecting: () => {
            statusEl.textContent = "Connecting...";
        },
        onOpen: () => {
            statusEl.textContent = "Connected";
        },
        onClosed: (info) => {
            statusEl.textContent = info?.willRetry
                ? "Disconnected (retrying...)"
                : "Disconnected";
        },
        onError: () => {
            statusEl.textContent = "Connection error";
        }
    });

    manager.start();
    client.start();

    window.addEventListener("beforeunload", () => {
        client.stop();
    });
});

function getWebSocketUrl() {
    const params = new URLSearchParams(window.location.search);
    const wsParam = params.get("ws");

    if (wsParam) {
        return `ws://${wsParam}/`;
    }

    return "ws://localhost:14081/";
}
