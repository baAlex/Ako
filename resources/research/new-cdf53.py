
#
# Released into the public domain, or under BSD Zero Clause License:
#
# Copyright (c) 2023 Alexander Brandt
#
# Permission to use, copy, modify, and/or distribute this software for any
# purpose with or without fee is hereby granted.
#
# THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
# WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
# MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
# SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER
# RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT,
# NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE
# USE OR PERFORMANCE OF THIS SOFTWARE.


import random


def Half(x):
	return x >> 1

def Rule(x):
	return (x + (x & 1)) >> 1


def NewApproach(length, randomness_max = 0):

	# Measures
	half = Half(length) # or 'highpass length'
	rule = Rule(length) # or 'lowpass length'
	print("\nLength: {}, half: {}, rule: {}".format(length, half, rule))

	# Generate data
	random.seed(808)
	# buffer_a = [(i + 1) for i in range(length)]
	buffer_a = [(i * 10 + 1 + random.randrange(randomness_max + 1)) for i in range(length)]

	# if rule == half:
	# 	buffer_a[length - 1] = buffer_a[length - 2]

	buffer_b = [""] * length # Poison this ones
	buffer_c = [""] * length
	print(buffer_a)

	# Forward lift scheme
	if 1:
		hp_l1 = 0 # 'l1' = 'less one' = 'previous hp'

		# Start/Middle
		# Part that receives Simd treatment, UwU
		for col in range(half - 1):

			# Get samples
			even = buffer_a[(col << 1) + 0]
			odd = buffer_a[(col << 1) + 1]
			even_p1 = buffer_a[(col << 1) + 2]

			# Cdf53 wavelet transformation
			hp = odd - int((even + even_p1) / 2)
			lp = even + int((hp_l1 + hp) / 4)

			# Output, lowpass at the left, highpass to the right
			buffer_b[col + 0] = lp
			buffer_b[col + rule] = hp

			# Save current Hp as previous one
			hp_l1 = hp

		# End
		# Welcome to the rice fields, ÒwÓ
		if half == rule:
			col = half - 1
			even = buffer_a[(col << 1) + 0]
			odd = buffer_a[(col << 1) + 1]
			even_p1 = even # Clamp
			hp = odd - int((even + even_p1) / 2)
			lp = even + int((hp_l1 + hp) / 4)
			buffer_b[col + 0] = lp
			buffer_b[col + rule] = hp
			# print(even, odd, even_p1, hp, hp_l1)

		else:
			col = half - 1
			even = buffer_a[(col << 1) + 0]
			odd = buffer_a[(col << 1) + 1]
			even_p1 = buffer_a[(col << 1) + 2]
			hp = odd - int((even + even_p1) / 2)
			lp = even + int((hp_l1 + hp) / 4)
			buffer_b[col + 0] = lp
			buffer_b[col + rule] = hp
			# print(even, odd, even_p1, hp, hp_l1)
			hp_l1 = hp

			col = half
			even = buffer_a[(col << 1) + 0]
			odd = even # Extend impair input
			even_p1 = even # Clamp
			hp = odd - int((even + even_p1) / 2)
			lp = even + int((hp_l1 + hp) / 4)
			buffer_b[col + 0] = lp
			# print(even, odd, even_p1, hp, hp_l1)

	print(buffer_b)

	# Inverse
	if 1:
		hp_l1 = 0

		# Start/Middle
		for col in range(half - 2):

			# Get samples
			lp = buffer_b[col + 0]
			lp_p1 = buffer_b[col + 1]
			hp = buffer_b[col + rule]
			hp_p1 = buffer_b[col + rule + 1]

			# Cdf53
			even = lp - int((hp_l1 + hp) / 4)
			even_p1 = lp_p1 - int((hp + hp_p1) / 4)
			odd = hp + int((even + even_p1) / 2)

			# Output
			buffer_c[(col << 1) + 0] = even
			buffer_c[(col << 1) + 1] = odd

			# Save current Hp
			hp_l1 = hp

		# End
		if half == rule:
			col = half - 2
			lp = buffer_b[col + 0]
			hp = buffer_b[col + rule]
			even = lp - int((hp_l1 + hp) / 4)
			lp_p1 = buffer_b[col + 1]
			hp_p1 = buffer_b[col + rule + 1]
			even_p1 = lp_p1 - int((hp + hp_p1) / 4)
			odd = hp + int((even + even_p1) / 2)
			buffer_c[(col << 1) + 0] = even
			buffer_c[(col << 1) + 1] = odd
			# print(even, odd, even_p1, hp, hp_l1)
			hp_l1 = hp

			col = half - 1
			lp = buffer_b[col + 0]
			hp = buffer_b[col + rule]
			even = lp - int((hp_l1 + hp) / 4)
			even_p1 = even # Clamp
			odd = hp + int((even + even_p1) / 2)
			buffer_c[(col << 1) + 0] = even
			buffer_c[(col << 1) + 1] = odd
			# print(even, odd, even_p1, hp, hp_l1)
			hp_l1 = hp

		else:
			col = half - 2
			lp = buffer_b[col + 0]
			lp_p1 = buffer_b[col + 1]
			hp = buffer_b[col + rule]
			hp_p1 = buffer_b[col + rule + 1]
			even = lp - int((hp_l1 + hp) / 4)
			even_p1 = lp_p1 - int((hp + hp_p1) / 4)
			odd = hp + int((even + even_p1) / 2)
			buffer_c[(col << 1) + 0] = even
			buffer_c[(col << 1) + 1] = odd
			# print(even, odd, even_p1, hp, hp_l1)
			hp_l1 = hp

			col = half - 1
			lp = buffer_b[col + 0]
			lp_p1 = buffer_b[col + 1]
			hp = buffer_b[col + rule]
			hp_p1 = 0 # See below
			even = lp - int((hp_l1 + hp) / 4)
			even_p1 = lp_p1 - int((hp + hp_p1) / 4)
			odd = hp + int((even + even_p1) / 2)
			buffer_c[(col << 1) + 0] = even
			buffer_c[(col << 1) + 1] = odd
			# print(even, odd, even_p1, hp, hp_l1)
			hp_l1 = hp

			col = half - 0
			lp = buffer_b[col + 0]
			hp = 0 # As 'odd - int((even + even_p1) / 2)' becomes 'even - int((even + even) / 2)'
			       # when the forward transform extends impair input lengths. Also the clamp operation
			       # is implicit here
			even = lp - int((hp_l1 + hp) / 4)
			buffer_c[(col << 1) + 0] = even
			# print(even, even, even, hp, hp_l1)

	# print(buffer_c)

	# Check
	for i in range(length):
		if buffer_a[i] != buffer_c[i]:
			good_bye = 2 / 0


