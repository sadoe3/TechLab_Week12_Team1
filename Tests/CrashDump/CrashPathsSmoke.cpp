#include "Core/Misc/Paths.h"
#include <fstream>
#include <iostream>

int wmain(int argc, wchar_t** argv)
{
	if (argc != 2)
		return 1;
	if (!FPaths::ProjectSavedDir().empty() || !FPaths::NewCrashReportDirectory().empty())
		return 2;

	const auto TestProjectDirectory = std::filesystem::absolute(argv[1]);
	std::filesystem::create_directories(TestProjectDirectory);
	const auto ProjectFilePath = TestProjectDirectory / L"Smoke.hproject";
	{
		std::ofstream ProjectFile(ProjectFilePath);
		ProjectFile << R"({"FileVersion":1,"Name":"Smoke"})";
	}
	if (!FPaths::SetProject(ProjectFilePath))
		return 3;

	const auto FirstCrashDirectory = FPaths::NewCrashReportDirectory();
	const auto SecondCrashDirectory = FPaths::NewCrashReportDirectory();
	if (FPaths::ProjectSavedDir() != TestProjectDirectory / L"Saved")
		return 4;
	if (!FirstCrashDirectory.is_absolute() || FirstCrashDirectory.parent_path() != TestProjectDirectory / L"Saved/Crashes")
		return 5;
	if (FirstCrashDirectory == SecondCrashDirectory || FirstCrashDirectory.filename().wstring().size() != 32)
		return 6;
	if (std::filesystem::exists(FirstCrashDirectory))
		return 7;

	std::cout << "PASS: unset project, project Saved path, unique absolute crash paths, no premature directory\n";
	return 0;
}
