
Ako
===

Lossy image codec using discrete wavelet transform.

It supports/implements:
- Deslauriers-Dubuc 13/7 and CDF 5/3, wavelets.
- Configurable quality loss ([examples](#examples) below).
- 8 bits per component. 4 channels.
- Reversible YCoCg color transformation.
- Elias coding + Rle compression. Nonetheless the codec can handle ratios of 1:12 before artifacts became visible.
- Good performance. There is care on cache and memory usage.
- Everything is done with integers. This ensure always identical outputs (even in lossy compression) and provides gain in performance.

And in a more experimental state:
- Lossless compression.


Compilation
-----------
Aside from C and C++ compilers (later only needed to build executable tools), the only requirement is [cmake][1]. On Ubuntu you can install them with:

```
sudo apt install cmake
```

Then compile with:
```
mkdir build
cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build . --config Release
```


Usage
-----
Two executables: `akoenc` and `akodec` will let you try the codec. Run them whitout any argument to read the usage help. But, in most cases is:

```
akoenc -q 16 -i "input.png" -o "out.ako"
```
- Where `-q 16` is the quantization step that controls loss.
- There is also a noise gate, with `-g 16`, it can be used as a denoiser to help with compression. It is possible to use both, or disable either one with a value of zero.


Examples
--------
Following examples are mere illustrations. Chosen compression ratios are too high to be useful in real life.

Note how fine details are discarded while sharp lines and overall shapes remain intact, this whitout visible blocks. And all modern codecs do this, however here it is remarkable since Ako lacks of quality estimation or rate-distortion optimizations (tasks to improve the codec further).

> **TODO, upss, already outdated**


References
----------

At the end of the day this is a toy-project, it's me having fun while learning how image codecs work. So, my thanks to following authors:

- **ADAMS, Michael David (2002)**. Reversible Integer-to-integer Wavelet Transforms For Image Coding. University of British Columbia.
- **ARNTZEN, Hans-Kristian (2014)**. Linelet, an Ultra-Low Complexity, Ultra-Low Latency Video Codec for Adaptation of HD-SDI to Ethernet. Norwegian University of Science and Technology Department of Electronics and Telecommunications.
- **CHRISTOPOULOS, Charilaos, SKODRAS, Athanassios & EBRAHIMI, Touradj (2000)**. The JPEG2000 Still Image Coding System: An Overview. IEEE Transactions on Consumer Electronics, Vol. 46, No. 4, pp. 1103-1127.
- **DAUBECHIES, Ingrid & SWELDENS, Wim (1998)**. Factoring Wavelet Transforms Into Lifting Steps. The Journal of Fourier Analysis and Applications 4, pp. 247–269.
- **FYFFE, Graham (2016)**. GFWX: Good, Fast Wavelet Codec. ICT Tech Report ICT-TR-01-2016. University of Southern California Institute.
- **KIELY, A., KLIMESH, M. (2003)**. The ICER Progressive Wavelet Image Compressor. IPN Progress Report 42-155.
- ? (2008 September 23). **Dirac Specification Version 2.2.3**.

And in the early days of the project, when terms like "lift" and "dyadic" were incredible obscure, hardware oriented papers gave me an invaluable help:

- **AL-AZAWI, Saad, ABBAS, Yasir Amer & JIDIN, Razali. (2014)**. Low Complexity Multidimensional CDF 5/3 DWT Architecture. 9th International Symposium on Communication Systems, Networks & Digital Sign (CSNDSP).
- **ANGELOPOULOU, Maria, CHEUNG, Peter, MASSELOS, Konstantinos & ANDREOPOULOS, Yiannis (2008)**. Implementation and Comparison of the 5/3 Lifting 2D Discrete Wavelet Transform Computation Schedules on FPGAs. Journal of VLSI Signal Processing. pp. 3-21.
- **HEDGE, Shriram & RAMACHADRAN, S. (2014)**. Implementation of CDF 5/3 Wavelet Transform. International Journal of Electrical, Electronics and Data Communication. Vol. 2, No. 11, pp. 36-38.

Not directly related, but influencing the project:

- **HANS, Mat & SCHAFER, Ronald (1999)**. Lossless Compression of Digital Audio. HP Laboratories Palo Alto.
- **ROBINSON, Tony (1994)**. Shorten: Simple Lossless And Near-Lossless Waveform Compression. Cambridge University Engineering Department Technical Report CUED/F-INFENG/TR156.
- **WEINBERGER, Marcelo, SEROUSSI, Gadiel & SAPIRO, Guillermo (2000)**. The LOCO-I Lossless Image Compression Algorithm: Principles and Standardization into JPEG-LS. IEEE Transactions on Image Processing, Vol. 9, No. 8, pp. 1309-1324.
- ? (????). **FLAC Format Specification**. Retrieved from: https://xiph.org/flac/format.html.


License
-------
Source code under MIT License. Terms specified in [LICENSE][2].

Each file includes the respective notice at the beginning.

____

[1]: https://cmake.org/
[2]: ./LICENSE
