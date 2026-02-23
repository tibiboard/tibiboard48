# 振動モーターとバッテリーLEDの両方をビルド対象にする
target_sources(app PRIVATE 
    ${CMAKE_CURRENT_SOURCE_DIR}/src/vibe_handler.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/battery_led.c
)

target_include_directories(app PRIVATE ${APPLICATION_SOURCE_DIR}/include)
