"""Arrange the supplied four-track piano reductions into eight PWM voices.

No audio samples or external Python packages are required. A frame is:
unsigned VLQ delta_ms, uint8 changed_mask, uint8 note_plus_one per changed voice.
Zero means silence. Simultaneous changes share a frame and a timestamp.
"""
import argparse
import collections
import hashlib
import json
import math
from pathlib import Path
from midi_score import read_midi

ROOT = Path(__file__).resolve().parents[1]


def encode_vlq(value):
    if value < 0:
        raise ValueError('Negative delta')
    result = [value & 127]
    value >>= 7
    while value:
        result.insert(0, (value & 127) | 128)
        value >>= 7
    return bytes(result)


def select_notes(active, now, previous):
    """Reserve orchestral melody, global bass, solo top/bottom and left hand.

    Tracks 1/2 are treated as solo reduction, 3/4 as accompaniment reduction,
    inferred from the opening score. All four tracks can borrow unused slots.
    Identical sounding pitches are merged. Recent attacks beat pedal tails.
    """
    candidates = list(active.values())
    chosen, pitches = [], set()
    old = {n.uid for n in previous if n is not None}

    def rank(n):
        return (int(n.key_end > now), n.start, n.velocity, int(n.uid in old), n.pitch, -n.uid)

    def reserve(pool, key):
        pool = [n for n in pool if n.pitch not in pitches]
        if pool and len(chosen) < 8:
            n = max(pool, key=key)
            chosen.append(n)
            pitches.add(n.pitch)

    orchestra = [n for n in candidates if n.track == 3]
    # Melody: newest attack group, then highest note. This also releases old
    # pedal tones when a descending melody attacks.
    reserve(orchestra, lambda n: (n.key_end > now, n.start, n.pitch, n.velocity))
    reserve(candidates, lambda n: (n.key_end > now, -n.pitch, n.start, n.velocity))
    solo = [n for n in candidates if n.track == 1]
    reserve(solo, lambda n: (n.key_end > now, n.start, n.pitch, n.velocity))
    reserve(solo, lambda n: (n.key_end > now, n.start, -n.pitch, n.velocity))
    reserve([n for n in candidates if n.track == 2],
            lambda n: (n.key_end > now, n.start, -n.pitch, n.velocity))
    # Give the lower accompaniment a seat before filling inner piano chords.
    reserve([n for n in candidates if n.track == 4], rank)
    for n in sorted(candidates, key=rank, reverse=True):
        if len(chosen) == 8:
            break
        if n.pitch not in pitches:
            chosen.append(n)
            pitches.add(n.pitch)
    return chosen


def arrange(notes, duration_ms, sustain_tail_ms=180):
    starts, ends = collections.defaultdict(list), collections.defaultdict(list)
    for n in notes:
        if n.channel == 9:
            continue
        # A square-wave buzzer has no piano decay. Limit only pedal extension;
        # actual held-key lengths remain intact.
        n.end = max(n.start + 1, min(n.end, n.key_end + sustain_tail_ms))
        starts[n.start].append(n)
        ends[n.end].append(n)
    times = sorted(set(starts) | set(ends) | {duration_ms})
    active, slots, frames = {}, [None] * 8, []
    source_max, distinct_max, selected_ids = 0, 0, set()
    overloaded_ms, previous_time, previous_distinct = 0, 0, 0
    for now in times:
        if previous_distinct > 8:
            overloaded_ms += now - previous_time
        for n in ends[now]:
            active.pop(n.uid, None)
        for n in starts[now]:
            active[n.uid] = n
        if now >= duration_ms:
            active.clear()
        source_max = max(source_max, len(active))
        distinct = len({n.pitch for n in active.values()})
        distinct_max = max(distinct_max, distinct)
        previous_time, previous_distinct = now, distinct
        chosen = select_notes(active, now, slots)
        selected_ids.update(n.uid for n in chosen)
        by_pitch = {n.pitch: n for n in chosen}
        new_slots = [None] * 8
        # Hold a pitch on the same physical buzzer whenever possible.
        for i, n in enumerate(slots):
            if n and n.pitch in by_pitch:
                new_slots[i] = by_pitch.pop(n.pitch)
        for n in chosen:
            if n.pitch in by_pitch:
                new_slots[new_slots.index(None)] = n
                del by_pitch[n.pitch]
        changes = {}
        for i, (old, new) in enumerate(zip(slots, new_slots)):
            a = old.pitch + 1 if old else 0
            b = new.pitch + 1 if new else 0
            # Emit repeated attacks even if the pitch stays the same.
            if a != b or (new and old and new.uid != old.uid and new.start == now):
                changes[i] = b
        if changes:
            frames.append((now, changes))
        slots = new_slots
    if not frames or frames[-1][0] != duration_ms:
        frames.append((duration_ms, dict.fromkeys(range(8), 0)))
    data, prev = bytearray(), 0
    for now, changes in frames:
        data.extend(encode_vlq(now - prev))
        data.append(sum(1 << i for i in changes))
        data.extend(changes[i] for i in sorted(changes))
        prev = now
    return bytes(data), frames, dict(max_source_polyphony=source_max,
        max_distinct_pitches=distinct_max, over_8_pitches_ms=overloaded_ms,
        source_notes=len(notes), represented_note_ids=len(selected_ids),
        frames=len(frames), encoded_bytes=len(data), sustain_tail_ms=sustain_tail_ms)


