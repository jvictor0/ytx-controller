#!/usr/bin/env python3
"""
Record a spoken MIDI hardware experiment.

The script records microphone audio and incoming MIDI events on one shared
monotonic timeline, transcribes the audio with whisper.cpp, and writes raw
lab-notebook artifacts for later human or AI interpretation.
"""

from __future__ import annotations

import argparse
import datetime as dt
import json
import re
import shutil
import subprocess
import sys
import tempfile
import threading
import time
from pathlib import Path
from typing import Any, Iterable, Optional

try:
    import mido
except ImportError as exc:  # pragma: no cover - runtime dependency check
    print(
        "error: missing dependency 'mido' (and a backend like python-rtmidi)\n"
        "hint: use tools/.venv/bin/python",
        file=sys.stderr,
    )
    raise SystemExit(2) from exc


YTX_ID = (0x79, 0x74, 0x78)
WISH_GET = 0
WISH_SET = 1
MESSAGE_CONFIGURATION = 1
BLOCK_CONFIGURATION = 0
BLOCK_ENCODER = 2

def list_midi_ports() -> None:
    print("MIDI input ports:")
    for i, name in enumerate(mido.get_input_names()):
        print(f"  [{i}] {name}")
    print("MIDI output ports:")
    for i, name in enumerate(mido.get_output_names()):
        print(f"  [{i}] {name}")


def list_audio_devices(ffmpeg_bin: str) -> int:
    cmd = [ffmpeg_bin, "-hide_banner", "-f", "avfoundation", "-list_devices", "true", "-i", ""]
    proc = subprocess.run(cmd, text=True, capture_output=True)
    output = (proc.stdout or "") + (proc.stderr or "")
    print(output.rstrip())
    # ffmpeg exits non-zero after listing devices. That is expected.
    return 0


def find_matching_port(names: Iterable[str], pattern: str) -> str:
    for name in names:
        if pattern.lower() in name.lower():
            return name
    raise RuntimeError(f"no MIDI port matched {pattern!r}; available={list(names)!r}")


def msg_to_dict(msg: mido.Message) -> dict[str, Any]:
    data = msg.dict()
    data["text"] = str(msg)
    return data


def encode_sysex_get(bank: int, block: int, section: int) -> mido.Message:
    data = [
        *YTX_ID,
        0,
        WISH_GET,
        MESSAGE_CONFIGURATION,
        bank,
        block,
        (section >> 7) & 0x7F,
        section & 0x7F,
    ]
    return mido.Message("sysex", data=data)


def decode_sysex(encoded: list[int]) -> bytes:
    out: list[int] = []
    index = 0
    while index < len(encoded):
        msb_pack = encoded[index]
        index += 1
        for bit in range(7):
            if index >= len(encoded):
                break
            out.append((encoded[index] & 0x7F) | (((msb_pack >> bit) & 1) << 7))
            index += 1
    return bytes(out)


def drain_input(in_port: "mido.ports.BaseInput", duration_s: float = 0.05) -> None:
    end_t = time.monotonic() + duration_s
    while time.monotonic() < end_t:
        drained = False
        for _ in in_port.iter_pending():
            drained = True
        if not drained:
            time.sleep(0.005)


def request_section(
    out_port: "mido.ports.BaseOutput",
    in_port: "mido.ports.BaseInput",
    bank: int,
    block: int,
    section: int,
    timeout_s: float = 3.0,
) -> bytes:
    drain_input(in_port)
    out_port.send(encode_sysex_get(bank, block, section))
    deadline = time.monotonic() + timeout_s
    while time.monotonic() < deadline:
        for msg in in_port.iter_pending():
            if msg.type != "sysex":
                continue
            data = list(msg.data)
            if data[:3] != list(YTX_ID):
                continue
            if (
                len(data) >= 10
                and data[4] == WISH_SET
                and data[5] == MESSAGE_CONFIGURATION
                and data[6] == bank
                and data[7] == block
                and ((data[8] << 7) | data[9]) == section
            ):
                return decode_sysex(data[10:])
        time.sleep(0.002)
    raise TimeoutError(f"timed out reading bank={bank} block={block} section={section}")


def u16le(blob: bytes, offset: int) -> int:
    return blob[offset] | (blob[offset + 1] << 8)


def parse_controller_config(blob: bytes) -> dict[str, Any]:
    return {
        "device_name": bytes(x for x in blob[22:38] if x).decode("latin1", errors="replace"),
        "fw_version": f"{blob[2]}.{blob[1]}",
        "config_version": f"{blob[39]}.{blob[38]}",
        "encoder_count": blob[134],
        "bank_count": blob[144],
    }


