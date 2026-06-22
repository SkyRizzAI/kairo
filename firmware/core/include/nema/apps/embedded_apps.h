// Built-in embedded JS apps — intentionally empty.
// Apps are installed dynamically via .papp.zip upload through Forge (Plan 86 Fase 6).
#pragma once
namespace nema {
struct EmbeddedApp { const char* id; const char* name; const char* js; };
inline const EmbeddedApp EMBEDDED_APPS[] = {};
inline constexpr int EMBEDDED_APP_COUNT = 0;
}
