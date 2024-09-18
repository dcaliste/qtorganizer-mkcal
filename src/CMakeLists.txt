set(SRC
  mkcalplugin.cpp
  itemcalendars.cpp
  helper.cpp)
set(HEADERS
  mkcalplugin.h
  itemcalendars.h
  helper.h)

add_library(qtorganizer_mkcal SHARED ${SRC} ${HEADERS})

target_link_libraries(qtorganizer_mkcal
        Qt5::Organizer
	KF5::CalendarCore
	PkgConfig::MKCAL)

install(TARGETS qtorganizer_mkcal
	DESTINATION ${CMAKE_INSTALL_LIBDIR}/qt5/plugins/organizer)
