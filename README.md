
Ako
===

Lossy image codec using discrete wavelet transform.

It supports/implements:
- Deslauriers-Dubuc 13/7 and CDF 5/3, wavelets.
- Configurable quality loss ([examples](#examples) below).
- 8 bits per component. 4 channels.
- Reversible YCoCg color transformation.
- Elias coding + Rle compression. Nonetheless can handle ratios of 1:12 before artifacts became visible.
- Good performance. There is care on cache and memory usage.
- Everything is done with integers. Ensure always identical outputs (even in lossy compression) and provides gain in performance.

And in an experimental state:
- Lossless compression.


Compilation
-----------
Aside from C and C++ compilers (later only needed to build executable tools), the only requirement is [cmake][1]. On Ubuntu you can install it with:

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
Two executables: `akoenc` and `akodec` will let you try the codec. Run them without any argument to read the usage help. But, in most cases is:

```
akoenc -q 16 -i "input.png" -o "out.ako"
```
- Where `-q 16` is the quantization step that controls loss.
- There is also a noise gate, with `-g 16`, it can be used as a denoiser to help with compression. It is possible to use both, or disable either one with a value of zero.


Examples
--------

### Cafe

| 12:1 (327.74 kB) | 24:1 (163.12 kB) | 48:1 (81.86 kB) |
| ---------------- | ---------------- | --------------- |
| [![cafe-thumb-12](https://user-images.githubusercontent.com/6278300/163314607-5b6d2a36-0825-47ed-921c-bea1bf0bea43.png)][5] | [![cafe-thumb-24](https://user-images.githubusercontent.com/6278300/163314614-4f64cc19-5ae1-47e4-877a-dca7b27df758.png)][6] | [![cafe-thumb-48](https://user-images.githubusercontent.com/6278300/163314619-8096aa3b-1ae7-4c96-ab02-1848dff241b9.png)][7] |

- From [Rec. ITU-T T.24](https://www.itu.int/net/itu-t/sigdb/genimage/T24-25.htm), 50% resized.
- [Uncompressed][4]: 3932.16 kB (3.9 MB), 1024x1280 px.


### Tractor

| 12:1 (973.95 kB) | 24:1 (506.31 kB) | 48:1 (252.05 kB) |
| ---------------- | ---------------- | ---------------- |
| [![tractor-thumb12](https://user-images.githubusercontent.com/6278300/163320662-eb990d27-3d95-4059-8002-dc6426640bd2.png)][9] | [![tractor-thumb24](https://user-images.githubusercontent.com/6278300/163320676-8c24c9dc-fcad-4834-9ff7-575425dc6ef8.png)][10] | [![tractor-thumb48](https://user-images.githubusercontent.com/6278300/163320856-f88460d1-fdd0-4640-add4-9dc67a3b9bdd.png)][11] |

- From [UNISI & UNIFI Dataset, University of Siena](http://clem.dii.unisi.it/~vipp/datasets.html), 50% resized.
- [Uncompressed][8]: 12063.74 kB (12 MB), 1632x2464 px.


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

[4]: https://user-images.githubusercontent.com/6278300/163313300-3123a313-134c-4d04-8c7d-57f64ce5dcb3.png
[5]: https://user-images.githubusercontent.com/6278300/163313563-27de4565-a0c1-4b5b-8fcb-565c1de7caf7.png
[6]: https://user-images.githubusercontent.com/6278300/163313671-b8ff9090-7d1f-4184-ab2f-699d66384c26.png
[7]: https://user-images.githubusercontent.com/6278300/163313720-99c4ea37-5480-42d2-9803-80aa3f6c6aeb.png

[8]: https://user-images.githubusercontent.com/6278300/163320329-2ca14c0c-d75e-4aa2-b2de-aa96a01908a2.png
[9]: https://user-images.githubusercontent.com/6278300/163320344-3241f9b7-5b71-4942-b58d-ef78ffed0e1f.png
[10]: https://user-images.githubusercontent.com/6278300/163320369-2e650566-408a-4999-9a09-2d0d8d7a2ca1.png
[11]: https://user-images.githubusercontent.com/6278300/163320386-249ca30c-3007-4b12-8b9f-026519dce9d7.png
