// Minimal hooks. Identity is by call order within a full render pass (same rule
// as React: don't call hooks conditionally). State persists across renders; a
// setState schedules a re-render via the host-provided callback. Full per-
// component fiber identity is a later refinement (Plan 37 Fase 3).

let states: any[] = [];
let cursor = 0;
let schedule: () => void = () => {};

export function __beginRender(scheduleRerender: () => void): void {
  cursor = 0;
  schedule = scheduleRerender;
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
}
