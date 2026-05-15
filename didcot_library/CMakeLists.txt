if(NOT DEFINED CMAKE_SCRIPT_MODE_FILE AND NOT DEFINED CMAKE_FIND_PACKAGE_NAME)
    file(GLOB MY_SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/src/*.c)
else()
    file(GLOB MY_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/src/dt*.c)
endif()

# ---- Zephyr -------------------------------------------------
if (ZEPHYR_BASE OR DEFINED Zephyr_FOUND)

    zephyr_library_named(didcot_library)

    zephyr_library_sources(${MY_SOURCES})
    
    zephyr_library_include_directories(
        ${DIDCOT_DIR}/didcot_library/include
        ${DTMC_ZEPHYR_DIR}/dtmc_zephyr_library/include
        ${DTMC_SERVICES_DIR}/dtmc_services_library/include
        ${DTMC_BASE_DIR}/dtmc_base_library/include
        ${DTCORE_DIR}/dtcore_library/include
    )    

# ---- ESP-IDF -------------------------------------------------
elseif(DEFINED ENV{IDF_PATH})
    idf_component_register(
        SRCS
            ${MY_SOURCES}
        INCLUDE_DIRS
            include
        REQUIRES
            dtcore_library
            dtmc_base_library
            dtmc_espidf_library
            dtmc_services_library
        )

# ---- Linux -------------------------------------------------
else()
    add_library(didcot_library STATIC ${MY_SOURCES})

    target_include_directories(didcot_library 
        PUBLIC
            ${DIDCOT_DIR}/didcot_library/include
            ${DTMC_LINUX_DIR}/dtmc_linux_library/include
            ${DTMC_BASE_DIR}/dtmc_base_library/include
            ${DTMC_SERVICES_DIR}/dtmc_services_library/include
            ${DTCORE_DIR}/dtcore_library/include
        )
    
    target_link_libraries(didcot_library 
        PUBLIC 
            dtmc_linux_library 
            dtmc_base_library 
            dtcore_library)

endif()
