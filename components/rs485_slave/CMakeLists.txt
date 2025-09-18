idf_component_register(
    SRCS "src/rs485_slave.cpp"
    INCLUDE_DIRS "include"
    REQUIRES driver esp-modbus uart
)
