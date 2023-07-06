############################################################################
# FindTheora.cmake
# Copyright (C) 2016-2023  Belledonne Communications, Grenoble France
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
# Find the theora library.
#
# Targets
# ^^^^^^^
#
# The following targets may be defined:
#
#  theora - If the theora library has been found
#
#
# Result variables
# ^^^^^^^^^^^^^^^^
#
# This module will set the following variables in your project:
#
#  Theora_FOUND - The theora library has been found
#  Theora_TARGET - The name of the CMake target for the theora library

find_path(_Theora_INCLUDE_DIRS
	NAMES theora/theora.h
	PATH_SUFFIXES include
)
find_library(_Theora_LIBRARY
	NAMES theora
	PATH_SUFFIXES bin lib
)

if(_Theora_INCLUDE_DIRS AND _Theora_LIBRARY)
		add_library(theora UNKNOWN IMPORTED)
		if(WIN32)
			set_target_properties(theora PROPERTIES
				INTERFACE_INCLUDE_DIRECTORIES "${_Theora_INCLUDE_DIRS}"
				IMPORTED_IMPLIB "${_Theora_LIBRARY}"
			)
		else()
			set_target_properties(theora PROPERTIES
				INTERFACE_INCLUDE_DIRECTORIES "${_Theora_INCLUDE_DIRS}"
				IMPORTED_LOCATION "${_Theora_LIBRARY}"
			)
		endif()

		set(Theora_TARGET theora)
endif()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(Theora REQUIRED_VARS Theora_TARGET)
mark_as_advanced(Theora_TARGET)
