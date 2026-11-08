#!/usr/bin/env python3
"""Reads a "Save instrument details" file from VSL Finesse.

On a track with Synchron Player loaded, the instrument menu's "Save
instrument details to Desktop" writes a JSON file. Its "state" field is
Synchron Player's saved state: JUCE's own base64 encoding of a binary
plist, whose PluginState entry is Zstandard-compressed JSON. This script
unpacks it (see docs/DEVELOPMENT.md, "Synchron Player's saved state").

    scripts/synchron-state.py details.json            the articulation tree and controls, as VSL Finesse sees them
    scripts/synchron-state.py details.json --json     the whole state as JSON

Needs Python 3.9+ and Zstandard: Python 3.14's compression.zstd, or
`pip install zstandard`. Note names follow Logic (C3 = middle C), like the
app; Synchron Player shows them an octave higher.
"""

import json
import plistlib
import sys

JUCE_BASE64 = ".ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+"
NOTE_NAMES = ["C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"]


def decompress(data):
    try:
        from compression import zstd  # Python 3.14+
        return zstd.decompress(data)
    except ImportError:
        pass
    try:
        import zstandard
    except ImportError:
        sys.exit("Zstandard is missing: pip install zstandard (or use Python 3.14+)")
    return zstandard.ZstdDecompressor().decompressobj().decompress(data)


def juce_base64_decode(text):
    """juce::MemoryBlock::toBase64Encoding(): "<size>.<6 bits per character, least significant first>"."""
    size, _, encoded = text.partition(".")
    value = 0
    for i, c in enumerate(encoded):
        value |= JUCE_BASE64.index(c) << (6 * i)
    return value.to_bytes((len(encoded) * 6 + 7) // 8, "little")[: int(size)]


def note_name(pitch):
    return f"{NOTE_NAMES[pitch % 12]}{pitch // 12 - 2}"


def is_white(pitch):
    return pitch % 12 in (0, 2, 4, 5, 7, 9, 11)


def child_key(node, first, index):
    if not node.get("whiteNoteSwitch"):
        return first + index
    key = first
    while not is_white(key):
        key += 1
    for _ in range(index):
        key += 1
        while not is_white(key):
            key += 1
    return key


def describe_switch(node, mappings):
    control = int(node.get("switchControl", 0))
    if 1 <= control <= 128:
        return f"keyswitch from {note_name(control - 1)}" + (" (white keys)" if node.get("whiteNoteSwitch") else "")
    if 129 <= control <= 256:
        return f"CC{control - 129}"
    if control >= 257:
        slot = control - 257
        source = mappings.get(f"nodeXFade{slot}", {}).get("source", "?")
        kind = "parallel" if node.get("parallel") else "switched"
        return f"Dim.Ctrl/{chr(ord('A') + slot)} ({source}), {kind}"
    return "nothing"


def print_tree(node, mappings, depth=0, key=None):
    indent = "  " * depth
    label = f"{note_name(key)}  " if key is not None else ""
    params = node.get("parameters", {})
    children = node.get("nodes") or []
    if children:
        title = node.get("title") or "(root)"
        print(f"{indent}{label}{title}  [{node.get('subTitle', '')}: {describe_switch(node, mappings)}, "
              f"selected {node.get('currentNodeIndex')}]")
        control = int(node.get("switchControl", 0))
        for i, child in enumerate(children):
            child_key_pitch = child_key(node, control - 1, i) if 1 <= control <= 128 else None
            print_tree(child, mappings, depth + 1, child_key_pitch)
    else:
        patch = node.get("patchName") or "(empty)"
        flags = []
        if params.get("velXFadeMode") == 1:
            flags.append("follows velocity")
        if params.get("nodeXFadeMode") == 1:
            flags.append("Mix")
        print(f"{indent}{label}{node.get('title')}  <{patch}>" + (f"  ({', '.join(flags)})" if flags else ""))


def main():
    if len(sys.argv) < 2 or sys.argv[1] in ("-h", "--help"):
        print(__doc__)
        return
    with open(sys.argv[1], encoding="utf-8") as f:
        details = json.load(f)
    plist = plistlib.loads(juce_base64_decode(details["state"]))
    if "PluginState" not in plist:
        sys.exit("This state has no PluginState: not Synchron Player's Audio Unit state.")
    state = json.loads(decompress(plist["PluginState"]))

    if "--json" in sys.argv[2:]:
        print(json.dumps(state, indent=2, sort_keys=True))
        return

    data = state["data"]
    custom = data.get("custom", {})
    mappings = custom.get("paramControl", {}).get("mappings", {})
    parameters = data.get("parameters", {})
    print(f"Preset: {data.get('currentPreset') or '(none)'}")
    print(f"View:   {data.get('meta', {}).get('gui', {}).get('currentSkin')}")
    print(f"Dynamic layers: {'crossfade with ' + mappings.get('velXFade', {}).get('source', '?') if parameters.get('velXFadeEnabled') else 'velocity'}")
    print()
    print("Controls (source, value 0-1):")
    for name in ["velXFade", "expression", "nodeXFade0", "nodeXFade1", "attack", "release", "legatoBlur"]:
        if name in mappings or name in parameters:
            print(f"  {name:12} {mappings.get(name, {}).get('source', '-'):10} {parameters.get(name, '-')}")
    print()
    print("Articulation tree:")
    print_tree(custom.get("sampler", {}).get("rootNode", {}), mappings, 1)


if __name__ == "__main__":
    main()
