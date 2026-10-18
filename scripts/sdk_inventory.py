#!/usr/bin/env python3
"""Inventory of the hardware-facing and SDK code a game executes, for the runtime HLE (issue #14).

Reads main.dol and a function map (the committed MAP.txt is enough) and writes a Markdown report:

1. Address bands: where the game, the SDK and the C library sit in the DOL.
2. Functions that touch memory-mapped hardware, per device. With the runtime's native hooks off
   these run as translated guest code, so each one needs an HLE, an emulated register, or a port.
3. The SDK entry points game code calls, ranked by number of call sites.
4. Which of them are already located in docs/nsmbw/hle-hooks.md or hooked by the runtime.
5. With --hooks, the coverage of the runtime's Mario Kart Wii hooks by this game's profile.

Addresses and counts only, never game bytes.

Usage:
  sdk_inventory.py main.dol MAP.txt docs/nsmbw/hle-hooks.md --hooks runtime/src/hle > docs/nsmbw/sdk-inventory.md
  sdk_inventory.py main.dol MAP.txt docs/nsmbw/hle-hooks.md --names named.txt > local.md

--hooks reads the runtime's hook registrations and adds the coverage section: which Mario Kart Wii hooks
have an SMNE01 registration, and why each of the others is not ported (NOT_PORTED below).

--names takes a map carrying names (see scripts/port_symbols.py --with-names) and adds them to the
report. Use it for your own reading only: the community's names are not committed (issue #6).
"""
import bisect
import collections
import re
import struct
import sys
from pathlib import Path

# Approximate: set from the boundaries of named game classes (nw4r snd, nw4r ut) and the first and last
# functions that touch SDK state. Correct them here if a band is wrong; the report follows.
BANDS = [
    ("startup (crt0, init)", 0x80004000, 0x80006780),
    ("game and engine code", 0x80006780, 0x8019F000),
    ("SDK: OS, VI, GX, DVD, AX, NAND, WPAD, KPAD", 0x8019F000, 0x80229000),
    ("engine libraries (nw4r, EGG)", 0x80229000, 0x802DC000),
    ("C library and compiler runtime", 0x802DC000, 0x802EA000),
]
SDK_BAND = 2

DEVICES = [
    (0xCC000000, 0xCC000FFF, "CP (command processor)"), (0xCC001000, 0xCC001FFF, "PE (pixel engine)"),
    (0xCC002000, 0xCC002FFF, "VI (video interface)"), (0xCC003000, 0xCC003FFF, "PI (processor interface)"),
    (0xCC004000, 0xCC004FFF, "MI (memory interface)"), (0xCC005000, 0xCC005FFF, "DSP"),
    (0xCC006000, 0xCC0063FF, "DI (disc)"), (0xCC006400, 0xCC0067FF, "SI (serial)"),
    (0xCC006800, 0xCC006BFF, "EXI"), (0xCC006C00, 0xCC006FFF, "AI (audio)"),
    (0xCC008000, 0xCC008FFF, "GX write-gather FIFO"),
    (0xCD000000, 0xCD0000FF, "IPC (IOS)"), (0xCD800000, 0xCD800FFF, "Hollywood registers"),
    (0xCD806000, 0xCD8067FF, "EXI/SI (Wii)"),
]
LIS_MMIO = {0xCC00, 0xCC01, 0xCC02, 0xCD00, 0xCD01, 0xCD80, 0xCD81}

