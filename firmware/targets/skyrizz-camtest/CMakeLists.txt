# SkyRizz E32 — Camera + Display bring-up test (standalone, no Kairo runtime).
# Direct ILI9341 + GC2145 DVP, verbose serial diagnostics.
cmake_minimum_required(VERSION 3.20)
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(kairo-skyrizz-camtest)
