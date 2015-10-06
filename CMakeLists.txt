CMAKE_MINIMUM_REQUIRED(VERSION 2.8)
CMAKE_POLICY(VERSION 2.8)

PROJECT(FADTTS)
OPTION(BUILD_TESTING "tests" OFF)

SET(CMAKE_MODULE_PATH
${${PROJECT_NAME}_SOURCE_DIR}/CMake
${${PROJECT_NAME}_BINARY_DIR}/CMake
${CMAKE_MODULE_PATH}
)

FIND_PACKAGE(QtToCppXML REQUIRED)
INCLUDE(${QtToCppXML_USE_FILE})

FIND_PACKAGE(Qt4 COMPONENTS QtCore QtGui REQUIRED)
INCLUDE(${QT_USE_FILE})

FIND_PACKAGE( SlicerExecutionModel REQUIRED )
INCLUDE( ${SlicerExecutionModel_USE_FILE} )

SET(FADTTS_SOURCES
FADTTSWindow.cpp
FADTTSWindowConfig.cpp
EditInputDialog.cpp
InfoDialog.cpp
Data.cpp
Processing.cpp
ScriptMatlab.cpp
)

SET(FADTTS_HEADERS
FADTTSWindow.h
FADTTSWindowConfig.h
EditInputDialog.h
InfoDialog.h
Data.h
ScriptMatlab.h
)

SET(FADTTS_FORMS
FADTTSWindow.ui
EditInputDialog.ui
InfoDialog.ui
)

SET(FADTTS_RESOURCES
icons.qrc
)

QT4_WRAP_CPP(FADTTS_HEADERS_MOC ${FADTTS_HEADERS})
QT4_WRAP_UI(FADTTS_FORMS_HEADERS ${FADTTS_FORMS})
QT4_ADD_RESOURCES(FADTTS_RESOURCES_RCC ${FADTTS_RESOURCES})

LIST( APPEND
FADTTS_SOURCES
${FADTTS_HEADERS_MOC}
${FADTTS_FORMS_HEADERS}
${FADTTS_RESOURCES_RCC}
)

QtToCppXML( SOURCES FADTTS_SOURCES NAME FADTTS UIXML_FILE FADTTSWindow.ui PREFIXES soft_ para_ )

SEMMacroBuildCLI(
NAME FADTTS
EXECUTABLE_ONLY
ADDITIONAL_SRCS ${FADTTS_SOURCES}
TARGET_LIBRARIES ${QT_LIBRARIES}
LINK_DIRECTORIES ${QT_LIBRARY_DIRS}
INCLUDE_DIRECTORIES ${QT_INCLUDE_DIR}
)

if (BUILD_TESTING)
  INCLUDE(CTest)
  ADD_SUBDIRECTORY(Testing)
endif ()