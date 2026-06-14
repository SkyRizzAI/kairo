<script lang="ts">
	import { tick } from 'svelte';
	import type { CliChunk } from '$lib/RemoteSession';

	// Flipper-style terminal over the KLP CLI channel. Transport-agnostic: the
	// parent wires `send` (host→device command line) and `subscribe` (device→host
	// output chunks), so this works for both /remote (RemoteSession) and the WASM
	// /simulator (simStore) without knowing which.
	let {
		send,
		subscribe,
		ready = true,
		sid = 1
	}: {
		send: (sid: number, line: string) => void;
		subscribe: (fn: (c: CliChunk) => void) => () => void;
		ready?: boolean;
		sid?: number; // this terminal's CLI session id (Plan 45)
	} = $props();

	type Line = { kind: 'in' | 'out'; text: string };
	let lines = $state<Line[]>([{ kind: 'out', text: "Palanu CLI — type 'help'." }]);
	let input = $state('');
	let busy = $state(false);
	let cwd = $state('/'); // shell working directory, from device prompt frames (Plan 44)
	let scroller = $state<HTMLElement>();
	const history: string[] = [];
	let histIdx = -1;

	async function pin() {
		await tick();
		if (scroller) scroller.scrollTop = scroller.scrollHeight;
	}

	$effect(() => {
		const off = subscribe((c) => {
			if (c.sid !== undefined && c.sid !== sid) return; // not our session
			if (c.done) {
				busy = false;
			} else if (c.prompt !== undefined) {
				cwd = c.prompt || '/';
			} else if (c.text !== undefined) {
				lines = [...lines.slice(-500), { kind: 'out', text: c.text }];
			}
			void pin();
		});
		return off;
	});

	function run() {
		const line = input.trim();
		if (!line || busy || !ready) return;
		lines = [...lines, { kind: 'in', text: line }];
		history.push(line);
		histIdx = history.length;
		input = '';
		busy = true;
		send(sid, line);
		void pin();
	}

	function onKey(e: KeyboardEvent) {
		if (e.key === 'Enter') {
			e.preventDefault();
			run();
		} else if (e.key === 'ArrowUp') {
			e.preventDefault();
			if (histIdx > 0) input = history[--histIdx] ?? '';
		} else if (e.key === 'ArrowDown') {
			e.preventDefault();
			if (histIdx < history.length - 1) input = history[++histIdx] ?? '';
			else {
				histIdx = history.length;
				input = '';
			}
		}
	}
</script>

<div class="flex h-full flex-col bg-black/40">
	<div bind:this={scroller} class="flex-1 overflow-auto p-2 font-mono text-[11px] leading-relaxed">
		{#each lines as l, i (i)}
			{#if l.kind === 'in'}
				<div class="text-sky-400"><span class="text-zinc-500">$</span> {l.text}</div>
			{:else}
				<div class="whitespace-pre-wrap break-words text-zinc-300">{l.text}</div>
			{/if}
		{/each}
		{#if busy}<div class="text-zinc-500">…</div>{/if}
	</div>
	<div class="border-border flex items-center gap-1.5 border-t px-2 py-1.5 font-mono text-[11px]">
		<span class="text-emerald-400">{cwd}</span><span class="text-zinc-500">$</span>
		<input
			class="flex-1 bg-transparent text-zinc-100 outline-none placeholder:text-zinc-600 disabled:opacity-50"
			placeholder={ready ? 'command (e.g. help, pwd, cd /apps, display)' : 'connecting…'}
			bind:value={input}
			onkeydown={onKey}
			disabled={!ready}
			spellcheck="false"
			autocapitalize="off"
			autocomplete="off"
		/>
	</div>
</div>
