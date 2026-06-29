#!/usr/bin/env python3
"""
Weaken ieee80211_raw_frame_sanity_check() in ESP-IDF's libnet80211.a so it can
be overridden, enabling raw TX of deauth/disassoc (0xC0 / 0xA0) frames.

The closed-source WiFi blob gates esp_wifi_80211_tx() behind an internal
ieee80211_raw_frame_sanity_check() that rejects deauth/disassoc management
frames ("unsupport frame type: 0c0"). The symbol is GLOBAL+strong, so simply
providing our own definition collides (multiple definition).

Instead of binary-patching the function body — which leaves dangling Xtensa
R_XTENSA_SLOT0_OP relocations the linker cannot decode ("dangerous relocation:
cannot decode instruction opcode") — we mark the blob's symbol WEAK with
objcopy. Our strong override in firmware/platforms/esp32/src/
esp32_wifi_raw_override.c then wins at link time, and esp_wifi_80211_tx()'s
symbol-referenced call binds to it. No instruction bytes are touched, so this is
robust across ESP-IDF / toolchain versions (no byte-pattern dependency).

Idempotent: if the symbol is already weak, it does nothing.
Reversible: the pristine library is backed up to <lib>.orig on first run.

Usage: patch_wifi_lib.py <libnet80211.a> <objcopy> <ar> <output.a>
"""
import sys, subprocess, shutil, tempfile, os

SYMBOL  = "ieee80211_raw_frame_sanity_check"
OBJNAME = "ieee80211_output.o"


def symbol_is_weak(nm, obj_path):
    """Return True if SYMBOL is already a weak global (nm code 'w'/'W')."""
    out = subprocess.check_output([nm, obj_path], text=True)
    for line in out.splitlines():
        parts = line.split()
        if len(parts) >= 3 and parts[2] == SYMBOL:
            return parts[1] in ("w", "W", "v", "V")
    return False


def main():
    if len(sys.argv) != 5:
        print(f"Usage: {sys.argv[0]} <libnet80211.a> <objcopy> <ar> <output.a>")
        sys.exit(1)

    lib_in, objcopy, ar, lib_out = sys.argv[1:5]
    nm = objcopy.replace("objcopy", "nm")

    with tempfile.TemporaryDirectory() as tmpdir:
        lib_work = os.path.join(tmpdir, "libnet80211.a")
        shutil.copy2(lib_in, lib_work)

        subprocess.check_call([ar, "x", lib_work, OBJNAME], cwd=tmpdir)
        obj_path = os.path.join(tmpdir, OBJNAME)

        if symbol_is_weak(nm, obj_path):
            print(f"{SYMBOL} already weak in {OBJNAME}, skipping")
            if os.path.abspath(lib_in) != os.path.abspath(lib_out):
                shutil.copy2(lib_in, lib_out)
            return

        # Back up the pristine library once so the change is reversible
        # (restore with: cp <lib>.orig <lib>).
        backup = lib_in + ".orig"
        if not os.path.exists(backup):
            shutil.copy2(lib_in, backup)
            print(f"Backed up original to {backup}")

        subprocess.check_call(
            [objcopy, f"--weaken-symbol={SYMBOL}", obj_path], cwd=tmpdir)
        subprocess.check_call([ar, "r", lib_work, OBJNAME], cwd=tmpdir)

        shutil.copy2(lib_work, lib_out)
        print(f"Weakened {SYMBOL} in {OBJNAME}; override now takes effect")


if __name__ == "__main__":
    main()
