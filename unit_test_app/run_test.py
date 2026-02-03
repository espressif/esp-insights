#!/usr/bin/env python3
"""Unit Test Runner - Automated test execution via serial connection"""

import argparse
import json
import re
import sys
import time
from datetime import datetime
from serial import serial_for_url, SerialException

def parse_args():
    """Parse command line arguments"""
    p = argparse.ArgumentParser(
        description="Toggle DTR/RTS, send commands, and wait for labels in sequence.")
    p.add_argument("port", help="Serial port (e.g. rfc://host:port, /dev/ttyUSB0, or COM3)")
    p.add_argument("-b", "--baud", type=int, default=115200, help="Baudrate (default 115200)")
    p.add_argument("-l", "--label", help="Single label to wait for (substring or regex)")
    p.add_argument("--regex", action="store_true", help="Treat label as regular expression")
    p.add_argument("--pulse-duration", type=float, default=0.5,
                   help="DTR/RTS pulse duration in seconds (default 0.5)")
    p.add_argument("--pre-pulse-delay", type=float, default=0.1,
                   help="Delay before toggling DTR/RTS (default 0.1s)")
    p.add_argument("--post-pulse-delay", type=float, default=0.1,
                   help="Delay after toggling before sending '*' (default 0.1s)")
    p.add_argument("--read-timeout", type=float, default=600.0,
                   help="Timeout waiting for label (0 = infinite)")
    p.add_argument("--no-toggle", action="store_true", help="Skip DTR/RTS toggle")
    p.add_argument("--log-file",
                   help="Save serial output to this file (default: auto-named with timestamp)")
    p.add_argument("-v", "--verbose", action="store_true", help="Verbose output")

    p.add_argument("--sequence", help="JSON file or JSON string defining command sequence")
    p.add_argument("--step-delay", type=float, default=2.0,
                   help="Delay between steps in sequence (default 2.0s)")

    return p.parse_args()

def verbose_print(enabled, *args, **kwargs):
    """Print message only if verbose mode is enabled"""
    if enabled:
        print(*args, **kwargs)

def load_sequence(sequence_arg):
    """Load test sequence from file path or JSON string"""
    if not sequence_arg:
        return None

    try:
        with open(sequence_arg, 'r', encoding='utf-8') as f:
            return json.load(f)
    except (FileNotFoundError, json.JSONDecodeError):
        pass

    try:
        return json.loads(sequence_arg)
    except json.JSONDecodeError:
        print(f"ERROR: Invalid sequence format: {sequence_arg}", file=sys.stderr)
        return None

def wait_for_label(ser, log_fh, label, timeout, verbose, use_regex=False):
    """Wait for a specific label in serial output"""
    if use_regex:
        pattern = re.compile(label)
        def match_function(text):
            return pattern.search(text)
    else:
        def match_function(text):
            return label in text

    buffer = ""
    start_time = time.time()
    verbose_print(verbose, f"Waiting for: '{label}'")

    last_print_time = start_time
    while True:
        current_time = time.time()
        elapsed = current_time - start_time

        if timeout > 0 and elapsed > timeout:
            print(f"ERROR: Timeout after {timeout}s waiting for '{label}'", file=sys.stderr)
            verbose_print(verbose, f"Last 500 chars in buffer: ...{buffer[-500:]}")
            return False

        if verbose and current_time - last_print_time > 60:
            print(f"Still waiting for '{label}' ({elapsed:.1f}s elapsed)...")
            last_print_time = current_time

        chunk = ser.read(1024)
        if not chunk:
            continue

        text = chunk.decode('utf-8', errors='replace')
        sys.stdout.write(text)
        sys.stdout.flush()
        log_fh.write(text)
        log_fh.flush()

        buffer += text
        if len(buffer) > 20000:
            buffer = buffer[-20000:]

        if match_function(buffer):
            verbose_print(verbose, f"Found: '{label}' (after {elapsed:.1f}s)")
            return True

def send_command(ser, command, verbose):
    """Send a command to the serial port"""
    if command == "":
        verbose_print(verbose, "Sending: [ENTER]")
    else:
        verbose_print(verbose, f"Sending command: '{command}'")
    ser.write(f"{command}\r\n".encode())
    ser.flush()

