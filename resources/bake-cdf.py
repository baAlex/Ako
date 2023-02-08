

import csv
import sys
import math

import codes as code


class CdfEntry:
	def __init__(self, c):
		self.r = code.DecodeRoot(c)
		self.sl = code.DecodeSuffixLength(c)
		self.f = 0
		self.c = 0


def Print(cdf):
	print("")
	print("const CdfEntry cdf[{} + 1] =".format(len(cdf) - 1))
	print("{")

	for i in range(CDF_LEN - 1):
		print("\t{{{}, {}, {}, {}}},".format(cdf[i].r, cdf[i].sl, cdf[i].f, cdf[i].c))

	print("\t{{{}, {}, {}, {}}}".format(cdf[-1].r, cdf[-1].sl, cdf[-1].f, cdf[-1].c))
	print("};")


def AddFile(cdf, filename, row_no):
	print("Reading '{}' row {}".format(filename, row_no))
	with open(filename) as fp:
		reader = csv.reader(fp)
		for v, row in enumerate(reader): # Value implicit in row number
			cdf[code.Encode(v)].f += int(row[row_no])


def FillZeros(cdf):
	for i in range(0, 2):
		if cdf[i].f == 0:
			cdf[i].f = (cdf[i + 1].f + cdf[i + 2].f + 2) >> 1

	for i in range(2, CDF_LEN - 2):
		if cdf[i].f == 0:
			cdf[i].f = (cdf[i - 2].f + cdf[i - 1].f + cdf[i + 1].f + cdf[i + 2].f + 2) >> 2

	for i in range(CDF_LEN - 2, CDF_LEN):
		if cdf[i].f == 0:
			cdf[i].f = (cdf[i - 2].f + cdf[i - 1].f + cdf[min(i + 1, CDF_LEN - 1)].f + cdf[min(i + 2, CDF_LEN - 1)].f + 2) >> 2


def Normalize(cdf, normalize_to = 65535):

	# Summate a first time
	summation = 0
	for i in range(CDF_LEN):
		summation += cdf[i].f

	if summation < normalize_to:
		raise("No enough data") # An exception inside other exception

	# Normalize
	division = summation / normalize_to
	final_summation = 0
	i_plus_one = 0

	for i in range(CDF_LEN):
		cdf[i].f = int(math.ceil(cdf[i].f / division))
		# Round or floor also work except that they
		# may yield a summation less that 'normalize_to',
		# wasting precision bits. Also it is easy to fix
		# an excess that a lack.

		if cdf[i].f != 0:
			final_summation += cdf[i].f
			if cdf[i].f > 1:
				i_plus_one = i
		else:
			cdf[i].f += 1
			cdf[i_plus_one].f -= 1
			if cdf[i_plus_one].f == 1:
				i_plus_one -= 1

	# Fix excess, as there is no perfect normalization
	excess_fixes = 0
	while final_summation > normalize_to:
		excess_fixes += 1
		final_summation -= 1
		cdf[i_plus_one].f -= 1
		if cdf[i_plus_one].f == 1:
			i_plus_one -= 1

	# Bye!
	print("Initial summation: {}".format(summation))
	print("Final summation: {} (d: {:.2f}, e: {})".format(final_summation, division, excess_fixes))


def Accumulate(cdf):
	cumulative = 0
	for i in range(CDF_LEN):
		cdf[i].c = cumulative
		cumulative += cdf[i].f


####

if __name__ == '__main__':

	CDF_LEN = 255 + 1
	c_cdf = [CdfEntry(i) for i in range(CDF_LEN)]
	d_cdf = [CdfEntry(i) for i in range(CDF_LEN)]

	for filename in sys.argv[1:]:
		AddFile(c_cdf, filename, 0)
		AddFile(d_cdf, filename, 1)

	FillZeros(c_cdf)
	c_cdf = sorted(c_cdf, key = lambda entry: (-entry.f))
	Normalize(c_cdf)
	c_cdf = sorted(c_cdf, key = lambda entry: (-entry.f, entry.r))
	Accumulate(c_cdf)

	FillZeros(d_cdf)
	d_cdf = sorted(d_cdf, key = lambda entry: (-entry.f))
	Normalize(d_cdf)
	d_cdf = sorted(d_cdf, key = lambda entry: (-entry.f, entry.r))
	Accumulate(d_cdf)

	Print(c_cdf)
	Print(d_cdf)
