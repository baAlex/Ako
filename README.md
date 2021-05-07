
Ako
===

Image codec using discrete wavelet transform (CDF 5/3), on top of [LZ4][12] compression (a Lempel-Ziv based algorithm).

**A toy-project/experiment**. It is me learning how image codecs works, having some fun following a bunch of papers. :)

It supports/implements:
- Image sizes power of two (4, 8, ..., 256, 512, 1024, etc.)
- Up to 4 channels
- YCoCg colorspace (can be disabled at compilation time)
- Configurable quality loss (some examples below)
- A Lempel-Ziv based compression is not the ideal, nonetheless the codec can handle ratios of **1:10** before artifacts became obvious
- A "good" performance. Except some care on cache and memory usage, there is no optimization done at the moment


Compilation
-----------
The requirements are [libpng][13] and [ninja][14]. On Ubuntu you can install them with:
```
sudo apt install ninja-build libpng-dev
```

To compile with:
```
git clone https://github.com/baAlex/Ako
cd Ako
ninja
```


Usage
-----
The two executables `akoenc` and `akodec` will let you try the codec. Execute them whitout any argument to read the full usage help. But is mostly:

```
akoenc -g 16 -i "input.png" -o "out.ako"
```
Where `-g 16` is the threshold of a noise gate that controls the loss.


Examples
--------
Please consider the following examples as mere illustrations, the codec is constantly improving.

![](./resources/guanaco-readme.png)
- [Uncompressed][1] (3.14 MB), [**1:17**][2] (183.84 kB), [**1:30**][3] (102.85 kB)
- Using noise gate thresholds: 16 and 32

![](./resources/kodak8-readme.png)
- [Uncompressed][4] (786.45 kB), [**1:10**][5] (78.73 kB), [**1:17**][6] (46.41 kB)
- Using noise gate thresholds: 16 and 32

![](./resources/cafe-readme.png)
- [Uncompressed][7] (12.6 MB), [**1:17**][8] (700.57 kB), [**1:29**][9] (430.95 kB)
- Using noise gate thresholds: 40 and 70


License
-------
- Copyright (c) 2021 Alexander Brandt. Source code under **MIT License**.
- Copyright (c) 2011-2020, Yann Collet. Under **BSD 2-Clause license**.

Terms specified in [LICENSE][10] and [THIRDPARTY_NOTICES][11]. Files in folder "library" under a combination of MIT and BSD 2-Clause licences. Each file includes the respective notice at the beginning.

____

[1]: ./test-images/guanaco.png
[2]: ./resources/guanaco-g16.png
[3]: ./resources/guanaco-g32.png

[4]: ./test-images/kodak8.png
[5]: ./resources/kodak8-g16.png
[6]: ./resources/kodak8-g32.png

[7]: ./test-images/cafe.png
[8]: ./resources/cafe-g40.png
[9]: ./resources/cafe-g70.png

[10]: ./LICENSE
[11]: ./THIRDPARTY_NOTICES

[12]: https://github.com/lz4/lz4
[13]: http://www.libpng.org/pub/png/libpng.html
[14]: https://ninja-build.org/
