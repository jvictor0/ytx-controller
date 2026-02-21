#!/usr/bin/env python3
"""
Upload YTX split firmware (main + aux) over the YTX bootloader SysEx protocol.

This mirrors the sequence used by the Yaeltex firmware manager:
1) REQUEST_UPLOAD_SELF + main firmware blocks
2) REQUEST_UPLOAD_OTHER + aux firmware blocks
3) REQUEST_RST
"""

from __future__ import annotations

import argparse
import os
import struct
import sys
import time
from typing import Iterable, List, Optional, Sequence, Tuple

try:
    import mido
except ImportError as exc:  # pragma: no cover - runtime dependency check
    print(
        "error: missing dependency 'mido' (and a backend like python-rtmidi)\n"
        "hint: use tools/.venv/bin/python or install with:\n"
        "  pip install mido python-rtmidi",
        file=sys.stderr,
    )
    raise SystemExit(2) from exc


DEFAULT_BOOT_PORT = "KilomuxBOOT"
DEFAULT_AUTO_BOOT_FROM = "WRLD.BLDR"

SYSEX_ID = (0x79, 0x74, 0x78)  # 'y' 't' 'x'
BOOT_HEADER = (SYSEX_ID[0], SYSEX_ID[1], SYSEX_ID[2], 0x00, 0x00, 0x00)
APP_HEADER = (SYSEX_ID[0], SYSEX_ID[1], SYSEX_ID[2], 0x00, 0x01, 0x00)

REQUEST_RST = 0x12
REQUEST_BOOT_MODE = 0x13
REQUEST_UPLOAD_SELF = 0x14
REQUEST_UPLOAD_OTHER = 0x15
REQUEST_FIRM_DATA_UPLOAD = 0x16

STATUS_ACK = 1
STATUS_NAK = 2

MCU_PAGE_SIZE = 64
PAGES_PER_BLOCK = 2
BLOCK_SIZE = MCU_PAGE_SIZE * PAGES_PER_BLOCK  # 128 bytes
ADDRESS_SIZE = 4


class UploadError(RuntimeError):
    pass


def checksum7(data: Sequence[int]) -> int:
    checksum = 0
    for value in data:
        checksum ^= value
    return checksum & 0x7F


def encode_sysex(data: bytes) -> List[int]:
    """
    7-bit safe SysEx encoding compatible with the bootloader decoder.
    Every 7 source bytes become 8 encoded bytes: [msb-pack, b0..b6(without msb)].
    """
    out: List[int] = []
    for offset in range(0, len(data), 7):
        chunk = data[offset : offset + 7]
        msb_pack = 0
        out.append(0)  # placeholder for msb_pack
        for i, value in enumerate(chunk):
            msb_pack |= ((value >> 7) & 0x01) << i
            out.append(value & 0x7F)
        out[-(len(chunk) + 1)] = msb_pack
    return out


def chunk_firmware(path: str) -> List[List[int]]:
    with open(path, "rb") as firmware_file:
        blob = firmware_file.read()

    messages: List[List[int]] = []
    for address in range(0, len(blob), BLOCK_SIZE):
        chunk = blob[address : address + BLOCK_SIZE]
        if len(chunk) < BLOCK_SIZE:
            chunk = chunk + (b"\x00" * (BLOCK_SIZE - len(chunk)))

        payload = struct.pack("<I", address) + chunk
        messages.append(encode_sysex(payload))

    # Keep behavior deterministic if firmware file is empty.
    if not messages:
        payload = struct.pack("<I", 0) + (b"\x00" * BLOCK_SIZE)
        messages.append(encode_sysex(payload))

    return messages


def find_first_match(names: Sequence[str], pattern: str) -> Optional[str]:
    pattern_lc = pattern.lower()
    for name in names:
        if pattern_lc in name.lower():
            return name
    return None


def has_pattern(names: Sequence[str], pattern: Optional[str]) -> bool:
    if not pattern:
        return False
    return find_first_match(names, pattern) is not None


def choose_io_ports(port_pattern: str) -> Tuple[Optional[str], Optional[str]]:
    outputs = mido.get_output_names()
    inputs = mido.get_input_names()

    out_name = find_first_match(outputs, port_pattern)
    in_name = find_first_match(inputs, port_pattern)

    if out_name and in_name:
        return out_name, in_name
    return None, None


def list_ports() -> None:
    print("MIDI output ports:")
    for i, name in enumerate(mido.get_output_names()):
        print(f"  [{i}] {name}")
    print("MIDI input ports:")
    for i, name in enumerate(mido.get_input_names()):
        print(f"  [{i}] {name}")


