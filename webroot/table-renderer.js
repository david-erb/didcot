export class TableRenderer {
    constructor(rootElement) {
        if (!rootElement) {
            throw new Error("TableRenderer requires rootElement");
        }

        this.rootElement = rootElement;
    }

    render(model) {
        if (!model) {
            this.rootElement.innerHTML = "<div>No data</div>";
            return;
        }

        const scans = Array.isArray(model.scans) ? model.scans : [];
        const valueColumnCount = this.getValueColumnCount(scans);

        const summaryHtml = this.buildSummaryHtml(model);
        const tableHtml = this.buildTableHtml(scans, valueColumnCount);

        this.rootElement.innerHTML = `
            <div class="scanlist-view">
                ${summaryHtml}
                ${tableHtml}
            </div>
        `;
    }

    buildSummaryHtml(model) {
        return `
            <div class="scanlist-summary">
                <div><strong>Last update:</strong> ${this.escapeHtml(this.formatWallClock(model.receivedAtMs))}</div>
                <div><strong>Scan count:</strong> ${this.escapeHtml(this.valueToDisplayString(model.scanCount))}</div>
                <div><strong>Channel count:</strong> ${this.escapeHtml(this.valueToDisplayString(model.channelCount))}</div>
                <div><strong>Bytes:</strong> ${this.escapeHtml(this.valueToDisplayString(model.messageByteLength))}</div>
            </div>
        `;
    }

    buildTableHtml(scans, valueColumnCount) {
        const headerHtml = this.buildHeaderHtml(valueColumnCount);
        const bodyHtml = this.buildBodyHtml(scans, valueColumnCount);

        return `
            <table class="scanlist-table">
                <thead>${headerHtml}</thead>
                <tbody>${bodyHtml}</tbody>
            </table>
        `;
    }

    buildHeaderHtml(valueColumnCount) {
        let html = `
            <tr>
                <th>Scan</th>
                <th>Timestamp (ns)</th>
                <th>Sequence</th>
        `;

        for (let i = 0; i < valueColumnCount; i++) {
            html += `<th>Ch ${i}</th>`;
        }

        html += `</tr>`;
        return html;
    }

    buildBodyHtml(scans, valueColumnCount) {
        if (scans.length === 0) {
            return `
                <tr>
                    <td colspan="${3 + valueColumnCount}">No scans</td>
                </tr>
            `;
        }

        let html = "";

        for (const scan of scans) {
            html += `<tr>`;
            html += `<td>${this.escapeHtml(this.valueToDisplayString(scan.index))}</td>`;
            html += `<td>${this.escapeHtml(this.valueToDisplayString(scan.timestampNs))}</td>`;
            html += `<td>${this.escapeHtml(this.valueToDisplayString(scan.sequenceNumber))}</td>`;

            for (let i = 0; i < valueColumnCount; i++) {
                const value = i < scan.values.length ? scan.values[i] : "";
                html += `<td>${this.escapeHtml(this.valueToDisplayString(value))}</td>`;
            }

            html += `</tr>`;
        }

        return html;
    }

    getValueColumnCount(scans) {
        let max = 0;

        for (const scan of scans) {
            const count = Array.isArray(scan.values) ? scan.values.length : 0;
            if (count > max) {
                max = count;
            }
        }

        return max;
    }

    formatWallClock(ms) {
        if (ms == null) {
            return "";
        }

        return new Date(ms).toLocaleTimeString();
    }

    valueToDisplayString(value) {
        if (value === null || value === undefined) {
            return "";
        }

        if (typeof value === "bigint") {
            return value.toString();
        }

        return String(value);
    }

    escapeHtml(value) {
        return String(value)
            .replaceAll("&", "&amp;")
            .replaceAll("<", "&lt;")
            .replaceAll(">", "&gt;")
            .replaceAll("\"", "&quot;")
            .replaceAll("'", "&#39;");
    }
}