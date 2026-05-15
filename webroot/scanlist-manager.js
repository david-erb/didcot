export class ScanlistManager {
    constructor(options = {}) {
        this.parser = options.parser ?? null;
        this.renderer = options.renderer ?? null;
        this.renderIntervalMs = options.renderIntervalMs ?? 1000;

        this.latestModel = null;
        this.renderTimerId = null;
    }

    start() {
        if (this.renderTimerId !== null) {
            return;
        }

        this.renderTimerId = setInterval(() => {
            this.renderLatest();
        }, this.renderIntervalMs);
    }

    stop() {
        if (this.renderTimerId !== null) {
            clearInterval(this.renderTimerId);
            this.renderTimerId = null;
        }
    }

    handleMessage(data, meta = {}) {
        try {
            const arrayBuffer = this.coerceToArrayBuffer(data);
            const decoded = this.parser.unpackScanlistFromArrayBuffer(arrayBuffer);
            this.latestModel = this.toRenderModel(decoded, meta);
        }
        catch (error) {
            console.error("ScanlistManager.handleMessage failed", error);
        }
    }

    coerceToArrayBuffer(data) {
        if (data instanceof ArrayBuffer) {
            return data;
        }

        if (typeof Blob !== "undefined" && data instanceof Blob) {
            throw new Error("Expected ArrayBuffer but got Blob. Set ws.binaryType = 'arraybuffer'.");
        }

        throw new Error(`Unsupported message type: ${Object.prototype.toString.call(data)}`);
    }

    toRenderModel(decoded, meta = {}) {
        const scans = Array.isArray(decoded.scans) ? decoded.scans : [];

        return {
            receivedAtMs: meta.receivedAtMs ?? Date.now(),
            messageByteLength: decoded.messageByteLength ?? null,
            scanCount: decoded.scanCount ?? scans.length,
            channelCount: decoded.channelCount ?? 0,
            scans: scans.map((scan, index) => ({
                index,
                timestampNs: scan.timestampNs ?? null,
                sequenceNumber: scan.sequenceNumber ?? null,
                values: Array.isArray(scan.values) ? scan.values.slice() : []
            }))
        };
    }

    renderLatest() {
        if (!this.latestModel) {
            return;
        }

        if (!this.renderer || typeof this.renderer.render !== "function") {
            return;
        }

        this.renderer.render(this.latestModel);
    }
}