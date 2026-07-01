<script lang="ts">
	import type { Snippet } from 'svelte';
	import { Button } from '$lib/components/ui/button';
	import { Key, type BoardProfile, type ScreenFrame } from '@palanu/link';

	// Shared virtual-device renderer: draws the board bezel, its components at the
	// device's real (profile) layout, and the 1-bit framebuffer into the LCD with
	// its aspect ratio preserved. Used by both /remote (SessionView) and the WASM
	// /simulator so the on-screen device always mirrors the real hardware. Before a
	// profile arrives it falls back to a generic D-pad.
	let {
		profile,
		frame,
		on: onColor = [230, 255, 230],
		off: offColor = [11, 14, 11],
		maxWidth = 920,
		reserve = 230, // vertical px reserved for header/controls when height-capping
		onkey,
		overlay
	}: {
		profile: BoardProfile | null;
		frame: ScreenFrame | null;
		on?: [number, number, number];
		off?: [number, number, number];
		maxWidth?: number;
		reserve?: number;
		onkey: (key: number) => void;
		overlay?: Snippet;
	} = $props();

	let canvas = $state<HTMLCanvasElement>();
	const SCALE = 2;

	function draw(f: ScreenFrame) {
		if (!canvas || f.px.length < f.w * f.h) return;
		const o = document.createElement('canvas');
		o.width = f.w;
		o.height = f.h;
		const octx = o.getContext('2d');
		if (!octx) return;
		const img = octx.createImageData(f.w, f.h);
		for (let i = 0; i < f.w * f.h; i++) {
			const c = f.px[i] !== 0 ? onColor : offColor;
			const k = i * 4;
			img.data[k] = c[0];
			img.data[k + 1] = c[1];
			img.data[k + 2] = c[2];
			img.data[k + 3] = 255;
		}
		octx.putImageData(img, 0, 0);
		canvas.width = f.w * SCALE;
		canvas.height = f.h * SCALE;
		const ctx = canvas.getContext('2d');
		if (!ctx) return;
		ctx.imageSmoothingEnabled = false;
		ctx.drawImage(o, 0, 0, canvas.width, canvas.height);
	}

	// Redraw whenever the frame, theme colors, or the canvas (re)mounts.
	$effect(() => {
		void onColor;
		void offColor;
		if (frame && canvas) draw(frame);
	});

	const pad = 'size-11 text-base';
</script>

{#if profile}
	<!-- Sized to dominate: as wide as fits, capped by viewport height via aspect. -->
	<div
		class="relative rounded-2xl bg-zinc-800 shadow-xl ring-1 ring-zinc-700"
		style="width:min(100%, {maxWidth}px, calc((100dvh - {reserve}px) * {(
			profile.w / profile.h
		).toFixed(4)})); aspect-ratio:{profile.w}/{profile.h}"
	>
		{#each profile.components as c (c.id)}
			{@const box = `left:${c.x * 100}%; top:${c.y * 100}%; width:${c.w * 100}%; height:${c.h * 100}%`}
			{#if c.type === 'display'}
				<div class="absolute overflow-hidden rounded bg-black ring-1 ring-zinc-600" style={box}>
					<!-- contain: keep the framebuffer aspect ratio (letterbox), never stretch -->
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
					onclick={() => c.key && onkey(c.key)}
				>
					{c.label}
				</button>
			{:else if c.type === 'led'}
				<!-- Addressable/indicator LED. Rendered as a small lens; live colour
				     needs LED-state telemetry (not wired yet), so shown neutral. -->
				<div class="absolute flex items-center justify-center" style={box} title="{c.label} (LED)">
					<span
						class="rounded-full bg-zinc-500 ring-1 ring-zinc-300/40 shadow-[0_0_6px_1px_rgba(255,255,255,0.18)]"
						style="width:min(100%,100%); height:auto; aspect-ratio:1;"
					></span>
				</div>
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
		{#if overlay}
			{@render overlay()}
		{/if}
	</div>
{:else}
	<!-- Generic fallback until the device sends its board profile. -->
	<div class="relative rounded-xl bg-zinc-800 p-2.5 ring-1 ring-zinc-700">
		<canvas bind:this={canvas} class="block" style="image-rendering: pixelated;"></canvas>
		{#if overlay}{@render overlay()}{/if}
	</div>
	<div class="flex items-center gap-6">
		<div class="grid grid-cols-3 grid-rows-3 gap-1">
			<span></span>
			<Button variant="outline" class={pad} onclick={() => onkey(Key.Up)}>▲</Button>
			<span></span>
			<Button variant="outline" class={pad} onclick={() => onkey(Key.Left)}>◄</Button>
			<span></span>
			<Button variant="outline" class={pad} onclick={() => onkey(Key.Right)}>►</Button>
			<span></span>
			<Button variant="outline" class={pad} onclick={() => onkey(Key.Down)}>▼</Button>
			<span></span>
		</div>
		<div class="flex flex-col gap-2">
			<Button class={pad} onclick={() => onkey(Key.Select)}>OK</Button>
			<Button variant="destructive" class={pad} onclick={() => onkey(Key.Cancel)}>✕</Button>
		</div>
	</div>
{/if}