def parse_encoder_cc(blob: bytes) -> int:
    return blob[4]


def fetch_encoder_cc_map(port_pattern: str, encoder_count_hint: int = 16) -> tuple[list[int], dict[str, Any]]:
    out_name = find_matching_port(mido.get_output_names(), port_pattern)
    in_name = find_matching_port(mido.get_input_names(), port_pattern)
    with mido.open_output(out_name) as out_port, mido.open_input(in_name) as in_port:
        config_blob = request_section(out_port, in_port, 0, BLOCK_CONFIGURATION, 0)
        config = parse_controller_config(config_blob)
        encoder_count = config.get("encoder_count") or encoder_count_hint
        cc_map = []
        for encoder_index in range(int(encoder_count)):
            cc_map.append(parse_encoder_cc(request_section(out_port, in_port, 0, BLOCK_ENCODER, encoder_index)))
    return cc_map, config


def run_ffmpeg_recording(ffmpeg_bin: str, audio_device: str, audio_path: Path) -> subprocess.Popen[bytes]:
    input_name = audio_device if audio_device.startswith(":") else f":{audio_device}"
    cmd = [
        ffmpeg_bin,
        "-hide_banner",
        "-loglevel",
        "error",
        "-f",
        "avfoundation",
        "-i",
        input_name,
        "-ac",
        "1",
        "-ar",
        "16000",
        "-y",
        str(audio_path),
    ]
    return subprocess.Popen(cmd, stdin=subprocess.PIPE)


def stop_process(proc: subprocess.Popen[bytes], timeout_s: float = 5.0) -> None:
    if proc.poll() is not None:
        return
    try:
        if proc.stdin:
            proc.stdin.write(b"q\n")
            proc.stdin.flush()
        proc.wait(timeout=timeout_s)
        return
    except Exception:
        pass
    proc.terminate()
    try:
        proc.wait(timeout=timeout_s)
    except subprocess.TimeoutExpired:
        proc.kill()
        proc.wait()


def record_session(args: argparse.Namespace) -> Path:
    timestamp = dt.datetime.now().strftime("%Y%m%d-%H%M%S")
    out_dir = Path(args.out_dir or f"experiments/midi-speech-{timestamp}").resolve()
    out_dir.mkdir(parents=True, exist_ok=True)

    audio_path = out_dir / "audio.wav"
    midi_jsonl_path = out_dir / "midi_events.jsonl"
    meta_path = out_dir / "meta.json"

    encoder_cc_map: list[int] = []
    controller_config: dict[str, Any] = {}
    if args.fetch_config:
        try:
            encoder_cc_map, controller_config = fetch_encoder_cc_map(args.midi_port)
            print(f"Fetched encoder CC map from controller: {encoder_cc_map}")
        except Exception as exc:
            print(f"warning: could not fetch controller config: {exc}", file=sys.stderr)

    in_name = find_matching_port(mido.get_input_names(), args.midi_port)
    print(f"Using MIDI input: {in_name}")
    print(f"Using audio device: {args.audio_device}")
    print(f"Output directory: {out_dir}")

    session_start_monotonic = time.monotonic()
    session_start_wall = dt.datetime.now(dt.timezone.utc).isoformat()
    ffmpeg_proc = run_ffmpeg_recording(args.ffmpeg_bin, args.audio_device, audio_path)

    events: list[dict[str, Any]] = []
    lock = threading.Lock()
    midi_file = midi_jsonl_path.open("w", encoding="utf-8")

    def on_midi(msg: mido.Message) -> None:
        now = time.monotonic()
        event = {
            "t": now - session_start_monotonic,
            "monotonic": now,
            "message": msg_to_dict(msg),
        }
        with lock:
            events.append(event)
            midi_file.write(json.dumps(event, sort_keys=True) + "\n")
            midi_file.flush()

    meta = {
        "session_start_wall_utc": session_start_wall,
        "session_start_monotonic": session_start_monotonic,
        "midi_input": in_name,
        "audio_device": args.audio_device,
        "audio_path": str(audio_path),
        "encoder_cc_map": encoder_cc_map,
        "controller_config": controller_config,
    }
    meta_path.write_text(json.dumps(meta, indent=2, sort_keys=True) + "\n", encoding="utf-8")

    try:
        with mido.open_input(in_name, callback=on_midi):
            print("Recording. Say each action, perform it, then continue.")
            if args.duration:
                print(f"Recording for {args.duration:.1f}s...")
                time.sleep(args.duration)
            else:
                input("Press Enter when finished...\n")
    finally:
        stop_process(ffmpeg_proc)
        midi_file.close()

    if ffmpeg_proc.returncode not in (0, 255, None):
        print(f"warning: ffmpeg exited with {ffmpeg_proc.returncode}", file=sys.stderr)

    print(f"Recorded {len(events)} MIDI events.")
    print(f"Audio: {audio_path}")
    if not args.skip_transcribe:
        transcribe_session(out_dir, args)
    return out_dir


