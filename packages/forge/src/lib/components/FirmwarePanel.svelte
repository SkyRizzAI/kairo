<script lang="ts">
	import { Button } from '$lib/components/ui/button';
	import { Upload } from '@lucide/svelte';

	// Firmware OTA panel (Plan 39). Transport-agnostic: the parent passes `update`
	// (RemoteSession.otaUpdate for /remote, simStore.otaUpdate for /simulator), so
	// the same UI drives a real device or the in-browser dry-run.
	let {
		update,
		ready = true
	}: {
		update: (image: Uint8Array, onProgress: (sent: number, total: number) => void) => Promise<boolean>;
		ready?: boolean;
	} = $props();

	let input = $state<HTMLInputElement>();
	let name = $state('');
	let size = $state(0);
	let busy = $state(false);
	let pct = $state(0);
	let msg = $state('');
	let data: Uint8Array | null = null;

	async function pick(e: Event) {
		const file = (e.target as HTMLInputElement).files?.[0];
		if (!file) return;
		data = new Uint8Array(await file.arrayBuffer());
		name = file.name;
		size = data.length;
		msg = '';
	}
	async function push() {
		if (!data || busy) return;
		busy = true;
		pct = 0;
		msg = 'uploading…';
		const ok = await update(data, (s, t) => (pct = Math.round((s / t) * 100)));
		busy = false;
		msg = ok ? 'done — device rebooting into the new image.' : 'failed (unsupported / transport error / bad image).';
	}
</script>

<div class="flex flex-1 flex-col gap-3 p-3 text-xs">
	<input bind:this={input} type="file" accept=".bin" class="hidden" onchange={pick} />
	<Button size="sm" variant="secondary" onclick={() => input?.click()} disabled={busy}>
		<Upload class="size-4" /> Choose .bin
	</Button>
	{#if name}
		<div class="text-muted-foreground break-all">{name} · {(size / 1024).toFixed(0)} KB</div>
		<Button size="sm" onclick={push} disabled={busy || !ready}>
			{busy ? `Updating… ${pct}%` : 'Push update'}
		</Button>
		{#if busy}
			<div class="bg-border h-1.5 w-full overflow-hidden rounded">
				<div class="h-full bg-sky-400" style="width:{pct}%"></div>
			</div>
		{/if}
	{/if}
	{#if msg}<div class="text-muted-foreground break-words">{msg}</div>{/if}
	<div class="text-muted-foreground mt-auto text-[10px] leading-relaxed">
		Streams the image over PLP to the inactive A/B slot, then the device reboots into it
		(rollback is automatic if it fails). In the WASM simulator this is a dry-run — the flow
		runs but no real image is swapped.
	</div>
</div>
