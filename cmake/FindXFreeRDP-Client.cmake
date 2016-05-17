############################################################################
# xFreeRDP-Client.txt
# Copyright (C) 2014  Belledonne Communications, Grenoble France
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
# - Find the xFreeRDP-Client library
#
#  XFREERDP-CLIENT_FOUND - system has xFreeRDP-Client
#  XFREERDP-CLIENT_LIBRARY - The libraries needed to use xFreeRDP-Client

include(CheckIncludeFile)
include(CheckSymbolExists)

find_library(XFREERDP-CLIENT_LIBRARY
	NAMES xfreerdp-client
	PATH_SUFFIXES bin lib
)

if(XFREERDP-CLIENT_LIBRARY)
	set(HAVE_XFREERDP_CLIENT 1)
endif()

find_package_handle_standard_args(XFREERDP-CLIENT
	DEFAULT_MSG
	HAVE_XFREERDP_CLIENT XFREERDP-CLIENT_LIBRARY
)

mark_as_advanced(XFREERDP-CLIENT_LIBRARY HAVE_XFREERDP_CLIENT)