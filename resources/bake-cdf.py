

import csv
import sys
import math

import codes as code


class CdfEntry:
	def __init__(self, symbol):
		self.s = symbol
		self.f = 0
		self.c = 0


def Print(cdf):
	print("")
	print("const CdfEntry cdf[{} + 1] =".format(len(cdf) - 1))
	print("{")

	for i in range(CDF_LEN - 1):
		print("\t{{{}, {}, {}}},".format(cdf[i].s, cdf[i].f, cdf[i].c))

	print("\t{{{}, {}, {}}}".format(cdf[-1].s, cdf[-1].f, cdf[-1].c))
	print("};")


def AddFile(cdf, filename):
	print("Reading '{}'".format(filename))
	with open(filename) as fp:
		reader = csv.reader(fp)
		for i, row in enumerate(reader): # Value implicit in row number
			cdf[code.Encode(i)].f += int(row[0]) # For now, merge both data and
			cdf[code.Encode(i)].f += int(row[1]) # instructions frequencies (each column)


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
		cdf[i].f = math.ceil(cdf[i].f / division) # Round or floor also work except that they
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
	while final_summation > normalize_to:
		final_summation -= 1
		cdf[i_plus_one].f -= 1
		if cdf[i_plus_one].f == 1:
			i_plus_one -= 1

	# Bye!
	print("Initial summation: {}".format(summation))
	print("Final summation: {} (d: {:.2f})".format(final_summation, division))


def Accumulate(cdf):
	cumulative = 0
	for i in range(CDF_LEN):
		cdf[i].c = cumulative
		cumulative += cdf[i].f


####

if __name__ == '__main__':

	CDF_LEN = 255 + 1
	cdf = [CdfEntry(code.Decode(i)) for i in range(CDF_LEN)]

	for filename in sys.argv[1:]:
		AddFile(cdf, filename)

	FillZeros(cdf)

	cdf = sorted(cdf, key = lambda entry: (-entry.f))
	Normalize(cdf)

	cdf = sorted(cdf, key = lambda entry: (-entry.f, entry.s))
	Accumulate(cdf)

	Print(cdf)
