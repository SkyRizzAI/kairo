// @palanu/app-sdk manifest types (Plan 48 Fase 3 / Plan 59).
// Shared schema for manifest.json — the app declaration file.

/** Runtime tier the app targets. Determines which sandbox the loader uses. */
export type RuntimeTier = "js" | "wasm" | "native";

/** Preferred display server. null/absent = any server accepted. */
export type DisplayServer = "aether" | "fbcon" | null;

export interface PappManifest {
  /** Reverse-domain app id, e.g. "com.palanu.clock" */
  id: string;

  /** Human-readable app name */
  name: string;

  /** SemVer app version, e.g. "1.2.0" */
  version: string;

  /** Entry point relative to app directory (default "App.tsx") */
  entry: string;

  /**
   * Capabilities the app needs (Plan 42).
   * Core interfaces (log, device, events, tasks, storage.kv) are always
   * available and don't need to be listed here.
   */
  needs: string[];

  /**
   * Nema System API version the app was built against (Plan 48).
   * Format: "major.minor" string, e.g. "1.0".
   * Checked at load time: major must match exactly, app.minor ≤ host.minor.
   */
  api_version: string;

  // ── Plan 59 fields ──────────────────────────────────────────────────────

  /**
   * Runtime tier this app targets (Plan 56/59).
   * "js" (default) = QuickJS sandbox; "wasm" = wasm3; "native" = C built-in.
   */
  runtime?: RuntimeTier;

  /**
   * Default argv injected when this app is launched from the icon (Plan 86).
   * Analogous to the Exec= args in a Linux .desktop shortcut.
   * e.g. ["--ui"] → argv = [id, "--ui"] when user taps the icon.
   * When launched from CLI (`run <app> args…`), the shell argv is used instead.
   */
  args?: string[];

  /**
   * Preferred display server (Plan 51/59).
   * The loader switches to this server before launching the app.
   * null or absent = accept whichever server is active.
   */
  display_server?: DisplayServer;

  /**
   * App icon (Plan 53/59).
   * May be a system icon handle ("status.wifi") or an asset path ("icon.png").
   * Shown in the launcher grid and app list.
   */
  icon?: string;

  /**
   * App category for the launcher (Plan 59).
   * e.g. "tools", "games", "media", "settings", "connectivity".
   */
  category?: string;
}
