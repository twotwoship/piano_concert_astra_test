"""Small dependency-free SMF reader. Times use the complete MIDI tempo map."""
import bisect
import collections
import struct
from dataclasses import dataclass
from pathlib import Path


@dataclass
class Note:
    start: int
    end: int
    pitch: int
    velocity: int
    track: int
    channel: int
    program: int
    uid: int
    key_end: int = 0


def vlq(data, pos):
    value = 0
    for _ in range(4):
        b = data[pos]
        pos += 1
        value = (value << 7) | (b & 127)
        if b < 128:
            return value, pos
    raise ValueError('Invalid MIDI VLQ')


def read_midi(path):
    data = Path(path).read_bytes()
    if data[:4] != b'MThd':
        raise ValueError('Not a MIDI file')
    size = int.from_bytes(data[4:8], 'big')
    fmt, tracks, ppq = struct.unpack('>HHH', data[8:14])
    if fmt not in (0, 1) or ppq & 0x8000 or not ppq:
        raise ValueError('Only synchronous PPQ MIDI formats 0/1 supported')
    pos = 8 + size
    events, names, ends = [], {}, []
    for track in range(tracks):
        if data[pos:pos + 4] != b'MTrk':
            raise ValueError('Missing track chunk')
        length = int.from_bytes(data[pos + 4:pos + 8], 'big')
        chunk = data[pos + 8:pos + 8 + length]
        pos += 8 + length
        i, tick, running, order = 0, 0, None, 0
        while i < len(chunk):
            delta, i = vlq(chunk, i)
            tick += delta
            status = chunk[i]
            if status & 128:
                i += 1
                if status < 0xf0:
                    running = status
            else:
                if running is None:
                    raise ValueError('Missing running status')
                status = running
            if status == 0xff:
                kind = chunk[i]
                i += 1
                n, i = vlq(chunk, i)
                payload = chunk[i:i+n]
                i += n
                if kind == 3:
                    names[track] = payload.decode('latin1').strip()
                if kind == 0x51:
                    events.append((tick, track, order, 'tempo', int.from_bytes(payload, 'big')))
                if kind == 0x2f:
                    break
            elif status in (0xf0, 0xf7):
                running = None
                n, i = vlq(chunk, i)
                i += n
            elif status < 0xf0:
                n = 1 if status >> 4 in (0xc, 0xd) else 2
                payload = tuple(chunk[i:i+n])
                i += n
                events.append((tick, track, order, status, payload))
            else:
                raise ValueError(f'Unsupported status {status:x}')
            order += 1
        ends.append(tick)
    events.sort(key=lambda e: e[:3])
    # Integer numerator avoids accumulated rounding at tempo changes.
    ticks, numerators, tempos = [0], [0], [500000]
    for tick, _, _, status, payload in events:
        if status == 'tempo':
            elapsed = numerators[-1] + (tick - ticks[-1]) * tempos[-1]
            if tick == ticks[-1]:
                tempos[-1] = payload
            else:
                ticks.append(tick)
                numerators.append(elapsed)
                tempos.append(payload)

    def ms(tick):
        idx = bisect.bisect_right(ticks, tick) - 1
        return (numerators[idx] + (tick - ticks[idx]) * tempos[idx] + ppq * 500) // (ppq * 1000)

    active = collections.defaultdict(collections.deque)
    held = collections.defaultdict(list)
    pedal, programs = [False] * 16, [0] * 16
    notes, controls = [], collections.Counter()
    unfinished = 0

    def finish(note, tick):
        note.end = max(note.start + 1, ms(tick))

    for tick, track, _, status, payload in events:
        if status == 'tempo':
            continue
        kind, channel = status >> 4, status & 15
        if kind == 0xc:
            programs[channel] = payload[0]
        elif kind == 9 and payload[1]:
            note = Note(ms(tick), 0, payload[0], payload[1], track, channel,
                        programs[channel], len(notes))
            notes.append(note)
            active[channel, payload[0]].append(note)
        elif kind == 8 or (kind == 9 and payload[1] == 0):
            queue = active[channel, payload[0]]
            if queue:
                note = queue.popleft()
                note.key_end = ms(tick)
                if pedal[channel]:
                    held[channel].append(note)
                else:
                    finish(note, tick)
        elif kind == 0xb:
            cc, value = payload
            controls[cc] += 1
            if cc == 64:
                pedal[channel] = value >= 64
                if not pedal[channel]:
                    for note in held[channel]:
                        finish(note, tick)
                    held[channel].clear()
            elif cc in (120, 121, 123):
                if cc == 121:
                    pedal[channel] = False
                if cc in (120, 121) or not pedal[channel]:
                    for note in held[channel]:
                        finish(note, tick)
                    held[channel].clear()
                if cc != 121:
                    for (ch, _), queue in active.items():
                        if ch == channel:
                            while queue:
                                note = queue.popleft()
                                note.key_end = ms(tick)
                                if cc == 123 and pedal[channel]:
                                    held[channel].append(note)
                                else:
                                    finish(note, tick)
    end_tick = max(ends)
    for note in notes:
        if not note.end:
            unfinished += 1
            finish(note, end_tick)
        if not note.key_end:
            note.key_end = note.end
    stats = []
    for track in range(tracks):
        ns = [n for n in notes if n.track == track]
        stats.append(dict(track=track, name=names.get(track, ''), notes=len(ns),
                          channels=sorted({n.channel + 1 for n in ns}),
                          programs=sorted({n.program for n in ns}),
                          pitch_range=[min(n.pitch for n in ns), max(n.pitch for n in ns)] if ns else []))
    return notes, dict(file=Path(path).name, format=fmt, ppq=ppq, tracks=stats,
                      duration_ms=ms(end_tick), tempo_changes=len(ticks),
                      controls=dict(controls), closed_at_end=unfinished,
                      pitch_bend_events=sum(e[3] != 'tempo' and e[3] >> 4 == 14 for e in events))


if __name__ == '__main__':
    import json
    import sys
    for path in sys.argv[1:]:
        notes, stats = read_midi(path)
        print(json.dumps(stats, indent=2, ensure_ascii=True))
