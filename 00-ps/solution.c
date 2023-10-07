#include <solution.h>
#include <dirent.h>
#include <stdlib.h>
#include <errno.h>
#include <stdio.h>

const char* proc_root_dir = "/proc";

void ps(void)
{
	DIR* root_dir = opendir(proc_root_dir);
	if (!root_dir) {
		report_error(proc_root_dir, errno);
		return 1;
	}
	
	struct dirent *entry;
	while ((entry = readdir (root_dir)) != NULL) {
		char *proc_dir = get_full_path(root_dir, entry->d_name);
		if (proc_dir == NULL) {
			return 1;
		}

		char *exe_path = get_exe_path(proc_dir);
		if (exe_path == NULL) {
			return 1;
		}



		free(proc_dir);
		free(exe_path);
  	}
}

char *get_full_path(const char *dir, const char *file) {
	// Consider '/' and NULL at the end
	size_t full_size = strlen(dir) + strlen(file) + 2;
	char *buf = (char *)calloc(full_size, sizeof(char));
	if (buf == NULL) {
		return NULL;
	}
	sprintf(buf, "%s/%s", dir, file);
	if (errno != 0) {
		// Do not care about error here
		free(buf);
		return NULL;
	}

	return buf;
}

char *get_exe_path(const char *proc_dir) {
	char* exe_link = get_full_path(proc_dir, "exe");
	if (exe_link == NULL) {
		return NULL;
	}

	return realpath(exe_link, NULL);
}

char** get_command_line_args(const char *proc_dir) {
	char* cl_path = get_full_path(proc_dir, "cmdline");
	if (cl_path == NULL) {
		return NULL;
	}

	FILE *file = fopen(cl_path, "rb");
	if (file == NULL) {
		return NULL;
	}
	int err = fseek(file, 0, SEEK_END);
	if (err != 0) {
		return NULL;
	}
	long file_size = ftell(file);
	
	fseek(file, 0, SEEK_SET);

	char *string = malloc(fsize + 1);
	fread(string, fsize, 1, f);
	fclose(f);

	string[fsize] = 0;
}

char** get_env(const char *proc_dir) {

}
