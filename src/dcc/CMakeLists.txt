project(dcc C)
set(SOURCES
    dcc_config.c
    dcc_application_main_track.c
    dcc_application_service_track.c
    dcc_application_command_station_main_track.c
    dcc_application_command_station_service_track.c
    dcc_application_command_station_packet.c
    dcc_application_decoder_cv.c
    dcc_application_decoder_railcom.c
    dcc_application_accessory_decoder_railcom.c
    dcc_packet_encoder.c
    dcc_bit_encoder.c
    dcc_scheduler.c
    dcc_service_mode_common.c
    dcc_service_mode_direct.c
    dcc_service_mode_paged.c
    dcc_service_mode_register.c
    dcc_service_mode_address.c
    dcc_railcom_command_station.c
    dcc_bit_decoder.c
    dcc_packet_decoder.c
    dcc_cv_storage.c
    dcc_failsafe.c
    dcc_railcom_decoder.c
    dcc_railcom_cutout.c
    dcc_service_mode_task_direct.c
    dcc_service_mode_task_paged.c
    dcc_service_mode_task_register.c
    dcc_service_mode_task_address.c
    dcc_service_mode_task_detect.c
)

if (${GTEST})
set(TESTS ${TESTS}
    ${ROOT_DIR}/src/dcc/dcc_application_command_station_main_track_Test.cxx
    ${ROOT_DIR}/src/dcc/dcc_application_command_station_service_track_Test.cxx
    ${ROOT_DIR}/src/dcc/dcc_config_Test.cxx
    ${ROOT_DIR}/src/dcc/dcc_application_command_station_packet_Test.cxx
    ${ROOT_DIR}/src/dcc/dcc_bit_encoder_Test.cxx
    ${ROOT_DIR}/src/dcc/dcc_scheduler_Test.cxx
    ${ROOT_DIR}/src/dcc/dcc_service_mode_common_Test.cxx
    ${ROOT_DIR}/src/dcc/dcc_service_mode_direct_Test.cxx
    ${ROOT_DIR}/src/dcc/dcc_service_mode_paged_Test.cxx
    ${ROOT_DIR}/src/dcc/dcc_service_mode_register_Test.cxx
    ${ROOT_DIR}/src/dcc/dcc_service_mode_address_Test.cxx
    ${ROOT_DIR}/src/dcc/dcc_railcom_command_station_Test.cxx
    ${ROOT_DIR}/src/dcc/dcc_bit_decoder_Test.cxx
    ${ROOT_DIR}/src/dcc/dcc_packet_decoder_Test.cxx
    ${ROOT_DIR}/src/dcc/dcc_cv_storage_Test.cxx
    ${ROOT_DIR}/src/dcc/dcc_failsafe_Test.cxx
    ${ROOT_DIR}/src/dcc/dcc_railcom_decoder_Test.cxx
    ${ROOT_DIR}/src/dcc/dcc_railcom_cutout_Test.cxx
    ${ROOT_DIR}/src/dcc/dcc_application_decoder_cv_Test.cxx
    ${ROOT_DIR}/src/dcc/dcc_application_decoder_railcom_Test.cxx
    ${ROOT_DIR}/src/dcc/dcc_application_accessory_decoder_railcom_Test.cxx
    ${ROOT_DIR}/src/dcc/dcc_application_main_track_api_Test.cxx
    ${ROOT_DIR}/src/dcc/dcc_application_service_track_api_Test.cxx
    ${ROOT_DIR}/src/dcc/dcc_packet_encoder_raw_Test.cxx
    ${ROOT_DIR}/src/dcc/dcc_service_mode_task_direct_Test.cxx
    ${ROOT_DIR}/src/dcc/dcc_service_mode_task_paged_Test.cxx
    ${ROOT_DIR}/src/dcc/dcc_service_mode_task_register_Test.cxx
    ${ROOT_DIR}/src/dcc/dcc_service_mode_task_address_Test.cxx
    ${ROOT_DIR}/src/dcc/dcc_service_mode_task_detect_Test.cxx

    PARENT_SCOPE
)
endif()

add_library(${PROJECT_NAME} ${SOURCES})

target_include_directories(${PROJECT_NAME}
    PUBLIC
        ${ROOT_DIR}/src
)
