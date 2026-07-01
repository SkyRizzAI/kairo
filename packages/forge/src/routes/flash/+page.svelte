<script lang="ts">
	// Web Serial firmware flasher (Plan 36). Flashes ESP32-S3 bundles published by
	// firmware/tools/publish-firmware.sh entirely client-side via esptool-js — the
	// only server touch is the firmware registry (tRPC firmware.list) + static .bin
	// fetch. No backend flashing, no native esptool.
	import { onMount } from 'svelte';
	import {
		ESPLoader,
		Transport,
		type FlashModeValues,
		type FlashFreqValues,
		type FlashSizeValues
	} from 'esptool-js';
	import { trpc } from '$lib/trpc/client';
	import type { FirmwareBuild } from '$lib/trpc/router';
	import { Button } from '$lib/components/ui/button';
	import { Badge } from '$lib/components/ui/badge';
	import { ScrollArea } from '$lib/components/ui/scroll-area';
	import { Usb, Cpu, Zap, Terminal } from '@lucide/svelte';

	type Phase = 'idle' | 'connecting' | 'flashing' | 'done' | 'error' | 'console';

	let builds = $state<FirmwareBuild[]>([]);
	let selected = $state<FirmwareBuild | null>(null);
	let phase = $state<Phase>('idle');
	let progress = $state(0);
	let errorMsg = $state('');
	let lines = $state<string[]>([]);
	let logEl: HTMLDivElement | undefined;

	// eslint-disable-next-line @typescript-eslint/no-explicit-any
	const serial = () => (navigator as any).serial;
	const supported = typeof navigator !== 'undefined' && !!serial();

	function log(s: string) {
		lines = [...lines.slice(-500), s];
		queueMicrotask(() => logEl?.scrollTo(0, logEl.scrollHeight));
	}

	// esptool-js terminal sink.
	const terminal = {
		clean: () => (lines = []),
		writeLine: (d: string) => log(d),
		write: (d: string) => {
			// esptool writes progress without newlines; coalesce onto the last line.
			if (lines.length === 0) lines = [d];
			else lines = [...lines.slice(0, -1), lines[lines.length - 1] + d];
		}
	};

	onMount(async () => {
		try {
			builds = await trpc.firmware.list.query();
			selected = builds[0] ?? null;
		} catch (e) {
			errorMsg = 'Could not load firmware registry: ' + (e as Error).message;
		}
	});

	async function fetchPart(path: string): Promise<Uint8Array> {
		const res = await fetch(path);
		if (!res.ok) throw new Error(`fetch ${path} → ${res.status}`);
		return new Uint8Array(await res.arrayBuffer());
	}

	let port: unknown = null;
	let transport: Transport | null = null;

	async function flash() {
		if (!selected || !supported) return;
		errorMsg = '';
		progress = 0;
		lines = [];
		phase = 'connecting';
		try {
			port = await serial().requestPort();
			transport = new Transport(port as never, true);
			const esploader = new ESPLoader({ transport, baudrate: 921600, terminal });
			const chip = await esploader.main();
			log(`Connected: ${chip}`);

			log(`Loading ${selected.parts.length} parts…`);
			const fileArray = await Promise.all(
				selected.parts.map(async (p) => ({
					data: await fetchPart(p.path),
					address: parseInt(p.offset, 16)
				}))
			);

			phase = 'flashing';
			await esploader.writeFlash({
				fileArray,
				flashSize: selected.flash.size as FlashSizeValues,
				flashMode: selected.flash.mode as FlashModeValues,
				flashFreq: selected.flash.freq as FlashFreqValues,
				eraseAll: false,
				compress: true,
				reportProgress: (idx: number, written: number, total: number) => {
					const base = (idx / fileArray.length) * 100;
					progress = Math.min(100, Math.round(base + (written / total) * (100 / fileArray.length)));
				}
			});
			progress = 100;
			log('Flash complete. Resetting…');
			try {
				await esploader.after();
			} catch {
				/* some chips reset via DTR/RTS only */
			}
			phase = 'done';
		} catch (e) {
			errorMsg = (e as Error).message || 'Flash failed';
			phase = 'error';
		} finally {
			try {
				await transport?.disconnect();
			} catch {
				/* ignore */
			}
		}
	}

	// Open a plain serial console on a port (after flashing or standalone).
	let consoleAbort: AbortController | null = null;
	async function openConsole() {
		if (!supported) return;
		errorMsg = '';
		lines = [];
		phase = 'console';
		try {
			// eslint-disable-next-line @typescript-eslint/no-explicit-any
			const sp = (port as any) ?? (await serial().requestPort());
			port = sp;
			await sp.open({ baudRate: 115200 });
			consoleAbort = new AbortController();
			const dec = new TextDecoder();
			const reader = sp.readable.getReader();
			let buf = '';
			while (!consoleAbort.signal.aborted) {
				const { value, done } = await reader.read();
				if (done) break;
				buf += dec.decode(value, { stream: true });
				const parts = buf.split('\n');
				buf = parts.pop() ?? '';
				for (const ln of parts) log(ln);
			}
			reader.releaseLock();
			await sp.close();
		} catch (e) {
			errorMsg = (e as Error).message || 'Console failed';
			phase = 'error';
		}
	}
	function stopConsole() {
		consoleAbort?.abort();
		phase = 'idle';
	}

	const busy = $derived(phase === 'connecting' || phase === 'flashing');