# Runtime hooks registered at Mario Kart Wii addresses that have no SMNE01 registration, keyed by the
# implementation's C++ name, with why. Categories:
#   mkw   - Mario Kart Wii game code or a library this game does not link.
#   lower - covered because a lower layer is hooked here (the NAND library runs translated on top of the
#           hooked IOS calls, DVDLow under the hooked DVD reads), or an inner label of a hooked function.
#   trans - runs as translated code here with no visible problem so far, or is not reached (see the note).
#   port  - needed and not located yet (filed as issues).
# A hook missing from this table is reported as unclassified.
NOT_PORTED = {
    "AIGetDSPSampleRate_8012409c": ("port", "audio sample rate"),
    "SoundPlayerSetVolume_800a35e0": ("mkw", "game code"),
    "AXFXReverbStdExpCallback_8012b830": ("port", "reverb effect callback"),
    "ConsoleWrite_HLE_80015500": ("trans", "debug console output"),
    "EGG_Decomp_decodeSZS_80218c2c": ("trans", "speed-up only; the translated decoder works"),
    "ESP_InitLib_stub": ("lower", "/dev/es ioctls are answered"),
    "ESP_CloseLib_stub": ("lower", "/dev/es ioctls are answered"),
    "ESP_GetTitleId_stub": ("lower", "/dev/es ioctls are answered"),
    "ESP_GetDataDir_stub": ("lower", "/dev/es ioctls are answered"),
    "nw4r__lyt__detail__DrawQuad_800847c0": ("trans", "Mario Kart Wii's inlined copy"),
    "nw4r__lyt__detail__DrawQuad_80084d20": ("trans", "Mario Kart Wii's inlined copy"),
    "EGG__DrawGX__SetVtxState_HLE": ("trans", "Mario Kart Wii's EGG build"),
    "EGG__LightTexture__SetupTevFinish_HLE_8022e2bc": ("trans", "Mario Kart Wii's EGG build"),
    "EGG__AsyncDisplay__endRender_HLE_8020ff9c": ("trans", "Mario Kart Wii's EGG build"),
    "GX__SetDrawSync_8016e9fc": ("trans", "GX state setter"),
    "GX__InitTexObj_switch_80170938": ("lower", "label inside the hooked GXInitTexObj"),
    "GX__InitTexObj_caseD_0_8017093c": ("lower", "label inside the hooked GXInitTexObj"),
    "GX__InitTexObj_caseD_1_80170950": ("lower", "label inside the hooked GXInitTexObj"),
    "GX__InitTexObj_caseD_3_80170964": ("lower", "label inside the hooked GXInitTexObj"),
    "GX__InitTexObj_caseD_6_80170978": ("lower", "label inside the hooked GXInitTexObj"),
    "GX__InitTexObj_caseD_e_8017098c": ("lower", "label inside the hooked GXInitTexObj"),
    "GX__InitTexObj_caseD_7_801709a0": ("lower", "label inside the hooked GXInitTexObj"),
    "GX__InitTexObjLODBias_80170b94": ("mkw", "not in this SDK build"),
    "GX__SetZScaleOffset_80173400": ("trans", "GX state setter"),
    "GX__SetArray_8016e1c4": ("lower", "switch artifact; GXSetArray itself is hooked"),
    "GX__End_80044b30": ("mkw", "inlined in game code"),
    "GX__End_80048c30": ("mkw", "inlined in game code"),
    "KPAD__GetUnifiedWpadStatus_HLE": ("port", "unified controller status"),
    "PAD__Init_HLE": ("trans", "GameCube pad; not reached by this game so far"),
    "PAD__Read_HLE": ("trans", "GameCube pad; not reached by this game so far"),
    "PAD__Reset_HLE": ("trans", "GameCube pad; not reached by this game so far"),
    "PAD__Recalibrate_HLE": ("trans", "GameCube pad; not reached by this game so far"),
    "PAD__ControlMotor_HLE": ("trans", "GameCube pad; not reached by this game so far"),
    "WPADGetDpdSensitivity_HLE": ("port", "pointer sensitivity"),
    "WPADInitSub_HLE": ("trans", "remote set-up; input works"),
    "WUDGetStatus_HLE": ("trans", "Bluetooth status; input works"),
    "NHTTPStartup_Reentrant_HLE_801d8d30": ("mkw", "NHTTP is not linked"),
    "RFLiIsWorking_HLE_800bd860": ("mkw", "RFL is not linked"),
    "OSInitAlarm_RecompModLateInit_801a961c": ("mkw", "mod loader hook"),
    "StaticRProlog_RecompModInit_8055531c": ("mkw", "mod loader hook"),
    "OSFatal_HLE_801a4ec4": ("port", "fatal error screen"),
    "GKI_delay_HLE_801301b4": ("trans", "Bluetooth stack; input is answered at WPAD level"),
    "BTM_IsDeviceUp_HLE_8013a300": ("trans", "Bluetooth stack; input is answered at WPAD level"),
    "OS__GetConsoleType_8019f33c": ("trans", "start-up"),
    "OS____PSInit_801a04a0": ("trans", "start-up"),
    "__init_hardware_80006348": ("trans", "start-up"),
    "OS__ExceptionInit_801a00e0": ("trans", "start-up"),
    "OSSetPowerCallback_801ab75c": ("trans", "power button callback"),
    "IPCWriteReg_80193020": ("lower", "IOS requests are hooked above the IPC registers"),
    "IPCReadReg_80193010": ("lower", "IOS requests are hooked above the IPC registers"),
    "OS__JamMessage_HLE_801a7500": ("trans", "message queue; send and receive are hooked"),
    "OS__Report_801a25d0": ("port", "debug report output"),
    "PPCHalt_8012E5A4": ("port", "halt after a fatal error"),
    "OS__Panic_801A2660_Cpu": ("port", "panic"),
    "OSResetSystem": ("port", "reset and return to the Wii Menu"),
    "Exit_801AE58C": ("port", "exit"),
    "OS__SleepTicks_HLE_801aaca8": ("trans", "inlined in this build"),
    "OS__GetTime_HLE": ("trans", "time base read by translated mftb"),
    "OS____GetSystemTime_801aad7c": ("trans", "time base read by translated mftb"),
    "SCCheckStatus_HLE": ("lower", "replaced by SCCheckStatus_SMNE01, which runs the original"),
    "SCGetEuRgb60Mode_HLE": ("trans", "the SC library reads SYSCONF"),
    "SCGetProductArea_HLE": ("trans", "the SC library reads setting.txt"),
    "SCGetProductCode_HLE": ("trans", "the SC library reads setting.txt"),
    "SCGetProductSN_HLE": ("trans", "the SC library reads setting.txt"),
    "SCGetProductGameRegion_HLE": ("trans", "the SC library reads setting.txt"),
    "OS__IsTitleInstalled": ("mkw", "channel install check"),
    "OS__CheckInstall": ("mkw", "channel install check"),
    "DVDLowReadDiskID_80164AAC": ("lower", "disc reads are hooked at the DVD level"),
    "DVDLowRead_80166330": ("lower", "disc reads are hooked at the DVD level"),
    "DVDLowUnencryptedRead_80165708": ("lower", "disc reads are hooked at the DVD level"),
    "NAND_IOS_OpenBody_HLE_801938FC": ("lower", "inner label of IOS_Open"),
    "ISFS_OpenLib_HLE_80169BCC": ("lower", "runs translated on top of the hooked IOS calls"),
    "StrapScene__CheckInput_Skip": ("trans", "stand-in in vi.cpp after 30 frames"),
    "TaskThread_run_HLE_80242d7c": ("mkw", "Mario Kart Wii's EGG build"),
    "Stub_8002001C": ("mkw", "debugger stub"),
    "VIInit_caseD_0_HLE_801b9934": ("lower", "label inside the hooked VIInit"),
    "VIInit_caseD_1_HLE_801b993c": ("lower", "label inside the hooked VIInit"),
    "VIInit_caseD_2_HLE_801b9944": ("lower", "label inside the hooked VIInit"),
}
# Every NAND library entry point: the library runs translated and reaches the hooked IOS calls.
NAND_LIBRARY_FILES = ("storage/nand_api.cpp", "storage/nand_async.cpp")
CATEGORY_NAMES = {
    "mkw": "Mario Kart Wii only",
    "lower": "Covered by a lower layer",
    "trans": "Runs translated (no hook needed so far)",
    "port": "To port (not located)",
    "unclassified": "Unclassified (add it to NOT_PORTED)",
}

