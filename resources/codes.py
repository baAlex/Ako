

def Design(e, m):
	if (e < 4):
		return (e << 4) + m

	base = (1 << e) + 48
	add = m << (e - 4)
	return base + add


def Decode(code):
	e = code >> 4
	m = code & 15

	if (e < 4):
		return (e << 4) + m

	base = (1 << e) + 48
	add = m << (e - 4)
	return base + add


def Encode(value):
	if value < 64:
		return value

	value -= 48

	e = 0
	while (value >= (1 << (e + 1))):
		e += 1

	base = (1 << e)
	m = (value - base) >> (e - 4)
	return (e << 4) | m


####

if __name__ == '__main__':

	if 1: # Code from an exponent and mantissa
		for e in range(16):
			for m in range(16):
				code = (e << 4) | m
				print("{} -> {} = {} -> {}".format(code, Design(e, m), Decode(code), Encode(Decode(code))))
			print("")

	if 0: # Encode/decode approach
		prev_code = -1
		for v in range(65536):
			code = Encode(v)
			decoded_v = Decode(code)
			if code != prev_code:
				print("{} -> {} -> {}".format(v, code, decoded_v))
			prev_code = code

	if 0: # 65536 values to plot (as a souvenir)
		for v in range(65536):
			code = Encode(v)
			print("{}".format(code))
