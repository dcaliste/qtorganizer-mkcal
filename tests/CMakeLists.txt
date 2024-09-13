add_executable(tst_engine tst_engine.cpp)

target_link_libraries(tst_engine
	Qt5::Test
	Qt5::Organizer
        PkgConfig::MKCAL
        KF5::CalendarCore)

add_test(tst_engine tst_engine)

install(TARGETS tst_engine DESTINATION /opt/tests/qtorganizer-mkcal)
