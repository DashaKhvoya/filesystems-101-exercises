#include <solution.h>
#include <dirent.h>
#include <errno.h>
#include <stdio.h>

const char* processes_root_dir = "/proc";

void ps(void)
{
	DIR* root_dir = opendir(processes_root_dir);
	if (!root_dir) {
		report_error(processes_root_dir, errno);
		return;
	}
	
	struct dirent *entry;
	while ((entry = readdir (root_dir)) != NULL) {
		const char* name  = entry->d_name;
		printf("pid = %d", name);
  	}
}
