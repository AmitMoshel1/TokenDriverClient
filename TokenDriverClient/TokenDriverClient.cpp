#include <iostream>
#include <vector>
#include <string>
#include <Windows.h>

#define TOKENDRIVER_TYPE 0x8001
#define IOCTL_SET_TOKEN CTL_CODE(TOKENDRIVER_TYPE, 0x802, METHOD_BUFFERED, FILE_ANY_ACCESS)

namespace TokenClient
{
	typedef struct TokenInfo
	{
		ULONG ProcessID;
		ULONG_PTR TokenPrivs;
	} TokenInfo, * PTokenInfo;

	class TokenModifier
	{
	private:
		std::vector<std::unique_ptr<TokenInfo>> TargetProcesses; // When creating a vector of "std::unique_ptr<>", each smart pointer
																 // entered into the array needs to be called with "std::make_unique<>()"
	public:
		TokenModifier(ULONG PIDs[], ULONG_PTR TokenPrivs[])
		{
			for (int i = 0; i < (sizeof(PIDs) / sizeof(ULONG_PTR)); i++)
			{
				std::unique_ptr<TokenInfo> CurrentProcess = std::make_unique<TokenInfo>();
				CurrentProcess->ProcessID = PIDs[0];
				CurrentProcess->TokenPrivs = TokenPrivs[0];
				TargetProcesses.push_back(std::move(CurrentProcess)); // std::move moves ownership to the vector
			}
		}

		VOID Execute()
		{
			HANDLE hDevice = CreateFile(L"\\\\.\\GlobalRoot\\Device\\TokenDriver", GENERIC_READ | GENERIC_WRITE, 0, nullptr, OPEN_EXISTING, 0, nullptr);
			if(hDevice == INVALID_HANDLE_VALUE)
			{
				printf("Error opening handle to device, GetLastError() %u\n", GetLastError());
				return;
			}

			for(int i = 0; i < this->TargetProcesses.size(); i++)
			{
				if(DeviceIoControl(hDevice, IOCTL_SET_TOKEN, (PTokenInfo&)TargetProcesses[i], sizeof(TokenInfo), nullptr, 0, 0, nullptr))
				{
					std::cout << "PID [" << TargetProcesses[i]->ProcessID << "] Changed to: 0x" << std::hex << TargetProcesses[i]->TokenPrivs << std::endl;
					continue;
				}
				std::cout << "Failed to modify PID [" << TargetProcesses[i]->ProcessID << "] - GetLastError(): " << GetLastError() << std::endl;
			}
			CloseHandle(hDevice);
		}

		std::vector<std::unique_ptr<TokenInfo>> GetTargetProcesses()
		{
			return std::move(this->TargetProcesses);
		}

		VOID SetTargetProcesses(std::vector<std::unique_ptr<TokenInfo>> TInfo)
		{
			TargetProcesses = std::move(TInfo);
		}
		
		~TokenModifier()
		{
			TargetProcesses.clear();
		}

	};

}

int main()
{
	std::cout << "Current PID: " << GetCurrentProcessId() << std::endl;
	ULONG PIDs[] = { GetCurrentProcessId() };				// I'll add more...
	ULONG_PTR TokenPrivs[] = { 0x0000001ff2ffffbc };       // basic token check


	// Verify that the number of PIDs and the number of privs are the same...
	if((sizeof(PIDs)/sizeof(ULONG)) == sizeof(TokenPrivs) / sizeof(ULONG_PTR))
	{
		TokenClient::TokenModifier T(PIDs, TokenPrivs);
				
		std::vector<std::unique_ptr<TokenClient::TokenInfo>> a = std::move(T.GetTargetProcesses());	
		for(int i = 0; i < a.size(); i++)
		{
			printf("PID: %d\n", a[i]->ProcessID);
			printf("Token: 0x%p\n", a[i]->TokenPrivs);
		}

		T.SetTargetProcesses(std::move(a));
		
		printf("Press Enter to elevate privileges...\n");
		getchar();

		T.Execute();
		
		printf("Privileges were elevated for PID %d\n", GetCurrentProcessId());
		system("cmd.exe");

		getchar();

		TerminateProcess(GetCurrentProcess(), 0);
	}

	return 0;
}

