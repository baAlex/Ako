/*

MIT License

Copyright (c) 2021-2022 Alexander Brandt

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/

#include "ako-private.hpp"

namespace ako
{

const uint32_t G_CDF1_LENGTH = 255 + 1;

const CdfEntry g_cdf1[G_CDF1_LENGTH] = {
    {0, 0, 11844, 0},      {2, 0, 8964, 11844},   {1, 0, 8230, 20808},   {4, 0, 4770, 29038},   {3, 0, 4250, 33808},
    {6, 0, 2735, 38058},   {5, 0, 2567, 40793},   {8, 0, 1846, 43360},   {7, 0, 1737, 45206},   {10, 0, 1362, 46943},
    {9, 0, 1279, 48305},   {12, 0, 1059, 49584},  {11, 0, 992, 50643},   {14, 0, 851, 51635},   {13, 0, 795, 52486},
    {16, 0, 703, 53281},   {15, 0, 656, 53984},   {18, 0, 587, 54640},   {17, 0, 549, 55227},   {20, 0, 500, 55776},
    {19, 0, 466, 56276},   {22, 0, 431, 56742},   {21, 0, 400, 57173},   {24, 0, 373, 57573},   {23, 0, 347, 57946},
    {26, 0, 326, 58293},   {25, 0, 303, 58619},   {28, 0, 286, 58922},   {27, 0, 266, 59208},   {30, 0, 253, 59474},
    {29, 0, 236, 59727},   {32, 0, 226, 59963},   {31, 0, 210, 60189},   {34, 0, 203, 60399},   {33, 0, 188, 60602},
    {36, 0, 182, 60790},   {35, 0, 170, 60972},   {38, 0, 164, 61142},   {37, 0, 152, 61306},   {40, 0, 148, 61458},
    {39, 0, 139, 61606},   {42, 0, 135, 61745},   {41, 0, 127, 61880},   {44, 0, 122, 62007},   {43, 0, 114, 62129},
    {46, 0, 112, 62243},   {45, 0, 106, 62355},   {48, 0, 100, 62461},   {47, 0, 97, 62561},    {50, 0, 93, 62658},
    {49, 0, 89, 62751},    {52, 0, 85, 62840},    {51, 0, 82, 62925},    {54, 0, 78, 63007},    {53, 0, 76, 63085},
    {56, 0, 72, 63161},    {55, 0, 70, 63233},    {58, 0, 66, 63303},    {57, 0, 64, 63369},    {60, 0, 61, 63433},
    {59, 0, 60, 63494},    {80, 1, 58, 63554},    {62, 0, 57, 63612},    {61, 0, 56, 63669},    {82, 1, 55, 63725},
    {64, 0, 53, 63780},    {84, 1, 52, 63833},    {63, 0, 51, 63885},    {66, 0, 49, 63936},    {65, 0, 48, 63985},
    {86, 1, 48, 64033},    {67, 0, 45, 64081},    {68, 0, 45, 64126},    {88, 1, 45, 64171},    {112, 2, 43, 64216},
    {69, 0, 42, 64259},    {70, 0, 42, 64301},    {90, 1, 42, 64343},    {92, 1, 40, 64385},    {71, 0, 39, 64425},
    {72, 0, 39, 64464},    {116, 2, 39, 64503},   {73, 0, 37, 64542},    {74, 0, 37, 64579},    {94, 1, 37, 64616},
    {96, 1, 35, 64653},    {75, 0, 34, 64688},    {76, 0, 34, 64722},    {120, 2, 34, 64756},   {98, 1, 33, 64790},
    {77, 0, 32, 64823},    {78, 0, 32, 64855},    {100, 1, 31, 64887},   {124, 2, 31, 64918},   {79, 0, 30, 64949},
    {102, 1, 29, 64979},   {104, 1, 28, 65008},   {128, 2, 28, 65036},   {106, 1, 25, 65064},   {108, 1, 25, 65089},
    {132, 2, 25, 65114},   {110, 1, 23, 65139},   {136, 2, 23, 65162},   {140, 2, 20, 65185},   {144, 2, 18, 65205},
    {148, 2, 16, 65223},   {176, 3, 16, 65239},   {152, 2, 15, 65255},   {156, 2, 13, 65270},   {184, 3, 13, 65283},
    {160, 2, 12, 65296},   {164, 2, 6, 65308},    {168, 2, 1, 65314},    {172, 2, 1, 65315},    {192, 3, 1, 65316},
    {200, 3, 1, 65317},    {208, 3, 1, 65318},    {216, 3, 1, 65319},    {224, 3, 1, 65320},    {232, 3, 1, 65321},
    {240, 3, 1, 65322},    {248, 3, 1, 65323},    {256, 3, 1, 65324},    {264, 3, 1, 65325},    {272, 3, 1, 65326},
    {280, 3, 1, 65327},    {288, 3, 1, 65328},    {296, 3, 1, 65329},    {304, 4, 1, 65330},    {320, 4, 1, 65331},
    {336, 4, 1, 65332},    {352, 4, 1, 65333},    {368, 4, 1, 65334},    {384, 4, 1, 65335},    {400, 4, 1, 65336},
    {416, 4, 1, 65337},    {432, 4, 1, 65338},    {448, 4, 1, 65339},    {464, 4, 1, 65340},    {480, 4, 1, 65341},
    {496, 4, 1, 65342},    {512, 4, 1, 65343},    {528, 4, 1, 65344},    {544, 4, 1, 65345},    {560, 5, 1, 65346},
    {592, 5, 1, 65347},    {624, 5, 1, 65348},    {656, 5, 1, 65349},    {688, 5, 1, 65350},    {720, 5, 1, 65351},
    {752, 5, 1, 65352},    {784, 5, 1, 65353},    {816, 5, 1, 65354},    {848, 5, 1, 65355},    {880, 5, 1, 65356},
    {912, 5, 1, 65357},    {944, 5, 1, 65358},    {976, 5, 1, 65359},    {1008, 5, 1, 65360},   {1040, 5, 1, 65361},
    {1072, 6, 1, 65362},   {1136, 6, 1, 65363},   {1200, 6, 1, 65364},   {1264, 6, 1, 65365},   {1328, 6, 1, 65366},
    {1392, 6, 1, 65367},   {1456, 6, 1, 65368},   {1520, 6, 1, 65369},   {1584, 6, 1, 65370},   {1648, 6, 1, 65371},
    {1712, 6, 1, 65372},   {1776, 6, 1, 65373},   {1840, 6, 1, 65374},   {1904, 6, 1, 65375},   {1968, 6, 1, 65376},
    {2032, 6, 1, 65377},   {2096, 7, 1, 65378},   {2224, 7, 1, 65379},   {2352, 7, 1, 65380},   {2480, 7, 1, 65381},
    {2608, 7, 1, 65382},   {2736, 7, 1, 65383},   {2864, 7, 1, 65384},   {2992, 7, 1, 65385},   {3120, 7, 1, 65386},
    {3248, 7, 1, 65387},   {3376, 7, 1, 65388},   {3504, 7, 1, 65389},   {3632, 7, 1, 65390},   {3760, 7, 1, 65391},
    {3888, 7, 1, 65392},   {4016, 7, 1, 65393},   {4144, 8, 1, 65394},   {4400, 8, 1, 65395},   {4656, 8, 1, 65396},
    {4912, 8, 1, 65397},   {5168, 8, 1, 65398},   {5424, 8, 1, 65399},   {5680, 8, 1, 65400},   {5936, 8, 1, 65401},
    {6192, 8, 1, 65402},   {6448, 8, 1, 65403},   {6704, 8, 1, 65404},   {6960, 8, 1, 65405},   {7216, 8, 1, 65406},
    {7472, 8, 1, 65407},   {7728, 8, 1, 65408},   {7984, 8, 1, 65409},   {8240, 9, 1, 65410},   {8752, 9, 1, 65411},
    {9264, 9, 1, 65412},   {9776, 9, 1, 65413},   {10288, 9, 1, 65414},  {10800, 9, 1, 65415},  {11312, 9, 1, 65416},
    {11824, 9, 1, 65417},  {12336, 9, 1, 65418},  {12848, 9, 1, 65419},  {13360, 9, 1, 65420},  {13872, 9, 1, 65421},
    {14384, 9, 1, 65422},  {14896, 9, 1, 65423},  {15408, 9, 1, 65424},  {15920, 9, 1, 65425},  {16432, 10, 1, 65426},
    {17456, 10, 1, 65427}, {18480, 10, 1, 65428}, {19504, 10, 1, 65429}, {20528, 10, 1, 65430}, {21552, 10, 1, 65431},
    {22576, 10, 1, 65432}, {23600, 10, 1, 65433}, {24624, 10, 1, 65434}, {25648, 10, 1, 65435}, {26672, 10, 1, 65436},
    {27696, 10, 1, 65437}, {28720, 10, 1, 65438}, {29744, 10, 1, 65439}, {30768, 10, 1, 65440}, {31792, 10, 1, 65441},
    {32816, 11, 1, 65442}, {34864, 11, 1, 65443}, {36912, 11, 1, 65444}, {38960, 11, 1, 65445}, {41008, 11, 1, 65446},
    {43056, 11, 1, 65447}, {45104, 11, 1, 65448}, {47152, 11, 1, 65449}, {49200, 11, 1, 65450}, {51248, 11, 1, 65451},
    {53296, 11, 1, 65452}, {55344, 11, 1, 65453}, {57392, 11, 1, 65454}, {59440, 11, 1, 65455}, {61488, 11, 1, 65456},
    {63536, 11, 1, 65457}};

}; // namespace ako
