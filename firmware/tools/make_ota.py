#!/usr/bin/env python3
"""Wrap an ESP-IDF app .bin into a Zigbee OTA Upgrade image (.ota).

The output is a standard Zigbee OTA file (OTA header + one upgrade-image
sub-element) that any compliant coordinator (zigbee2mqtt, ZHA, deCONZ) can serve.

By default the version, manufacturer code and image type are read straight from
the firmware headers (main/version.h, main/ota_driver.h), so a release is just:

  idf.py build
  python tools/make_ota.py --in build/light_switch.bin --out dimmer.ota

Pass --version / --manuf / --image-type only to override. The script prints the
fileVersion/fileSize/sha512 you need for the zigbee2mqtt override index.
"""
import argparse
import hashlib
import json
import os
import re
import struct

OTA_FILE_IDENTIFIER = 0x0BEEF11E  # Zigbee OTA magic
OTA_HEADER_VERSION  = 0x0100
ZIGBEE_STACK_PRO    = 0x0002
TAG_UPGRADE_IMAGE   = 0x0000
HEADER_LEN          = 56  # no optional header fields

MAIN_DIR = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "main")

# Where published .ota images are served from (raw GitHub). Used to build the
# index `url` when --base-url is not given.
DEFAULT_BASE_URL = ("https://raw.githubusercontent.com/dagroe/"
                    "zigbee-rotary-dimmer-switch/main/firmware/z2m/images")


def auto_int(x):
    return int(x, 0)


def _read(path):
    try:
        with open(path) as f:
            return f.read()
    except OSError:
        return ""


def version_from_headers():
    """FW_VERSION_OTA computed from main/version.h, or None if unparsable."""
    h = _read(os.path.join(MAIN_DIR, "version.h"))

    def num(name):
        m = re.search(name + r"\s+(\d+)", h)
        return int(m.group(1)) if m else None

    major, minor, patch = num("FW_VERSION_MAJOR"), num("FW_VERSION_MINOR"), num("FW_VERSION_PATCH")
    if None in (major, minor, patch):
        return None
    return (major << 24) | (minor << 16) | patch


def define_from_ota_header(name):
    """Hex #define value from main/ota_driver.h, or None."""
    m = re.search(name + r"\s+(0x[0-9A-Fa-f]+)", _read(os.path.join(MAIN_DIR, "ota_driver.h")))
    return int(m.group(1), 0) if m else None


def resolve(value, fallback, label):
    if value is not None:
        return value
    if fallback is not None:
        return fallback
    raise SystemExit(f"error: --{label} not given and could not be read from the headers")


def main():
    p = argparse.ArgumentParser(description=__doc__,
                                formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument("--in", dest="infile", required=True, help="input app .bin")
    p.add_argument("--out", dest="outfile", required=True, help="output .ota")
    p.add_argument("--manuf", type=auto_int, default=None, help="override manufacturer code")
    p.add_argument("--image-type", type=auto_int, default=None, help="override image type")
    p.add_argument("--version", type=auto_int, default=None, help="override file version")
    p.add_argument("--header-string", default="DG Dimmer OTA", help="<=32 char label")
    p.add_argument("--index", default=None,
                   help="OTA index JSON to add/replace this image's entry in (for z2m auto-discovery)")
    p.add_argument("--base-url", default=DEFAULT_BASE_URL,
                   help="base URL the .ota is served from; index url = base-url + output filename")
    args = p.parse_args()

    manuf   = resolve(args.manuf, define_from_ota_header("OTA_UPGRADE_MANUFACTURER"), "manuf")
    itype   = resolve(args.image_type, define_from_ota_header("OTA_UPGRADE_IMAGE_TYPE"), "image-type")
    version = resolve(args.version, version_from_headers(), "version")

    with open(args.infile, "rb") as f:
        fw = f.read()

    header_string = args.header_string.encode("ascii")[:32].ljust(32, b"\x00")
    sub_element = struct.pack("<HI", TAG_UPGRADE_IMAGE, len(fw)) + fw
    total_size = HEADER_LEN + len(sub_element)

    header = struct.pack(
        "<IHHHHHIH32sI",
        OTA_FILE_IDENTIFIER,
        OTA_HEADER_VERSION,
        HEADER_LEN,
        0x0000,            # field control: no optional fields
        manuf,
        itype,
        version,
        ZIGBEE_STACK_PRO,
        header_string,
        total_size,
    )
    assert len(header) == HEADER_LEN, len(header)

    blob = header + sub_element
    with open(args.outfile, "wb") as f:
        f.write(blob)

    sha512 = hashlib.sha512(blob).hexdigest()
    print(f"Wrote {args.outfile}")
    print(f"  app bytes   : {len(fw)}")
    print(f"  fileVersion : {version}        (0x{version:08X})")
    print(f"  manufactCode: {manuf}            (0x{manuf:04X})")
    print(f"  imageType   : {itype}            (0x{itype:04X})")
    print(f"  fileSize    : {len(blob)}")
    print(f"  sha512      : {sha512}")

    if args.index:
        entry = {
            "fileVersion": version,
            "fileSize": len(blob),
            "manufacturerCode": manuf,
            "imageType": itype,
            "sha512": sha512,
            "url": args.base_url.rstrip("/") + "/" + os.path.basename(args.outfile),
        }
        try:
            with open(args.index) as f:
                index = json.load(f)
            if not isinstance(index, list):
                index = []
        except (OSError, ValueError):
            index = []
        # Keep one entry per (manufacturerCode, imageType): drop older ones.
        index = [e for e in index
                 if not (e.get("manufacturerCode") == manuf and e.get("imageType") == itype)]
        index.append(entry)
        with open(args.index, "w") as f:
            json.dump(index, f, indent=2)
            f.write("\n")
        print(f"  index       : updated {args.index} -> {entry['url']}")


if __name__ == "__main__":
    main()
