# Writes OUT with the build time, so each build can be told apart on the console.
# Run on every build (see the buildstamp target in CMakeLists.txt).
string(TIMESTAMP stamp "%Y-%m-%d %H:%M")
set(content "extern char const *const g_buildStamp;\nchar const *const g_buildStamp = \"${stamp}\";\n")
if(EXISTS "${OUT}")
	file(READ "${OUT}" old)
endif()
if(NOT old STREQUAL content)
	file(WRITE "${OUT}" "${content}")
endif()