def run_sequence(ser, log_fh, sequence, step_delay, timeout, verbose):
    """Execute a sequence of commands and waits"""
    verbose_print(verbose, f"Starting sequence with {len(sequence)} steps")
    verbose_print(verbose, f"Step delay: {step_delay}s, Timeout: {timeout}s")

    for i, step in enumerate(sequence, 1):
        verbose_print(verbose, f"=== STEP {i}/{len(sequence)} ===")
        verbose_print(verbose, f"Description: {step.get('description', 'No description')}")

        command = step.get('command', '')
        if 'command' in step:
            send_command(ser, command, verbose)

        wait_label = step.get('wait_for', '')
        if wait_label:
            success = wait_for_label(ser, log_fh, wait_label, timeout, verbose)
            if not success:
                print(f"FAILED at step {i}: Could not find '{wait_label}'", file=sys.stderr)
                return False

        if i < len(sequence):
            verbose_print(verbose, f"Waiting {step_delay}s before next step...")
            time.sleep(step_delay)

    verbose_print(verbose, f"All {len(sequence)} steps completed successfully!")
    return True

def main():
    """Main function to execute the test runner"""
    args = parse_args()

    if not args.sequence and not args.label:
        print("ERROR: Must specify either --sequence or --label", file=sys.stderr)
        sys.exit(1)

    url = args.port
    if url.startswith("rfc://"):
        url = url.replace("rfc://", "rfc2217://", 1)

    verbose_print(args.verbose, f"Opening serial port: {url} @ {args.baud} baud...")

    try:
        ser = serial_for_url(url, baudrate=args.baud, timeout=1)
    except SerialException as e:
        print(f"ERROR: could not open serial port {url}: {e}", file=sys.stderr)
        sys.exit(2)

    verbose_print(args.verbose, "Opened serial port successfully.")

    log_file = args.log_file
    if not log_file:
        timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
        log_file = f"serial_log_{timestamp}.log"

    try:
        log_fh = open(log_file, "w", encoding="utf-8")
        verbose_print(args.verbose, f"Logging serial output to: {log_file}")
    except Exception as e:
        print(f"ERROR: cannot open log file {log_file}: {e}", file=sys.stderr)
        sys.exit(5)

    try:
        if not args.no_toggle:
            verbose_print(args.verbose, f"Waiting {args.pre_pulse_delay}s before toggling DTR/RTS...")
            time.sleep(args.pre_pulse_delay)

            verbose_print(args.verbose, "Pulsing DTR/RTS...")
            try:
                ser.setDTR(False)
                ser.setRTS(False)
            except Exception:
                try:
                    ser.dtr = False
                    ser.rts = False
                except Exception:
                    verbose_print(args.verbose, "Warning: unable to toggle DTR/RTS on this port.")
            time.sleep(args.pulse_duration)

            try:
                ser.setDTR(True)
                ser.setRTS(True)
            except Exception:
                try:
                    ser.dtr = True
                    ser.rts = True
                except Exception:
                    pass
            verbose_print(args.verbose,
                          f"Pulsed DTR/RTS for {args.pulse_duration}s. "
                          f"Waiting {args.post_pulse_delay}s...")
            time.sleep(args.post_pulse_delay)
        else:
            verbose_print(args.verbose, "Skipping DTR/RTS toggle (--no-toggle used).")

        if args.sequence:
            verbose_print(args.verbose, "Running custom test sequence...")
            sequence = load_sequence(args.sequence)
            if not sequence:
                sys.exit(4)
            success = run_sequence(ser, log_fh, sequence, args.step_delay,
                                   args.read_timeout, args.verbose)
            if success:
                print("✅ Custom test sequence completed successfully.")
            else:
                print("❌ Custom test sequence failed.", file=sys.stderr)
                sys.exit(3)
        else:
            verbose_print(args.verbose, "Running single command mode...")
            send_command(ser, "*", args.verbose)

            success = wait_for_label(ser, log_fh, args.label, args.read_timeout,
                                     args.verbose, args.regex)
            if success:
                print(f"✅ Label '{args.label}' detected. Exiting successfully.")
            else:
                sys.exit(3)

    except KeyboardInterrupt:
        print("\nInterrupted by user.")
    finally:
        ser.close()
        log_fh.close()
        verbose_print(args.verbose,
                      "Serial port closed and log saved.")

if __name__ == "__main__":
    main()
