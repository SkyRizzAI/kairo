<script lang="ts">
	import { Button } from '$lib/components/ui/button';
	import { Upload, Globe, RefreshCw } from '@lucide/svelte';
	import { env } from '$env/dynamic/public';

	interface GhAsset {
		name: string;
		size: number;
		browser_download_url: string;
	}
	interface GhRelease {
		id: number;
		tag_name: string;
		name: string;
		published_at: string;
		prerelease: boolean;
		draft: boolean;
		assets: GhAsset[];
	}

	const DEFAULT_REPO = env.PUBLIC_FIRMWARE_REPO || 'SkyRizzAI/kairo';
	// OTA streams a single app image to the inactive A/B slot, so ONLY app-only
	// (`-ota.bin`) builds belong here. A `-factory.bin` is a full-flash image (→ 0x0,
	// far larger than an OTA slot) — it must never appear as an OTA option, or picking
	// it would overflow the slot / brick the device. (.wasm = simulator build.)
	const OTA_SLOT_BYTES = 0x500000; // ota_0/ota_1 size in partitions.csv (5 MB)
	const isFwAsset = (a: GhAsset) =>
		a.name.startsWith('palanu-') && (a.name.endsWith('-ota.bin') || a.name.endsWith('.wasm'));

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

	// ── mode ────────────────────────────────────────────────────────────────────
	type Mode = 'release' | 'custom' | 'file';
	let mode = $state<Mode>('release');

	// ── repo / release state ─────────────────────────────────────────────────────
	let customRepo = $state('');
	let releases = $state<GhRelease[]>([]);
	let selectedRelease = $state<GhRelease | null>(null);
	let selectedAsset = $state<GhAsset | null>(null);
	let fetching = $state(false);
	let fetchError = $state('');

	const activeRepo = $derived(mode === 'custom' ? customRepo.trim() : DEFAULT_REPO);
	const fwAssets = $derived(
		(selectedRelease?.assets ?? []).filter(isFwAsset)
	);

	async function fetchReleases() {
		if (!activeRepo) return;
		fetching = true;
		fetchError = '';
		releases = [];
		selectedRelease = null;
		selectedAsset = null;
		try {
			const res = await fetch(`https://api.github.com/repos/${activeRepo}/releases`, {
				headers: { Accept: 'application/vnd.github+json' }
			});
			if (!res.ok) throw new Error(`GitHub API: HTTP ${res.status}`);
			const all: GhRelease[] = await res.json();
			releases = all.filter((r) => !r.draft);
			if (releases.length) {
				selectedRelease = releases[0];
				const first = releases[0].assets.filter(isFwAsset);
				if (first.length) selectedAsset = first[0];
			}
		} catch (e) {
			fetchError = String(e);
		}
		fetching = false;
	}

	// Auto-fetch when switching to release mode
	$effect(() => {
		if (mode === 'release' && releases.length === 0 && !fetching) fetchReleases();
	});

	// ── local file state ─────────────────────────────────────────────────────────
	let fileInput = $state<HTMLInputElement>();
	let fileName = $state('');
	let fileSize = $state(0);
	let fileData = $state<Uint8Array | null>(null);

	async function pickFile(e: Event) {
		const file = (e.target as HTMLInputElement).files?.[0];
		if (!file) return;
		fileData = new Uint8Array(await file.arrayBuffer());
		fileName = file.name;
		fileSize = fileData.length;
		log = [];
		sent = 0;
		total = 0;
	}

	// ── flash progress (shared) ───────────────────────────────────────────────────
	let busy = $state(false);
	let sent = $state(0);
	let total = $state(0);
	let log = $state<string[]>([]);
	let flashStart = $state(0);
	let now = $state(0);
	let _ticker: ReturnType<typeof setInterval> | undefined;

	const pct = $derived(total > 0 ? Math.round((sent / total) * 100) : 0);
	const elapsed = $derived(flashStart > 0 && now > 0 ? Math.floor((now - flashStart) / 1000) : 0);
	const eta = $derived(
		sent > 0 && elapsed > 0 && total > sent
			? Math.round(((total - sent) / sent) * elapsed)
			: 0
	);

	function startTicker() {
		flashStart = Date.now();
		now = Date.now();
		_ticker = setInterval(() => { now = Date.now(); }, 1000);
	}
	function stopTicker() {
		clearInterval(_ticker);
	}

	async function flash(image: Uint8Array) {
		if (image.length > OTA_SLOT_BYTES) {
			log = [
				`refusing: image is ${(image.length / 1048576).toFixed(1)} MB — larger than the ` +
					`${(OTA_SLOT_BYTES / 1048576).toFixed(0)} MB OTA slot. This looks like a ` +
					`-factory.bin (cable-flash to 0x0), not an OTA image.`
			];
			return;
		}
		busy = true;
		sent = 0;
		total = image.length;
		log = [];
		startTicker();
		await update(image, (s) => (sent = s), (m) => (log = [...log, m]));
		stopTicker();
		busy = false;
	}

	async function flashRelease() {
		if (!selectedAsset || busy) return;
		busy = true;
		sent = 0;
		total = selectedAsset.size;
		log = [`downloading ${selectedAsset.name} (${(selectedAsset.size / 1024).toFixed(0)} KB)…`];
		startTicker();
		try {
			const res = await fetch(`/api/firmware-proxy?url=${encodeURIComponent(selectedAsset.browser_download_url)}`);
			if (!res.ok) throw new Error(`download failed: HTTP ${res.status}`);
			const image = new Uint8Array(await res.arrayBuffer());
			if (image.length > OTA_SLOT_BYTES) {
				throw new Error(
					`${selectedAsset.name} is ${(image.length / 1048576).toFixed(1)} MB — larger than the ` +
						`OTA slot; that's a factory image for cable flashing (0x0), not OTA.`
				);
			}
			log = [...log, 'download complete — flashing…'];
			total = image.length;
			sent = 0;
			await update(image, (s) => (sent = s), (m) => (log = [...log, m]));
		} catch (e) {
			log = [...log, `error: ${e}`];
		}
		stopTicker();
		busy = false;
	}

	function onModeChange(m: Mode) {
		mode = m;
		log = [];
		sent = 0;
		total = 0;
	}
