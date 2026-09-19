#!/usr/bin/env python3
"""Send OpenTrack UDP pose packets, for driving scripted poses during testing.

The wire format is six little-endian doubles: x, y, z in centimetres, then yaw,
pitch and roll in degrees. Sending from this machine to 127.0.0.1 makes the mod
classify the connection as local, which is what a real OpenTrack setup on the
same PC does.
"""

import argparse
import math
import socket
import struct
import time


def pack(x, y, z, yaw, pitch, roll):
    return struct.pack("<6d", x, y, z, yaw, pitch, roll)


def main():
    p = argparse.ArgumentParser()
    p.add_argument("--port", type=int, default=4242)
    p.add_argument("--host", default="127.0.0.1")
    p.add_argument("--rate", type=float, default=120.0)
    p.add_argument("--seconds", type=float, default=10.0)
    p.add_argument("--x", type=float, default=0.0, help="centimetres, + is head left")
    p.add_argument("--y", type=float, default=0.0, help="centimetres, + is head up")
    p.add_argument("--z", type=float, default=0.0, help="centimetres, + is head back")
    p.add_argument("--yaw", type=float, default=0.0, help="degrees")
    p.add_argument("--pitch", type=float, default=0.0, help="degrees")
    p.add_argument("--roll", type=float, default=0.0, help="degrees")
    p.add_argument("--sweep", choices=["none", "x", "y", "z", "yaw", "pitch", "roll"],
                   default="none", help="oscillate this axis between +/- its value")
    p.add_argument("--period", type=float, default=4.0, help="sweep period in seconds")
    args = p.parse_args()

    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    interval = 1.0 / args.rate
    start = time.monotonic()
    axes = {"x": args.x, "y": args.y, "z": args.z,
            "yaw": args.yaw, "pitch": args.pitch, "roll": args.roll}

    while True:
        now = time.monotonic() - start
        if now >= args.seconds:
            break
        frame = dict(axes)
        if args.sweep != "none":
            frame[args.sweep] = axes[args.sweep] * math.sin(2 * math.pi * now / args.period)
        sock.sendto(pack(frame["x"], frame["y"], frame["z"],
                         frame["yaw"], frame["pitch"], frame["roll"]),
                    (args.host, args.port))
        time.sleep(interval)

    # Leave the tracker at rest rather than mid-pose: the mod holds the last
    # pose it saw when packets stop, so a run that ends mid-sweep leaves the view
    # leaning until the next run starts.
    for _ in range(30):
        sock.sendto(pack(0, 0, 0, 0, 0, 0), (args.host, args.port))
        time.sleep(interval)


if __name__ == "__main__":
    main()
