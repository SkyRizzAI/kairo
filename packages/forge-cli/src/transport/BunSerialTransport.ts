// Bun-native serial transport: bun:ffi for termios raw-mode + node:fs for I/O.
// The 'serialport' npm package depends on libuv (uv_default_loop) which Bun
// doesn't implement — this replaces it with direct POSIX syscalls via FFI.

import { openSync, readSync, writeSync, closeSync, constants as FS } from "node:fs";
import { dlopen, FFIType, ptr } from "bun:ffi";
import type { ILinkTransport } from "@palanu/link";

// macOS arm64 struct termios (44 bytes):
//   [0]  c_iflag  [4]  c_oflag  [8]  c_cflag  [12] c_lflag
//   [16] c_cc[20] [36] c_ispeed [40] c_ospeed
const TERMIOS_SIZE = 44;
const TCSANOW = 0;

// cfmakeraw flag masks (macOS/POSIX values)
const IGNBRK=0x01, BRKINT=0x02, PARMRK=0x08, ISTRIP=0x20,
      INLCR=0x40, IGNCR=0x80, ICRNL=0x100, IXON=0x200, IXOFF=0x400, IXANY=0x800;
const OPOST=0x01;
const CSIZE=0x300, CS8=0x300, PARENB=0x1000, CREAD=0x800, CLOCAL=0x8000;
const ECHO=0x08, ECHOE=0x02, ECHOK=0x04, ECHONL=0x10,
      ICANON=0x100, ISIG=0x80, IEXTEN=0x400;
const VMIN=16, VTIME=17;   // c_cc[] indices on macOS

const lib = dlopen("libc.dylib", {
    tcgetattr: { args: [FFIType.i32, FFIType.ptr], returns: FFIType.i32 },
    tcsetattr: { args: [FFIType.i32, FFIType.i32, FFIType.ptr], returns: FFIType.i32 },
});

function setRaw(fd: number): void {
    const buf = Buffer.alloc(TERMIOS_SIZE);
    lib.symbols.tcgetattr(fd, ptr(buf));
    buf.writeUInt32LE(buf.readUInt32LE(0)  & ~(IGNBRK|BRKINT|PARMRK|ISTRIP|INLCR|IGNCR|ICRNL|IXON|IXOFF|IXANY), 0);
    buf.writeUInt32LE(buf.readUInt32LE(4)  & ~OPOST, 4);
    buf.writeUInt32LE((buf.readUInt32LE(8) & ~(CSIZE|PARENB)) | CS8 | CREAD | CLOCAL, 8);
    buf.writeUInt32LE(buf.readUInt32LE(12) & ~(ECHO|ECHOE|ECHOK|ECHONL|ICANON|ISIG|IEXTEN), 12);
    buf[16 + VMIN] = 0;   // non-blocking: return immediately if no data
    buf[16 + VTIME] = 0;
    lib.symbols.tcsetattr(fd, TCSANOW, ptr(buf));
}

export class BunSerialTransport implements ILinkTransport {
    readonly kind = "serial";
    #fd = -1;
    #path: string;
    #dataCb: ((d: Uint8Array) => void) | null = null;
    #stateCb: ((c: boolean) => void) | null = null;
    #connected = false;
    #timer: ReturnType<typeof setInterval> | null = null;

    constructor(path: string) { this.#path = path; }

    boot(): Promise<void> {
        return new Promise((resolve, reject) => {
            try {
                // O_RDWR | O_NOCTTY (0x20000 on macOS) | O_NONBLOCK
                this.#fd = openSync(this.#path, FS.O_RDWR | 0x20000 | FS.O_NONBLOCK);
                setRaw(this.#fd);
                this.#connected = true;
                this.#stateCb?.(true);
                this.#poll();
                resolve();
            } catch (e) {
                reject(new Error(`cannot open ${this.#path}: ${(e as Error).message}`));
            }
        });
    }

    #poll() {
        const buf = Buffer.alloc(8192);
        this.#timer = setInterval(() => {
            if (this.#fd < 0) return;
            // Drain all available bytes per tick so the kernel RX buffer can't fill
            // while the device is streaming screen frames at ~100KB/s.
            for (;;) {
                try {
                    const n = readSync(this.#fd, buf, 0, buf.length, null);
                    if (n <= 0) break;
                    this.#dataCb?.(new Uint8Array(buf.buffer, buf.byteOffset, n));
                    if (n < buf.length) break; // likely exhausted available bytes
                } catch (e: any) {
                    if (e?.code !== "EAGAIN" && e?.code !== "EWOULDBLOCK") {
                        console.error(`serial: ${e?.message}`);
                        this.close();
                    }
                    break;
                }
            }
        }, 5);
    }

    send(data: Uint8Array): void {
        if (this.#fd < 0) return;
        let off = 0;
        while (off < data.length) {
            try {
                off += writeSync(this.#fd, data, off, data.length - off, null);
            } catch (e: any) {
                if (e?.code === "EAGAIN" || e?.code === "EWOULDBLOCK") {
                    // Kernel TX buffer full — sleep 1ms to let USB drain before retrying.
                    Bun.sleepSync(1);
                    continue;
                }
                console.error(`serial: write: ${e?.message}`);
                break;
            }
        }
    }

    onData(fn: (d: Uint8Array) => void): void  { this.#dataCb = fn; }
    onState(fn: (c: boolean) => void): void     { this.#stateCb = fn; fn(this.#connected); }
    isConnected(): boolean                       { return this.#connected && this.#fd >= 0; }

    close(): void {
        if (this.#timer) { clearInterval(this.#timer); this.#timer = null; }
        if (this.#fd >= 0) { closeSync(this.#fd); this.#fd = -1; }
        this.#connected = false;
        this.#stateCb?.(false);
    }
}
