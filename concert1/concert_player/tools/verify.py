"""MIDI semantic fixtures, score/source invariants and production C replay QA."""
import collections
import json
import math
import struct
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path
from midi_score import read_midi
from convert_midi import ROOT, encode_vlq, decode_frames


def smf(track, ppq=480):
    return b'MThd' + struct.pack('>IHHH', 6, 0, 1, ppq) + b'MTrk' + struct.pack('>I', len(track)) + track


def event(delta, payload):
    return encode_vlq(delta) + payload


class Verification(unittest.TestCase):
    def read_fixture(self, track):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / 'fixture.mid'
            path.write_bytes(smf(track))
            return read_midi(path)

    def test_tempo_and_running_status(self):
        track = event(0, b'\xff\x51\x03\x07\xa1\x20')
        track += event(0, b'\x90\x3c\x60')
        track += event(480, b'\x3c\x00')
        track += event(0, b'\xff\x51\x03\x0f\x42\x40')
        track += event(0, b'\x90\x40\x60')
        track += event(480, b'\x40\x00') + event(0, b'\xff\x2f\x00')
        notes, stats = self.read_fixture(track)
        self.assertEqual([(n.start, n.end) for n in notes], [(0, 500), (500, 1500)])
        self.assertEqual(stats['duration_ms'], 1500)

    def test_sustain_and_repeated_pitch(self):
        track = event(0, b'\xb0\x40\x7f') + event(0, b'\x90\x3c\x60')
        track += event(240, b'\x80\x3c\x00') + event(0, b'\x90\x3c\x70')
        track += event(240, b'\x80\x3c\x00') + event(480, b'\xb0\x40\x00')
        track += event(0, b'\xff\x2f\x00')
        notes, _ = self.read_fixture(track)
        self.assertEqual([(n.start, n.key_end, n.end) for n in notes], [(0, 250, 1000), (250, 500, 1000)])

    def test_all_generated_notes_and_timing(self):
        reports = json.loads((ROOT/'generated/analysis.json').read_text())['movements']
        for m, report in enumerate(reports, 1):
            notes, stats = read_midi(ROOT/'midi'/report['file'])
            starts, ends = collections.defaultdict(list), collections.defaultdict(list)
            for n in notes:
                if n.channel == 9:
                    continue
                end = max(n.start + 1, min(n.end, n.key_end + report['sustain_tail_ms']))
                starts[n.start].append(n.pitch)
                ends[end].append(n.pitch)
            source_times = sorted(set(starts) | set(ends))
            index, active, slots = 0, collections.Counter(), [0]*8
            frames = list(decode_frames((ROOT/f'generated/movement{m}.score').read_bytes()))
            last = 0
            for now, changes in frames:
                self.assertGreaterEqual(now, last)
                while index < len(source_times) and source_times[index] <= now:
                    t = source_times[index]
                    active.subtract(ends[t])
                    active.update(starts[t])
                    index += 1
                for voice, note in changes.items():
                    self.assertIn(voice, range(8))
                    self.assertIn(note, range(129))
                    slots[voice] = note
                sounded = [n-1 for n in slots if n]
                self.assertLessEqual(len(sounded), 8)
                self.assertEqual(len(sounded), len(set(sounded)))
                for pitch in sounded:
                    self.assertGreater(active[pitch], 0, (m, now, pitch))
                last = now
            self.assertEqual(last, stats['duration_ms'])
            self.assertEqual(slots, [0]*8)

    def test_arm_image_vectors_and_size(self):
        data = (ROOT/'concert.bin').read_bytes()
        stack, entry = struct.unpack_from('<II', data)
        self.assertEqual(stack, 0x20020000)
        self.assertTrue(entry & 1)
        self.assertTrue(0x08000000 <= (entry & ~1) < 0x08000000 + len(data))
        self.assertLess(len(data), 512*1024)
        systick = struct.unpack_from('<I', data, 15*4)[0]
        invalid = struct.unpack_from('<I', data, 7*4)[0]
        self.assertNotEqual(systick, invalid)

    def test_production_c_player(self):
        executable = ROOT/'tools/player_host_test.exe'
        if not executable.exists():
            self.fail('Build tools/player_host_test.exe with host GCC before running verification')
        result = subprocess.run([str(executable)], capture_output=True, text=True, check=True)
        print(result.stdout.strip())
        for m in range(3):
            result = subprocess.run([str(executable), str(m)], capture_output=True, text=True, check=True)
            actual = [tuple(map(int, line.split(','))) for line in result.stdout.splitlines()]
            frames = list(decode_frames((ROOT/f'generated/movement{m+1}.score').read_bytes()))
            expected = [(0, v, 0) for v in range(8)]
            expected += [(t, v, n) for t, changes in frames for v, n in changes.items()]
            expected += [(frames[-1][0], v, 0) for v in range(8)]
            self.assertEqual(actual, expected, f'Production C outputs differ from score in movement {m+1}')


if __name__ == '__main__':
    unittest.main(verbosity=2)
