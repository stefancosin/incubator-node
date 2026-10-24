idf_component_register(
    SRCS "dht11.cpp"
    INCLUDE_DIRS "include"
    REQUIRES driver freertos esp_timer esp_rom
)
