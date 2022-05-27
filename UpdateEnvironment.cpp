
#define _AMD64_

#include <Windows.h>
#include <winternl.h>
#include <stdio.h>

typedef DWORD(__stdcall* NtQueryInformationProcessPtr)(HANDLE, DWORD, PVOID, ULONG, PULONG);

void print_usage(void)
{
	printf("UpdateEnvironment ver 1.0\n");
	printf("\n");
	printf("Usage:\n\nUpdateEnvironment <process id> <variable name to change> <variable value>\n");
}

int wmain(int argc, wchar_t* argv[])
{
	wchar_t* varToChange;
	wchar_t* varValue;

	if (argc < 3) {
		print_usage();
		return 0;
	}

	varToChange = argv[2];
	varValue = argv[3];

	HMODULE hNtDll = GetModuleHandleA("ntdll.dll");
	NtQueryInformationProcessPtr NtQueryInformationProcess = (NtQueryInformationProcessPtr)GetProcAddress(hNtDll, "NtQueryInformationProcess");

	int processId = _wtoi(argv[1]);
	printf("Target PID: %u\n", processId);

	// open the process with read+write access
	HANDLE hProcess = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION | PROCESS_VM_READ | PROCESS_VM_WRITE | PROCESS_VM_OPERATION, 0, processId);
	if (hProcess == NULL)
	{
		printf("Error opening process (%u).  Try running this with admin privileges.\n", GetLastError());
		return 15;
	}

	// find the location of the PEB
	PROCESS_BASIC_INFORMATION pbi = { 0 };
	NTSTATUS status = NtQueryInformationProcess(hProcess, ProcessBasicInformation, &pbi, sizeof(pbi), NULL);
	if (status != 0)
	{
		printf("Error from ProcessBasicInformation (0x%8X)\n", status);
		return 6;
	}
	printf("PEB: %p\n", pbi.PebBaseAddress);

	// find the process parameters
	char* processParamsOffset = (char*)pbi.PebBaseAddress + 0x20; // hard coded offset for x64 apps
	char* processParameters = NULL;
	if (ReadProcessMemory(hProcess, processParamsOffset, &processParameters, sizeof(processParameters), NULL))
	{
		printf("UserProcessParameters: %p\n", processParameters);
	}
	else
	{
		printf("Error from ReadProcessMemory (%u)\n", GetLastError());
		return 1;
	}

	// find the address to the environment table
	char* environmentOffset = processParameters + 0x80; // hard coded offset for x64 apps
	char* environment = NULL;
	if (!ReadProcessMemory(hProcess, environmentOffset, &environment, sizeof(environment), NULL))
	{
		printf("Error reading block %u\n", GetLastError());
		return 2;
	}
	printf("environment: %p\n", environment);

	// copy the environment table into our own memory for scanning
	wchar_t* localEnvBlock = new wchar_t[64 * 1024];
	size_t bytesRead = 0;
	size_t totalBytesRead = 0;
	int tries = 0;
	if (!ReadProcessMemory(hProcess, environment, localEnvBlock, sizeof(wchar_t) * 64 * 1024, &bytesRead))
	{
		if (299 == GetLastError()) {
			totalBytesRead = bytesRead;
			while (totalBytesRead < (64 * 1024) && ++tries < 100000) {
				ReadProcessMemory(hProcess, environment, localEnvBlock + totalBytesRead, sizeof(wchar_t) * 256, &bytesRead);
				totalBytesRead += bytesRead;
			}
		}
		printf("Error reading whole block: %u\n", GetLastError());
	}

	// find the variable to edit
	wchar_t* found = NULL;
	wchar_t* varOffset = localEnvBlock;
	size_t varNameLen = wcslen(varToChange);

	while (varOffset < (localEnvBlock + (64 * 1024) - varNameLen))
	{
		if (wcsncmp(varOffset, varToChange, varNameLen) == 0)
		{
			if (varOffset[varNameLen] == L'=')
			{
				found = varOffset;
				break;
			}
		}
		varOffset++;
	}

	// check to see if we found one
	if (found)
	{
		size_t offset = (found - localEnvBlock) * sizeof(wchar_t);
		printf("Offset: %Iu\n", offset);

		wchar_t replacement[1024] = { 0 };
		wcscpy_s(replacement, varToChange);
		wcscat_s(replacement, L"=");
		wcscat_s(replacement, varValue);

		// write a new version (if the size of the value changes then we have to rewrite the entire block)
		if (!WriteProcessMemory(hProcess, environment + offset, replacement, (wcslen(replacement) + 1) * sizeof(wchar_t), NULL))
		{
			printf("Error WriteProcessMemory (%u)\n", GetLastError());
			return 17;
		}
	}

	// cleanup
	delete[] localEnvBlock;
	CloseHandle(hProcess);

	return 0;
}
