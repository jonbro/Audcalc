import numpy

lookup_tables_32 = []

sample_rate = 3200
excursion = 65536 * 65536.0

# Create table for pitch.
a4_midi = 69
a4_pitch = 440.0
highest_octave = 128-12

notes = numpy.arange(
    highest_octave * 128.0,
    (highest_octave + 12) * 128.0 + 16,
    16)
#print((notes-a4_midi*128)/128)

# 13.75 * (2 ^ ((note_num - 9) / 12))

# ok! so the way this works is that it stores a table _only_ for the highest octave
# the table includes phase steps per note
# then when looking up the phase in the LUT, it bitshifts to get the lower octave phases (since bitshifting is equivelent to /2)
# the other magic here is that the notes are multiplied by 128 right off the bat (since that is the base resolution of the input pitch)
# this line here will give me the speeds for the top octave

speeds = 2 ** ((notes - a4_midi * 128) / (128 * 12))

# now we just need to multiply by the excursion to get the fixed point representation of these values
# I'm going to go with 5q(32-5) - i.e. multiply by 27 bits worth of value to get as much data in the phase as possible
# we can always shift down later if we want a phase value that shifts less than 32 bits

# this is 7ff (11bits) * ffff (16 bits)
speeds = speeds * (65536 * 2048.0)

pitches = a4_pitch * 2 ** ((notes - a4_midi * 128) / (128 * 12))

def noteToFreq(N):
     return 440.0 * pow(2.0, ((N) - 69) / 12.0)


def noteToFreq2(note):
    exp = note * (1.0 / 12.0) + 3.0313597
    return 2.0**exp

increments = excursion / sample_rate * pitches
# print(increments / 44100)
myNotes = numpy.arange(
    highest_octave * 128.0,
    (highest_octave + 12) * 128.0 + 16,
    16)
#print(pitches/128)
increment = 1.0/sample_rate*pitches
per_hertz_phase_inc = 32000.0 / 69 * 128

delays = sample_rate / pitches * 65536 * 4096

lookup_tables_32.append(
    ('oscillator_increments', increments.astype(int)))

#print(lookup_tables_32)