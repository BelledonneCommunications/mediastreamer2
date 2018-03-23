############################################################################
# FindZxing.txt
# Copyright (C) 2018  Belledonne Communications, Grenoble France
#
############################################################################
#
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License
# as published by the Free Software Foundation; either version 2
# of the License, or (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
#
############################################################################
#
# - Find the zxing include file and library
#
#  ZXING_FOUND - system has zxing
#  ZXING_INCLUDE_DIRS - the zxing include directory
#  ZXING_LIBRARIES - The libraries needed to use zxing

find_path(ZXING_INCLUDE_DIRS
	NAMES
		zxing/common/Counted.h
		zxing/Binarizer.h
		zxing/MultiFormatReader.h
		zxing/Result.h
		zxing/ReaderException.h
		zxing/common/GlobalHistogramBinarizer.h
		zxing/common/HybridBinarizer.h
		zxing/common/IllegalArgumentException.h
		zxing/BinaryBitmap.h
		zxing/DecodeHints.h
		zxing/qrcode/QRCodeReader.h
	PATH_SUFFIXES include
)

find_library(ZXING_LIBRARIES
	NAMES zxing libzxing
	PATH_SUFFIXES bin lib
)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(Zxing
	DEFAULT_MSG
	ZXING_INCLUDE_DIRS ZXING_LIBRARIES
)

mark_as_advanced(ZXING_INCLUDE_DIRS ZXING_LIBRARIES)

