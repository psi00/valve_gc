#!/usr/bin/env python3
"""Re-pin the CS2 GameProfile constants in csgo_gc/game_profile.cpp.

Pulls the current offsets from a2x/cs2-dumper and the current game version from
SteamDatabase/GameTracking-CS2, then rewrites the CS2 block of game_profile.cpp
in place. requiredAppIdVersion is derived from the patch version (dots stripped,
which is how CS2 computes its build number). requiredAppIdVersion2 (the demo
header's build_num) only exists in PBDEMS2 demos, so pass --demo to refresh it.

Usage:
    python tools/update_cs2_pins.py                 # offsets + build stamp + version
    python tools/update_cs2_pins.py --demo x.dem    # also requiredAppIdVersion2
    python tools/update_cs2_pins.py --check         # exit 1 if pins are stale

Only the standard library is used.
"""

import argparse
import json
import re
import sys
import urllib.request
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
PROFILE = ROOT / "csgo_gc" / "game_profile.cpp"

DUMPER_OFFSETS = "https://raw.githubusercontent.com/a2x/cs2-dumper/main/output/offsets.json"
DUMPER_INFO = "https://raw.githubusercontent.com/a2x/cs2-dumper/main/output/info.json"
GAMETRACKING_STEAM_INF = (
    "https://raw.githubusercontent.com/SteamDatabase/GameTracking-CS2/master/game/csgo/steam.inf"
)

# GameProfile field -> (cs2-dumper module, offsets.json key)
OFFSET_FIELDS = {
    "localPlayerControllerOffset": ("client.dll", "dwLocalPlayerController"),
    "networkGameClientOffset": ("engine2.dll", "dwNetworkGameClient"),
    "networkGameClientLocalPlayer": ("engine2.dll", "dwNetworkGameClient_localPlayer"),
}

DEMO_MAGIC = b"PBDEMS2\x00"
# CDemoFileHeader (Protobufs/demo.proto): field 2 = patch_version (CS2's build
# number, the slot CS:GO called network_protocol), field 13 = build_num.
DEMO_FIELD_PROTOCOL = 2
DEMO_FIELD_BUILD_NUM = 13


def fetch(url):
    request = urllib.request.Request(url, headers={"User-Agent": "valve_gc-pin-updater"})
    with urllib.request.urlopen(request, timeout=30) as response:
        return response.read()


def fetch_json(url):
    return json.loads(fetch(url).decode("utf-8"))


def read_steam_inf():
    values = {}
    for line in fetch(GAMETRACKING_STEAM_INF).decode("utf-8").splitlines():
        line = line.strip()
        if not line or line.startswith("//") or "=" not in line:
            continue
        key, value = line.split("=", 1)
        values[key] = value
    return values


def read_varint(data, pos):
    result = 0
    shift = 0
    while True:
        if pos >= len(data):
            raise ValueError("truncated varint")
        byte = data[pos]
        pos += 1
        result |= (byte & 0x7F) << shift
        if not byte & 0x80:
            return result, pos
        shift += 7
        if shift > 63:
            raise ValueError("varint too long")


def parse_demo_header(path):
    """Return {field_number: value} for the CDemoFileHeader of a CS2 demo."""
    data = Path(path).read_bytes()
    if data[: len(DEMO_MAGIC)] != DEMO_MAGIC:
        raise ValueError(f"{path}: not a PBDEMS2 demo")

    # magic (8 bytes) + uint32 fileinfo offset, then the header protobuf.
    pos = 12
    fields = {}
    while pos < len(data):
        key, pos = read_varint(data, pos)
        field, wire = key >> 3, key & 7
        if field == 0:
            break
        if wire == 0:
            value, pos = read_varint(data, pos)
        elif wire == 1:
            value, pos = data[pos : pos + 8], pos + 8
        elif wire == 2:
            length, pos = read_varint(data, pos)
            value, pos = data[pos : pos + length], pos + length
        elif wire == 5:
            value, pos = data[pos : pos + 4], pos + 4
        else:
            break
        fields.setdefault(field, value)
    return fields


def sub_field(text, field, value):
    pattern = re.compile(r"(\." + field + r"\s*=\s*)0x[0-9A-Fa-f]+")
    if not pattern.search(text):
        raise SystemExit(f"game_profile.cpp: field {field} not found")
    new, count = pattern.subn(lambda m: f"{m.group(1)}0x{value:X}", text)
    return new