HOOK_PATTERN = re.compile(
    r"(?:PPC_NATIVE_OVERRIDE(?:_VOID)?)\(\s*(?:0x)?([0-9A-Fa-f]{8})u?\s*,\s*(\w+)"
    r"|(?:REGISTER_NATIVE_FUNCTION(?:_AS)?|REGISTER_TRANSLATED_FUNCTION)\(\s*0x([0-9A-Fa-f]{8})u?\s*,\s*(\w+)")


def runtime_hooks(root):
    """(address, implementation, file) for every hook registration under runtime/src/hle, split into
    the shared (Mario Kart Wii) ones and this game's profile."""
    shared, profile = [], []
    root = Path(root)
    for path in sorted(root.rglob("*.cpp")) + sorted(root.rglob("*.h")):
        relative = path.relative_to(root).as_posix()
        for m in HOOK_PATTERN.finditer(path.read_text(encoding="utf-8", errors="replace")):
            address = int(m.group(1) or m.group(3), 16)
            name = m.group(2) or m.group(4)
            (profile if relative.startswith("profiles/smne01/") else shared).append((address, name, relative))
    return shared, profile


def hook_coverage(w, root):
    shared, profile = runtime_hooks(root)
    ported_names = {name for _a, name, _f in profile}
    w("## Runtime hook coverage\n")
    w(f"The runtime registers {len(shared)} hooks at Mario Kart Wii addresses (all off for this game: "
      f"`runtime.native_hooks: false`) and {len(profile)} at SMNE01 addresses in "
      "`runtime/src/hle/profiles/smne01`. A shared hook counts as ported when the profile registers the "
      "same implementation. `gx/gx_fatal_stubs.cpp` holds stops for GX functions Mario Kart Wii never "
      "calls; here those functions run translated, so they are not ported.\n")
    per_file = collections.defaultdict(lambda: [0, 0, 0])
    missing = []
    for address, name, relative in shared:
        entry = per_file[relative]
        entry[0] += 1
        if name in ported_names:
            entry[1] += 1
        elif relative == "gx/gx_fatal_stubs.cpp":
            entry[2] += 1
        else:
            if relative in NAND_LIBRARY_FILES:
                category, note = "lower", "the NAND library runs translated on top of the hooked IOS calls"
            elif relative == "os/os_init.cpp" and name.startswith("PPC"):
                category, note = "trans", "processor register accessor; the translator handles the instruction"
            else:
                category, note = NOT_PORTED.get(name, ("unclassified", ""))
            missing.append((category, relative, address, name, note))
    w("| Source file | Shared hooks | Ported | GX stops | Not ported |")
    w("| --- | --- | --- | --- | --- |")
    for relative, (count, ported, stops) in sorted(per_file.items()):
        w(f"| `{relative}` | {count} | {ported} | {stops} | {count - ported - stops} |")
    totals = [sum(v[i] for v in per_file.values()) for i in range(3)]
    w(f"| **Total** | {totals[0]} | {totals[1]} | {totals[2]} | {totals[0] - totals[1] - totals[2]} |")
    w("")
    w("Hooks not ported, by reason (from `NOT_PORTED` in the script):\n")
    w("| Reason | Hooks |")
    w("| --- | --- |")
    counts = collections.Counter(category for category, *_rest in missing)
    for category in CATEGORY_NAMES:
        if counts[category]:
            w(f"| {CATEGORY_NAMES[category]} | {counts[category]} |")
    w("")
    for category in ("port", "unclassified", "trans", "lower", "mkw"):
        rows = [row for row in missing if row[0] == category]
        if not rows:
            continue
        w(f"### {CATEGORY_NAMES[category]}\n")
        w("| Implementation | Mario Kart Wii address | Source file | Note |")
        w("| --- | --- | --- | --- |")
        for _c, relative, address, name, note in sorted(rows, key=lambda r: (r[1], r[3])):
            w(f"| `{name}` | `0x{address:08X}` | `{relative}` | {note} |")
        w("")
    extra = sorted(name for name in ported_names if name not in {n for _a, n, _f in shared})
    w(f"Hooks written for this game only ({len(extra)}): " + ", ".join(f"`{n}`" for n in extra) + ".\n")
