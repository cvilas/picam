find_program(LINTER_BIN NAMES clang-tidy QUIET)
if(LINTER_BIN)
  set(LINTER_ARGS
    -extra-arg=-Wno-ignored-optimization-argument
    -extra-arg=-Wno-unknown-warning-option)
  set(LINTER_INVOKE_COMMAND ${LINTER_BIN} -p ${CMAKE_BINARY_DIR} ${LINTER_ARGS})
  set(CMAKE_C_CLANG_TIDY ${LINTER_INVOKE_COMMAND})
  set(CMAKE_CXX_CLANG_TIDY ${LINTER_INVOKE_COMMAND})
else()
  message(WARNING "Linter (clang-tidy) not found.")
endif()