def has_boot_marker(name: str) -> bool:
    lowered = name.lower()
    return ".bldr" in lowered or "boot" in lowered


def parse_port_index(value: Optional[str]) -> int:
    if value is None:
        return 0
    try:
        parsed = int(value)
    except ValueError as exc:
        raise UploadError(f"invalid port index '{value}'") from exc
    if parsed < 0:
        raise UploadError(f"invalid port index '{value}' (must be >= 0)")
    return parsed


def pick_named_port(
    outputs: Sequence[str],
    inputs: Sequence[str],
    pattern: str,
    match_index: int,
) -> Tuple[Optional[str], Optional[str]]:
    matching_output_indexes = [
        i for i, output_name in enumerate(outputs) if pattern.lower() in output_name.lower()
    ]
    if not matching_output_indexes:
        return None, None
    if match_index >= len(matching_output_indexes):
        return None, None

    out_idx = matching_output_indexes[match_index]
    out_name = outputs[out_idx]

    # Mirror the pairing logic from the Yaeltex loader:
    # for duplicated output names, pair with the input at the same duplicate ordinal.
    same_output_name_indexes = [i for i, name in enumerate(outputs) if name == out_name]
    duplicate_ordinal = same_output_name_indexes.index(out_idx)

    matching_input_indexes = [
        i for i, input_name in enumerate(inputs) if out_name.lower() in input_name.lower()
    ]
    if duplicate_ordinal < len(matching_input_indexes):
        in_name = inputs[matching_input_indexes[duplicate_ordinal]]
    else:
        fallback_inputs = [
            input_name for input_name in inputs if pattern.lower() in input_name.lower()
        ]
        in_name = fallback_inputs[match_index] if match_index < len(fallback_inputs) else None

    return out_name, in_name


def build_sysex(
    header: Sequence[int],
    request_id: int,
    payload: Optional[Iterable[int]] = None,
    with_checksum: bool = False,
) -> mido.Message:
    data = list(header) + [request_id]
    if payload is not None:
        data.extend(payload)
    if with_checksum:
        data.append(checksum7(data))
    return mido.Message("sysex", data=data)


def drain_input(port: "mido.ports.BaseInput", duration_s: float = 0.05) -> None:
    end_t = time.monotonic() + duration_s
    while time.monotonic() < end_t:
        drained_any = False
        for _ in port.iter_pending():
            drained_any = True
        if not drained_any:
            time.sleep(0.005)


def wait_for_bootloader_ports(
    boot_port_pattern: str, timeout_s: float, boot_port_index: int = 0
) -> Tuple[str, str]:
    deadline = time.monotonic() + timeout_s
    while time.monotonic() < deadline:
        out_name, in_name = pick_named_port(
            mido.get_output_names(),
            mido.get_input_names(),
            boot_port_pattern,
            boot_port_index,
        )
        if out_name and in_name:
            return out_name, in_name
        time.sleep(0.2)
    raise UploadError(
        f"timed out waiting for bootloader MIDI ports matching '{boot_port_pattern}'"
    )


def wait_for_ack(
    in_port: "mido.ports.BaseInput", timeout_s: float, context: str, debug: bool = False
) -> None:
    deadline = time.monotonic() + timeout_s
    while time.monotonic() < deadline:
        for msg in in_port.iter_pending():
            if msg.type != "sysex":
                if debug:
                    print(f"[rx/{context}] non-sysex: {msg}")
                continue
            data = list(msg.data)
            if len(data) < 4:
                if debug:
                    print(f"[rx/{context}] short sysex: {data}")
                continue
            if tuple(data[:3]) != SYSEX_ID:
                if debug:
                    print(f"[rx/{context}] non-ytx sysex: {data}")
                continue

            status = data[3]
            if debug and (
                context.startswith("begin") or status != STATUS_ACK or len(data) != 4
            ):
                print(f"[rx/{context}] sysex={data} status={status}")
            if status == STATUS_ACK:
                return
            if status == STATUS_NAK:
                raise UploadError(f"NAK received while waiting for ACK ({context})")
        time.sleep(0.002)
    raise UploadError(f"timeout waiting for ACK ({context})")


def send_with_ack(
    out_port: "mido.ports.BaseOutput",
    in_port: "mido.ports.BaseInput",
    msg: mido.Message,
    timeout_s: float,
    context: str,
    debug: bool = False,
) -> None:
    drain_input(in_port)
    if debug and context.startswith("begin"):
        print(f"[tx/{context}] sysex={list(msg.data)}")
    out_port.send(msg)
    wait_for_ack(in_port, timeout_s=timeout_s, context=context, debug=debug)


