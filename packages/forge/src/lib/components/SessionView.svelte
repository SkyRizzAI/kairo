<script lang="ts">
	import { onMount, tick } from 'svelte';
	import {
		RemoteSession,
		Key,
		Power,
		type BoardProfile,
		type ScreenFrame
	} from '$lib/RemoteSession';
	import { Button } from '$lib/components/ui/button';
	import { Badge } from '$lib/components/ui/badge';
	import { ScrollArea } from '$lib/components/ui/scroll-area';
	import { Power as PowerIcon, RotateCw, PanelRightClose, PanelRightOpen } from '@lucide/svelte';

	let {
		session,
		label,
		title = 'Remote'
	}: { session: RemoteSession; label: string; title?: string } = $props();

	let canvas = $state<HTMLCanvasElement>();
	let connected = $state(false);
	let logs = $state<{ c: string; m: string }[]>([]);
	let dims = $state('—');
	let profile = $state<BoardProfile | null>(null);
	let showLogs = $state(true);
	let last: ScreenFrame | null = null;

	const INK: [number, number, number] = [230, 255, 230];
	const BG: [number, number, number] = [11, 14, 11];
	const SCALE = 2;

	function draw(f: ScreenFrame) {
		last = f;
		dims = `${f.w}×${f.h}`;
		if (!canvas || f.px.length < f.w * f.h) return;
		const off = document.createElement('canvas');
		off.width = f.w;
		off.height = f.h;
		const octx = off.getContext('2d');
		if (!octx) return;
		const img = octx.createImageData(f.w, f.h);
		for (let i = 0; i < f.w * f.h; i++) {
			const on = f.px[i] !== 0;
			const o = i * 4;
			img.data[o] = on ? INK[0] : BG[0];
			img.data[o + 1] = on ? INK[1] : BG[1];
			img.data[o + 2] = on ? INK[2] : BG[2];
			img.data[o + 3] = 255;
		}
		octx.putImageData(img, 0, 0);
		canvas.width = f.w * SCALE;
		canvas.height = f.h * SCALE;
		const ctx = canvas.getContext('2d');
		if (!ctx) return;
		ctx.imageSmoothingEnabled = false;
		ctx.drawImage(off, 0, 0, canvas.width, canvas.height);
	}

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
		connected = session.ready;
		profile = session.profile;
		const offReady = session.on('ready', () => (connected = true));
		const offProfile = session.on('profile', (p) => {
			profile = p;
			// The canvas re-mounts inside the board bezel — repaint the last frame.
			tick().then(() => last && draw(last));
		});
		const offScreen = session.on('screen', (f) => draw(f));
		const offLog = session.on('log', (l) => (logs = [...logs.slice(-200), { c: l.component, m: l.message }]));
		const onKey = (e: KeyboardEvent) => {
			const t = (e.target as HTMLElement)?.tagName;
			if (t === 'INPUT' || t === 'TEXTAREA') return;
			const k = KEYMAP[e.key];
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

	const pad = 'size-11 text-base';
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
		<div class="flex flex-1 flex-col items-center justify-center gap-3 overflow-auto p-6">
			{#if profile}
				<!-- Virtual board: bezel + components at the device's real layout.
				     Sized to dominate the page: as wide as fits, capped by viewport height. -->
				<div
					class="relative rounded-2xl bg-zinc-800 shadow-xl ring-1 ring-zinc-700"
					style="width:min(100%, 920px, calc((100dvh - 230px) * {(profile.w / profile.h).toFixed(
						4
					)})); aspect-ratio:{profile.w}/{profile.h}"
				>
					{#each profile.components as c (c.id)}
						{@const box = `left:${c.x * 100}%; top:${c.y * 100}%; width:${c.w * 100}%; height:${c.h * 100}%`}
						{#if c.type === 'display'}
							<div class="absolute overflow-hidden rounded bg-black ring-1 ring-zinc-600" style={box}>
								<!-- contain: keep the framebuffer's aspect ratio (letterbox), never stretch -->
								<canvas
									bind:this={canvas}
									class="h-full w-full"
									style="image-rendering: pixelated; object-fit: contain;"
								></canvas>
							</div>
						{:else if c.type === 'button'}
							<button
								class="absolute flex items-center justify-center rounded-md bg-zinc-700 text-xs font-semibold text-zinc-200 ring-1 ring-zinc-600 transition hover:bg-zinc-600 active:scale-95 active:bg-zinc-500"
								style={box}
								title={c.label}
								onclick={() => c.key && session.sendKey(c.key)}
							>
								{c.label}
							</button>
						{:else}
							<div
								class="absolute flex items-center justify-center rounded bg-zinc-900/60 text-[8px] text-zinc-500 ring-1 ring-zinc-700/60"
								style={box}
								title="{c.label} ({c.type})"
							>
								{c.label}
							</div>
						{/if}
					{/each}
				</div>
				<p class="text-muted-foreground text-xs">{profile.id} — layout mirrors the device</p>
			{:else}
				<div class="rounded-xl bg-zinc-800 p-2.5 ring-1 ring-zinc-700">
					<canvas bind:this={canvas} class="block" style="image-rendering: pixelated;"></canvas>
				</div>
				<div class="flex items-center gap-6">
					<div class="grid grid-cols-3 grid-rows-3 gap-1">
						<span></span>
						<Button variant="outline" class={pad} onclick={() => session.sendKey(Key.Up)}>▲</Button>
						<span></span>
						<Button variant="outline" class={pad} onclick={() => session.sendKey(Key.Left)}>◄</Button>
						<span></span>
						<Button variant="outline" class={pad} onclick={() => session.sendKey(Key.Right)}>►</Button>
						<span></span>
						<Button variant="outline" class={pad} onclick={() => session.sendKey(Key.Down)}>▼</Button>
						<span></span>
					</div>
					<div class="flex flex-col gap-2">
						<Button class={pad} onclick={() => session.sendKey(Key.Select)}>OK</Button>
						<Button variant="destructive" class={pad} onclick={() => session.sendKey(Key.Cancel)}>✕</Button>
					</div>
				</div>
			{/if}
		</div>

		{#if showLogs}
			<aside class="border-border flex w-80 shrink-0 flex-col overflow-hidden border-l">
				<div class="border-border text-muted-foreground border-b px-3 py-1.5 text-xs font-bold">
					Device logs (KLP)
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
