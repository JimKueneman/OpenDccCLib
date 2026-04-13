project(dcc C)
set(SOURCES
    dcc_config.c
    dcc_application_main_track.c
    dcc_application_service_track.c
    dcc_packet_encoder.c
    dcc_bit_encoder.c
    dcc_scheduler.c
    dcc_service_mode_common.c
    dcc_service_mode_direct.c
    dcc_service_mode_paged.c
    dcc_service_mode_register.c
    dcc_service_mode_address.c
    dcc_railcom_decoder.c
    dcc_bit_decoder.c
    dcc_packet_decoder.c
    dcc_cv_storage.c
    dcc_railcom_encoder.c
    dcc_railcom_cutout.c
)

if (${GTEST})
set(TESTS ${TESTS}
    ${ROOT_DIR}/src/dcc/dcc_application_main_track_Test.cxx
    ${ROOT_DIR}/src/dcc/dcc_application_service_track_Test.cxx
    ${ROOT_DIR}/src/dcc/dcc_config_Test.cxx
    ${ROOT_DIR}/src/dcc/dcc_packet_encoder_Test.cxx
    ${ROOT_DIR}/src/dcc/dcc_bit_encoder_Test.cxx
    ${ROOT_DIR}/src/dcc/dcc_scheduler_Test.cxx
    ${ROOT_DIR}/src/dcc/dcc_service_mode_common_Test.cxx
    ${ROOT_DIR}/src/dcc/dcc_service_mode_direct_Test.cxx
    ${ROOT_DIR}/src/dcc/dcc_service_mode_paged_Test.cxx
    ${ROOT_DIR}/src/dcc/dcc_service_mode_register_Test.cxx
    ${ROOT_DIR}/src/dcc/dcc_service_mode_address_Test.cxx
    ${ROOT_DIR}/src/dcc/dcc_railcom_decoder_Test.cxx
    ${ROOT_DIR}/src/dcc/dcc_bit_decoder_Test.cxx
    ${ROOT_DIR}/src/dcc/dcc_packet_decoder_Test.cxx
    ${ROOT_DIR}/src/dcc/dcc_cv_storage_Test.cxx
    ${ROOT_DIR}/src/dcc/dcc_railcom_encoder_Test.cxx
    ${ROOT_DIR}/src/dcc/dcc_railcom_cutout_Test.cxx

    PARENT_SCOPE
)
endif()

add_library(${PROJECT_NAME} ${SOURCES})

target_include_directories(${PROJECT_NAME}
    PUBLIC
        ${ROOT_DIR}/src
)