LOAD_STORE = set(range(32, 56))


def u32(b, o): return struct.unpack_from(">I", b, o)[0]
def sx(v): return v - 0x10000 if v & 0x8000 else v


class Dol:
    def __init__(self, data):
        self.data = data
        self.text = [(u32(data, i * 4), u32(data, 0x48 + i * 4), u32(data, 0x90 + i * 4)) for i in range(7)]
        self.text = [t for t in self.text if t[2]]

    def words(self):
        for off, addr, size in self.text:
            for i in range(size // 4):
                yield addr + i * 4, u32(self.data, off + i * 4)


def device(ea):
    for lo, hi, name in DEVICES:
        if lo <= ea <= hi:
            return name
    return None


def band(address):
    for i, (_n, lo, hi) in enumerate(BANDS):
        if lo <= address < hi:
            return i
    return None


def hle_table(path):
    """Rows of docs/nsmbw/hle-hooks.md: SMNE01 address -> SDK function name."""
    names = {}
    for line in Path(path).read_text(encoding="utf-8").splitlines():
        cells = [c.strip() for c in line.strip().strip("|").split("|")]
        if len(cells) >= 4 and re.fullmatch(r"`0x[0-9A-Fa-f]{8}`", cells[2]):
            name = re.sub(r"\s*\(.*", "", cells[0]).strip("` ")
            names[int(cells[2].strip("`"), 16)] = name
    return names


def read_names(path):
    names = {}
    for line in Path(path).read_text(encoding="utf-8").splitlines():
        m = re.match(r"([0-9A-Fa-f]{8})\s+(\S+)", line)
        if m and not m.group(2).startswith("sub_"):
            names[int(m.group(1), 16)] = m.group(2)
    return names


def main():
    args = sys.argv[1:]
    local_names = {}
    if "--names" in args:
        i = args.index("--names")
        local_names = read_names(args[i + 1])
        del args[i:i + 2]
    hooks_root = None
    if "--hooks" in args:
        i = args.index("--hooks")
        hooks_root = args[i + 1]
        del args[i:i + 2]
    if len(args) != 3:
        sys.exit(__doc__)
    sys.argv = [sys.argv[0]] + args
    dol = Dol(Path(sys.argv[1]).read_bytes())
    starts = sorted(int(m.group(1), 16) for line in Path(sys.argv[2]).read_text(encoding="utf-8").splitlines()
                    if (m := re.match(r"([0-9A-Fa-f]{8})\s", line)))
    startset = set(starts)
    known = hle_table(sys.argv[3])
    located_count = len(known)
    for address, name in local_names.items():
        known.setdefault(address, name)
    if hooks_root:
        # The runtime's own implementation names (not the community's) for the functions it replaces.
        for address, name, _relative in runtime_hooks(hooks_root)[1]:
            known.setdefault(address, f"hook: {name}")

    def owner(address):
        return starts[bisect.bisect_right(starts, address) - 1]

    # Direct calls (bl) and tail calls (b to another function's first instruction).
    calls = collections.defaultdict(list)  # callee -> caller functions
    mmio = collections.defaultdict(set)    # function -> devices
    words = list(dol.words())
    index = {a: i for i, (a, _w) in enumerate(words)}
    for i, (address, word) in enumerate(words):
        opcode = word >> 26
        if opcode == 18:
            li = word & 0x03FFFFFC
            li -= 0x04000000 if li & 0x02000000 else 0
            target = (li if word & 2 else address + li) & 0xFFFFFFFF
            if target in startset and (word & 1 or owner(address) != target):
                calls[target].append(owner(address))
        elif opcode == 15 and (word & 0xFFFF) in LIS_MMIO:
            register, base = (word >> 21) & 31, (word & 0xFFFF) << 16
            for j in range(1, 12):
                if i + j >= len(words):
                    break
                nxt = words[i + j][1]
                if (nxt >> 16) & 31 == register and nxt >> 26 in LOAD_STORE | {14}:
                    dev = device((base + sx(nxt & 0xFFFF)) & 0xFFFFFFFF)
                    if dev:
                        mmio[owner(address)].add(dev)
                if (nxt >> 21) & 31 == register and nxt >> 26 in (14, 15, 24, 32):
                    break

    out = []
    w = out.append
    w("# SDK and hardware inventory (SMNE01 rev 2)\n")
    w("Generated by `scripts/sdk_inventory.py` from `main.dol` and `projects/nsmbw-usa/MAP.txt`. "
      "Addresses and counts only. Direct calls only: calls through function pointers and vtables are "
      "not in the counts, so every number is a lower bound.\n")

    w("## Address bands\n")
    w("Approximate: the boundaries come from the named game classes on either side. They are set in "
      "`BANDS` in the script.\n")
    w("| Band | Range | Function starts |")
    w("| --- | --- | --- |")
    for name, lo, hi in BANDS:
        n = sum(1 for s in starts if lo <= s < hi)
        w(f"| {name} | `0x{lo:08X}` - `0x{hi:08X}` | {n:,} |")
    w("")

    per_device = collections.defaultdict(list)
    for f, ds in mmio.items():
        for d in ds:
            per_device[d].append(f)
    w("## Functions that touch hardware registers\n")
    w(f"{len(mmio)} functions reference a memory-mapped hardware register. Running as translated "
      "code, each one reads or writes a host address the runtime has to answer. `In SDK` is the number "
      "inside the SDK band; the rest sit in game or engine code, which writes the GX FIFO inline.\n")
    w("| Device | Functions | In SDK band | Lowest | Highest |")
    w("| --- | --- | --- | --- | --- |")
    for dev, fs in sorted(per_device.items(), key=lambda x: -len(x[1])):
        fs = sorted(fs)
        in_sdk = sum(1 for f in fs if band(f) == SDK_BAND)
        w(f"| {dev} | {len(fs)} | {in_sdk} | `0x{fs[0]:08X}` | `0x{fs[-1]:08X}` |")
    w("")
    outside = sorted(f for f, ds in mmio.items() if band(f) != SDK_BAND)
    w(f"Outside the SDK band, {len(outside)} functions touch hardware directly "
      f"(`GX write-gather FIFO` in almost every case). They are inlined GX calls in game and engine code: "
      "the runtime has to answer FIFO writes wherever they come from, not only inside `GX*` functions.\n")

    api = {t: callers for t, callers in calls.items() if band(t) == SDK_BAND}
    used = {t: [c for c in cs if band(c) != SDK_BAND] for t, cs in api.items()}
    used = {t: cs for t, cs in used.items() if cs}
    sdk_total = sum(1 for s in starts if band(s) == SDK_BAND)
    w("## SDK entry points called from game code\n")
    w(f"The SDK band holds {sdk_total:,} function starts. Game and engine code calls {len(used):,} of them "
      "directly; the rest are reached from inside the SDK, through pointers, or not at all.\n")
    w("| SDK function | Direct call sites | Distinct callers | Touches | Known name |")
    w("| --- | --- | --- | --- | --- |")
    for target, callers in sorted(used.items(), key=lambda x: -len(x[1]))[:60]:
        touches = ", ".join(sorted(mmio.get(target, []))) or ""
        name = known.get(target, "")
        w(f"| `0x{target:08X}` | {len(callers)} | {len(set(callers))} | {touches} | {name} |")
    w("")

    if hooks_root:
        hooked = {address for address, _n, _f in runtime_hooks(hooks_root)[1]}
        replaced = sum(1 for target in used if target in hooked)
        w(f"Of the {len(used):,} SDK functions game code calls directly, {replaced} are replaced by a runtime "
          "hook for this game; the others run as translated code.\n")
        hook_coverage(w, hooks_root)
    print("\n".join(out))


if __name__ == "__main__":
    main()
