<script lang="ts">
	// Rich simulator UI — same 3-column layout as before (device + buttons |
	// settings tabs | logs/events/services), but the firmware runs in WASM and all
	// data flows over KLP (simStore). Engine changed; UI unchanged.
	import { onMount } from 'svelte';
	import { simStore } from '$lib/simStore.svelte';
	import { Key, Power, type ScreenFrame } from '$lib/RemoteSession';
	import { Button } from '$lib/components/ui/button';
	import { Badge } from '$lib/components/ui/badge';
	import { Input } from '$lib/components/ui/input';
	import { ScrollArea } from '$lib/components/ui/scroll-area';
	import * as Tabs from '$lib/components/ui/tabs';
	import { Power as PowerIcon, RotateCw, Play } from '@lucide/svelte';

	// ── device screen (themes) ──
	type Theme = 'eink' | 'phosphor' | 'amber';
	const THEMES: Record<Theme, { bg: [number, number, number]; fg: [number, number, number]; label: string }> = {
		eink: { bg: [240, 237, 224], fg: [20, 20, 20], label: 'E-Ink' },
		phosphor: { bg: [10, 20, 10], fg: [51, 255, 51], label: 'Phosphor' },
		amber: { bg: [15, 10, 0], fg: [255, 180, 20], label: 'Amber' }
	};
	const themeKeys = Object.keys(THEMES) as Theme[];
	let theme = $state<Theme>('phosphor');
	let canvas: HTMLCanvasElement;
	const TARGET = 528;

	function draw(f: ScreenFrame, t: Theme) {
		if (!canvas || !f || f.px.length < f.w * f.h) return;
		const scale = Math.max(1, Math.round(TARGET / Math.max(1, f.w)));
		const off = document.createElement('canvas');
		off.width = f.w;
		off.height = f.h;
		const octx = off.getContext('2d');
		if (!octx) return;
		const img = octx.createImageData(f.w, f.h);
		const th = THEMES[t];
		for (let i = 0; i < f.w * f.h; i++) {
			const on = f.px[i] !== 0;
			const o = i * 4;
			const c = on ? th.fg : th.bg;
			img.data[o] = c[0];
			img.data[o + 1] = c[1];
			img.data[o + 2] = c[2];
			img.data[o + 3] = 255;
		}
		octx.putImageData(img, 0, 0);
		canvas.width = f.w * scale;
		canvas.height = f.h * scale;
		const ctx = canvas.getContext('2d');
		if (!ctx) return;
		ctx.imageSmoothingEnabled = false;
		ctx.drawImage(off, 0, 0, canvas.width, canvas.height);
	}

	// Device power state lives in the store. The WASM firmware is NOT loaded on
	// page open — the device starts OFF until the user presses Boot.
	const off = $derived(simStore.power === 'off');

	$effect(() => {
		if (simStore.power === 'on' && simStore.frame) draw(simStore.frame, theme);
	});

	const AUTOBOOT = 'kairo:autoboot';

	// Power controls. The WASM firmware can only load once per page lifetime, so:
	//  Boot     = load + boot the firmware in place (off → on).
	//  Restart  = reload the page, flagged to auto-boot → fresh firmware instance.
	//  Shutdown = reload to a clean OFF state (fully tears down WASM + workers).
	function boot() {
		simStore.boot();
	}
	function restart() {
		sessionStorage.setItem(AUTOBOOT, '1');
		location.reload();
	}
	function shutdown() {
		simStore.sendPower(Power.Shutdown); // best-effort signal to firmware
		sessionStorage.removeItem(AUTOBOOT);
		location.reload();
	}

	const dims = $derived(simStore.frame ? `${simStore.frame.w}×${simStore.frame.h}` : '—');

	// ── WiFi router ──
	interface SimNet {
		ssid: string;
		password: string;
		rssi: number;
		online: boolean;
	}
	let nets = $state<SimNet[]>([
		{ ssid: 'MyHomeWiFi', password: 'password123', rssi: -42, online: true },
		{ ssid: 'CoffeeShop_Free', password: '', rssi: -68, online: true },
		{ ssid: 'Neighbour_5G', password: 'secret', rssi: -74, online: false }
	]);
	const pushNets = () => simStore.wifiSetNetworks($state.snapshot(nets));
	const addNet = () => {
		nets = [...nets, { ssid: 'NewAP', password: '', rssi: -60, online: true }];
		pushNets();
	};
	const removeNet = (i: number) => {
		nets = nets.filter((_, j) => j !== i);
		pushNets();
	};

	// ── System: inject event ──
	let evtName = $state('CustomEvent');

	// ── keyboard ──
	const KEYMAP: Record<string, number> = {
		ArrowUp: Key.Up,
		ArrowDown: Key.Down,
		ArrowLeft: Key.Left,
		ArrowRight: Key.Right,
		Enter: Key.Select,
		' ': Key.Select,
		Escape: Key.Cancel,
		Backspace: Key.Cancel
	};

	onMount(() => {
		simStore.init(); // wire telemetry listeners; device stays OFF until Boot
		if (sessionStorage.getItem(AUTOBOOT)) {
			sessionStorage.removeItem(AUTOBOOT);
			simStore.boot(); // came here via Restart → power back on automatically
		}
		const onKey = (e: KeyboardEvent) => {
			const tag = (e.target as HTMLElement)?.tagName;
			if (tag === 'INPUT' || tag === 'TEXTAREA') return;
			const k = KEYMAP[e.key];
			if (k) {
				e.preventDefault();
				simStore.sendKey(k);
			}
		};
		window.addEventListener('keydown', onKey);
		return () => window.removeEventListener('keydown', onKey);
	});

	const levelColor = (l: number) =>
		l >= 4 ? 'text-red-400' : l === 3 ? 'text-yellow-400' : l <= 1 ? 'text-muted-foreground' : 'text-foreground';
	const pad = 'size-10 text-base';
