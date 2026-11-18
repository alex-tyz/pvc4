"""Measure board-reported completed OLED frames under optional SCALE command load."""
import argparse
import json
import time
from pathlib import Path
import serial


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('port')
    parser.add_argument('--seconds', type=float, default=15)
    parser.add_argument('--load-rate', type=float, default=0, help='SCALE commands per second, 0..25')
    parser.add_argument('--output', type=Path, help='save actual board measurements as JSON')
    args = parser.parse_args()
    if args.seconds < 3 or not 0 <= args.load_rate <= 25:
        parser.error('--seconds must be >=3 and --load-rate must be 0..25')
    snapshots = []
    with serial.Serial(args.port, 38400, timeout=0.02, write_timeout=0.2) as port:
        start = time.monotonic()
        next_stats = start + 2  # warm up
        next_load = next_stats
        pending = b''
        while time.monotonic() - start < args.seconds + 2:
            now = time.monotonic()
            if now >= next_stats:
                port.write(b'STATS\n')
                next_stats = now + 1
            if args.load_rate and now >= next_load:
                port.write(b'SCALE 100 100 100\n')
                next_load = now + 1 / args.load_rate
            pending += port.read(256)
            while b'\n' in pending:
                raw, pending = pending.split(b'\n', 1)
                fields = raw.decode('ascii', errors='replace').strip().split()
                if len(fields) == 8 and fields[0] == 'STATS':
                    try:
                        snapshots.append(list(map(int, fields[1:])))
                    except ValueError:
                        continue
            if len(pending) > 2048:
                raise RuntimeError('Unterminated UART response')
    if len(snapshots) < 2:
        raise RuntimeError('No STATS responses; check port, firmware and 38400 8N1')
    first, last = snapshots[0], snapshots[-1]
    intervals = []
    for a, b in zip(snapshots, snapshots[1:]):
        ms = (b[0] - a[0]) & 0xffffffff
        frames = (b[1] - a[1]) & 0xffffffff
        if ms:
            intervals.append(frames * 1000 / ms)
    result = {
        'port': args.port, 'load_commands_per_second': args.load_rate,
        'minimum_interval_fps': min(intervals), 'max_frame_gap_ms_since_boot': last[6],
        'max_render_ms_since_boot': last[2],
        'new_deadline_misses': (last[3] - first[3]) & 0xffffffff,
        'new_display_errors': (last[4] - first[4]) & 0xffffffff,
        'new_rx_errors': (last[5] - first[5]) & 0xffffffff, 'snapshots': snapshots,
    }
    result['passed'] = (min(intervals) >= 20 and last[6] <= 50
                        and all(result[k] == 0 for k in
                                ['new_deadline_misses', 'new_display_errors', 'new_rx_errors']))
    print(json.dumps(result, ensure_ascii=False, indent=2))
    if args.output:
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(json.dumps(result, ensure_ascii=False, indent=2), encoding='utf-8')
    return 0 if result['passed'] else 1


if __name__ == '__main__':
    raise SystemExit(main())
