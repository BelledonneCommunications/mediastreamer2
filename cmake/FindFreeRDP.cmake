############################################################################
# FindFreeRDP.txt
# Copyright (C) 2016  Belledonne Communications, Grenoble France
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
# Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
#
############################################################################
#
# - Find the FreeRdp include file and library
#
#  FREERDP_FOUND - system has FREERDP
#  FREERDP_INCLUDE_DIRS - the FREERDP include directory
#  FREERDP_LIBRARIES - The libraries needed to use FREERDP

find_path(FREERDP_INCLUDE_DIRS
	NAMES freerdp/freerdp.h
	PATH_SUFFIXES include include
	)

find_path(WINPR_INCLUDE_DIRS
	NAMES winpr/winpr.h
	PATH_SUFFIXES include include
	)

list(APPEND FREERDP_INCLUDE_DIRS ${WINPR_INCLUDE_DIRS})
list(REMOVE_DUPLICATES FREERDP_INCLUDE_DIRS)

find_library(FREERDP_LIBRARIES NAMES freerdp)
find_library(FREERDP_CLIENT_LIBRARIES NAMES freerdp-client)
find_library(FREERDP_CLIENT_X11_LIBRARIES NAMES xfreerdp-client)
find_library(FREERDP_SERVER_LIBRARIES NAMES freerdp-server)
find_library(FREERDP_SHADOW_LIBRARIES NAMES freerdp-shadow)
find_library(FREERDP_SHADOW_SUB_LIBRARIES NAMES freerdp-shadow-subsystem)

list(APPEND FREERDP_LIBRARIES ${FREERDP_CLIENT_LIBRARIES} ${FREERDP_CLIENT_X11_LIBRARIES} ${FREERDP_SERVER_LIBRARIES} ${FREERDP_SHADOW_LIBRARIES} ${FREERDP_SHADOW_SUB_LIBRARIES})

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(freerdp
	DEFAULT_MSG
	FREERDP_INCLUDE_DIRS FREERDP_LIBRARIES
)

mark_as_advanced(FREERDP_INCLUDE_DIRS FREERDP_LIBRARIES)
