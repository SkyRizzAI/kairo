// Kairo UI components — React mirror of native C++ components.
// Renders in the simulator preview canvas using the same design language:
// 1-bit, 264×176, pixel font 5×8, retro e-ink aesthetic.
//
// Usage: import { Label, Button, Row, Col, MenuItem, HRule } from "@kairo/ui"
// (from simulator context; for real JS apps on device, same API via kairo.display.*)

export { Label }    from "./Label";
export { Button }   from "./Button";
export { Row }      from "./Row";
export { Col }      from "./Col";
export { MenuItem } from "./MenuItem";
export { HRule }    from "./HRule";
export type { Size, StyleProps } from "./types";
