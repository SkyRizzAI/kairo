<script lang="ts">
	import './layout.css';
	import favicon from '$lib/assets/favicon.svg';
	import { page } from '$app/state';
	import { Toaster } from '$lib/components/ui/sonner';
	import { Cpu, Bluetooth, Usb } from '@lucide/svelte';

	let { children } = $props();

	const nav = [
		{ href: '/simulator', label: 'Simulator', icon: Cpu },
		{ href: '/remote', label: 'Remote', icon: Bluetooth },
		{ href: '/flash', label: 'Flash', icon: Usb }
	];
</script>

<svelte:head><link rel="icon" href={favicon} /></svelte:head>

<div class="bg-background text-foreground flex h-screen">
	<aside class="border-border flex w-56 shrink-0 flex-col border-r">
		<div class="px-4 py-4 text-lg font-semibold tracking-tight">Palanu Forge</div>
		<nav class="flex flex-col gap-1 px-2">
			{#each nav as item (item.href)}
				{@const active = page.url.pathname.startsWith(item.href)}
				{@const Icon = item.icon}
				<a
					href={item.href}
					class="flex items-center gap-2 rounded-md px-3 py-2 text-sm transition-colors {active
						? 'bg-accent text-accent-foreground'
						: 'text-muted-foreground hover:bg-accent/50 hover:text-foreground'}"
				>
					<Icon class="size-4" />
					{item.label}
				</a>
			{/each}
		</nav>
		<div class="text-muted-foreground mt-auto px-4 py-3 text-xs">Palanu Forge · v0.0.1</div>
	</aside>

	<main class="flex-1 overflow-hidden">
		{@render children()}
	</main>
</div>

<Toaster />
