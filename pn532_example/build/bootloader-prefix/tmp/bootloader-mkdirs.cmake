# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.

cmake_minimum_required(VERSION 3.5)

file(MAKE_DIRECTORY
  "C:/Users/romanchekk/esp/v5.2/esp-idf/components/bootloader/subproject"
  "D:/Global/pn532_example/build/bootloader"
  "D:/Global/pn532_example/build/bootloader-prefix"
  "D:/Global/pn532_example/build/bootloader-prefix/tmp"
  "D:/Global/pn532_example/build/bootloader-prefix/src/bootloader-stamp"
  "D:/Global/pn532_example/build/bootloader-prefix/src"
  "D:/Global/pn532_example/build/bootloader-prefix/src/bootloader-stamp"
)

set(configSubDirs )
foreach(subDir IN LISTS configSubDirs)
    file(MAKE_DIRECTORY "D:/Global/pn532_example/build/bootloader-prefix/src/bootloader-stamp/${subDir}")
endforeach()
if(cfgdir)
  file(MAKE_DIRECTORY "D:/Global/pn532_example/build/bootloader-prefix/src/bootloader-stamp${cfgdir}") # cfgdir has leading slash
endif()