</script>

<div class="mx-auto flex h-full max-w-3xl flex-col gap-4 p-6">
	<div class="flex items-center gap-3">
		<div class="bg-accent flex size-10 items-center justify-center rounded-md">
			<Usb class="size-5" />
		</div>
		<div>
			<h1 class="text-lg font-semibold">Flash</h1>
			<p class="text-muted-foreground text-sm">
				Flash a Palanu device over USB (Web Serial · esptool-js). Fully client-side.
			</p>
		</div>
		{#if phase === 'done'}<Badge class="ml-auto">flashed</Badge>{/if}
	</div>

	{#if !supported}
		<div class="rounded-md border border-yellow-500/40 bg-yellow-500/10 px-3 py-2 text-sm text-yellow-400">
			Web Serial is not available in this browser. Use Chrome, Edge, or Opera on desktop.
		</div>
	{/if}

	{#if errorMsg}
		<div class="rounded-md border border-red-500/40 bg-red-500/10 px-3 py-2 text-sm text-red-400">
			{errorMsg}
		</div>
	{/if}

	<!-- Firmware picker -->
	<div class="flex flex-col gap-2">
		<div class="text-muted-foreground text-[10px] font-bold tracking-wider uppercase">Firmware</div>
		{#if builds.length === 0}
			<p class="text-muted-foreground text-sm">
				No firmware published. Run
				<code class="bg-muted rounded px-1">firmware/tools/publish-firmware.sh</code>
				after an <code class="bg-muted rounded px-1">idf.py build</code>.
			</p>
		{:else}
			<div class="grid gap-2">
				{#each builds as b (b.id)}
					<button
						class="border-border flex items-center gap-3 rounded-lg border p-3 text-left {selected?.id === b.id
							? 'ring-2 ring-sky-500'
							: ''}"
						onclick={() => (selected = b)}
					>
						<Cpu class="size-5 shrink-0" />
						<div class="min-w-0 flex-1">
							<div class="font-medium">{b.board}</div>
							<div class="text-muted-foreground text-xs">
								{b.chip} · {(b.appSize / 1024 / 1024).toFixed(2)} MB · {b.version}
							</div>
						</div>
						<span class="text-muted-foreground font-mono text-[10px]">{b.parts.length} parts</span>
					</button>
				{/each}
			</div>
		{/if}
	</div>

	<!-- Actions -->
	<div class="flex gap-2">
		<Button disabled={!supported || !selected || busy} onclick={flash}>
			<Zap class="size-4" />
			{phase === 'connecting' ? 'Connecting…' : phase === 'flashing' ? 'Flashing…' : 'Flash device'}
		</Button>
		{#if phase === 'console'}
			<Button variant="secondary" onclick={stopConsole}>Stop console</Button>
		{:else}
			<Button variant="secondary" disabled={!supported || busy} onclick={openConsole}>
				<Terminal class="size-4" /> Serial console
			</Button>
		{/if}
	</div>

	{#if busy || phase === 'done'}
		<div class="bg-muted h-2 w-full overflow-hidden rounded-full">
			<div class="h-full bg-sky-500 transition-all" style="width: {progress}%"></div>
		</div>
	{/if}

	<!-- Log / console -->
	<div class="border-border min-h-0 flex-1 overflow-hidden rounded-lg border">
		<ScrollArea class="h-full">
			<div bind:this={logEl} class="p-2 font-mono text-[11px] leading-relaxed">
				{#each lines as l, i (i)}
					<div class="break-all whitespace-pre-wrap">{l}</div>
				{:else}
					<p class="text-muted-foreground p-1">
						Connect a device, pick firmware, and press Flash. esptool output appears here.
					</p>
				{/each}
			</div>
		</ScrollArea>
	</div>

	<p class="text-muted-foreground text-[10px] leading-relaxed">
		Tip: hold BOOT then tap RESET to enter download mode if auto-reset fails. Flashing writes
		every part at the offsets from the firmware manifest (bootloader 0x0, partition table
		0x8000, app 0x20000, …).
	</p>
</div>
