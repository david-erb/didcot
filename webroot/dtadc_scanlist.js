class DtPackxReader {
    constructor(buffer) {
        if (!(buffer instanceof ArrayBuffer)) {
            throw new Error("DtPackxReader requires an ArrayBuffer");
        }

        this.buffer = buffer;
        this.view = new DataView(buffer);
        this.offset = 0;
    }

    remaining() {
        return this.view.byteLength - this.offset;
    }

    requireBytes(count) {
        if (this.remaining() < count) {
            throw new Error(
                `Buffer underrun: need ${count} bytes, have ${this.remaining()} at offset ${this.offset}`
            );
        }
    }

    readInt32() {
        this.requireBytes(4);

        // false = big-endian
        const value = this.view.getInt32(this.offset, true);
        this.offset += 4;
        return value;
    }

    readInt64() {
        this.requireBytes(8);

        // false = big-endian
        const value = this.view.getBigInt64(this.offset, true);
        this.offset += 8;
        return value;
    }
}

class DtAdcScan {
    constructor(timestampNs, sequenceNumber, channels) {
        this.timestampNs = timestampNs;         // BigInt
        this.sequenceNumber = sequenceNumber;   // BigInt
        this.channels = channels;               // number[]
    }

    static unpack(reader, channelCount) {
        if (channelCount < 0) {
            throw new Error("channelCount cannot be negative");
        }

        const timestampNs = reader.readInt64();
        const sequenceNumber = reader.readInt64();

        const channels = new Array(channelCount);
        for (let i = 0; i < channelCount; i++) {
            channels[i] = reader.readInt32();
        }

        return new DtAdcScan(timestampNs, sequenceNumber, channels);
    }
}

class DtAdcScanlist {
    constructor(scanCount, channelCount, scans) {
        this.scanCount = scanCount;
        this.channelCount = channelCount;
        this.scans = scans; // DtAdcScan[]
    }

    static unpackFromArrayBuffer(buffer) {
        const reader = new DtPackxReader(buffer);
        const result = DtAdcScanlist.unpack(reader);

        if (reader.remaining() !== 0) {
            throw new Error(
                `Extra trailing bytes after scanlist: ${reader.remaining()}`
            );
        }

        return result;
    }

    static unpack(reader) {
        const scanCount = reader.readInt32();
        const channelCount = reader.readInt32();

        if (scanCount < 0) {
            throw new Error("scanCount cannot be negative");
        }

        if (channelCount < 0) {
            throw new Error("channelCount cannot be negative");
        }

        const scans = new Array(scanCount);
        for (let i = 0; i < scanCount; i++) {
            scans[i] = DtAdcScan.unpack(reader, channelCount);
        }

        return new DtAdcScanlist(scanCount, channelCount, scans);
    }
}