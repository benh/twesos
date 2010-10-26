#include <gtest/gtest.h>

#include "external_test.hpp"

#include "config/config.hpp"


// Run each of the sample frameworks in local mode
TEST_EXTERNAL(SampleFrameworks, CFramework)
TEST_EXTERNAL(SampleFrameworks, CppFramework)
#if MESOS_HAS_JAVA
  TEST_EXTERNAL(SampleFrameworks, JavaFramework)
  TEST_EXTERNAL(SampleFrameworks, JavaExceptionFramework)
#endif 
#if MESOS_HAS_PYTHON
  TEST_EXTERNAL(SampleFrameworks, PythonFramework)
#endif

// Some tests for command-line and environment configuration
TEST_EXTERNAL(SampleFrameworks, CFrameworkCmdlineParsing)
TEST_EXTERNAL(SampleFrameworks, CFrameworkInvalidCmdline)
TEST_EXTERNAL(SampleFrameworks, CFrameworkInvalidEnv)
