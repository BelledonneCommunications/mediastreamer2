############################################################################
# FindSupport.cmake
# Copyright (C) 2017-2023  Belledonne Communications, Grenoble France
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
# - Find the Android support include file and library
#
#  SUPPORT_FOUND - system has libsupport
#  SUPPORT_LIBRARIES - The libraries needed to use libsupport

if(TARGET support)

	set(SUPPORT_LIBRARIES support)

else()

	find_library(SUPPORT_LIBRARIES
		NAMES support
	)

endif()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(Support
	DEFAULT_MSG
	SUPPORT_LIBRARIES
)

mark_as_advanced(SUPPORT_LIBRARIES)
