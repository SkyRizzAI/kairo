// Nema SDK core types. A KElement is the lightweight "virtual node" jsx() emits;
// renderToTree() expands it into a serialisable node-desc the device bridge turns
// into a native UiNode (Plan 37 §2.2). One tree, two producers (Plan 33).

export type Style = {
  flexDirection?: "row" | "column";
  flexGrow?: number;
  width?: number;
  height?: number;
  padding?: number;
  gap?: number;
  alignItems?: "start" | "center" | "end" | "stretch";
  justifyContent?: "start" | "center" | "end" | "space-between";
  border?: boolean;
  background?: boolean;
};

export type TextVariant = "body" | "title" | "caption";

export type KChild = KElement | string | number | boolean | null | undefined | KChild[];

// What jsx() returns. `type` is an intrinsic marker fn (View/Text/…) or a user
// component function. `props` carries children + handlers + style.
export interface KElement {
  $$nema: true;
  type: IntrinsicFn | ComponentFn;
  props: Record<string, any>;
}

// Intrinsic component markers carry a __tag so the renderer emits a native node
// instead of calling them (see components.ts / render.ts).
export interface IntrinsicFn {
  (props: any): KElement;
  __tag: string;
}

export type ComponentFn = (props: any) => KElement | null;

// Serialisable node-desc produced by renderToTree() and consumed by the device.
export interface NodeDesc {
  type: string;                 // "View" | "Text" | "Pressable" | "Scroll" | "Slider" | "#text"
  text?: string;                // for #text / folded Text content
  style?: Style;
  props?: Record<string, any>;  // non-style props (value, min, max, variant, handler ids…)
  onPress?: number;             // handler id (assigned by host at reify time, not here)
  children?: NodeDesc[];
}
