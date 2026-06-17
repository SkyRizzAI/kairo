#!/usr/bin/env bun
// Phase-0 verification: render the counter app's TSX through the SDK runtime and
// assert the produced node-desc tree matches what the device bridge expects.
// (Validates jsx-runtime + components + hooks + renderToTree end-to-end.)
import App from "../templates/counter/App";
import { renderToTree } from "../src/render";

let fail = 0;
const check = (cond: boolean, msg: string) =>
  cond ? console.log(`  ok:   ${msg}`) : (console.log(`  FAIL: ${msg}`), fail++);

const tree = renderToTree(App as any);
console.log(JSON.stringify(tree, null, 2));

check(!!tree && tree.type === "View", "root is a View");
const kids = tree?.children ?? [];
const title = kids[0];
check(title?.type === "Text" && /Count: 0/.test(title?.text ?? ""), 'title Text folds "Count: 0"');
const row = kids[1];
check(row?.type === "View" && row?.style?.flexDirection === "row", "Row → View(row)");
const minus = row?.children?.[0];
check(minus?.type === "Pressable", "row has a Pressable (-)");
check("onPress" in (minus?.props ?? {}), "Pressable carries onPress (handler bound at reify)");
const reset = kids[2];
check(reset?.type === "Pressable" && reset?.children?.[0]?.text === "Reset", "Reset Pressable with label");

console.log(fail === 0 ? "== ALL PASS ==" : `== ${fail} FAILURES ==`);
process.exit(fail === 0 ? 0 : 1);