def upload_firmware_blocks(
    out_port: "mido.ports.BaseOutput",
    in_port: "mido.ports.BaseInput",
    blocks: Sequence[Sequence[int]],
    timeout_s: float,
    label: str,
    debug: bool = False,
) -> None:
    total = len(blocks)
    for index, encoded_block in enumerate(blocks, start=1):
        message = build_sysex(
            BOOT_HEADER,
            REQUEST_FIRM_DATA_UPLOAD,
            payload=encoded_block,
            with_checksum=True,
        )
        send_with_ack(
            out_port,
            in_port,
            message,
            timeout_s=timeout_s,
            context=f"{label} block {index}/{total}",
            debug=debug,
        )
        if index == 1 or index == total or index % 64 == 0:
            print(f"{label}: {index}/{total}")


def default_app_port_name(boot_port_name: str) -> Optional[str]:
    suffix = ".BLDR"
    if boot_port_name.upper().endswith(suffix):
        return boot_port_name[: -len(suffix)]
    return None


def request_boot_mode(app_port_pattern: str) -> None:
    outputs = mido.get_output_names()
    matches = [name for name in outputs if app_port_pattern.lower() in name.lower()]
    if not matches:
        raise UploadError(
            f"cannot find application MIDI output port matching '{app_port_pattern}'"
        )

    # Prefer a non-boot-marked app port when pattern is broad (e.g. "WRLD").
    non_boot_matches = [name for name in matches if not has_boot_marker(name)]
    out_name = non_boot_matches[0] if non_boot_matches else matches[0]
    if not out_name:
        raise UploadError(
            f"cannot find application MIDI output port matching '{app_port_pattern}'"
        )

    print(f"Requesting bootloader mode via app port: {out_name}")
    with mido.open_output(out_name) as out_port:
        out_port.send(build_sysex(APP_HEADER, REQUEST_BOOT_MODE))


def run_install(
    main_bin: str,
    aux_bin: str,
    boot_port_pattern: str,
    app_port_pattern: Optional[str],
    auto_boot_from_pattern: Optional[str],
    timeout_s: float,
    begin_timeout_s: float,
    boot_port_index: int,
    out_port_name: Optional[str],
    in_port_name: Optional[str],
    debug: bool,
    force_boot: bool,
) -> None:
    if not os.path.isfile(main_bin):
        raise UploadError(f"main firmware not found: {main_bin}")
    if not os.path.isfile(aux_bin):
        raise UploadError(f"aux firmware not found: {aux_bin}")

    outputs = mido.get_output_names()
    inputs = mido.get_input_names()

    out_name: Optional[str] = None
    in_name: Optional[str] = None

    if out_port_name and in_port_name:
        out_name = out_port_name
        in_name = in_port_name
    else:
        if has_pattern(outputs, auto_boot_from_pattern):
            print(
                f"Detected app-like port '{auto_boot_from_pattern}'. "
                f"Requesting bootloader and waiting for '{boot_port_pattern}'."
            )
            request_boot_mode(auto_boot_from_pattern or "")
            out_name, in_name = wait_for_bootloader_ports(
                boot_port_pattern,
                begin_timeout_s,
                boot_port_index=boot_port_index,
            )
        elif force_boot:
            if app_port_pattern is None:
                app_port_pattern = default_app_port_name(boot_port_pattern)
            if not app_port_pattern:
                raise UploadError(
                    "force-boot requested but no app port was provided or derivable"
                )
            request_boot_mode(app_port_pattern)
            out_name, in_name = wait_for_bootloader_ports(
                boot_port_pattern,
                begin_timeout_s,
                boot_port_index=boot_port_index,
            )
        else:
            out_name, in_name = pick_named_port(
                outputs, inputs, boot_port_pattern, boot_port_index
            )
            if not out_name or not in_name:
                if app_port_pattern is None:
                    app_port_pattern = default_app_port_name(boot_port_pattern)
                if not app_port_pattern:
                    raise UploadError(
                        f"bootloader port '{boot_port_pattern}' not found and no app port provided"
                    )
                request_boot_mode(app_port_pattern)
                out_name, in_name = wait_for_bootloader_ports(
                    boot_port_pattern,
                    begin_timeout_s,
                    boot_port_index=boot_port_index,
                )

    main_blocks = chunk_firmware(main_bin)
    aux_blocks = chunk_firmware(aux_bin)
    print(f"Main blocks: {len(main_blocks)} ({main_bin})")
    print(f"Aux blocks : {len(aux_blocks)} ({aux_bin})")

    for attempt in range(2):
        if not out_name or not in_name:
            raise UploadError("failed to select MIDI ports")

        print(f"Using bootloader output: {out_name}")
        print(f"Using bootloader input : {in_name}")

        try:
            with mido.open_output(out_name) as out_port, mido.open_input(in_name) as in_port:
                send_with_ack(
                    out_port,
                    in_port,
                    build_sysex(BOOT_HEADER, REQUEST_UPLOAD_SELF),
                    timeout_s=begin_timeout_s,
                    context="begin main upload",
                    debug=debug,
                )
                upload_firmware_blocks(
                    out_port,
                    in_port,
                    main_blocks,
                    timeout_s=timeout_s,
                    label="MAIN",
                    debug=debug,
                )

                send_with_ack(
                    out_port,
                    in_port,
                    build_sysex(BOOT_HEADER, REQUEST_UPLOAD_OTHER),
                    timeout_s=begin_timeout_s,
                    context="begin aux upload",
                    debug=debug,
                )
                upload_firmware_blocks(
                    out_port,
                    in_port,
                    aux_blocks,
                    timeout_s=timeout_s,
                    label="AUX",
                    debug=debug,
                )

                out_port.send(build_sysex(BOOT_HEADER, REQUEST_RST))
                print("Reset command sent.")
                return
        except UploadError as err:
            can_retry = (
                attempt == 0
                and "begin main upload" in str(err)
                and app_port_pattern is not None
                and out_port_name is None
                and in_port_name is None
            )
            if not can_retry:
                raise

            print("No ACK to begin upload. Requesting bootloader mode and retrying once.")
            request_boot_mode(app_port_pattern)
            out_name, in_name = wait_for_bootloader_ports(
                boot_port_pattern,
                begin_timeout_s,
                boot_port_index=boot_port_index,
            )

    raise UploadError("upload failed after retry")


