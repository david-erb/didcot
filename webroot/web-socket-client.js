export class WebSocketClient {
    constructor(options = {}) {
        this.wsUrl = options.wsUrl ?? null;
        this.manager = options.manager ?? null;
        this.reconnectDelayMs = options.reconnectDelayMs ?? 1000;

        this.onConnecting = options.onConnecting ?? null;
        this.onOpen = options.onOpen ?? null;
        this.onClosed = options.onClosed ?? null;
        this.onError = options.onError ?? null;

        this.ws = null;
        this.isStopping = false;
        this.reconnectTimer = null;
    }

    start() {
        this.isStopping = false;
        this.clearReconnectTimer();
        this.tryConnect();
    }

    stop() {
        this.isStopping = true;
        this.clearReconnectTimer();

        if (this.ws) {
            this.ws.close();
            this.ws = null;
        }
    }

    tryConnect() {
        if (!this.wsUrl) {
            throw new Error("WebSocketClient requires wsUrl");
        }

        this.raise(this.onConnecting);

        const ws = new WebSocket(this.wsUrl);
        ws.binaryType = "arraybuffer";

        ws.onopen = () => {
            console.log(`WebSocket connected: ${this.wsUrl}`);
            this.raise(this.onOpen);
        };

        ws.onmessage = (event) => {
            this.handleMessage(event);
        };

        ws.onclose = () => {
            console.log("WebSocket closed");
            this.ws = null;

            const willRetry = !this.isStopping;

            this.raise(this.onClosed, {
                willRetry,
                reconnectDelayMs: this.reconnectDelayMs
            });

            if (willRetry) {
                this.reconnectTimer = setTimeout(() => {
                    this.reconnectTimer = null;
                    this.tryConnect();
                }, this.reconnectDelayMs);
            }
        };

        ws.onerror = (error) => {
            console.error("WebSocket error", error);
            this.raise(this.onError, error);
        };

        this.ws = ws;
    }

    handleMessage(event) {
        if (!this.manager || typeof this.manager.handleMessage !== "function") {
            return;
        }

        this.manager.handleMessage(event.data, {
            receivedAtMs: Date.now()
        });
    }

    clearReconnectTimer() {
        if (this.reconnectTimer !== null) {
            clearTimeout(this.reconnectTimer);
            this.reconnectTimer = null;
        }
    }

    raise(callback, arg) {
        if (typeof callback === "function") {
            callback(arg);
        }
    }
}