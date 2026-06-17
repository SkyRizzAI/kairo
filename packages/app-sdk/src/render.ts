// renderToTree() — expand a KElement tree (with user components + hooks) into a
// serialisable NodeDesc tree the device bridge turns into native UiNodes. Host-
// side runtime (not bundled into the app — the app imports `nema` as external).
import type { KElement, NodeDesc } from "./types";
import { __beginRender } from "./hooks";

function isElement(x: any): x is KElement {
  return x != null && typeof x === "object" && x.$$nema === true;
}

// Expand any child value into 0..n NodeDescs.
function expand(node: any, out: NodeDesc[]): void {
  if (node == null || node === false || node === true) return;
  if (Array.isArray(node)) { for (const c of node) expand(c, out); return; }
  if (typeof node === "string" || typeof node === "number") {
    out.push({ type: "#text", text: String(node) });
    return;
  }
  if (!isElement(node)) return;

  const { type, props } = node;

  // Intrinsic (tagged) → emit a native node.
  if (typeof type === "function" && (type as any).__tag) {
    const tag = (type as any).__tag as string;
    const { children, style, ...rest } = props;
    const nd: NodeDesc = { type: tag };
    if (style) nd.style = style;
    const restKeys = Object.keys(rest);
    if (restKeys.length) nd.props = rest;

    const kids: NodeDesc[] = [];
    expand(children, kids);

    // Fold pure-text children of a Text node into nd.text (matches UiNode.text).
    if (tag === "Text" && kids.length && kids.every((k) => k.type === "#text")) {
      nd.text = kids.map((k) => k.text).join("");
    } else if (kids.length) {
      nd.children = kids;
    }
    out.push(nd);
    return;
  }

  // User component → call it (hooks run here), expand its result.
  expand((type as Function)(props), out);
}

export function renderToTree(
  app: () => KElement | null,
  scheduleRerender: () => void = () => {},
): NodeDesc | null {
  __beginRender(scheduleRerender);
  const out: NodeDesc[] = [];
  expand(app(), out);
  if (out.length === 0) return null;
  if (out.length === 1) return out[0];
  return { type: "View", children: out };   // wrap multiple roots
}