def parse_args(argv: Sequence[str]) -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Install split YTX firmware (main + aux) over bootloader MIDI SysEx"
    )
    parser.add_argument("--main-bin", default=None, help="Path to main firmware .bin")
    parser.add_argument("--aux-bin", default=None, help="Path to aux firmware .bin")
    parser.add_argument(
        "--boot-port",
        default=DEFAULT_BOOT_PORT,
        help=f"Substring to match bootloader MIDI ports (default: {DEFAULT_BOOT_PORT})",
    )
    parser.add_argument(
        "--app-port",
        default=None,
        help="Substring to match application MIDI output port used to request boot mode",
    )
    parser.add_argument(
        "--auto-boot-from",
        default=DEFAULT_AUTO_BOOT_FROM,
        help=(
            "If this app-mode port substring exists, request boot mode there and wait "
            f"for --boot-port (default: {DEFAULT_AUTO_BOOT_FROM})"
        ),
    )
    parser.add_argument(
        "--timeout",
        type=float,
        default=20.0,
        help="Per-block ACK timeout in seconds (default: 20)",
    )
    parser.add_argument(
        "--begin-timeout",
        type=float,
        default=90.0,
        help="Begin-upload/bootloader wait timeout in seconds (default: 90)",
    )
    parser.add_argument(
        "--list-ports",
        action="store_true",
        help="List MIDI ports and exit",
    )
    parser.add_argument(
        "--boot-port-index",
        default="0",
        help="Match index among boot-port output matches (default: 0)",
    )
    parser.add_argument(
        "--out-port",
        default=None,
        help="Exact output port name override",
    )
    parser.add_argument(
        "--in-port",
        default=None,
        help="Exact input port name override",
    )
    parser.add_argument(
        "--debug",
        action="store_true",
        help="Enable protocol debug logging",
    )
    parser.add_argument(
        "--force-boot",
        action="store_true",
        help="Always request bootloader mode through --app-port before uploading",
    )
    return parser.parse_args(argv)


def main(argv: Sequence[str]) -> int:
    args = parse_args(argv)

    if args.list_ports:
        list_ports()
        return 0

    if not args.main_bin or not args.aux_bin:
        print("error: --main-bin and --aux-bin are required", file=sys.stderr)
        return 2

    try:
        boot_port_index = parse_port_index(args.boot_port_index)
        if (args.out_port is None) != (args.in_port is None):
            raise UploadError("--out-port and --in-port must be provided together")
        run_install(
            main_bin=args.main_bin,
            aux_bin=args.aux_bin,
            boot_port_pattern=args.boot_port,
            app_port_pattern=args.app_port,
            auto_boot_from_pattern=args.auto_boot_from,
            timeout_s=args.timeout,
            begin_timeout_s=args.begin_timeout,
            boot_port_index=boot_port_index,
            out_port_name=args.out_port,
            in_port_name=args.in_port,
            debug=args.debug,
            force_boot=args.force_boot,
        )
    except UploadError as err:
        print(f"error: {err}", file=sys.stderr)
        return 1

    print("Firmware install completed.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
