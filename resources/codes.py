

def Design(e, m):
	if (e < 4):
		return (e << 4) + m

	base = (1 << e) + 48
	add = m << (e - 4)
	return base + add


def DecodeRoot(code):
	e = code >> 4
	m = code & 15

	if (e < 4):
		return (e << 4) + m

	base = (1 << e) + 48
	add = m << (e - 4)
	return base + add


def DecodeSuffixLength(code):
	e = code >> 4

	if (e < 5):
		return 0

	return e - 4


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
				print("{} -> {} = {} -> {}, sl: {}".format(code, Design(e, m), DecodeRoot(code), Encode(DecodeRoot(code)), DecodeSuffixLength(code)))
			print("")

	if 0: # Encode/decode approach
		prev_code = -1
		for v in range(65536):
			code = Encode(v)
			decoded_r = DecodeRoot(code)
			if code != prev_code:
				print("{} -> {} -> {}".format(v, code, decoded_r))
			prev_code = code

	if 0: # 65536 values to plot (as a souvenir)
		for v in range(65536):
			code = Encode(v)
			print("{}".format(code))
