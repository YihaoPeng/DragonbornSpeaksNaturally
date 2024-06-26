cmake_minimum_required(VERSION 3.5)

project(dsn_service LANGUAGES CSharp)

include(CSharpUtilities)
set(CMAKE_CSharp_FLAGS "/langversion:6 /platform:anycpu /define:TRACE")


###############
# Source codes and Targets
###############

file(GLOB_RECURSE DSN_SERVICE_SRC
    dsn_service/*.cs
    dsn_service/*.xml
    dsn_service/*.config
    dsn_service/*.txt
)

add_executable(dsn_service ${DSN_SERVICE_SRC})


###############
# NuGet package restore
###############

add_custom_target(Please_Reinstall_NuGet_Packages_Manually COMMAND
    echo ========================================================================================================================== &&
    echo For technical reasons, CMake can't automatically restore the NuGet package and update references for project dsn_service.  &&
    echo If you met assembly missing, please restore NuGet packages manually and run this command in NuGet Package Manager Console: &&
    echo Update-Package -reinstall -projectname dsn_service                                                                         &&
    echo ==========================================================================================================================
)

configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/dsn_service/packages.config
    ${CMAKE_CURRENT_BINARY_DIR}/packages.config
    COPYONLY
)

configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/dsn_service/FodyWeavers.xml
    ${CMAKE_CURRENT_BINARY_DIR}/FodyWeavers.xml
    COPYONLY
)

add_dependencies(dsn_service Please_Reinstall_NuGet_Packages_Manually)


###############
# Project Information
###############

set(VS_DOTNET_REFERENCES
    Microsoft.CSharp
    System
    System.Core
    System.Data
    System.Data.DataSetExtensions
    System.Speech
    System.Web.Extensions
    System.Xml
    System.Xml.Linq
)

set_target_properties(dsn_service PROPERTIES
  VS_GLOBAL_ProjectGuid "{DEA491EE-C426-4B79-A443-CF5B1D795288}"
  VS_DOTNET_TARGET_FRAMEWORK_VERSION "v4.8"
  VS_DOTNET_REFERENCES "${VS_DOTNET_REFERENCES}"
  OUTPUT_NAME "DragonbornSpeaksNaturally"
)


###############
# Install and Package
###############

if (SVR_DIR)
    set(SVR_PLUGIN_DIR "${SVR_DIR}/Data/Plugins/Sumwunn/")
    message("-- SkyrimVR plugin install path: ${SVR_PLUGIN_DIR}")

    add_custom_command(
        TARGET dsn_service POST_BUILD VERBATIM
        COMMAND
            ${CMAKE_COMMAND} -E copy "$<TARGET_FILE:dsn_service>" ${SVR_PLUGIN_DIR} &&
            echo file copied: "$<TARGET_FILE:dsn_service> -> ${SVR_PLUGIN_DIR}"
    )
endif()

if (SSE_DIR)
    set(SSE_PLUGIN_DIR "${SSE_DIR}/Data/Plugins/Sumwunn/")
    message("-- SkyrimSE plugin install path: ${SSE_PLUGIN_DIR}")

    add_custom_command(
        TARGET dsn_service POST_BUILD VERBATIM
        COMMAND
            ${CMAKE_COMMAND} -E copy "$<TARGET_FILE:dsn_service>" ${SSE_PLUGIN_DIR} &&
            echo file copied: "$<TARGET_FILE:dsn_service> -> ${SSE_PLUGIN_DIR}"
    )
endif()

if (SAE_DIR)
    set(SAE_PLUGIN_DIR "${SAE_DIR}/Data/Plugins/Sumwunn/")
    message("-- SkyrimAE plugin install path: ${SAE_PLUGIN_DIR}")

    add_custom_command(
        TARGET dsn_service POST_BUILD VERBATIM
        COMMAND
            ${CMAKE_COMMAND} -E copy "$<TARGET_FILE:dsn_service>" ${SAE_PLUGIN_DIR} &&
            echo file copied: "$<TARGET_FILE:dsn_service> -> ${SAE_PLUGIN_DIR}"
    )
endif()

#
# ZIP Package 
#
option(PACKAGE "Generate NMM/Vortex Compatible ZIP Package" ON)
if (PACKAGE)
    if (NOT IS_SUB_PROJECT)
        message("-- Generate NMM/Vortex Compatible ZIP Package: On (-DPACKAGE=ON)")
    endif()

    set(CMAKE_INSTALL_PREFIX ${CMAKE_CURRENT_BINARY_DIR}/package_tmp)

    # dsn_service exe
    install(
        TARGETS dsn_service
        RUNTIME DESTINATION SkyrimVR/Data/Plugins/Sumwunn
    )
    install(
        TARGETS dsn_service
        RUNTIME DESTINATION SkyrimSE/Data/Plugins/Sumwunn
    )
    install(
        TARGETS dsn_service
        RUNTIME DESTINATION SkyrimAE/Data/Plugins/Sumwunn
    )

    # Resources dir
    install(
        DIRECTORY
            ${CMAKE_CURRENT_SOURCE_DIR}/Resources
        DESTINATION SkyrimVR/Data/Plugins/Sumwunn
    )
    install(
        DIRECTORY
            ${CMAKE_CURRENT_SOURCE_DIR}/Resources
        DESTINATION SkyrimSE/Data/Plugins/Sumwunn
    )
    install(
        DIRECTORY
            ${CMAKE_CURRENT_SOURCE_DIR}/Resources
        DESTINATION SkyrimAE/Data/Plugins/Sumwunn
    )

    set(CPACK_GENERATOR ZIP)
    set(CPACK_INCLUDE_TOPLEVEL_DIRECTORY OFF)
    if (NOT IS_SUB_PROJECT)
        include(CPack)
    endif()
else()
    if (NOT IS_SUB_PROJECT)
        message("-- Generate NMM/Vortex Compatible ZIP Package: Off (-DPACKAGE=OFF)")
    endif()
endif()

