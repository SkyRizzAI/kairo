<script lang="ts">
	// WASM simulator. Layout mirrors /remote: a large centered virtual device
	// (shared BoardVisual, driven by the board profile over PLP) with compact,
	// toggleable side panels — Settings (WiFi/Display/System) hidden by default so
	// it never clutters, and Logs/Events/Services on the right.
	import { onMount } from 'svelte';
	import { simStore } from '$lib/simStore.svelte';
	import { Key, Power, KEY_MAP, frameDims } from '@palanu/link';
	import { Button } from '$lib/components/ui/button';
	import { Badge } from '$lib/components/ui/badge';
	import { Input } from '$lib/components/ui/input';
	import { ScrollArea } from '$lib/components/ui/scroll-area';
	import * as Tabs from '$lib/components/ui/tabs';
	import BoardVisual from '$lib/components/BoardVisual.svelte';
	import CliTerminal from '$lib/components/CliTerminal.svelte';
	import FileBrowser from '$lib/components/FileBrowser.svelte';
	import FirmwarePanel from '$lib/components/FirmwarePanel.svelte';
	import {
		Power as PowerIcon,
		RotateCw,
		Play,
		Settings2,
		PanelRightClose,
		PanelRightOpen,
		TerminalIcon,
		FolderTree,
		Upload
	} from '@lucide/svelte';

	// ── device screen (fallback glass tint) ──
	// The DEVICE drives the screen colours now (Plan 92 Fase B): simStore.palette
	// follows Settings → Appearances → Theme. These presets are only a fallback tint
	// used until the firmware sends its palette (e.g. before boot).
	type Theme = 'eink' | 'phosphor' | 'amber';
	const THEMES: Record<Theme, { bg: [number, number, number]; fg: [number, number, number]; label: string }> = {
		eink: { bg: [240, 237, 224], fg: [20, 20, 20], label: 'E-Ink' },
		phosphor: { bg: [10, 20, 10], fg: [51, 255, 51], label: 'Phosphor' },
		amber: { bg: [15, 10, 0], fg: [255, 180, 20], label: 'Amber' }
	};
	const themeKeys = Object.keys(THEMES) as Theme[];
	let theme = $state<Theme>('phosphor');

	const off = $derived(simStore.power === 'off');
	const dims = $derived(frameDims(simStore.frame));

	let showSettings = $state(false);
	let showLogs = $state(true);
	let showCli = $state(true);   // CLI-first demo: the terminal is the primary surface
	let showFiles = $state(false);
	let showFw = $state(false);

	const AUTOBOOT = 'nema:autoboot';

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

	onMount(() => {
		simStore.init(); // wire telemetry listeners; device stays OFF until Boot
		if (sessionStorage.getItem(AUTOBOOT)) {
			sessionStorage.removeItem(AUTOBOOT);
			simStore.boot(); // came here via Restart → power back on automatically
		}
		let holdTimer: ReturnType<typeof setTimeout> | null = null;

		const onKey = (e: KeyboardEvent) => {
			const tag = (e.target as HTMLElement)?.tagName;
			if (tag === 'INPUT' || tag === 'TEXTAREA') return;
			const k = KEY_MAP[e.key];
			if (!k) return;
			e.preventDefault();
			// Select (Enter/Space): hold 1 s → Menu, tap → Select.
			if (k === Key.Select) {
				if (e.repeat) return;
				holdTimer ??= setTimeout(() => {
					holdTimer = null;
					simStore.sendKey(Key.Menu);
				}, 1000);
				return;
			}
			simStore.sendKey(k);
		};
		const onKeyUp = (e: KeyboardEvent) => {
			const tag = (e.target as HTMLElement)?.tagName;
			if (tag === 'INPUT' || tag === 'TEXTAREA') return;
			if (KEY_MAP[e.key] !== Key.Select) return;
			if (holdTimer !== null) {
				clearTimeout(holdTimer);
				holdTimer = null;
				simStore.sendKey(Key.Select);
			}
		};
		window.addEventListener('keydown', onKey);
		window.addEventListener('keyup', onKeyUp);
		return () => {
			window.removeEventListener('keydown', onKey);
			window.removeEventListener('keyup', onKeyUp);
		};
	});

	const levelColor = (l: number) =>
		l >= 4 ? 'text-red-400' : l === 3 ? 'text-yellow-400' : l <= 1 ? 'text-muted-foreground' : 'text-foreground';
</script>

