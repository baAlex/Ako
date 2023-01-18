

def DecodeRoot(code):
	if code < 247:
		return code
	return 0 # TODO


def DecodeSuffixLength(code):
	if code < 247:
		return 0
	return code - 247 + 8


def Encode(value):
	if value < 247:
		return value

	e = 0
	while (value >= (1 << e)):
		e += 1

	return 247 + e - 8


####

if __name__ == '__main__':

	if 1:
		prev_code = -1

		for v in range(65536):
			code = Encode(v)

			if code != prev_code:
				print("{} -> {},\troot: {}, suffix: {} bits".format(v, code, DecodeRoot(code), DecodeSuffixLength(code)))
				prev_code = code