def transcribe_session(out_dir: Path, args: argparse.Namespace) -> Path:
    audio_path = out_dir / "audio.wav"
    if not audio_path.exists():
        raise FileNotFoundError(audio_path)
    output_base = out_dir / "whisper"
    cmd = [
        args.whisper_bin,
        "-m",
        args.whisper_model,
        "-l",
        args.language,
        "-oj",
        "-of",
        str(output_base),
        str(audio_path),
    ]
    if args.no_gpu:
        cmd.append("-ng")
    print("Transcribing audio with whisper-cli...")
    subprocess.run(cmd, check=True)
    transcript_path = output_base.with_suffix(".json")
    print(f"Transcript: {transcript_path}")
    export_raw_notebook(out_dir)
    return transcript_path


def load_jsonl(path: Path) -> list[dict[str, Any]]:
    if not path.exists():
        return []
    return [json.loads(line) for line in path.read_text(encoding="utf-8").splitlines() if line.strip()]


def parse_timestamp_value(value: Any) -> float:
    if value is None:
        return 0.0
    if isinstance(value, (int, float)):
        return float(value)
    text = str(value)
    match = re.match(r"(?:(\d+):)?(\d+):(\d+)[,.](\d+)", text)
    if not match:
        return 0.0
    hours = int(match.group(1) or 0)
    minutes = int(match.group(2))
    seconds = int(match.group(3))
    millis = int(match.group(4).ljust(3, "0")[:3])
    return hours * 3600 + minutes * 60 + seconds + millis / 1000.0


def parse_whisper_offset_value(value: Any) -> float:
    if isinstance(value, (int, float)):
        # whisper.cpp JSON offsets are 10 us ticks in the Dictator parser:
        # offset / 10 = milliseconds, so seconds are offset / 10000.
        return float(value) / 10000.0
    return parse_timestamp_value(value)


def load_transcript_segments(path: Path) -> list[dict[str, Any]]:
    data = json.loads(path.read_text(encoding="utf-8"))
    raw_segments = data.get("transcription") or data.get("segments") or []
    segments: list[dict[str, Any]] = []
    for raw in raw_segments:
        offsets = raw.get("offsets") or raw.get("timestamps") or {}
        parse_time = parse_whisper_offset_value if offsets else parse_timestamp_value
        start_value = offsets["from"] if "from" in offsets else raw.get("start")
        end_value = offsets["to"] if "to" in offsets else raw.get("end")
        segments.append(
            {
                "start_s": parse_time(start_value),
                "end_s": parse_time(end_value),
                "text": str(raw.get("text") or "").strip(),
            }
        )
    if not segments and data.get("text"):
        segments.append({"start_s": 0.0, "end_s": 0.0, "text": str(data["text"]).strip()})
    return segments


def export_raw_notebook(out_dir: Path) -> None:
    transcript_path = out_dir / "whisper.json"
    if not transcript_path.exists():
        return

    segments = load_transcript_segments(transcript_path)
    midi_events = load_jsonl(out_dir / "midi_events.jsonl")

    (out_dir / "transcript_segments.json").write_text(
        json.dumps(segments, indent=2, sort_keys=True) + "\n",
        encoding="utf-8",
    )

    transcript_lines = ["# Transcript", ""]
    for segment in segments:
        transcript_lines.append(f"- {segment['start_s']:.3f}-{segment['end_s']:.3f}s: {segment['text']}")
    (out_dir / "transcript.md").write_text("\n".join(transcript_lines) + "\n", encoding="utf-8")

    timeline_items: list[dict[str, Any]] = []
    for segment in segments:
        timeline_items.append(
            {
                "type": "speech_segment",
                "t": segment["start_s"],
                "end_t": segment["end_s"],
                "text": segment["text"],
            }
        )
    for event in midi_events:
        timeline_items.append({"type": "midi", **event})

    timeline_items.sort(key=lambda item: (float(item.get("t", 0.0)), item.get("type", "")))
    with (out_dir / "timeline.jsonl").open("w", encoding="utf-8") as timeline_file:
        for item in timeline_items:
            timeline_file.write(json.dumps(item, sort_keys=True) + "\n")

    notes = [
        "# MIDI Speech Lab Notebook",
        "",
        "Raw artifacts:",
        "- `audio.wav`: microphone recording",
        "- `midi_events.jsonl`: incoming MIDI events with timestamps relative to recording start",
        "- `whisper.json`: raw whisper.cpp JSON output",
        "- `transcript_segments.json`: timestamped transcript segments normalized to seconds",
        "- `transcript.md`: readable transcript segment list",
        "- `timeline.jsonl`: speech segments and MIDI events merged by timestamp",
        "- `meta.json`: session metadata and controller config/CC map if fetched",
        "",
        "No behavioral interpretation was performed by this script.",
    ]
    (out_dir / "README.md").write_text("\n".join(notes) + "\n", encoding="utf-8")
    print(f"Raw notebook exports written under: {out_dir}")


