#pragma once

namespace nema { class Runtime; }

namespace aether {

// Plan 80 — display bring-up entry point for a target main.
//
// Constructs the display servers (Aether 1-bit UI + the FbCon text console),
// configures Aether's presentational state (theme / canvas scale / FPS overlay)
// from config, registers both with the kernel's IDisplayServer registry, and
// starts the GUI render loop. The servers and the GuiService are owned here
// (function-static) for the whole process lifetime.
//
// Call once from the target main right after rt.start() and before installing
// apps (so AppRegistry capability checks can see the registered servers):
//
//     rt.start();
//     aether::bootDisplay(rt);
//     // … install apps, push home screen …
//
// Swapping display servers later = link a different server lib and call its
// bring-up instead; nema core never names a concrete server.
void bootDisplay(nema::Runtime& rt);

}  // namespace aether