<div class="flex h-full flex-col">
	<!-- Header -->
	<header class="border-border flex items-center gap-3 border-b px-4 py-2.5">
		<h1 class="text-base font-semibold">Simulator</h1>
		<Badge variant="secondary">WASM · client-side</Badge>
		<Badge variant={off ? 'destructive' : simStore.power === 'on' ? 'default' : 'secondary'}>
			{off ? 'off' : simStore.power === 'on' ? 'running' : 'booting WASM…'}
		</Badge>
		{#if simStore.profile}
			<Badge variant="outline">{simStore.profile.name}</Badge>
		{/if}
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
			<div class="bg-border mx-1 w-px self-stretch"></div>
			<Button
				size="sm"
				variant={showSettings ? 'default' : 'ghost'}
				title="Settings (WiFi, display, system)"
				onclick={() => (showSettings = !showSettings)}
			>
				<Settings2 class="size-4" />
			</Button>
			<Button
				size="sm"
				variant={showCli ? 'default' : 'ghost'}
				title="Terminal (CLI)"
				onclick={() => (showCli = !showCli)}
			>
				<TerminalIcon class="size-4" />
			</Button>
			<Button
				size="sm"
				variant={showFiles ? 'default' : 'ghost'}
				title="Files"
				onclick={() => (showFiles = !showFiles)}
			>
				<FolderTree class="size-4" />
			</Button>
				<Button
					size="sm"
					variant={showFw ? 'default' : 'ghost'}
					title="Update firmware (OTA)"
					onclick={() => (showFw = !showFw)}
				>
					<Upload class="size-4" />
				</Button>
			<Button
				size="sm"
				variant="ghost"
				title={showLogs ? 'Hide logs' : 'Show logs'}
				onclick={() => (showLogs = !showLogs)}
			>
				{#if showLogs}
					<PanelRightClose class="size-4" />
				{:else}
					<PanelRightOpen class="size-4" />
				{/if}
			</Button>
		</div>
	</header>

	<div class="flex flex-1 overflow-hidden">
		<!-- Settings panel (toggleable, hidden by default) -->
		{#if showSettings}
			<aside class="border-border bg-card/40 w-80 shrink-0 overflow-hidden border-r">
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
							<div class="space-y-3 text-xs">
								<div>
									<div class="text-muted-foreground mb-1.5">Theme</div>
									<div class="flex gap-1">
										{#each themeKeys as t (t)}
											<button
												class="flex-1 rounded border px-1.5 py-1 text-[10px] {theme === t
													? 'border-sky-400 text-sky-300'
													: 'border-border/40 text-muted-foreground'}"
												onclick={() => (theme = t)}>{THEMES[t].label}</button
											>
										{/each}
									</div>
								</div>
								<div class="text-muted-foreground">Resolution: <b class="text-foreground">{dims}</b></div>
								<p class="text-muted-foreground text-[10px] leading-relaxed">
									Firmware renders 1-bit; theme is a client-side display tint.
								</p>
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
			</aside>
		{/if}

		<!-- Device (centered, dominant) + terminal panel -->
		<div class="flex flex-1 flex-col overflow-hidden">
			<div class="flex flex-1 flex-col items-center justify-center gap-3 overflow-auto p-6">
				<BoardVisual
					profile={simStore.profile}
					frame={off ? null : simStore.frame}
					on={simStore.palette?.fg ?? THEMES[theme].fg}
					off={simStore.palette?.bg ?? THEMES[theme].bg}
					reserve={250}
					led={simStore.led}
					onkey={(k) => simStore.sendKey(k)}
				>
					{#snippet overlay()}
						{#if off}
							<div class="absolute inset-0 z-10 flex flex-col items-center justify-center gap-2 rounded-2xl bg-black/85 text-sm text-zinc-300">
								<PowerIcon class="size-6" />
								Device off — press Boot
							</div>
						{/if}
					{/snippet}
				</BoardVisual>
				{#if simStore.profile}
					<p class="text-muted-foreground text-xs">{simStore.profile.id} — layout mirrors the device</p>
				{/if}
			</div>
			{#if showCli}
				<div class="border-border h-64 shrink-0 border-t">
					<CliTerminal
						send={(sid, line) => simStore.sendCli(sid, line)}
						subscribe={(fn) => simStore.onCli(fn)}
						ready={simStore.power === 'on'}
					/>
				</div>
			{/if}
		</div>

		<!-- Files (toggleable) -->
		{#if showFiles}
			<aside class="border-border w-80 shrink-0 overflow-hidden border-l">
				<div class="border-border text-muted-foreground border-b px-3 py-1.5 text-xs font-bold">
					Files (PLP · VFS)
				</div>
				<div class="h-[calc(100%-2rem)]">
					<FileBrowser fs={simStore} ready={simStore.power === 'on'} onPappInstall={() => simStore.appScan()} />
				</div>
			</aside>
		{/if}

		{#if showFw}
			<aside class="border-border w-80 shrink-0 overflow-hidden border-l">
				<div class="border-border text-muted-foreground border-b px-3 py-1.5 text-xs font-bold">
					Firmware (PLP · OTA · sim dry-run)
				</div>
				<FirmwarePanel update={(img, p, s) => simStore.otaUpdate(img, p, s)} ready={simStore.power === 'on'} />
			</aside>
		{/if}

		<!-- Logs / Events / Services (toggleable) -->
		{#if showLogs}
			<aside class="border-border w-80 shrink-0 overflow-hidden border-l">
				<Tabs.Root value="logs" class="flex h-full flex-col">
					<Tabs.List class="w-full rounded-none">
						<Tabs.Trigger value="logs" class="flex-1">Logs ({simStore.logs.length})</Tabs.Trigger>
						<Tabs.Trigger value="events" class="flex-1">Events ({simStore.events.length})</Tabs.Trigger>
						<Tabs.Trigger value="services" class="flex-1">Services</Tabs.Trigger>
					</Tabs.List>
					<Tabs.Content value="logs" class="flex-1 overflow-hidden">
						<ScrollArea class="h-full">
							<div class="p-2 font-mono text-[10px] leading-relaxed">
								{#each simStore.logs as l, i (i)}
									<div class="flex gap-2 py-px">
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
							<div class="p-2 font-mono text-[10px] leading-relaxed">
								{#each simStore.events as e, i (i)}
									<div class="py-px">
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
							<div class="p-2 font-mono text-[10px] leading-relaxed">
								{#each Object.entries(simStore.services) as [name, state] (name)}
									<div class="flex justify-between py-px"><span>{name}</span><span class="text-muted-foreground">{state}</span></div>
								{:else}
									<p class="text-muted-foreground p-1">No services reported.</p>
								{/each}
							</div>
						</ScrollArea>
					</Tabs.Content>
				</Tabs.Root>
			</aside>
		{/if}
	</div>
</div>
