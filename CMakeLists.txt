cmake_minimum_required(VERSION 2.8)
cmake_policy(VERSION 2.8)

project(FADTTS)
option(BUILD_TESTING "tests" ON)
option(CREATE_BUNDLE "Create MACOSX_BUNDLE" OFF)

# Setting paths
set(CMAKE_MODULE_PATH
${${PROJECT_NAME}_SOURCE_DIR}/CMake
${${PROJECT_NAME}_BINARY_DIR}/CMake
${CMAKE_MODULE_PATH}
)

# find Qt5 headers
find_package(Qt5 COMPONENTS Widgets OpenGL REQUIRED)

include_directories(${Qt5Widgets_INCLUDE_DIRS})
add_definitions(${Qt5Widgets_DEFINITIONS})
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${Qt5Widgets_EXECUTABLE_COMPILE_FLAGS}")
set(QT_LIBRARIES ${Qt5Widgets_LIBRARIES})

# find VTK headers
find_package(VTK REQUIRED)
include(${VTK_USE_FILE})

string(FIND "${VTK_MODULES_ENABLED}" "GL2PS" index)

if( NOT ${index} EQUAL -1)
  add_definitions("-DGL2PSENABLED")
endif()

# find SlicerExecutionModel headers
find_package(SlicerExecutionModel REQUIRED)
include(${SlicerExecutionModel_USE_FILE})

add_subdirectory(src)

# Generate a bundle
if(CREATE_BUNDLE)

  if(APPLE)
    set(OS_BUNDLE MACOSX_BUNDLE)
  elseif(WIN32)
    set(OS_BUNDLE WIN32)
  endif()

  set(bundle_name FADTTSter${version})

  #--------------------------------------------------------------------------------
  SET(qtconf_dest_dir bin)
  SET(APPS "\${CMAKE_INSTALL_PREFIX}/bin/${bundle_name}")
  IF(APPLE)
    SET(qtconf_dest_dir ${bundle_name}.app/Contents/Resources)
    SET(APPS "\${CMAKE_INSTALL_PREFIX}/${bundle_name}.app")
  ENDIF(APPLE)
  IF(WIN32)
    SET(APPS "\${CMAKE_INSTALL_PREFIX}/bin/${bundle_name}.exe")
  ENDIF(WIN32)

  set(MACOSX_BUNDLE_BUNDLE_NAME "${bundle_name}")
  set(MACOSX_BUNDLE_INFO_STRING "FADTTS: functional analysis of diffusion tensor tract statistics.")
  set(MACOSX_BUNDLE_ICON_FILE "")
  set(MACOSX_BUNDLE_GUI_IDENTIFIER "niral.unc.edu.${bundle_name}")
  set(MACOSX_BUNDLE_LONG_VERSION_STRING "FADTTSter version - ${version}")
  set(MACOSX_BUNDLE_SHORT_VERSION_STRING "${version}")
  set(MACOSX_BUNDLE_BUNDLE_VERSION "${version}")
  set(MACOSX_BUNDLE_COPYRIGHT "Copyright 2016 University of North Carolina , Chapel Hill.")
     

  include_directories(${CMAKE_CURRENT_BINARY_DIR})
  add_executable(${bundle_name} ${OS_BUNDLE}
    ${FADTTS_src}
  )
  target_link_libraries(${bundle_name} ${QT_LIBRARIES} ${VTK_LIBRARIES})

  #--------------------------------------------------------------------------------
  # Install the QtTest application, on Apple, the bundle is at the root of the
  # install tree, and on other platforms it'll go into the bin directory.
  INSTALL(TARGETS ${bundle_name}
    DESTINATION . COMPONENT Runtime
    RUNTIME DESTINATION bin COMPONENT Runtime
  )

  macro(install_qt5_plugin _qt_plugin_name _qt_plugins_var)
    get_target_property(_qt_plugin_path "${_qt_plugin_name}" LOCATION)
    if(EXISTS "${_qt_plugin_path}")
      get_filename_component(_qt_plugin_file "${_qt_plugin_path}" NAME)
      get_filename_component(_qt_plugin_type "${_qt_plugin_path}" PATH)
      get_filename_component(_qt_plugin_type "${_qt_plugin_type}" NAME)
      set(_qt_plugin_dest "${bundle_name}.app/Contents/PlugIns/${_qt_plugin_type}")
      install(FILES "${_qt_plugin_path}"
        DESTINATION "${_qt_plugin_dest}"
        COMPONENT Runtime)
      set(${_qt_plugins_var}
        "${${_qt_plugins_var}};\${CMAKE_INSTALL_PREFIX}/${_qt_plugin_dest}/${_qt_plugin_file}")
    else()
      message(FATAL_ERROR "QT plugin ${_qt_plugin_name} not found")
    endif()
  endmacro()
  install_qt5_plugin("Qt5::QCocoaIntegrationPlugin" QT_PLUGINS)

  #--------------------------------------------------------------------------------
  # install a qt.conf file
  # this inserts some cmake code into the install script to write the file
  INSTALL(CODE "
      file(WRITE \"\${CMAKE_INSTALL_PREFIX}/${qtconf_dest_dir}/qt.conf\" \"[Paths]\nPlugins = PlugIns\n\")
      " COMPONENT Runtime)

  # Install the license
  INSTALL(FILES 
    ${CMAKE_CURRENT_SOURCE_DIR}/License.txt
    DESTINATION "${CMAKE_INSTALL_PREFIX}/${qtconf_dest_dir}"
    COMPONENT Runtime)
 
  get_target_property(Qt5_location Qt5::Widgets LOCATION)
  string(FIND ${Qt5_location} "/QtWidgets" length)
  string(SUBSTRING ${Qt5_location} 0 ${length} Qt5_location)
  #Fix the bundle
  install(CODE "
    include(BundleUtilities)
    fixup_bundle(\"${APPS}\" \"${QT_PLUGINS};\" \"${Qt5_location}\")
   "
    COMPONENT Runtime)
 
# To Create a package, one can run "cpack -G DragNDrop CPackConfig.cmake" on Mac OS X
# where CPackConfig.cmake is created by including CPack
# And then there's ways to customize this as well
set(CPACK_BINARY_DRAGNDROP ON)
include(CPack)

endif()
