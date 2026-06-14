<script lang="ts">
	import { onMount } from 'svelte';
	import {
		RemoteSession,
		Power,
		KEY_MAP,
		frameDims,
		type BoardProfile,
		type ScreenFrame
	} from '$lib/RemoteSession';
	import { Button } from '$lib/components/ui/button';
	import { Badge } from '$lib/components/ui/badge';
	import { ScrollArea } from '$lib/components/ui/scroll-area';
	import BoardVisual from '$lib/components/BoardVisual.svelte';
	import CliTerminal from '$lib/components/CliTerminal.svelte';
	import FileBrowser from '$lib/components/FileBrowser.svelte';
	import {
		Power as PowerIcon,
		RotateCw,
		PanelRightClose,
		PanelRightOpen,
		TerminalIcon,
		FolderTree,
		Upload
	} from '@lucide/svelte';

	let {
		session,
		label,
		title = 'Remote'
	}: { session: RemoteSession; label: string; title?: string } = $props();

	let connected = $state(false);
	let logs = $state<{ c: string; m: string }[]>([]);
	let frame = $state<ScreenFrame | null>(null);
	let profile = $state<BoardProfile | null>(null);
	let showLogs = $state(true);
	let showCli = $state(false);
	let showFiles = $state(false);
	let showFw = $state(false);

	// Firmware OTA (Plan 39) — push a .bin over the PLP Ota channel.
	let fwInput = $state<HTMLInputElement>();
	let fwName = $state('');
	let fwSize = $state(0);
	let fwBusy = $state(false);
	let fwPct = $state(0);
	let fwMsg = $state('');
	let fwData: Uint8Array | null = null;

	async function onFwPick(e: Event) {
		const file = (e.target as HTMLInputElement).files?.[0];
		if (!file) return;
		fwData = new Uint8Array(await file.arrayBuffer());
		fwName = file.name;
		fwSize = fwData.length;
		fwMsg = '';
	}
	async function pushFirmware() {
		if (!fwData || fwBusy) return;
		fwBusy = true;
		fwPct = 0;
		fwMsg = 'uploading…';
		const ok = await session.otaUpdate(fwData, (s, t) => (fwPct = Math.round((s / t) * 100)));
		fwBusy = false;
		fwMsg = ok
			? 'done — device rebooting into the new image.'
			: 'failed (unsupported, transport error, or bad image).';
	}

	const dims = $derived(frameDims(frame));

	onMount(() => {
		connected = session.ready;
		profile = session.profile;
		const offReady = session.on('ready', () => (connected = true));
		const offProfile = session.on('profile', (p) => (profile = p));
		const offScreen = session.on('screen', (f) => (frame = f));
		const offLog = session.on('log', (l) => (logs = [...logs.slice(-200), { c: l.component, m: l.message }]));
		const onKey = (e: KeyboardEvent) => {
			const t = (e.target as HTMLElement)?.tagName;
			if (t === 'INPUT' || t === 'TEXTAREA') return;
			const k = KEY_MAP[e.key];
			if (k) {
				e.preventDefault();
				session.sendKey(k);
			}
		};
		window.addEventListener('keydown', onKey);
		return () => {
			window.removeEventListener('keydown', onKey);
			offReady();
			offProfile();
			offScreen();
			offLog();
		};
	});
</script>

<div class="flex h-full flex-col">
	<header class="border-border flex items-center gap-3 border-b px-4 py-2.5">
		<h1 class="text-base font-semibold">{title}</h1>
		<Badge variant="secondary">{label}</Badge>
		<Badge variant={connected ? 'default' : 'secondary'}>
			{connected ? 'connected' : 'connecting…'}
		</Badge>
		{#if profile}
			<Badge variant="outline">{profile.name}</Badge>
		{/if}
		<span class="text-xs font-bold text-sky-400">{dims}</span>
		<div class="ml-auto flex gap-2">
			<Button size="sm" variant="secondary" onclick={() => session.power(Power.Restart)}>
				<RotateCw class="size-4" /> Restart
			</Button>
			<Button size="sm" variant="destructive" onclick={() => session.power(Power.Shutdown)}>
				<PowerIcon class="size-4" /> Shutdown
			</Button>
			<div class="bg-border mx-1 w-px self-stretch"></div>
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
		<div class="flex flex-1 flex-col overflow-hidden">
			<div class="flex flex-1 flex-col items-center justify-center gap-3 overflow-auto p-6">
				<BoardVisual {profile} {frame} onkey={(k) => session.sendKey(k)} />
				{#if profile}
					<p class="text-muted-foreground text-xs">{profile.id} — layout mirrors the device</p>
				{/if}
			</div>
			{#if showCli}
				<div class="border-border h-64 shrink-0 border-t">
					<CliTerminal
						send={(sid, line) => session.sendCli(sid, line)}
						subscribe={(fn) => session.on('cli', fn)}
						ready={connected}
					/>
				</div>
			{/if}
		</div>

		{#if showFiles}
			<aside class="border-border flex w-80 shrink-0 flex-col overflow-hidden border-l">
				<div class="border-border text-muted-foreground border-b px-3 py-1.5 text-xs font-bold">
					Files (PLP · VFS)
				</div>
				<div class="min-h-0 flex-1">
					<FileBrowser fs={session} ready={connected} />
				</div>
			</aside>
		{/if}

			{#if showFw}
				<aside class="border-border flex w-80 shrink-0 flex-col overflow-hidden border-l">
					<div class="border-border text-muted-foreground border-b px-3 py-1.5 text-xs font-bold">
						Firmware (PLP · OTA)
					</div>
					<div class="flex flex-1 flex-col gap-3 p-3 text-xs">
						<input bind:this={fwInput} type="file" accept=".bin" class="hidden" onchange={onFwPick} />
						<Button size="sm" variant="secondary" onclick={() => fwInput?.click()} disabled={fwBusy}>
							<Upload class="size-4" /> Choose .bin
						</Button>
						{#if fwName}
							<div class="text-muted-foreground break-all">{fwName} · {(fwSize / 1024).toFixed(0)} KB</div>
							<Button size="sm" onclick={pushFirmware} disabled={fwBusy || !connected}>
								{fwBusy ? `Updating… ${fwPct}%` : 'Push update'}
							</Button>
							{#if fwBusy}
								<div class="bg-border h-1.5 w-full overflow-hidden rounded">
									<div class="h-full bg-sky-400" style="width:{fwPct}%"></div>
								</div>
							{/if}
						{/if}
						{#if fwMsg}<div class="text-muted-foreground break-words">{fwMsg}</div>{/if}
						<div class="text-muted-foreground mt-auto text-[10px] leading-relaxed">
							Streams the image over PLP (USB/BLE) to the inactive A/B slot, then the
							device reboots into it. Rollback is automatic if the new image fails to boot.
						</div>
					</div>
				</aside>
			{/if}

		{#if showLogs}
			<aside class="border-border flex w-80 shrink-0 flex-col overflow-hidden border-l">
				<div class="border-border text-muted-foreground border-b px-3 py-1.5 text-xs font-bold">
					Device logs (PLP)
				</div>
				<ScrollArea class="flex-1">
					<div class="p-2 font-mono text-[10px] leading-relaxed">
						{#each logs as l, i (i)}
							<div class="py-px break-words">
								<span class="text-muted-foreground">{l.c}</span>
								{l.m}
							</div>
						{:else}
							<p class="text-muted-foreground p-1">Waiting for device…</p>
						{/each}
					</div>
				</ScrollArea>
			</aside>
		{/if}
	</div>
</div>
