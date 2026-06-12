<script lang="ts">
	import { wasmSession } from '$lib/wasmSim';
	import { toast } from 'svelte-sonner';

	let text = $state('');
	let fileName = $state('');

	async function onFile(e: Event) {
		const f = (e.target as HTMLInputElement).files?.[0];
		if (!f) return;
		fileName = f.name;
		text = await f.text();
		toast.success(`Loaded ${f.name} (${text.length} bytes)`);
	}

	function install() {
		if (!text.trim().startsWith('KAPP1')) {
			toast.error('Not a .kapp', {
				description: 'Expected a KAPP1 header. Build one first with `nema-build`.'
			});
			return;
		}
		try {
			wasmSession().installApp(text);
			toast.success('Installed to device', {
				description: 'Open the Simulator → Apps; your app should appear (volatile).'
			});
		} catch (err) {
			toast.error('Install failed', { description: String(err) });
		}
	}
</script>

<div class="h-full overflow-y-auto p-6">
	<div class="mx-auto max-w-2xl space-y-5">
		<header class="space-y-1">
			<h1 class="text-xl font-semibold tracking-tight">Install custom app (OTA)</h1>
			<p class="text-muted-foreground text-sm">
				Push a built <code class="bg-muted rounded px-1">.kapp</code> to the running simulator over
				the virtual cable — the same KLP path as BLE/USB on real hardware. The app installs live and
				appears in <em>Apps</em>, no reflash. Volatile for now (lost on reboot). Boot the device on the
				<a class="underline" href="/simulator">Simulator</a> page first.
			</p>
		</header>

		<div class="space-y-2">
			<label class="text-sm font-medium" for="kappfile">Pick a .kapp file</label>
			<input
				id="kappfile"
				type="file"
				accept=".kapp,text/plain"
				onchange={onFile}
				class="border-border bg-background text-foreground file:bg-accent file:text-accent-foreground w-full rounded-md border p-2 text-sm file:mr-3 file:rounded file:border-0 file:px-3 file:py-1"
			/>
			{#if fileName}
				<p class="text-muted-foreground text-xs">file: {fileName}</p>
			{/if}
		</div>

		<div class="space-y-2">
			<label class="text-sm font-medium" for="kapptext">…or paste a .kapp</label>
			<textarea
				id="kapptext"
				bind:value={text}
				rows="8"
				placeholder={'KAPP1\\n{manifest}\\n<js>'}
				class="border-border bg-background text-foreground w-full rounded-md border p-2 font-mono text-xs"
			></textarea>
		</div>

		<div class="flex items-center gap-3">
			<button
				onclick={install}
				class="bg-primary text-primary-foreground hover:bg-primary/90 rounded-md px-4 py-2 text-sm font-medium"
			>
				Install to device
			</button>
			<a class="text-muted-foreground hover:text-foreground text-sm underline" href="/simulator"
				>→ open Simulator</a
			>
		</div>
	</div>
</div>
