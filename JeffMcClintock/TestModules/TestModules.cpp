#include "pch.h"
#include "CppUnitTest.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

#ifdef _DEBUG
#define CONFIGURATION "Debug"
#else
#define CONFIGURATION "Release"
#endif

#define SYNTHEDIT_PATH "C:\\SE\\SE14\\SynthEdit\\bin\\x64\\" CONFIGURATION "\\SynthEdit.exe"
#define CANCELLATION_UTILITY_PATH "C:\\SE\\SE14\\OtherProjects\\cancellation\\x64\\Release\\cancellation.exe"
#define TEST_FOLDER "C:\\SE\\CommunityModules\\tests\\"

// TODO: Add command-line arg to SE to set default wave output folder. Maybe render time.
// maybe add args to this so test folder is configurable.

namespace TestModules
{
	TEST_CLASS(TestModules)
	{
		int deleteOutput(std::string testName)
		{
			std::string path{ TEST_FOLDER "Output\\" };
			path += testName;
			path += ".wav";

			return remove(path.c_str());
		}

		int render(std::string testName)
		{
			// run SE.
			std::string path{ SYNTHEDIT_PATH };

			path += " " TEST_FOLDER "Projects\\";
			path += testName;
			path += ".se1";

			path += " /autorender";

			return system(path.c_str());
		}

		// run cancellation on output file.
		int cancel(std::string testName)
		{
			std::string path{ CANCELLATION_UTILITY_PATH };

			path += " " TEST_FOLDER "Reference\\" CONFIGURATION "\\";
			path += testName;
			path += ".wav";

			path += " " TEST_FOLDER "Output\\";
			path += testName;
			path += ".wav";

			return system(path.c_str());
		}

	public:
		
		TEST_METHOD(Oscillator)
		{
			deleteOutput("Oscillator");

			render("Oscillator");

			// run cancellation on output file.
			const auto r = cancel("Oscillator");
			Assert::AreEqual(0, r);
		}
	};
}