def sub_uint(text, field, value):
    pattern = re.compile(r"(\." + field + r"\s*=\s*)\d+")
    if not pattern.search(text):
        raise SystemExit(f"game_profile.cpp: field {field} not found")
    new, count = pattern.subn(lambda m: f"{m.group(1)}{value}", text)
    return new


def build_number(patch_version):
    """CS2's build number is the patch version with the dots stripped (1.41.9.3 -> 14193)."""
    digits = patch_version.replace(".", "")
    if not digits.isdigit():
        raise SystemExit(f"steam.inf PatchVersion {patch_version!r} is not numeric once dotted")
    return int(digits)


def main():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--demo", help="PBDEMS2 demo used to refresh requiredAppIdVersion2 (build_num)")
    parser.add_argument("--check", action="store_true", help="exit 1 if the pins are stale")
    parser.add_argument("--profile", default=str(PROFILE), help="path to game_profile.cpp")
    args = parser.parse_args()

    profile_path = Path(args.profile)
    raw = profile_path.read_bytes().decode("utf-8")
    original = raw

    block_match = re.search(r"const GameProfile g_profileCS2 = \{.*?\n\}\;", raw, re.S)
    if not block_match:
        raise SystemExit("game_profile.cpp: g_profileCS2 initializer not found")
    block = block_match.group(0)

    info = fetch_json(DUMPER_INFO)
    offsets = fetch_json(DUMPER_OFFSETS)
    steam_inf = read_steam_inf()

    stale = []
    for field, (module, key) in OFFSET_FIELDS.items():
        module_offsets = offsets.get(module, {})
        if key not in module_offsets:
            raise SystemExit(f"cs2-dumper has no {module}!{key}")
        value = module_offsets[key]
        current = re.search(r"\." + field + r"\s*=\s*0x([0-9A-Fa-f]+)", block)
        if not current or int(current.group(1), 16) != value:
            stale.append(f"{field}: 0x{value:X}")
        block = sub_field(block, field, value)

    build = build_number(steam_inf.get("PatchVersion", ""))
    current = re.search(r"\.requiredAppIdVersion\s*=\s*(\d+)", block)
    if not current or int(current.group(1)) != build:
        stale.append(f"requiredAppIdVersion: {build}")
    block = sub_uint(block, "requiredAppIdVersion", build)

    pinned = (
        f"// Pinned: build {info['build_number']} | ClientVersion {steam_inf.get('ClientVersion', '?')}"
        f" | PatchVersion {steam_inf.get('PatchVersion', '?')} | cs2-dumper {info['timestamp'][:10]}"
    )
    if not re.search(r"^// Pinned: [^\r\n]*", raw, re.MULTILINE):
        raise SystemExit("game_profile.cpp: '// Pinned:' line not found")
    old_pinned = re.search(r"^// Pinned: [^\r\n]*", raw, re.MULTILINE).group(0)
    if old_pinned != pinned:
        stale.append(pinned[len("// Pinned: ") :])
    raw = re.sub(r"^// Pinned: [^\r\n]*", pinned, raw, count=1, flags=re.MULTILINE)

    if args.demo:
        header = parse_demo_header(args.demo)
        if DEMO_FIELD_PROTOCOL not in header or DEMO_FIELD_BUILD_NUM not in header:
            raise SystemExit(f"{args.demo}: header has no field 2/13")
        if header[DEMO_FIELD_PROTOCOL] != build:
            print(
                f"note: {args.demo} has patch_version {header[DEMO_FIELD_PROTOCOL]},"
                f" steam.inf implies {build}; keeping the steam.inf value"
            )
        block = sub_uint(block, "requiredAppIdVersion2", header[DEMO_FIELD_BUILD_NUM])
        print(f"demo {args.demo}: build_num={header[DEMO_FIELD_BUILD_NUM]}")

    raw = raw[: block_match.start()] + block + raw[block_match.end() :]

    if raw == original:
        print(f"pins up to date ({pinned[len('// Pinned: '):]})")
        return 0

    if args.check:
        print("pins are stale:")
        for line in stale:
            print(f"  {line}")
        return 1

    profile_path.write_bytes(raw.encode("utf-8"))
    print(f"updated {profile_path}")
    print(f"  {pinned}")
    for line in stale:
        print(f"  {line}")
    if not args.demo:
        print("  requiredAppIdVersion2 untouched (pass --demo to refresh it)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
