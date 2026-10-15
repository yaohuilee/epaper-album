idf_component_register(SRCS "core.c" "bmp.c" "announcement_core.c" "audio_sequence.c" "pcm_resample.c" "image.c" INCLUDE_DIRS "include" REQUIRES json)

set_source_files_properties("image.c" PROPERTIES COMPILE_OPTIONS "-O2")
