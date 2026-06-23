<script lang="ts">
	import { RemoteSession } from '@palanu/link';
	import { wasmSession } from '$lib/wasmSim';
	import { activeRemote, setRemote, clearRemote } from '$lib/remoteLink';
	import SessionView from '$lib/components/SessionView.svelte';
	import { Button } from '$lib/components/ui/button';
	import { Cpu, Bluetooth, Usb, Wifi, Unplug } from '@lucide/svelte';

	// Resume the surviving session when navigating back to this page — the cable
	// (Web Serial port / BLE link) stays open across navigation (see remoteLink).
	let session = $state<RemoteSession | null>(activeRemote()?.session ?? null);
	let label = $state(activeRemote()?.label ?? '');
	let error = $state('');
	let busy = $state('');
	let netHost = $state('skyrizz-e32.local');

	function pickSimulator() {
		error = '';
		label = 'Simulator (WASM)';
		const s = wasmSession();
		void s.boot(); // power on the live sim if it isn't already running
		setRemote(s, label, false); // shared singleton — never closed from here
		session = s;
	}

	async function pickBle() {
		error = '';
		busy = 'ble';
		try {
			const { BleTransport } = await import('$lib/transport/BleTransport');
			if (!BleTransport.available()) throw new Error('Web Bluetooth not supported (use Chrome/Edge)');
			const t = new BleTransport();
			await t.connect(); // opens the browser device chooser
			label = 'Bluetooth (BLE)';
			// Per-transport token key so one device's token can't clobber another's (F5).
			const s = new RemoteSession(t, { tokenKey: 'palanu.remote.token.ble' });
			setRemote(s, label);
			session = s;
		} catch (e) {
			error = (e as Error).message || 'Bluetooth connection cancelled';
		} finally {
			busy = '';
		}
	}

	async function pickUsb() {
		error = '';
		busy = 'usb';
		try {
			const { SerialTransport } = await import('$lib/transport/SerialTransport');
			if (!SerialTransport.available()) throw new Error('Web Serial not supported (use Chrome/Edge)');
			const t = new SerialTransport();
			await t.connect();
			label = 'USB (Serial)';
			const s = new RemoteSession(t, { tokenKey: 'palanu.remote.token.usb' });
			setRemote(s, label);
			session = s;
		} catch (e) {
			error = (e as Error).message || 'USB connection cancelled';
		} finally {
			busy = '';
		}
	}

	async function pickNet() {
		error = '';
		busy = 'net';
		try {
			const { WebSocketTransport } = await import('$lib/transport/WebSocketTransport');
			if (!WebSocketTransport.available()) throw new Error('WebSocket not supported in this browser');
			if (!netHost.trim()) throw new Error('Enter the device host (e.g. skyrizz-e32.local)');
			const t = new WebSocketTransport(netHost);
			// Construct the session first so onData/onState are wired before the socket
			// opens; onState(true) then fires immediately and drives the PLP handshake.
			const s = new RemoteSession(t, { tokenKey: `palanu.remote.token.net.${netHost.trim()}` });
			await t.boot();
			label = `Network (${netHost})`;
			setRemote(s, label);
			session = s;
		} catch (e) {
			error = (e as Error).message || 'Network connection failed';
		} finally {
			busy = '';
		}
	}

	function disconnect() {
		clearRemote(); // closes the cable for owned (BLE/USB/Network) sessions
		session = null;
	}

	const items = [
		{
			key: 'sim',
			icon: Cpu,
			title: 'Simulator — virtual USB (WASM)',
			desc: 'Connect to the live in-browser simulator over its virtual USB interface. No hardware, no server.',
			cta: 'Connect',
			ready: true,
			on: pickSimulator
		},
		{
			key: 'ble',
			icon: Bluetooth,
			title: 'Bluetooth (BLE)',
			desc: 'A physical Palanu device over Web Bluetooth. Requires a flashed device.',
			cta: 'Scan…',
			ready: false,
			on: pickBle
		},
		{
			key: 'usb',
			icon: Usb,
			title: 'USB (Serial)',
			desc: 'A physical Palanu device over Web Serial / USB-CDC. Requires a flashed device.',
			cta: 'Connect…',
			ready: false,
			on: pickUsb
		}
	];
</script>

{#if session}
	<div class="flex h-full flex-col">
		<div class="border-border flex items-center gap-2 border-b px-2 py-1.5">
			<Button variant="ghost" size="sm" onclick={disconnect}>
				<Unplug class="size-4" /> Disconnect
			</Button>
		</div>
		<div class="flex-1 overflow-hidden">
			<SessionView {session} {label} />
		</div>
	</div>
{:else}
	<div class="mx-auto flex h-full max-w-3xl flex-col gap-4 p-8">
		<div>
			<h1 class="text-lg font-semibold">Remote — Discovery</h1>
			<p class="text-muted-foreground text-sm">
				Pick a target. Same Palanu Link Protocol, different transport.
			</p>
		</div>

		{#if error}
			<div class="rounded-md border border-red-500/40 bg-red-500/10 px-3 py-2 text-sm text-red-400">
				{error}
			</div>
		{/if}

		<div class="grid gap-3">
			{#each items as it (it.key)}
				{@const Icon = it.icon}
				<div class="border-border flex items-center gap-4 rounded-lg border p-4">
					<div class="bg-accent flex size-10 shrink-0 items-center justify-center rounded-md">
						<Icon class="size-5" />
					</div>
					<div class="min-w-0 flex-1">
						<div class="flex items-center gap-2">
							<span class="font-medium">{it.title}</span>
							{#if it.ready}
								<span class="rounded bg-lime-500/15 px-1.5 py-0.5 text-[10px] text-lime-400">ready</span>
							{:else}
								<span class="bg-muted text-muted-foreground rounded px-1.5 py-0.5 text-[10px]">
									needs device
								</span>
							{/if}
						</div>
						<p class="text-muted-foreground text-xs">{it.desc}</p>
					</div>
					<Button size="sm" disabled={busy === it.key} onclick={it.on}>
						{busy === it.key ? '…' : it.cta}
					</Button>
				</div>
			{/each}

			<!-- Network (WiFi) — PLP over WebSocket (Plan 75). Needs a flashed, online device. -->
			<div class="border-border flex items-center gap-4 rounded-lg border p-4">
				<div class="bg-accent flex size-10 shrink-0 items-center justify-center rounded-md">
					<Wifi class="size-5" />
				</div>
				<div class="min-w-0 flex-1">
					<div class="flex items-center gap-2">
						<span class="font-medium">Network (Wi-Fi)</span>
						<span class="bg-muted text-muted-foreground rounded px-1.5 py-0.5 text-[10px]">
							needs device
						</span>
					</div>
					<p class="text-muted-foreground text-xs">
						A physical Palanu device over WiFi (WebSocket). Enter its hostname or IP.
					</p>
					<input
						class="border-border bg-background mt-2 w-full rounded-md border px-2 py-1 text-xs"
						placeholder="skyrizz-e32.local or 192.168.1.23"
						bind:value={netHost}
						onkeydown={(e) => e.key === 'Enter' && pickNet()}
					/>
				</div>
				<Button size="sm" disabled={busy === 'net'} onclick={pickNet}>
					{busy === 'net' ? '…' : 'Connect…'}
				</Button>
			</div>
		</div>
	</div>
{/if}
