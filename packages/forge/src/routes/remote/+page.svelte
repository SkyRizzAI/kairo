<script lang="ts">
	import { RemoteSession } from '$lib/RemoteSession';
	import { wasmSession } from '$lib/wasmSim';
	import { activeRemote, setRemote, clearRemote } from '$lib/remoteLink';
	import SessionView from '$lib/components/SessionView.svelte';
	import { Button } from '$lib/components/ui/button';
	import { Cpu, Bluetooth, Usb, Unplug } from '@lucide/svelte';

	// Resume the surviving session when navigating back to this page — the cable
	// (Web Serial port / BLE link) stays open across navigation (see remoteLink).
	let session = $state<RemoteSession | null>(activeRemote()?.session ?? null);
	let label = $state(activeRemote()?.label ?? '');
	let error = $state('');
	let busy = $state('');

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
			const s = new RemoteSession(t);
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
			const s = new RemoteSession(t);
			setRemote(s, label);
			session = s;
		} catch (e) {
			error = (e as Error).message || 'USB connection cancelled';
		} finally {
			busy = '';
		}
	}

	function disconnect() {
		clearRemote(); // closes the cable for owned (BLE/USB) sessions
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
		</div>
	</div>
{/if}
