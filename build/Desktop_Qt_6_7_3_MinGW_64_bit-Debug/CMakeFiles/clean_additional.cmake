# Additional clean files
cmake_minimum_required(VERSION 3.16)

if("${CONFIG}" STREQUAL "" OR "${CONFIG}" STREQUAL "Debug")
  file(REMOVE_RECURSE
  "Conduit\\CMakeFiles\\Conduit_autogen.dir\\AutogenUsed.txt"
  "Conduit\\CMakeFiles\\Conduit_autogen.dir\\ParseCache.txt"
  "Conduit\\Conduit_autogen"
  "Oracle\\CMakeFiles\\Oracle_autogen.dir\\AutogenUsed.txt"
  "Oracle\\CMakeFiles\\Oracle_autogen.dir\\ParseCache.txt"
  "Oracle\\Oracle_autogen"
  )
endif()
