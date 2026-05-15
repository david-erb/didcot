export class ScanlistParser {
    constructor(options = {}) {
        this.littleEndian = options.littleEndian ?? true;
    }

    unpackScanlistFromArrayBuffer(arrayBuffer) {
        const reader = new BufferReader(arrayBuffer, this.littleEndian);

        const scanCount = reader.readInt32();
        const channelCount = reader.readInt32();

        if (scanCount < 0) {
            throw new Error(`scanCount cannot be negative: ${scanCount}`);
        }

        if (channelCount < 0) {
            throw new Error(`channelCount cannot be negative: ${channelCount}`);
        }

        const scans = new Array(scanCount);

        for (let i = 0; i < scanCount; i++) {
            scans[i] = this.unpackScan(reader, channelCount);
        }

        if (!reader.isAtEnd()) {
            console.warn(
                `ScanlistParser: ${reader.remainingBytes()} trailing byte(s) remained after decode`
            );
        }

        return {
            messageByteLength: arrayBuffer.byteLength,
            scanCount,
            channelCount,
            scans
        };
    }

    unpackScan(reader, channelCount) {
        const timestampNs = reader.readBigInt64();
        const sequenceNumber = reader.readBigInt64();

        const values = new Array(channelCount);
        for (let i = 0; i < channelCount; i++) {
            values[i] = reader.readInt32();
        }

        return {
            timestampNs,
            sequenceNumber,
            values
        };
    }
}

class BufferReader {
    constructor(arrayBuffer, littleEndian) {
        this.view = new DataView(arrayBuffer);
        this.offset = 0;
        this.littleEndian = littleEndian;
    }

    ensureAvailable(byteCount) {
        if (this.offset + byteCount > this.view.byteLength) {
            throw new Error(
                `Buffer underrun: need ${byteCount} byte(s) at offset ${this.offset}, length=${this.view.byteLength}`
            );
        }
    }

    readInt32() {
        this.ensureAvailable(4);
        const value = this.view.getInt32(this.offset, this.littleEndian);
        this.offset += 4;
        return value;
    }

    readBigInt64() {
        this.ensureAvailable(8);
        const value = this.view.getBigInt64(this.offset, this.littleEndian);
        this.offset += 8;
        return value;
    }

    remainingBytes() {
        return this.view.byteLength - this.offset;
    }

    isAtEnd() {
        return this.offset === this.view.byteLength;
    }
}