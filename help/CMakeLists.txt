############################################################################
# Copyright (c) 2010-2022 Belledonne Communications SARL.
#
# This file is part of mediastreamer2 
# (see https://gitlab.linphone.org/BC/public/mediastreamer2).
#
############################################################################
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU Affero General Public License as
# published by the Free Software Foundation, either version 3 of the
# License, or (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU Affero General Public License for more details.
#
# You should have received a copy of the GNU Affero General Public License
# along with this program. If not, see <http://www.gnu.org/licenses/>.
############################################################################

find_package(Doxygen)

if(DOXYGEN_FOUND)
	if(DOXYGEN_DOT_FOUND)
                set(top_srcdir ${PROJECT_SOURCE_DIR})
		configure_file(${CMAKE_CURRENT_SOURCE_DIR}/Doxyfile.in ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile)
		file(GLOB DOC_INPUT_FILES
			[^.]*.dox
			../include/mediastreamer2/[^.]*.h
			src/[^.]*/[^.]*.h
			src/[^.]*/[^.]*.c
		)
		add_custom_command(OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/doc/html/index.html"
			COMMAND ${DOXYGEN_EXECUTABLE} ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile
			DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile ${DOC_INPUT_FILES}
		)
		add_custom_target(mediastreamer2-html-doc ALL DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/doc/html/index.html")
		install(DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/doc/html"
			DESTINATION "${CMAKE_INSTALL_DATADIR}/doc/mediastreamer2-${MEDIASTREAMER_VERSION}")
	else()
		message(WARNING "The dot program is needed to generate the mediastreamer2 documentation. You can get it from http://www.graphviz.org/.")
	endif()
endif()
