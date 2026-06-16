#!/usr/bin/env python3
"""Wrap an ESP-IDF app .bin into a Zigbee OTA Upgrade image (.ota).

The output is a standard Zigbee OTA file (OTA header + one upgrade-image
sub-element) that any compliant coordinator (zigbee2mqtt, ZHA, deCONZ) can serve.

Usage:
  python make_ota.py --in build/light_switch.bin --out dimmer_v2.ota \\
      --manuf 0x131B --image-type 0x1010 --version 0x01000001

manuf / image-type / version MUST match the firmware's OTA_UPGRADE_* defines in
main/ota_driver.h, and version must be greater than the running firmware's.
The script prints the fileVersion/fileSize/sha512 you need for the z2m index.
"""
import argparse
import hashlib
import struct

OTA_FILE_IDENTIFIER = 0x0BEEF11E  # Zigbee OTA magic
OTA_HEADER_VERSION  = 0x0100
ZIGBEE_STACK_PRO    = 0x0002
TAG_UPGRADE_IMAGE   = 0x0000
HEADER_LEN          = 56  # no optional header fields


def auto_int(x):
    return int(x, 0)


def main():
    p = argparse.ArgumentParser(description=__doc__,
                                formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument("--in", dest="infile", required=True, help="input app .bin")
    p.add_argument("--out", dest="outfile", required=True, help="output .ota")
    p.add_argument("--manuf", type=auto_int, required=True, help="manufacturer code, e.g. 0x131B")
    p.add_argument("--image-type", type=auto_int, required=True, help="image type, e.g. 0x1010")
    p.add_argument("--version", type=auto_int, required=True, help="file version, e.g. 0x01000001")
    p.add_argument("--header-string", default="DG Dimmer OTA", help="<=32 char label")
    args = p.parse_args()

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
        args.manuf,
        args.image_type,
        args.version,
        ZIGBEE_STACK_PRO,
        header_string,
        total_size,
    )
    assert len(header) == HEADER_LEN, len(header)

    blob = header + sub_element
    with open(args.outfile, "wb") as f:
        f.write(blob)

    print(f"Wrote {args.outfile}")
    print(f"  app bytes   : {len(fw)}")
    print(f"  fileVersion : {args.version}        (0x{args.version:08X})")
    print(f"  manufactCode: {args.manuf}            (0x{args.manuf:04X})")
    print(f"  imageType   : {args.image_type}            (0x{args.image_type:04X})")
    print(f"  fileSize    : {len(blob)}")
    print(f"  sha512      : {hashlib.sha512(blob).hexdigest()}")


if __name__ == "__main__":
    main()