def CurrentApproach(length, randomness_max = 0):

	# Measures
	half = Half(length)
	rule = Rule(length)
	print("\nLength: {}, half: {}, rule: {}".format(length, half, rule))

	# Generate data
	random.seed(808)
	buffer_a = [(i * 10 + 1 + random.randrange(randomness_max + 1)) for i in range(length)]

	# if rule == half:
	# 	buffer_a[length - 1] = buffer_a[length - 2]

	buffer_b = [""] * length
	buffer_c = [""] * length
	print(buffer_a)

	# Forward
	if 1:
		# Highpass
		for col in range(half):
			even = buffer_a[(col << 1) + 0]
			odd = buffer_a[(col << 1) + 1]

			even_p1 = even # Clamp
			if col < half - 1:
				even_p1 = buffer_a[(col << 1) + 2]

			buffer_b[col + rule] = odd - int((even + even_p1) / 2)

		# Lowpass
		for col in range(half):
			even = buffer_a[(col << 1) + 0]
			hp = buffer_b[col + rule]

			hp_l1 = hp # Clamp
			if col > 0:
				hp_l1 = buffer_b[col + rule - 1]

			buffer_b[col + 0] = even + int((hp_l1 + hp) / 4)

		# Complete lowpass
		if rule != half:
			col = half
			even = buffer_a[(col << 1) + 0]
			hp = buffer_b[col + half + 0]
			hp_l1 = buffer_b[col + half - 1]

			buffer_b[col] = even + int((hp_l1 + hp) / 4)

	print(buffer_b)

	# Inverse
	if 1:
		# Evens
		for col in range(half):
			lp = buffer_b[col + 0]
			hp = buffer_b[col + rule]

			hp_l1 = hp # Clamp
			if col > 0:
				hp_l1 = buffer_b[col + rule - 1]

			buffer_c[(col << 1) + 0] = lp - int((hp_l1 + hp) / 4)

		# Complete evens
		if rule != half:
			col = half
			lp = buffer_b[col + 0]
			hp = buffer_b[col + rule - 1]
			hp_l1 = buffer_b[col + rule - 2] # Clamp

			buffer_c[(col << 1) + 0] = lp - int((hp_l1 + hp) / 4)

		# Odds
		for col in range(half):
			hp = buffer_b[col + rule]
			even = buffer_c[(col << 1) + 0]

			even_p1 = even # Clamp
			if col < half - 1:
				even_p1 = buffer_c[(col << 1) + 2]

			buffer_c[(col << 1) + 1] = hp + int((even + even_p1) / 2)

	# print(buffer_c)

	# Check
	for i in range(length):
		if buffer_a[i] != buffer_c[i]:
			good_bye = 2 / 0


def OldApproach(length, randomness_max = 0):

	# Measures
	half = Half(length) # or 'highpass length'
	rule = Rule(length) # or 'lowpass length'
	print("\nLength: {}, half: {}, rule: {}".format(length, half, rule))

	# Generate data
	random.seed(808)
	# buffer_a = [(i + 1) for i in range(length)]
	buffer_a = [(i * 10 + 1 + random.randrange(randomness_max + 1)) for i in range(length)]

	# if rule == half:
	# 	buffer_a[length - 1] = buffer_a[length - 2]

	buffer_b = [""] * length # Poison this ones
	buffer_c = [""] * length
	print(buffer_a)

	# Forward lift scheme
	if 1:
		# Highpass, except last
		for col in range(rule - 1):
			even = buffer_a[(col << 1) + 0]
			odd = buffer_a[(col << 1) + 1]
			even_p1 = buffer_a[(col << 1) + 2]

			buffer_b[col + rule] = odd - int((even + even_p1) / 2)

		# Highpass, last value
		if half == rule: # 'fake_last == 0' in the original
			col = (rule - 1)
			even = buffer_a[(col << 1) + 0]
			odd = buffer_a[(col << 1) + 1];
			even_p1 = even # Clamp

			buffer_b[col + rule] = odd - int((even + even_p1) / 2)

		# Lowpass, first value
		for col in range(rule):
			even = buffer_a[(col << 1) + 0]

			hp = 0
			if half != rule and col == rule - 2: # More like the new approach
				hp = buffer_b[col + rule]

			hp_l1 = hp
			if col > 0:
				hp_l1 = buffer_b[col + rule - 1]

			buffer_b[col + 0] = even + int((hp_l1 + hp) / 4)

	print(buffer_b)



####

print("\nNew approach:")
NewApproach(20, 5)
NewApproach(19, 5)

print("\nCurrent approach:")
CurrentApproach(20, 5)
CurrentApproach(19, 5)

print("\nOld approach:")
OldApproach(20, 5)
OldApproach(19, 5)
