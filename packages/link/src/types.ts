// Shared types for PLP RemoteSession — used by Forge web and Forge CLI.

export interface ScreenFrame {
	w: number;
	h: number;
	px: Uint8Array; // w*h, 0/1
}

export interface LogEntry {
	level: number;
	component: string;
	message: string;
}

export interface EventEntry {
	name: string;
	fields: Record<string, string>;
}

// CLI output chunk. `text` is a line of output; `done` marks end-of-command (the
// device sent EOT) so the terminal can re-enable its prompt; `prompt` carries the
// device's current working directory (Plan 44) for a shell-like prompt.
export interface CliChunk {
	sid?: number; // session id (Plan 45) — terminals filter to their own session
	text?: string;
	done?: boolean;
	prompt?: string;
}

// One directory entry from the FILE channel (mirrors firmware FsEntry).
export interface FileEntry {
	name: string;
	isDir: boolean;
	size: number;
}

// Board profile — the device's physical layout (SYSTEM GetInfo reply, Plan 33).
// Mirrors firmware BoardProfile/ComponentDef; coordinates are normalized 0–1
// over the board rect, `w`/`h` on the profile are the physical aspect (mm).
export interface BoardComponent {
	id: number;
	label: string;
	type: 'display' | 'button' | 'led' | 'sensor' | 'speaker' | 'mic' | 'camera' | 'port' | 'other';
	key?: number; // input Key to send when this (button) is pressed remotely
	x: number;
	y: number;
	w: number;
	h: number;
}

export interface BoardProfile {
	id: string;
	name: string;
	w: number;
	h: number;
	components: BoardComponent[];
}

export const Power = { Restart: 0x10, Sleep: 0x11, Shutdown: 0x12 } as const;
export const Key = { Up: 1, Down: 2, Left: 3, Right: 4, Select: 5, Cancel: 6, Menu: 7 } as const;

// Browser keyboard → Palanu Key. Shared by every view that forwards keystrokes to
// a device (/remote SessionView, /simulator) so the mapping lives in one place.
export const KEY_MAP: Record<string, number> = {
	ArrowUp: Key.Up,
	ArrowDown: Key.Down,
	ArrowLeft: Key.Left,
	ArrowRight: Key.Right,
	Enter: Key.Select,
	' ': Key.Select,
	Escape: Key.Cancel,
	Backspace: Key.Cancel
};

// "WxH" of a screen frame, or "—" before the first frame. Shared by the headers
// of /remote and /simulator so the formatting stays identical.
export function frameDims(frame: ScreenFrame | null): string {
	return frame ? `${frame.w}×${frame.h}` : '—';
}
