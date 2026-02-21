#include "pch.h"

#include "CppUnitTest.h"
using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace OpenNetUnitTest
{
    TEST_CLASS(CppUnitTests)
    {
    public:
        TEST_METHOD(CppTestOne)
        {
            Assert::IsTrue(2 < 3);
        }
    };
}
