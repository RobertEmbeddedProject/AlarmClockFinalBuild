set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
idf_component_register(SRCS "KY040 Rotary.c" "main.c" "ssd1309.c" "smallfont.c" "mp3.c" "songs.c"
                       INCLUDE_DIRS "."
                       PRIV_REQUIRES driver)
#PRIV_REQUIRES tells IDF that the program uses headers from the driver/ component..