// SimAudio — the simulator speaker's "DAC". The WASM firmware synthesizes the
// real PCM the NS4168 would receive and streams the RAW samples here (via
// Module.nemaAudioPcm, wired in VirtualCableTransport). We do NOT re-synthesize
// anything: whatever int16 samples arrive get played, exactly like a physical
// speaker fed from I2S. This stays correct for any future audio (melodies, WAV,
// PCM playback) — the firmware decides the waveform, the browser just plays it.
//
// Chunks are scheduled back-to-back on a shared timeline so a continuous stream
// plays seamlessly; one-shot beeps just play immediately. Autoplay policy: the
// AudioContext is created lazily and resume()d per chunk (the user has already
// interacted with the page by the time any sound plays).

let ctx: AudioContext | null = null;
let nextStart = 0; // shared playback cursor (seconds, ctx.currentTime domain)

function context(): AudioContext | null {
	if (typeof window === 'undefined') return null; // SSR guard
	if (!ctx) {
		const Ctor =
			window.AudioContext ||
			(window as unknown as { webkitAudioContext?: typeof AudioContext }).webkitAudioContext;
		if (!Ctor) return null;
		ctx = new Ctor();
	}
	return ctx;
}

// Play one chunk of RAW int16 mono PCM at the given sample rate. Called once per
// firmware audio write; chunks queue seamlessly.
export function playPcm(samples: Int16Array, sampleRate: number): void {
	const ac = context();
	if (!ac || samples.length === 0) return;
	try {
		if (ac.state === 'suspended') void ac.resume();

		// int16 → float32 [-1, 1).
		const f32 = new Float32Array(samples.length);
		for (let i = 0; i < samples.length; i++) f32[i] = samples[i] / 32768;

		const buf = ac.createBuffer(1, f32.length, sampleRate);
		buf.copyToChannel(f32, 0);

		const src = ac.createBufferSource();
		src.buffer = buf;
		src.connect(ac.destination);

		// Schedule contiguously: resume the stream where it left off, or start now
		// if the cursor has fallen behind (gap / first chunk).
		const now = ac.currentTime;
		const start = Math.max(now, nextStart);
		src.start(start);
		nextStart = start + buf.duration;
		src.onended = () => src.disconnect();
	} catch (e) {
		console.warn('[SimAudio] playPcm failed', e);
	}
}
