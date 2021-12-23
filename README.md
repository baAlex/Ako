
Ako
===

Lossy image codec using discrete wavelet transform.

**A toy-project**. It is me learning how image codecs works, having fun following a bunch of papers. :)

It supports/implements:
- Deslauriers-Dubuc 13/7 wavelet.
- Up to 4 channels.
- 8 bits per component.
- YCoCg colorspace (can be disabled at compilation time).
- Configurable quality loss (examples below).
- Simple Elias-gamma entropy compression, nonetheless the codec can handle ratios of 1:10 before artifacts became obvious.
- A "good" performance. There is some care on cache and memory usage, and almost everything is done with integers... still there is space for improvement.


Compilation
-----------
Build requirements are [git][14] and [cmake][15] or [ninja][16]. As runtime dependency [libpng][17].

On Ubuntu you can install them with:
```
sudo apt install git ninja-build cmake libpng-dev
```

### Using Cmake
```
git clone https://github.com/baAlex/Ako
cd Ako
mkdir build
cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
cmake --build .
```

### Using Ninja
```
git clone https://github.com/baAlex/Ako
cd Ako
ninja
```


Usage
-----
The two executables `akoenc` and `akodec` will let you try the codec. Run them whitout any argument to read the usage help. But is mostly:

```
akoenc -q 16 -i "input.png" -o "out.ako"
```
- Where `-q 16` is the quantization step that controls loss.
- There is also a noise gate, with `-n 16`, that can be used as a denoiser to help with compression. Is possible to use both, or disabled either one with a value of zero.


Examples
--------
Consider the following examples as mere illustrations, the codec is constantly improving, and compression ratios are too high to be useful in real life.

For most purposes the examples are good on showcase how a simple wavelet-based codec degrades images, and what kind of artifacts expect of it.
- Note that there is no blocks.
- And how fine details are lost while sharp lines and overall shapes remain somewhat intact. All codecs do this, but is remarkable since right now there is no analysis, quality estimation nor any rate-distortion optimization (tasks to improve the codec further).

![](./resources/guanaco1-readme.png)
- [Uncompressed][1] (2.76 MB), [**1:27**][2] (100.86 kB), [**1:60**][3] (46.02 kB), [**1:146**][4] (18.92 kB)
- Using quantization steps of: 16, 32 and 64

![](./resources/kodak8-readme.png)
- [Uncompressed][5] (1.17 MB), [**1:15**][6] (78.27 kB), [**1:28**][7] (41.91 kB), [**1:60**][8] (19.62 kB)
- Using quantization steps of: 16, 32 and 64

![](./resources/vincent-readme.png)
- [Uncompressed][9] (7.14 MB), [**1:10**][10] (714.35 kB), [**1:44**][11] (163.46 kB), [**1:44**][12] (163.18 kB)
- Using quantization steps of: 2, 2 and 30
- With noise gate thresholds of: 16, 48 and 0
- Last two serve as comparison between a noise gate compression and a quantization compression, both set to an identical ratio


References
----------

- **ADAMS, Michael David (2002)**. Reversible Integer-to-integer Wavelet Transforms For Image Coding. University of British Columbia.
- **ARNTZEN, Hans-Kristian (2014)**. Linelet, an Ultra-Low Complexity, Ultra-Low Latency Video Codec for Adaptation of HD-SDI to Ethernet. Norwegian University of Science and Technology Department of Electronics and Telecommunications.
- **CHRISTOPOULOS, Charilaos, SKODRAS, Athanassios & EBRAHIMI, Touradj (2000)**. The JPEG2000 Still Image Coding System: An Overview. IEEE Transactions on Consumer Electronics, Vol. 46, No. 4, pp. 1103-1127.
- **DAUBECHIES, Ingrid & SWELDENS, Wim (1998)**. Factoring Wavelet Transforms Into Lifting Steps. The Journal of Fourier Analysis and Applications 4, pp. 247–269.
- **FYFFE, Graham (2016)**. GFWX: Good, Fast Wavelet Codec. ICT Tech Report ICT-TR-01-2016. University of Southern California Institute.
- **KIELY, A., KLIMESH, M. (2003)**. The ICER Progressive Wavelet Image Compressor. IPN Progress Report 42-155.
- ? (2008 September 23). **Dirac Specification Version 2.2.3**.


In the early days of the project, when terms like "lift" and "dyadic" were incredible obscure to me, hardware oriented papers gave me an invaluable help:

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
Source code under MIT License. Terms specified in [LICENSE][13].

Each file includes the respective notice at the beginning.

____

[1]: ./test-images/guanaco1.png
[2]: ./resources/guanaco1.akoQ16N0.png
[3]: ./resources/guanaco1.akoQ32N0.png
[4]: ./resources/guanaco1.akoQ64N0.png

[5]: ./test-images/kodak8.png
[6]: ./resources/kodak8.akoQ16N0.png
[7]: ./resources/kodak8.akoQ32N0.png
[8]: ./resources/kodak8.akoQ64N0.png

[9]: ./test-images/vincent.png
[10]: ./resources/vincent.akoQ2N16.png
[11]: ./resources/vincent.akoQ2N48.png
[12]: ./resources/vincent.akoQ30N0.png

[13]: ./LICENSE

[14]: https://git-scm.com/
[15]: https://cmake.org/
[16]: https://ninja-build.org/
[17]: http://www.libpng.org/pub/png/libpng.html