</script>

<div class="flex h-full flex-col gap-2 p-3 text-xs">
	<!-- Mode selector -->
	<div class="flex rounded border border-border overflow-hidden text-[10px]">
		{#each ([['release', 'Official'], ['custom', 'Custom Repo'], ['file', 'Local File']] as const) as [m, label]}
			<button
				class="flex-1 px-2 py-1 transition-colors {mode === m
					? 'bg-sky-500/20 text-sky-300 font-medium'
					: 'text-muted-foreground hover:bg-accent'}"
				onclick={() => onModeChange(m)}
			>
				{label}
			</button>
		{/each}
	</div>

	<!-- Release / Custom Repo mode -->
	{#if mode === 'release' || mode === 'custom'}
		{#if mode === 'custom'}
			<div class="flex gap-1">
				<input
					class="flex-1 rounded border border-border bg-background px-2 py-1 text-[10px] outline-none focus:border-sky-500"
					placeholder="owner/repo"
					bind:value={customRepo}
					onkeydown={(e) => e.key === 'Enter' && fetchReleases()}
				/>
				<Button size="sm" variant="secondary" onclick={fetchReleases} disabled={fetching || !customRepo.trim()}>
					{#if fetching}<RefreshCw class="size-3 animate-spin" />{:else}<RefreshCw class="size-3" />{/if}
				</Button>
			</div>
		{:else}
			<div class="flex items-center gap-1 text-muted-foreground">
				<Globe class="size-3 shrink-0" />
				<span class="truncate">{DEFAULT_REPO}</span>
				<button class="ml-auto shrink-0" onclick={fetchReleases} disabled={fetching} title="Refresh">
					<RefreshCw class="size-3 {fetching ? 'animate-spin' : ''}" />
				</button>
			</div>
		{/if}

		{#if fetchError}
			<div class="text-red-400 text-[10px]">{fetchError}</div>
		{:else if fetching}
			<div class="text-muted-foreground text-[10px]">Fetching releases…</div>
		{:else if releases.length}
			<!-- Release picker -->
			<select
				class="rounded border border-border bg-background px-2 py-1 text-[10px] outline-none focus:border-sky-500"
				onchange={(e) => {
					selectedRelease = releases.find((r) => String(r.id) === (e.target as HTMLSelectElement).value) ?? null;
					selectedAsset = null;
					if (selectedRelease) {
						const a = selectedRelease.assets.filter(isFwAsset);
						if (a.length) selectedAsset = a[0];
					}
				}}
			>
				{#each releases as r (r.id)}
					<option value={String(r.id)}>
						{r.tag_name}{r.prerelease ? ' (pre)' : ''} — {r.name || r.tag_name}
					</option>
				{/each}
			</select>

			<!-- Asset picker -->
			{#if fwAssets.length}
				<div class="flex flex-col gap-1">
					{#each fwAssets as asset}
						<button
							class="flex items-center gap-1 rounded border px-2 py-1 text-left transition-colors {selectedAsset?.name === asset.name
								? 'border-sky-500 bg-sky-500/10 text-sky-300'
								: 'border-border hover:bg-accent text-muted-foreground'}"
							onclick={() => (selectedAsset = asset)}
						>
							<span class="flex-1 font-mono truncate">{asset.name}</span>
							<span class="shrink-0 tabular-nums">{(asset.size / 1024).toFixed(0)} KB</span>
						</button>
					{/each}
				</div>
				<Button size="sm" onclick={flashRelease} disabled={busy || !ready || !selectedAsset}>
					{busy ? `Flashing… ${pct}%` : `Flash ${selectedAsset?.name ?? '…'}`}
				</Button>
			{:else if selectedRelease}
				<div class="text-muted-foreground text-[10px]">No firmware assets in this release.</div>
			{/if}
		{:else if mode === 'release'}
			<div class="text-muted-foreground text-[10px]">No releases found.</div>
		{/if}
	{/if}

	<!-- Local file mode -->
	{#if mode === 'file'}
		<input bind:this={fileInput} type="file" accept=".bin,.wasm" class="hidden" onchange={pickFile} />
		<Button size="sm" variant="secondary" onclick={() => fileInput?.click()} disabled={busy}>
			<Upload class="size-4" /> Choose .bin / .wasm
		</Button>
		{#if fileName}
			<div class="text-muted-foreground break-all">{fileName} · {(fileSize / 1024).toFixed(0)} KB</div>
			<Button size="sm" onclick={() => fileData && flash(fileData)} disabled={busy || !ready || !fileData}>
				{busy ? `Updating… ${pct}%` : 'Push update'}
			</Button>
		{/if}
	{/if}

	<!-- Progress bar (shared) -->
	{#if busy || sent > 0}
		<div class="bg-border h-1.5 w-full overflow-hidden rounded">
			<div class="h-full bg-sky-400 transition-all" style="width:{pct}%"></div>
		</div>
		<div class="text-muted-foreground tabular-nums">
			{(sent / 1024).toFixed(0)} / {(total / 1024).toFixed(0)} KB · {pct}% · {elapsed}s{eta > 0 ? ` · ~${eta}s left` : ''}
		</div>
	{/if}

	<!-- Status log (shared) -->
	{#if log.length}
		<div class="border-border mt-1 max-h-40 flex-1 overflow-auto rounded border bg-black/30 p-2 font-mono text-[10px] leading-relaxed">
			{#each log as line, i (i)}
				<div class="break-words text-zinc-300">{line}</div>
			{/each}
		</div>
	{/if}

	<div class="text-muted-foreground mt-auto text-[10px] leading-relaxed">
		Streams the image over PLP to the inactive A/B slot, then the device reboots into it
		(rollback is automatic if it fails). Official releases are fetched from
		<span class="font-mono">{DEFAULT_REPO}</span>.
	</div>
</div>