def run_self_test() -> int:
    with tempfile.TemporaryDirectory() as temp_dir:
        out_dir = Path(temp_dir)
        (out_dir / "whisper.json").write_text(
            json.dumps(
                {
                    "transcription": [
                        {
                            "text": "turning encoder three to the left",
                            "offsets": {"from": 1000, "to": 10000},
                        }
                    ]
                }
            ),
            encoding="utf-8",
        )
        (out_dir / "midi_events.jsonl").write_text(
            json.dumps(
                {
                    "t": 1.3,
                    "monotonic": 123.4,
                    "message": {"type": "control_change", "control": 2, "value": 62},
                },
                sort_keys=True,
            )
            + "\n",
            encoding="utf-8",
        )
        export_raw_notebook(out_dir)
        segments = json.loads((out_dir / "transcript_segments.json").read_text(encoding="utf-8"))
        timeline = load_jsonl(out_dir / "timeline.jsonl")
        if segments != [{"end_s": 1.0, "start_s": 0.1, "text": "turning encoder three to the left"}]:
            print(f"self-test failed: bad segments {segments}", file=sys.stderr)
            return 1
        if [item["type"] for item in timeline] != ["speech_segment", "midi"]:
            print(f"self-test failed: bad timeline {timeline}", file=sys.stderr)
            return 1
    print("self-test passed")
    return 0


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--midi-port", default="WRLD.BLDR", help="substring of the MIDI input/output port")
    parser.add_argument("--audio-device", default="0", help="AVFoundation audio device index/name; e.g. 3")
    parser.add_argument("--out-dir", default=None, help="session output directory")
    parser.add_argument("--duration", type=float, default=None, help="recording duration in seconds")
    parser.add_argument("--list-midi", action="store_true", help="list MIDI ports and exit")
    parser.add_argument("--list-audio-devices", action="store_true", help="list ffmpeg AVFoundation devices and exit")
    parser.add_argument("--skip-transcribe", action="store_true", help="record only; do not run whisper/export")
    parser.add_argument("--export-existing", default=None, help="write raw notebook exports for an existing session")
    parser.add_argument("--transcribe-existing", default=None, help="run whisper for an existing session directory")
    parser.add_argument("--self-test", action="store_true", help="run timestamp/export self-test and exit")
    parser.add_argument("--fetch-config", action=argparse.BooleanOptionalAction, default=True)
    parser.add_argument("--ffmpeg-bin", default=shutil.which("ffmpeg") or "/opt/homebrew/bin/ffmpeg")
    parser.add_argument("--whisper-bin", default=shutil.which("whisper-cli") or "/opt/homebrew/bin/whisper-cli")
    parser.add_argument("--whisper-model", default="/Users/joyo/Sheaf/models/ggml-small.en.bin")
    parser.add_argument("--language", default="en")
    parser.add_argument("--no-gpu", action="store_true")
    return parser


def main(argv: Optional[list[str]] = None) -> int:
    parser = build_parser()
    args = parser.parse_args(argv)

    if args.self_test:
        return run_self_test()
    if args.list_midi:
        list_midi_ports()
        return 0
    if args.list_audio_devices:
        return list_audio_devices(args.ffmpeg_bin)
    if args.transcribe_existing:
        transcribe_session(Path(args.transcribe_existing).resolve(), args)
        return 0
    if args.export_existing:
        export_raw_notebook(Path(args.export_existing).resolve())
        return 0

    record_session(args)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
