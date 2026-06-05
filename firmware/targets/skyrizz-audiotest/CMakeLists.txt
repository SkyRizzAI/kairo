# SkyRizz E32 — Mic + Speaker bring-up test (standalone, no Kairo runtime).
# Direct ES7243E (I2S RX) + NS4168 (I2S TX), verbose serial diagnostics.
cmake_minimum_required(VERSION 3.20)
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(kairo-skyrizz-audiotest)