def decode_frames(data):
    i, now = 0, 0
    while i < len(data):
        delta = 0
        while True:
            b = data[i]
            i += 1
            delta = (delta << 7) | (b & 127)
            if b < 128:
                break
        now += delta
        mask = data[i]
        i += 1
        changes = {}
        for voice in range(8):
            if mask & (1 << voice):
                changes[voice] = data[i]
                i += 1
        yield now, changes


def convert(sustain_tail_ms=180):
    out = ROOT / 'generated'
    out.mkdir(exist_ok=True)
    source = ['/* Generated by tools/convert_midi.py; do not edit manually. */',
              '#include "score_data.h"']
    reports, descriptors = [], []
    for movement in range(1, 4):
        matches = list((ROOT / 'midi').glob(f'*_1_{movement}_*.mid'))
        if len(matches) != 1:
            raise ValueError(f'Expected one MIDI for movement {movement}')
        path = matches[0]
        notes, report = read_midi(path)
        data, frames, summary = arrange(notes, report['duration_ms'], sustain_tail_ms)
        assert list(decode_frames(data)) == frames
        report.update(summary)
        report['sha256'] = hashlib.sha256(path.read_bytes()).hexdigest()
        reports.append(report)
        (out / f'movement{movement}.score').write_bytes(data)
        source.append(f'static const uint8_t movement{movement}[] = {{')
        for i in range(0, len(data), 24):
            source.append('    ' + ','.join(str(b) for b in data[i:i+24]) + ',')
        source.append('};')
        descriptors.append(f'    {{movement{movement}, sizeof(movement{movement}), {report["duration_ms"]}U}}')
    source.append('const Score scores[3] = {\n' + ',\n'.join(descriptors) + '\n};')
    # Maximize timer resolution independently for each MIDI pitch. All eight
    # timer inputs are 96 MHz under clock.c / option.h.
    source.append('const PitchTimer pitch_timers[128] = {')
    worst = 0
    for pitch in range(128):
        hz = 440 * 2 ** ((pitch - 69) / 12)
        divider = max(1, math.ceil(96000000 / hz / 65536))
        period = round(96000000 / divider / hz)
        assert 1 <= divider <= 65536 and 2 <= period <= 65536
        cents = abs(1200 * math.log2(96000000 / divider / period / hz))
        worst = max(worst, cents)
        source.append(f'    {{{divider-1}U, {period-1}U}}, /* MIDI {pitch} */')
    source.append('};\n')
    (out / 'score_data.c').write_text('\n'.join(source), encoding='utf-8')
    (out / 'analysis.json').write_text(json.dumps(dict(movements=reports,
        total_score_bytes=sum(r['encoded_bytes'] for r in reports),
        max_timer_quantization_cents=worst,
        arrangement='Tracks 1/2 solo and 3/4 accompaniment inferred from opening; heuristic eight-note reduction, original pitches and tempo map.'),
        indent=2), encoding='utf-8')
    for r in reports:
        print(f'{r["file"]}: {r["duration_ms"]/1000:.3f}s, {r["source_notes"]} notes, '
              f'peak {r["max_distinct_pitches"]} pitches -> 8, {r["encoded_bytes"]} bytes')
    print(f'Total score: {sum(r["encoded_bytes"] for r in reports)} bytes; timer error <= {worst:.4f} cents')


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--sustain-tail-ms', type=int, default=180)
    args = parser.parse_args()
    if args.sustain_tail_ms < 0:
        parser.error('sustain tail must be nonnegative')
    convert(args.sustain_tail_ms)
