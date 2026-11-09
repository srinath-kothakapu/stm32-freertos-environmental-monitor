"""Capture the CSV stream from the STM32 over UART and print latency statistics.

The firmware prints: tick_ms,temperature_c,humidity_pct,alert,latency_ms

Examples:
    python tools/capture_uart.py --port COM5 --seconds 120 --out captures/run1.csv
    python tools/capture_uart.py --from-file captures/run1.csv
"""

import argparse
import csv
import statistics
import time


def parse_row(line):
    """Return (tick_ms, temperature_c, humidity_pct, alert, latency_ms) or None for bad lines."""
    parts = line.strip().split(",")
    if len(parts) != 5:
        return None
    try:
        return int(parts[0]), float(parts[1]), float(parts[2]), int(parts[3]), int(parts[4])
    except ValueError:
        return None


def summarize(rows):
    """Summary of a capture: sample count, latency min/mean/max and alert count."""
    if not rows:
        return None
    latencies = [r[4] for r in rows]
    return {
        "samples": len(rows),
        "latency_min_ms": min(latencies),
        "latency_mean_ms": statistics.mean(latencies),
        "latency_max_ms": max(latencies),
        "alerts": sum(r[3] for r in rows),
    }


def read_file(path):
    with open(path) as f:
        return [r for r in (parse_row(line) for line in f) if r]


def read_serial(port, baud, seconds, out_path):
    import serial  # needs: pip install pyserial

    rows = []
    end = time.time() + seconds
    with serial.Serial(port, baud, timeout=2) as ser, open(out_path, "w", newline="") as f:
        writer = csv.writer(f)
        writer.writerow(["tick_ms", "temperature_c", "humidity_pct", "alert", "latency_ms"])
        while time.time() < end:
            row = parse_row(ser.readline().decode("utf-8", errors="ignore"))
            if row:
                rows.append(row)
                writer.writerow(row)
                f.flush()
                print(row)
    return rows


def main():
    p = argparse.ArgumentParser(description="STM32 UART capture and latency summary")
    p.add_argument("--port")
    p.add_argument("--baud", type=int, default=115200)
    p.add_argument("--seconds", type=float, default=60)
    p.add_argument("--out", default="captures/run.csv")
    p.add_argument("--from-file")
    args = p.parse_args()

    if args.from_file:
        rows = read_file(args.from_file)
    elif args.port:
        rows = read_serial(args.port, args.baud, args.seconds, args.out)
    else:
        p.error("use --port <port> or --from-file <csv>")

    s = summarize(rows)
    if s is None:
        print("No valid rows captured")
        return
    print(f"\nSamples: {s['samples']}   Alerts: {s['alerts']}")
    print(f"Latency (sensor read to alert output): min {s['latency_min_ms']} ms, "
          f"mean {s['latency_mean_ms']:.2f} ms, max {s['latency_max_ms']} ms")


if __name__ == "__main__":
    main()
