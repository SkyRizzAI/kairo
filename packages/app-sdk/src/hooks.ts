// Minimal hooks. Identity is by call order within a full render pass (same rule
// as React: don't call hooks conditionally). State persists across renders; a
// setState schedules a re-render via the host-provided callback. Full per-
// component fiber identity is a later refinement (Plan 37 Fase 3).

let states: any[] = [];
let cursor = 0;
let schedule: () => void = () => {};
// Back-button handler registered (fresh) on each render. The host calls __callBack()
// when the physical Back/Cancel key is pressed; if it returns true the app handled it
// (e.g. navigated to a previous screen) and the app is NOT exited. Returns false → the
// host exits the app (default). This is how a JS app gets the same in-app Back
// navigation the native screens have.
let backHandler: (() => boolean) | null = null;

export function __beginRender(scheduleRerender: () => void): void {
  cursor = 0;
  schedule = scheduleRerender;
  backHandler = null;   // re-registered by useBackHandler() during this render
}

// Register a handler for the physical Back key. Call it during render (like any hook).
// Return true to consume Back (stay in the app), false to let the app exit.
export function useBackHandler(fn: () => boolean): void {
  backHandler = fn;
}

// Host entrypoint — invoked by the engine on a Back press. Not for app use.
export function __callBack(): boolean {
  return backHandler ? !!backHandler() : false;
}

export function useState<T>(initial: T): [T, (v: T | ((prev: T) => T)) => void] {
  const i = cursor++;
  if (i >= states.length) states[i] = initial;
  const set = (v: T | ((prev: T) => T)) => {
    states[i] = typeof v === "function" ? (v as (p: T) => T)(states[i]) : v;
    schedule();
  };
  return [states[i] as T, set];
}

export function useRef<T>(initial: T): { current: T } {
  const i = cursor++;
  if (i >= states.length) states[i] = { current: initial };
  return states[i];
}

export function useEffect(fn: () => void | (() => void), deps?: any[]): void {
  const i = cursor++;
  const prev = states[i];
  const changed =
    !prev || !deps || !prev.deps || deps.some((d: any, k: number) => d !== prev.deps[k]);
  if (changed) {
    if (prev && prev.cleanup) prev.cleanup();
    const cleanup = fn();
    states[i] = { deps, cleanup };
  }
}

// Host calls this when an app is torn down so effects can clean up + state resets.
export function __reset(): void {
  for (const s of states) if (s && s.cleanup) s.cleanup();
  states = [];
  cursor = 0;
  backHandler = null;
}
