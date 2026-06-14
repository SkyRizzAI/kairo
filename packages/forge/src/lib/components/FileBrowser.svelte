<script lang="ts">
	import type { FileEntry } from '$lib/RemoteSession';
	import { Button } from '$lib/components/ui/button';
	import {
		Folder,
		File as FileIcon,
		CornerLeftUp,
		Upload,
		FolderPlus,
		Trash2,
		RotateCw,
		Save,
		X
	} from '@lucide/svelte';

	// Transport-agnostic file browser over the PLP FILE channel. Both RemoteSession
	// and simStore expose the same fs methods, so the parent passes either as `fs`.
	interface Fs {
		listDir(path: string): Promise<FileEntry[] | null>;
		readFile(path: string): Promise<Uint8Array | null>;
		writeFile(path: string, data: Uint8Array): Promise<boolean>;
		mkdir(path: string): Promise<boolean>;
		removeFile(path: string): Promise<boolean>;
	}
	let { fs, ready = true }: { fs: Fs; ready?: boolean } = $props();

	let cwd = $state('/');
	let entries = $state<FileEntry[]>([]);
	let loading = $state(false);
	let err = $state('');
	let open = $state<{ path: string; text: string } | null>(null); // file viewer/editor
	let mkOpen = $state(false);
	let mkName = $state('');
	let fileInput: HTMLInputElement;

	const join = (dir: string, name: string) => (dir === '/' ? '/' + name : dir + '/' + name);
	const parent = (p: string) => (p === '/' ? '/' : p.slice(0, p.lastIndexOf('/')) || '/');

	async function refresh() {
		if (!ready) return;
		loading = true;
		err = '';
		const list = await fs.listDir(cwd);
		loading = false;
		if (list === null) {
			err = 'cannot read ' + cwd;
			entries = [];
			return;
		}
		// dirs first, then files, alphabetical
		entries = list.sort(
			(a, b) => Number(b.isDir) - Number(a.isDir) || a.name.localeCompare(b.name)
		);
	}

	// Reload when the panel becomes ready or the directory changes.
	let lastKey = '';
	$effect(() => {
		const key = `${ready}:${cwd}`;
		if (key !== lastKey) {
			lastKey = key;
			void refresh();
		}
	});

	function enter(e: FileEntry) {
		if (e.isDir) {
			open = null;
			cwd = join(cwd, e.name);
		} else {
			void view(e);
		}
	}

	async function view(e: FileEntry) {
		const data = await fs.readFile(join(cwd, e.name));
		open = { path: join(cwd, e.name), text: data ? new TextDecoder().decode(data) : '' };
	}

	async function save() {
		if (!open) return;
		await fs.writeFile(open.path, new TextEncoder().encode(open.text));
		open = null;
		await refresh();
	}

	async function del(e: FileEntry) {
		await fs.removeFile(join(cwd, e.name));
		await refresh();
	}

	async function makeDir() {
		const n = mkName.trim();
		if (!n) return;
		await fs.mkdir(join(cwd, n));
		mkName = '';
		mkOpen = false;
		await refresh();
	}

	async function onUpload(ev: Event) {
		const f = (ev.target as HTMLInputElement).files?.[0];
		if (!f) return;
		const data = new Uint8Array(await f.arrayBuffer());
		await fs.writeFile(join(cwd, f.name), data);
		(ev.target as HTMLInputElement).value = '';
		await refresh();
	}

	const fmtSize = (n: number) => (n < 1024 ? `${n} B` : `${(n / 1024).toFixed(1)} KB`);
</script>

<div class="flex h-full flex-col bg-black/20 text-xs">
	<!-- toolbar -->
	<div class="border-border flex items-center gap-1 border-b px-2 py-1.5">
		<Button
			size="icon"
			variant="ghost"
			class="size-7"
			title="Up"
			disabled={cwd === '/'}
			onclick={() => {
				open = null;
				cwd = parent(cwd);
			}}
		>
			<CornerLeftUp class="size-3.5" />
		</Button>
		<span class="truncate font-mono text-[11px] text-zinc-300">{cwd}</span>
		<div class="ml-auto flex items-center gap-1">
			<Button size="icon" variant="ghost" class="size-7" title="New folder" onclick={() => (mkOpen = !mkOpen)}>
				<FolderPlus class="size-3.5" />
			</Button>
			<Button size="icon" variant="ghost" class="size-7" title="Upload file" onclick={() => fileInput.click()}>
				<Upload class="size-3.5" />
			</Button>
			<Button size="icon" variant="ghost" class="size-7" title="Refresh" onclick={refresh}>
				<RotateCw class="size-3.5" />
			</Button>
			<input bind:this={fileInput} type="file" class="hidden" onchange={onUpload} />
		</div>
	</div>

	{#if mkOpen}
		<div class="border-border flex items-center gap-1.5 border-b px-2 py-1.5">
			<input
				class="flex-1 rounded border border-zinc-700 bg-transparent px-1.5 py-1 font-mono text-[11px] outline-none"
				placeholder="folder name"
				bind:value={mkName}
				onkeydown={(e) => e.key === 'Enter' && makeDir()}
			/>
			<Button size="sm" class="h-7" onclick={makeDir}>Create</Button>
		</div>
	{/if}

	<!-- body: file viewer OR directory list -->
	{#if open}
		<div class="border-border flex items-center gap-2 border-b px-2 py-1">
			<span class="truncate font-mono text-[11px] text-sky-400">{open.path}</span>
			<div class="ml-auto flex gap-1">
				<Button size="sm" class="h-7" onclick={save}><Save class="size-3.5" /> Save</Button>
				<Button size="icon" variant="ghost" class="size-7" onclick={() => (open = null)}>
					<X class="size-3.5" />
				</Button>
			</div>
		</div>
		<textarea
			class="flex-1 resize-none bg-transparent p-2 font-mono text-[11px] leading-relaxed text-zinc-200 outline-none"
			bind:value={open.text}
			spellcheck="false"
		></textarea>
	{:else}
		<div class="flex-1 overflow-auto">
			{#if err}
				<p class="p-2 text-red-400">{err}</p>
			{:else if loading}
				<p class="text-muted-foreground p-2">loading…</p>
			{:else if entries.length === 0}
				<p class="text-muted-foreground p-2">empty directory</p>
			{:else}
				{#each entries as e (e.name)}
					<div class="group hover:bg-accent/40 flex items-center gap-2 px-2 py-1">
						<button class="flex min-w-0 flex-1 items-center gap-2 text-left" onclick={() => enter(e)}>
							{#if e.isDir}
								<Folder class="size-3.5 shrink-0 text-sky-400" />
							{:else}
								<FileIcon class="text-muted-foreground size-3.5 shrink-0" />
							{/if}
							<span class="truncate font-mono text-[11px]">{e.name}</span>
							{#if !e.isDir}
								<span class="text-muted-foreground ml-auto shrink-0 text-[10px]">{fmtSize(e.size)}</span>
							{/if}
						</button>
						<button
							class="text-muted-foreground shrink-0 opacity-0 group-hover:opacity-100 hover:text-red-400"
							title="Delete"
							onclick={() => del(e)}
						>
							<Trash2 class="size-3.5" />
						</button>
					</div>
				{/each}
			{/if}
		</div>
	{/if}
</div>
