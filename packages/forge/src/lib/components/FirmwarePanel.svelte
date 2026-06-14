<script lang="ts">
	import { Button } from '$lib/components/ui/button';
	import { Upload } from '@lucide/svelte';

	// Firmware OTA panel (Plan 39). Transport-agnostic: the parent passes `update`
	// (RemoteSession.otaUpdate for /remote, simStore.otaUpdate for /simulator), so
	// the same UI drives a real device or the in-browser dry-run. Shows a live
	// progress bar + a status log so an upload is debuggable on real hardware.
	let {
		update,
		ready = true
	}: {
		update: (
			image: Uint8Array,
			onProgress: (sent: number, total: number) => void,
			onStatus: (msg: string) => void
		) => Promise<boolean>;
		ready?: boolean;
	} = $props();

	let input = $state<HTMLInputElement>();
	let name = $state('');
	let size = $state(0);
	let busy = $state(false);
	let sent = $state(0);
	let total = $state(0);
	let log = $state<string[]>([]);
	let data: Uint8Array | null = null;

	const pct = $derived(total > 0 ? Math.round((sent / total) * 100) : 0);

	async function pick(e: Event) {
		const file = (e.target as HTMLInputElement).files?.[0];
		if (!file) return;
		data = new Uint8Array(await file.arrayBuffer());
		name = file.name;
		size = data.length;
		log = [];
		sent = 0;
		total = 0;
	}
	async function push() {
		if (!data || busy) return;
		busy = true;
		sent = 0;
		total = data.length;
		log = [];
		await update(
			data,
			(s) => (sent = s),
			(m) => (log = [...log, m])
		);
		busy = false;
	}
</script>

<div class="flex h-full flex-col gap-2 p-3 text-xs">
	<input bind:this={input} type="file" accept=".bin" class="hidden" onchange={pick} />
	<Button size="sm" variant="secondary" onclick={() => input?.click()} disabled={busy}>
		<Upload class="size-4" /> Choose .bin
	</Button>
	{#if name}
		<div class="text-muted-foreground break-all">{name} · {(size / 1024).toFixed(0)} KB</div>
		<Button size="sm" onclick={push} disabled={busy || !ready}>
			{busy ? `Updating… ${pct}%` : 'Push update'}
		</Button>
	{/if}

	{#if busy || sent > 0}
		<div class="bg-border h-1.5 w-full overflow-hidden rounded">
			<div class="h-full bg-sky-400 transition-all" style="width:{pct}%"></div>
		</div>
		<div class="text-muted-foreground tabular-nums">
			{(sent / 1024).toFixed(0)} / {(total / 1024).toFixed(0)} KB · {pct}%
		</div>
	{/if}

	{#if log.length}
		<div class="border-border mt-1 max-h-40 flex-1 overflow-auto rounded border bg-black/30 p-2 font-mono text-[10px] leading-relaxed">
			{#each log as line, i (i)}
				<div class="break-words text-zinc-300">{line}</div>
			{/each}
		</div>
	{/if}

	<div class="text-muted-foreground mt-auto text-[10px] leading-relaxed">
		Streams the image over PLP to the inactive A/B slot, then the device reboots into it
		(rollback is automatic if it fails). The device's screen may freeze while it erases/writes
		flash. In the WASM simulator this is a dry-run — the flow runs but no real image is swapped.
	</div>
</div>