</script>

<div class="flex h-full flex-col">
	<!-- Header -->
	<header class="border-border flex items-center gap-3 border-b px-4 py-2.5">
		<h1 class="text-base font-semibold">Simulator</h1>
		<Badge variant="secondary">WASM · client-side</Badge>
		<Badge variant={off ? 'destructive' : simStore.power === 'on' ? 'default' : 'secondary'}>
			{off ? 'off' : simStore.power === 'on' ? 'running' : 'booting WASM…'}
		</Badge>
		<span class="text-xs font-bold text-sky-400">{dims}</span>
		<div class="ml-auto flex gap-2">
			<Button size="sm" onclick={boot} disabled={!off}>
				<Play class="size-4" /> Boot
			</Button>
			<Button size="sm" variant="secondary" onclick={restart} disabled={off}>
				<RotateCw class="size-4" /> Restart
			</Button>
			<Button size="sm" variant="destructive" onclick={shutdown} disabled={off}>
				<PowerIcon class="size-4" /> Shutdown
			</Button>
		</div>
	</header>

	<!-- 3-column grid -->
	<div class="grid flex-1 grid-cols-[auto_330px_1fr] overflow-hidden">
		<!-- Device + buttons -->
		<div class="border-border flex flex-col items-center gap-3 overflow-auto border-r bg-black/20 p-4">
			<div class="flex items-center gap-1 self-end">
				{#each themeKeys as t (t)}
					<button
						class="rounded border px-1.5 py-0.5 text-[10px] {theme === t
							? 'border-border'
							: 'border-border/40 opacity-50'}"
						onclick={() => (theme = t)}>{THEMES[t].label}</button
					>
				{/each}
			</div>
			<div class="relative rounded-xl bg-zinc-800 p-2.5 ring-1 ring-zinc-700">
				<canvas bind:this={canvas} class="block" style="image-rendering: pixelated;"></canvas>
				{#if off}
					<div class="absolute inset-0 flex flex-col items-center justify-center gap-2 rounded-xl bg-black/90 text-sm text-zinc-400">
						<PowerIcon class="size-6" />
						Device off — press Boot
					</div>
				{/if}
			</div>
			<div class="flex items-center gap-6 pt-2">
				<div class="grid grid-cols-3 grid-rows-3 gap-1">
					<span></span>
					<Button variant="outline" class={pad} onclick={() => simStore.sendKey(Key.Up)}>▲</Button>
					<span></span>
					<Button variant="outline" class={pad} onclick={() => simStore.sendKey(Key.Left)}>◄</Button>
					<span></span>
					<Button variant="outline" class={pad} onclick={() => simStore.sendKey(Key.Right)}>►</Button>
					<span></span>
					<Button variant="outline" class={pad} onclick={() => simStore.sendKey(Key.Down)}>▼</Button>
					<span></span>
				</div>
				<div class="flex flex-col gap-2">
					<Button class={pad} onclick={() => simStore.sendKey(Key.Select)}>OK</Button>
					<Button variant="destructive" class={pad} onclick={() => simStore.sendKey(Key.Cancel)}>✕</Button>
				</div>
			</div>
		</div>

		<!-- Settings tabs -->
		<div class="border-border overflow-hidden border-r">
			<Tabs.Root value="wifi" class="flex h-full flex-col">
				<Tabs.List class="w-full rounded-none">
					<Tabs.Trigger value="wifi" class="flex-1">WiFi</Tabs.Trigger>
					<Tabs.Trigger value="display" class="flex-1">Display</Tabs.Trigger>
					<Tabs.Trigger value="system" class="flex-1">System</Tabs.Trigger>
				</Tabs.List>
				<div class="flex-1 overflow-auto p-3">
					<Tabs.Content value="wifi">
						<div class="text-muted-foreground mb-2 text-[10px] font-bold tracking-wider uppercase">
							Nearby Networks (virtual router)
						</div>
						<div class="flex flex-col gap-2">
							{#each nets as n, i (i)}
								<div class="border-border flex flex-col gap-1.5 rounded-md border p-2">
									<div class="flex gap-1.5">
										<Input class="h-7 flex-1 text-xs" bind:value={n.ssid} onchange={pushNets} />
										<Button variant="ghost" size="icon" class="size-7 text-red-400" onclick={() => removeNet(i)}>×</Button>
									</div>
									<Input class="h-7 text-xs" bind:value={n.password} placeholder="password" onchange={pushNets} />
									<div class="flex items-center gap-2">
										<input type="range" min={-90} max={-30} bind:value={n.rssi} onchange={pushNets} class="flex-1 accent-sky-400" />
										<span class="text-muted-foreground w-12 text-right text-[10px]">{n.rssi}dBm</span>
										<label class="flex items-center gap-1 text-[10px] text-lime-400">
											<input type="checkbox" bind:checked={n.online} onchange={pushNets} />
											{n.online ? 'on' : 'off'}
										</label>
									</div>
								</div>
							{/each}
							<Button variant="outline" size="sm" onclick={addNet}>+ Add network</Button>
							<p class="text-muted-foreground text-[10px] leading-relaxed">
								Device: Settings → WiFi → Scan → pick → type password.
							</p>
						</div>
					</Tabs.Content>

					<Tabs.Content value="display">
						<div class="text-muted-foreground mb-2 text-[10px] font-bold tracking-wider uppercase">Display</div>
						<div class="text-muted-foreground space-y-2 text-xs">
							<div>Resolution: <b class="text-foreground">{dims}</b></div>
							<div>Theme: <b class="text-foreground">{THEMES[theme].label}</b> (pick top-left of screen)</div>
							<p class="text-[10px] leading-relaxed">Firmware renders 1-bit; theme is a client-side display tint.</p>
						</div>
					</Tabs.Content>

					<Tabs.Content value="system">
						<div class="flex flex-col gap-2">
							<div class="text-muted-foreground text-[10px] font-bold tracking-wider uppercase">Runtime</div>
							<div class="flex gap-1.5">
								<Button size="sm" variant="secondary" disabled={off} onclick={restart}>Restart</Button>
								<Button size="sm" variant="destructive" disabled={off} onclick={shutdown}>Shutdown</Button>
							</div>
							<div class="text-muted-foreground text-[10px] font-bold tracking-wider uppercase">Inject Event</div>
							<div class="flex items-center gap-1.5">
								<Input class="h-7 flex-1 text-xs" bind:value={evtName} />
								<Button size="sm" disabled={!evtName} onclick={() => simStore.injectEvent(evtName)}>Inject</Button>
							</div>
							<p class="text-muted-foreground text-[10px] leading-relaxed">
								Engine: WASM (client-side). No native binary, no server.
							</p>
						</div>
					</Tabs.Content>
				</div>
			</Tabs.Root>
		</div>

		<!-- Logs / Events / Services -->
		<div class="overflow-hidden">
			<Tabs.Root value="logs" class="flex h-full flex-col">
				<Tabs.List class="w-full rounded-none">
					<Tabs.Trigger value="logs" class="flex-1">Logs ({simStore.logs.length})</Tabs.Trigger>
					<Tabs.Trigger value="events" class="flex-1">Events ({simStore.events.length})</Tabs.Trigger>
					<Tabs.Trigger value="services" class="flex-1">Services</Tabs.Trigger>
				</Tabs.List>
				<Tabs.Content value="logs" class="flex-1 overflow-hidden">
					<ScrollArea class="h-full">
						<div class="p-2 font-mono text-[11px]">
							{#each simStore.logs as l, i (i)}
								<div class="flex gap-2 py-0.5">
									<span class="w-20 shrink-0 truncate {levelColor(l.level)}">{l.component}</span>
									<span class="break-all">{l.message}</span>
								</div>
							{:else}
								<p class="text-muted-foreground p-1">No logs yet.</p>
							{/each}
						</div>
					</ScrollArea>
				</Tabs.Content>
				<Tabs.Content value="events" class="flex-1 overflow-hidden">
					<ScrollArea class="h-full">
						<div class="p-2 font-mono text-[11px]">
							{#each simStore.events as e, i (i)}
								<div class="py-0.5">
									<span class="text-sky-400">{e.name}</span>
									{#if Object.keys(e.fields).length}<span class="text-muted-foreground/70"> {JSON.stringify(e.fields)}</span>{/if}
								</div>
							{:else}
								<p class="text-muted-foreground p-1">No events yet.</p>
							{/each}
						</div>
					</ScrollArea>
				</Tabs.Content>
				<Tabs.Content value="services" class="flex-1 overflow-hidden">
					<ScrollArea class="h-full">
						<div class="p-2 font-mono text-[11px]">
							{#each Object.entries(simStore.services) as [name, state] (name)}
								<div class="flex justify-between py-0.5"><span>{name}</span><span class="text-muted-foreground">{state}</span></div>
							{:else}
								<p class="text-muted-foreground p-1">No services reported.</p>
							{/each}
						</div>
					</ScrollArea>
				</Tabs.Content>
			</Tabs.Root>
		</div>
	</div>
</div>
