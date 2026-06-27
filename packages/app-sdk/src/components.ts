// Intrinsic components. Each is a marker function tagged with the native node
// type; renderToTree() emits a native node for tagged types instead of calling
// them. Maps 1:1 to firmware NodeType + the C builders (Plan 33/37).
import type { KElement, IntrinsicFn, Style, TextVariant } from "./types";

function intrinsic(tag: string): IntrinsicFn {
  const fn = ((props: any): KElement => ({ $$nema: true, type: fn, props: props || {} })) as IntrinsicFn;
  fn.__tag = tag;
  return fn;
}

// Common prop shapes (typed for the editor; runtime just forwards them).
type Common = { style?: Style; children?: any; key?: string | number };

export interface ViewProps extends Common {}
export interface TextProps extends Common { variant?: TextVariant }
export interface PressableProps extends Common { onPress?: () => void }
export interface ScrollViewProps extends Common {}
export interface SliderProps extends Common {
  value: number; min?: number; max?: number; step?: number;
  onChange?: (v: number) => void;
}
// On/off switch — same native component the built-in settings use. <Switch on={...}/>
export interface SwitchProps extends Common { on?: boolean }
// Read-only progress/usage bar (0..100). <ProgressBar pct={60}/>
export interface ProgressBarProps extends Common { pct: number }
// Animated busy spinner. <Spinner size={13}/>
export interface SpinnerProps extends Common { size?: number }

export const View        = intrinsic("View")        as (p: ViewProps) => KElement;
export const Text        = intrinsic("Text")        as (p: TextProps) => KElement;
export const Pressable   = intrinsic("Pressable")   as (p: PressableProps) => KElement;
export const ScrollView  = intrinsic("Scroll")      as (p: ScrollViewProps) => KElement;
export const Slider      = intrinsic("Slider")      as (p: SliderProps) => KElement;
export const Switch      = intrinsic("Switch")      as (p: SwitchProps) => KElement;
export const ProgressBar = intrinsic("ProgressBar") as (p: ProgressBarProps) => KElement;
export const Spinner     = intrinsic("Spinner")     as (p: SpinnerProps) => KElement;

// Row/Col are Views with a forced direction (sugar, like the C builders).
export const Row: (p: ViewProps) => KElement = (p) =>
  View({ ...p, style: { ...(p.style || {}), flexDirection: "row" } });
export const Col: (p: ViewProps) => KElement = (p) =>
  View({ ...p, style: { ...(p.style || {}), flexDirection: "column" } });
