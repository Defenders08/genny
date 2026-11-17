cmake_minimum_required(VERSION 3.20)

# Genny v1.54 — сборка VST2-плагина под macOS (Apple Silicon / Intel).
# Оригинальный проект собирался только под Windows (Visual Studio + MFC-ресурсы),
# здесь описан переносимый вариант: ядро DSP + GUI на VSTGUI 4.10 (Cocoa).

project(GennyMac VERSION 1.5.4 LANGUAGES C CXX OBJCXX)

set(CMAKE_C_STANDARD 99)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT CMAKE_OSX_DEPLOYMENT_TARGET)
	set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "" FORCE)
endif()
if(NOT CMAKE_OSX_ARCHITECTURES)
	set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "" FORCE)
endif()

set(GENNY_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/Genny)
set(VSTGUI_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/VSTGui)
set(VSTGUI_LIB ${VSTGUI_ROOT}/src/vstgui)
set(VSTSDK ${VSTGUI_ROOT}/lib/vstsdk2.4)

# ------------------------------------------------------------------ исходники

set(GENNY_SOURCES
	${GENNY_ROOT}/src/Genny2612.cpp
	${GENNY_ROOT}/src/GennyExtParam.cpp
	${GENNY_ROOT}/src/GennyLoaders.cpp
	${GENNY_ROOT}/src/GennyVST.cpp
	${GENNY_ROOT}/src/IndexBaron.cpp
	${GENNY_ROOT}/src/VST/VirtualInstrument.cpp
	${GENNY_ROOT}/src/base/VSTBase.cpp
	${GENNY_ROOT}/src/platform/mac_support.mm
	${GENNY_ROOT}/src/platform/win32_compat_shim.mm
)

