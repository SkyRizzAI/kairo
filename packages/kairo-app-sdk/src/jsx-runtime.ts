// Automatic JSX runtime (tsconfig: jsx="react-jsx", jsxImportSource="kairo").
// TSX `<View x={1}>hi</View>` compiles to jsx(View, { x:1, children:"hi" }).
// We just package it into a KElement; expansion happens in renderToTree().
import type { KElement, ComponentFn, IntrinsicFn } from "./types";

export function jsx(type: IntrinsicFn | ComponentFn, props: Record<string, any>): KElement {
  return { $$kairo: true, type, props: props || {} };
}

// jsxs is the multi-child variant — identical shape for us.
export const jsxs = jsx;

// Dev runtime (Bun/TS emit jsxDEV in development); ignore the extra debug args.
export function jsxDEV(type: IntrinsicFn | ComponentFn, props: Record<string, any>): KElement {
  return jsx(type, props);
}

// Fragment groups children without a wrapper node.
export const Fragment: ComponentFn = (props: any) => props.children;
