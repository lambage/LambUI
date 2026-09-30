include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

install(TARGETS lambui
    EXPORT LambUITargets
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

install(DIRECTORY ${PROJECT_SOURCE_DIR}/include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
install(FILES ${PROJECT_BINARY_DIR}/generated/lambui_export.h
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/LambUI
)

install(EXPORT LambUITargets
    FILE LambUITargets.cmake
    NAMESPACE LambUI::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/LambUI
)

configure_package_config_file(
    ${PROJECT_SOURCE_DIR}/cmake/LambUIConfig.cmake.in
    ${PROJECT_BINARY_DIR}/LambUIConfig.cmake
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/LambUI
)

write_basic_package_version_file(
    ${PROJECT_BINARY_DIR}/LambUIConfigVersion.cmake
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)

install(FILES
    ${PROJECT_BINARY_DIR}/LambUIConfig.cmake
    ${PROJECT_BINARY_DIR}/LambUIConfigVersion.cmake
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/LambUI
)