file(GLOB GENNY_IFACE ${GENNY_ROOT}/src/Interface/*.cpp)
list(APPEND GENNY_SOURCES ${GENNY_IFACE})

file(GLOB GENNY_GZIP ${GENNY_ROOT}/src/gzip/*.c)
list(APPEND GENNY_SOURCES ${GENNY_GZIP})

set(GENNY_DSP_SOURCES
	${GENNY_ROOT}/src/YM2612/YM2612.cpp
	${GENNY_ROOT}/src/YM2612/YM2612Processor.cpp
	${GENNY_ROOT}/src/YM2612/ym2612_new.cpp
	${GENNY_ROOT}/src/YM2612/sn76489.cpp
	${GENNY_ROOT}/src/YM2612/SN76489Chip.cpp
	${GENNY_ROOT}/src/YM2612/sn76489_plusgx.cpp
	${GENNY_ROOT}/src/YM2612/WaveData.cpp
	${GENNY_ROOT}/src/YM2612/DrumSet.cpp
	${GENNY_ROOT}/src/YM2612/VGMLogger.cpp
	${GENNY_ROOT}/src/YM2612/blip_buf.cpp
	${GENNY_ROOT}/src/YM2612/ym3438.c
	${GENNY_ROOT}/src/YM2612/resampler/Fir_Resampler.cpp
	${GENNY_ROOT}/src/YM2612/resampler/filterkit.c
	${GENNY_ROOT}/src/YM2612/resampler/resample.c
	${GENNY_ROOT}/src/YM2612/resampler/resamplesubs.c
)
list(APPEND GENNY_SOURCES ${GENNY_DSP_SOURCES})

file(GLOB VSTGUI_CORE ${VSTGUI_LIB}/lib/*.cpp)
file(GLOB VSTGUI_CONTROLS ${VSTGUI_LIB}/lib/controls/*.cpp)
file(GLOB VSTGUI_ANIMATION ${VSTGUI_LIB}/lib/animation/*.cpp)
file(GLOB VSTGUI_PLATFORM_COMMON ${VSTGUI_LIB}/lib/platform/common/*.cpp)
set(VSTGUI_PLATFORM_FACTORY ${VSTGUI_LIB}/lib/platform/platformfactory.cpp)
file(GLOB VSTGUI_PLATFORM_MAC_CPP ${VSTGUI_LIB}/lib/platform/mac/*.cpp)
file(GLOB VSTGUI_PLATFORM_MAC_MM ${VSTGUI_LIB}/lib/platform/mac/*.mm)
file(GLOB VSTGUI_PLATFORM_MAC_COCOA ${VSTGUI_LIB}/lib/platform/mac/cocoa/*.mm)

set(VSTGUI_SOURCES
	${VSTGUI_CORE}
	${VSTGUI_CONTROLS}
	${VSTGUI_ANIMATION}
	${VSTGUI_PLATFORM_COMMON}
	${VSTGUI_PLATFORM_FACTORY}
	${VSTGUI_PLATFORM_MAC_CPP}
	${VSTGUI_PLATFORM_MAC_MM}
	${VSTGUI_PLATFORM_MAC_COCOA}
	${VSTGUI_LIB}/plugin-bindings/aeffguieditor.cpp
)

set(VSTSDK_SOURCES
	${VSTSDK}/public.sdk/source/vst2.x/audioeffect.cpp
	${VSTSDK}/public.sdk/source/vst2.x/audioeffectx.cpp
	${VSTSDK}/public.sdk/source/vst2.x/vstplugmain.cpp
)

add_library(Genny MODULE
	${GENNY_SOURCES}
	${VSTGUI_SOURCES}
	${VSTSDK_SOURCES}
)

set_target_properties(Genny PROPERTIES
	PREFIX ""
	SUFFIX ""
	OUTPUT_NAME "Genny"
	XCODE_ATTRIBUTE_GCC_ENABLE_CPP_EXCEPTIONS "YES"
)

target_compile_definitions(Genny PRIVATE
	BUILD_VST=1
	MAC=1
	__APPLE__=1
	TARGET_API_MAC_CARBON=0
	VSTGUI_ENABLE_DEPRECATED_METHODS=1
	VSTGUI_LIVE_EDITING=0
	VSTGUI_OPENGL_SUPPORT=0
	VSTGUI_USE_CAVIEWLAYER=0
	_CRT_SECURE_NO_DEPRECATE=1
)

target_include_directories(Genny PRIVATE
	${CMAKE_CURRENT_SOURCE_DIR}/compat
	${CMAKE_CURRENT_SOURCE_DIR}/compat/shim
	${GENNY_ROOT}
	${GENNY_ROOT}/src
	${GENNY_ROOT}/src/VST
	${GENNY_ROOT}/src/YM2612
	${VSTGUI_ROOT}/lib/fpsdk
	${VSTGUI_ROOT}/src
	${VSTGUI_ROOT}/src/vstgui
	${VSTSDK}
)

# Принудительный заголовок совместимости: заменяет MSVC-специфику.
# Только для C++/ObjC++ — C-файлы (gzip, resampler) не должны видеть C++-типы.
target_compile_options(Genny PRIVATE
	$<$<COMPILE_LANGUAGE:CXX,OBJCXX>:-include${CMAKE_CURRENT_SOURCE_DIR}/compat/genny_mac_compat.h>
	$<$<COMPILE_LANGUAGE:C>:-includeunistd.h>
	-Wno-deprecated-declarations
	-Wno-unused-command-line-argument
	-Wno-invalid-source-encoding
	-Wno-ignored-attributes
	-Wno-c++11-narrowing
	-Wno-inconsistent-missing-override
	-fms-extensions
	-fno-objc-arc
)

target_link_libraries(Genny PRIVATE
	"-framework Cocoa"
	"-framework CoreFoundation"
	"-framework CoreGraphics"
	"-framework CoreText"
	"-framework QuartzCore"
	"-framework Accelerate"
	"-framework AudioToolbox"
	"-framework CoreAudio"
	"-framework Foundation"
)

# ------------------------------------------------------------------- бандл

set(BUNDLE_DIR ${CMAKE_CURRENT_BINARY_DIR}/dist/Genny.vst)
set(RESOURCE_DIR ${CMAKE_CURRENT_BINARY_DIR}/res)

add_custom_target(genny_resources
	COMMAND ${CMAKE_COMMAND} -E make_directory ${RESOURCE_DIR}
	COMMAND python3 ${CMAKE_CURRENT_SOURCE_DIR}/tools/prepare_resources.py
		--source ${GENNY_ROOT} --out ${RESOURCE_DIR}
	COMMENT "Подготовка ресурсов Genny"
)

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/cmake/Info.plist.in ${CMAKE_CURRENT_BINARY_DIR}/Info.plist @ONLY)

add_custom_command(TARGET Genny POST_BUILD
	COMMAND ${CMAKE_COMMAND} -E make_directory ${BUNDLE_DIR}/Contents/MacOS
	COMMAND ${CMAKE_COMMAND} -E make_directory ${BUNDLE_DIR}/Contents/Resources
	COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:Genny> ${BUNDLE_DIR}/Contents/MacOS/Genny
	COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_BINARY_DIR}/Info.plist ${BUNDLE_DIR}/Contents/Info.plist
	COMMAND ${CMAKE_COMMAND} -E copy_directory ${RESOURCE_DIR} ${BUNDLE_DIR}/Contents/Resources
	COMMAND chmod -R a+rX ${BUNDLE_DIR}
	COMMAND codesign --force --deep --sign - ${BUNDLE_DIR}
	COMMENT "Сборка бандла Genny.vst"
)

add_dependencies(Genny genny_resources)
