// @ts-nocheck — validated at runtime by `bun test`, not by svelte-check.
import { test, expect } from 'bun:test';
import { Channel, Flags, crc8, encodeFrame, FrameParser, rleEncode, rleDecode } from './codec';
import { loopbackPair } from './transport';

test('encode → parse roundtrip', () => {
	const payload = new Uint8Array([1, 2, 3, 4, 5]);
	const bytes = encodeFrame(Channel.Input, payload, Flags.None);
	const frames = new FrameParser().push(bytes);
	expect(frames.length).toBe(1);
	expect(frames[0].channel).toBe(Channel.Input);
	expect([...frames[0].payload]).toEqual([1, 2, 3, 4, 5]);
});

test('parser reassembles frames split across chunks', () => {
	const f = encodeFrame(Channel.Screen, new Uint8Array([9, 8, 7]), Flags.Compressed);
	const p = new FrameParser();
	// feed byte-by-byte
	let got: ReturnType<FrameParser['push']> = [];
	for (const b of f) got = got.concat(p.push(new Uint8Array([b])));
	expect(got.length).toBe(1);
	expect(got[0].flags & Flags.Compressed).toBeTruthy();
	expect([...got[0].payload]).toEqual([9, 8, 7]);
});

test('parser handles two frames in one chunk', () => {
	const a = encodeFrame(Channel.Log, new Uint8Array([1]));
	const b = encodeFrame(Channel.System, new Uint8Array([2, 2]));
	const merged = new Uint8Array([...a, ...b]);
	const frames = new FrameParser().push(merged);
	expect(frames.map((x) => x.channel)).toEqual([Channel.Log, Channel.System]);
});

test('CRC corruption is rejected then resyncs', () => {
	const good = encodeFrame(Channel.Control, new Uint8Array([5, 5, 5]));
	const bad = good.slice();
	bad[bad.length - 1] ^= 0xff; // wreck the CRC
	const next = encodeFrame(Channel.Input, new Uint8Array([7]));
	const frames = new FrameParser().push(new Uint8Array([...bad, ...next]));
	// the corrupted frame is dropped; the parser resyncs to the good one
	expect(frames.some((f) => f.channel === Channel.Input)).toBeTruthy();
});

test('crc8 is deterministic', () => {
	expect(crc8(new Uint8Array([0x00]))).toBe(0x00);
	expect(crc8(new Uint8Array([1, 2, 3, 4]))).toBe(crc8(new Uint8Array([1, 2, 3, 4])));
});

test('RLE roundtrip on a 1-bit framebuffer', () => {
	const w = 64,
		h = 8;
	const px = new Uint8Array(w * h);
	for (let i = 0; i < px.length; i++) px[i] = i % 17 < 3 ? 1 : 0; // some runs
	const enc = rleEncode(px);
	const dec = rleDecode(enc, px.length);
	expect(dec.length).toBe(px.length);
	expect([...dec]).toEqual([...px]);
	expect(enc.length).toBeLessThan(px.length); // actually compresses
});

test('virtual cable (loopback) carries KLP frames both ways', () => {
	const [a, b] = loopbackPair('wasm');
	const pa = new FrameParser();
	const pb = new FrameParser();
	const recvA: number[] = [];
	const recvB: number[] = [];
	a.onData((d) => pa.push(d).forEach((f) => recvA.push(f.channel)));
	b.onData((d) => pb.push(d).forEach((f) => recvB.push(f.channel)));

	// device (a) streams a screen frame; remote (b) sends input back
	a.send(encodeFrame(Channel.Screen, rleEncode(new Uint8Array([0, 0, 1, 1])), Flags.Compressed));
	b.send(encodeFrame(Channel.Input, new Uint8Array([2])));

	expect(recvB).toContain(Channel.Screen); // remote received the screen
	expect(recvA).toContain(Channel.Input); // device received the input
});
