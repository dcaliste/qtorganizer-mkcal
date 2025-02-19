cmake_minimum_required(VERSION 3.16)

project(qtorganizer-mkcal
	VERSION 0.1
	DESCRIPTION "Mkcal plugin for QtOrganizer")

set(CMAKE_AUTOMOC ON)

set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_CXX_STANDARD 11)

find_package(ECM REQUIRED NO_MODULE)
set(CMAKE_MODULE_PATH ${ECM_MODULE_PATH})

include(CTest)
include(FeatureSummary)
include(GNUInstallDirs)

set(QT_MIN_VERSION "5.6.0")
find_package(Qt5 ${QT_MIN_VERSION} COMPONENTS Organizer Test REQUIRED)
find_package(KF5 COMPONENTS CalendarCore REQUIRED)

find_package(PkgConfig REQUIRED)
pkg_check_modules(MKCAL libmkcal-qt5 IMPORTED_TARGET REQUIRED)

add_subdirectory(src)
if(BUILD_TESTING)
	add_subdirectory(tests)
endif()

feature_summary(WHAT ALL FATAL_ON_MISSING_REQUIRED_PACKAGES)
