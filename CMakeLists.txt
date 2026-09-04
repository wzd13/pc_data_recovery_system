cmake_minimum_required(VERSION 3.20)
project(PCDataRecovery VERSION 1.0.0 LANGUAGES CXX RC)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(MSVC)
  add_compile_options(/W4 /permissive- /Zc:__cplusplus /utf-8)
  add_compile_definitions(UNICODE _UNICODE NOMINMAX WIN32_LEAN_AND_MEAN)
  set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

set(PCDR_SOURCES
  src/main.cpp
  src/utils/Logger.cpp
  src/utils/StringUtil.cpp
  src/utils/FileUtil.cpp
  src/utils/TimeUtil.cpp
  src/utils/PathUtil.cpp
  src/settings/Settings.cpp
  src/database/HistoryDatabase.cpp
  src/drive/DriveDetector.cpp
  src/drive/PhysicalDisk.cpp
  src/drive/VolumeReader.cpp
  src/filesystem/FileSystemFactory.cpp
  src/filesystem/NtfsScanner.cpp
  src/filesystem/Fat32Scanner.cpp
  src/filesystem/ExFatScanner.cpp
  src/carving/SignatureDatabase.cpp
  src/carving/RawCarver.cpp
  src/scanner/ScanEngine.cpp
  src/scanner/ScanProgress.cpp
  src/scanner/ResultStore.cpp
  src/recovery/RecoveryEngine.cpp
  src/preview/PreviewEngine.cpp
  src/ui/AppWindow.cpp
  src/ui/Theme.cpp
  src/ui/DrivePanel.cpp
  src/ui/ResultsList.cpp
  src/ui/PreviewPanel.cpp
  src/ui/ProgressBar.cpp
  src/ui/SettingsDialog.cpp
  src/ui/HistoryDialog.cpp
  src/ui/Controls.cpp
  resources/app.rc
)

set(PCDR_HEADERS
  include/utils/Logger.hpp
  include/utils/StringUtil.hpp
  include/utils/FileUtil.hpp
  include/utils/TimeUtil.hpp
  include/utils/PathUtil.hpp
  include/utils/Types.hpp
  include/settings/Settings.hpp
  include/database/HistoryDatabase.hpp
  include/drive/DriveDetector.hpp
  include/drive/PhysicalDisk.hpp
  include/drive/VolumeReader.hpp
  include/filesystem/IFileSystemScanner.hpp
  include/filesystem/FileSystemFactory.hpp
  include/filesystem/NtfsScanner.hpp
  include/filesystem/Fat32Scanner.hpp
  include/filesystem/ExFatScanner.hpp
  include/carving/FileSignature.hpp
  include/carving/SignatureDatabase.hpp
  include/carving/RawCarver.hpp
  include/scanner/ScanTypes.hpp
  include/scanner/ScanEngine.hpp
  include/scanner/ScanProgress.hpp
  include/scanner/ResultStore.hpp
  include/recovery/RecoveryEngine.hpp
  include/preview/PreviewEngine.hpp
  include/ui/AppWindow.hpp
  include/ui/Theme.hpp
  include/ui/DrivePanel.hpp
  include/ui/ResultsList.hpp
  include/ui/PreviewPanel.hpp
  include/ui/ProgressBar.hpp
  include/ui/SettingsDialog.hpp
  include/ui/HistoryDialog.hpp
  include/ui/Controls.hpp
  include/ui/ResourceIds.hpp
)

add_executable(PCDataRecovery WIN32 ${PCDR_SOURCES} ${PCDR_HEADERS})

target_include_directories(PCDataRecovery PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/include
  ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(PCDataRecovery PRIVATE
  comctl32
  comdlg32
  shell32
  shlwapi
  ole32
  uuid
  gdiplus
  setupapi
  advapi32
  uxtheme
  dwmapi
  windowscodecs
)

# Portable distribution folder after build
set(PCDR_DIST_DIR "${CMAKE_SOURCE_DIR}/dist/PCDataRecovery")
add_custom_command(TARGET PCDataRecovery POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E make_directory "${PCDR_DIST_DIR}/config"
  COMMAND ${CMAKE_COMMAND} -E make_directory "${PCDR_DIST_DIR}/data"
  COMMAND ${CMAKE_COMMAND} -E make_directory "${PCDR_DIST_DIR}/logs"
  COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:PCDataRecovery> "${PCDR_DIST_DIR}/PCDataRecovery.exe"
  COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_SOURCE_DIR}/dist/PCDataRecovery/README.txt" "${PCDR_DIST_DIR}/README.txt"
  COMMENT "Assembling portable PCDataRecovery distribution"
)

install(TARGETS PCDataRecovery RUNTIME DESTINATION .)
install(DIRECTORY dist/PCDataRecovery/config DESTINATION .)
install(DIRECTORY dist/PCDataRecovery/data DESTINATION .)
install(DIRECTORY dist/PCDataRecovery/logs DESTINATION .)
install(FILES dist/PCDataRecovery/README.txt DESTINATION .)

# Developer smoke test (console)
add_executable(pcdr_smoke_test tools/smoke_test.cpp
  src/utils/Logger.cpp
  src/utils/StringUtil.cpp
  src/utils/FileUtil.cpp
  src/utils/TimeUtil.cpp
  src/utils/PathUtil.cpp
  src/drive/DriveDetector.cpp
  src/drive/PhysicalDisk.cpp
  src/drive/VolumeReader.cpp
  src/carving/SignatureDatabase.cpp
)
target_include_directories(pcdr_smoke_test PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/include
)
target_link_libraries(pcdr_smoke_test PRIVATE setupapi advapi32)
if(MSVC)
  set_target_properties(pcdr_smoke_test PROPERTIES LINK_FLAGS "/SUBSYSTEM:CONSOLE")
endif()

add_executable(pcdr_scan_diag tools/scan_diag.cpp
  src/utils/Logger.cpp
  src/utils/StringUtil.cpp
  src/utils/FileUtil.cpp
  src/utils/TimeUtil.cpp
  src/utils/PathUtil.cpp
  src/drive/DriveDetector.cpp
  src/drive/PhysicalDisk.cpp
  src/drive/VolumeReader.cpp
  src/filesystem/FileSystemFactory.cpp
  src/filesystem/NtfsScanner.cpp
  src/filesystem/Fat32Scanner.cpp
  src/filesystem/ExFatScanner.cpp
  src/scanner/ScanProgress.cpp
)
target_include_directories(pcdr_scan_diag PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(pcdr_scan_diag PRIVATE setupapi advapi32)
if(MSVC)
  set_target_properties(pcdr_scan_diag PROPERTIES LINK_FLAGS "/SUBSYSTEM:CONSOLE")
endif()
