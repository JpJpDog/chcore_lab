#include <print.h>
#include <syscall.h>
#include <launcher.h>
#include <defs.h>
#include <bug.h>
#include <fs_defs.h>
#include <ipc.h>
#include <string.h>
#include <proc.h>

#define SERVER_READY_FLAG(vaddr) (*(int *)(vaddr))
#define SERVER_EXIT_FLAG(vaddr)  (*(int *)((u64)vaddr+ 4))

extern ipc_struct_t *tmpfs_ipc_struct;
static ipc_struct_t ipc_struct;
static int tmpfs_scan_pmo_cap;

/* fs_server_cap in current process; can be copied to others */
int fs_server_cap;

#define BUFLEN	4096

static char pwd[BUFLEN] = {0};
static char path_buf[BUFLEN];

static inline void make_path(const char *dir, char *dst) {
	if (*dir == '/') { //absolute path
		strcpy(dst, dir);
	} else { //relative path
		strcpy(dst, pwd);
		strcat(dst, "/");
		strcat(dst, dir);
	}
}

// a helper function that scan every dirent under `dir` and call `callback` for each of them.
// return if a callback return > 0. return 0 if all callback return 0. return < 0 if any error.
static int fs_scan(const char *dir, int (*callback)(struct dirent *, void *), void *arg){
	// TODO: your code here
	if (!callback || !dir ) {
		return -EINVAL;
	}
	struct fs_request req = {
		.req = FS_REQ_SCAN,
		.count = PAGE_SIZE,
		.offset = 0,
		.buff = TMPFS_SCAN_BUF_VADDR,
	};
	make_path(dir, req.path);
	int req_size = sizeof(req);
	while (1) {
		ipc_msg_t *ipc_msg = ipc_create_msg(&ipc_struct, req_size, 1);
		ipc_set_msg_cap(ipc_msg, 0, tmpfs_scan_pmo_cap);
		ipc_set_msg_data(ipc_msg, (char*)&req, 0, req_size);
		int ret = ipc_call(&ipc_struct, ipc_msg);
		ipc_destroy_msg(ipc_msg);
		if (ret < 0) {
			return ret;
		} else if (ret == 0) {
			break;
		}
		req.offset += ret;
		struct dirent *dirent = (struct dirent*)TMPFS_SCAN_BUF_VADDR;
		for (int i = 0 ;i < ret; i++) {
			int cb_ret = callback(dirent, arg);
			if (cb_ret < 0) {
				return cb_ret;
			} else if (cb_ret > 0) { // jump out if callback ret 1
				return cb_ret;
			}
			dirent = (void *)dirent + dirent->d_reclen;
		}
	}
	return 0;
}

static int complement_callback(struct dirent *dirent, void *args) {
	char *complement = (char *)args;
	int comp_len = strlen(complement);
	if (!strncmp(dirent->d_name, complement, comp_len)) {
		strcpy(complement, dirent->d_name);
		return strlen(dirent->d_name) - comp_len;
	}
	return 0;
}

static int do_complement(char *buf, char *complement)
{
	// TODO: your code here
	make_path(complement, path_buf);
	int comp_l = strlen(complement);
	int i = comp_l;
	while (i >= 0 && complement[i] != '/') {
		--i;
	}
	complement += i + 1;
	int comp_len = comp_l - i - 1;
	path_buf[strlen(path_buf) - comp_len] = '\0';
	int ret = fs_scan(path_buf, complement_callback, (void *)complement);
	if (ret < 0) {
		printf("complement: error %d\n", ret);
		return ret;
	} else if (!ret) {
		return 0;
	}
	strcat(buf, complement + comp_l);
	printf("%s", complement + comp_l);
	return ret;
}

extern char getch();

// read a command from stdin leading by `prompt`
// put the commond in `buf` and return `buf`
// What you typed should be displayed on the screen
char *readline(const char *prompt)
{
	static char buf[BUFLEN];
	int i = 0, j = 0;
	signed char c = 0;
	char complement[BUFLEN];

	if (prompt != NULL) {
		printf("%s", prompt);
	}

	while (1) {
		c = getch();
		if (c < 0)
			return NULL;
		// TODO: your code here
		if (c == ' ') { //clear the complement
			j = 0;
		}
		if (c >= 0x20 && c < 0x7f) { // printable character
			buf[i++] = c;
			complement[j++] = c;
			usys_putc(c);
		} else if (c == 0x7f) { // backspace
			if (i > 0) {
				i--;
				j--;
				printf("\b \b");
			}
		} else if (c == '\n' || c == '\r') {
			usys_putc('\n');
			buf[i] = '\0';
			break;
		} else if (c == '\t') {
			int ret = 0;
			if (j) {
				complement[j] = buf[i] = '\0';
				ret = do_complement(buf, complement);
				if (ret > 0) {
					i += ret;
					j = 0;
				}
			}
			if (!ret) {
				buf[i++] = c;
				usys_putc('\t');
			}
		}
	}
	return buf;
}

static int cd_callback(struct dirent *dirent, void *args) {
	strcpy(pwd, (char *)args);
	return 0;
}

int do_cd(char *cmdline)
{
	char *args = cmdline + 2;
	while (*args == ' ') {
		args++;
	}
	if (*args == '\0') {
		strcpy(pwd, "/");
		return 0;
	}
	int ret = fs_scan(args, cd_callback, (void *)args);
	if (ret < 0) {
		printf("cd: error %d\n", ret);
	}
	return 0;
}

int do_top()
{
	// TODO: your code here
	usys_top();
	return 0;
}

static int ls_callback(struct dirent *dirent, void *arg) {
	printf("%s\n", dirent->d_name);
	return 0;
}

int do_ls(char *cmdline)
{
	char *args = cmdline += 2;
	while (*args == ' ') {
		args++;
	}
	int ret = fs_scan(args, ls_callback, NULL);
	if (ret < 0) {
		printf("ls: error %d\n", ret);
	}
	return 0;
}

int do_cat(char *cmdline)
{
	char *args = cmdline + 3;
	while (*args == ' ') {
		args++;
	}
	struct fs_request req = {
		.req = FS_REQ_READ,
		.count = PAGE_SIZE - 1, //reserve 1 byte in buffer for '\0'
		.offset = 0,
		.buff = TMPFS_SCAN_BUF_VADDR,
	};
	make_path(args, req.path);
	int req_size = sizeof(req);
	char *read_buf = (char *)TMPFS_SCAN_BUF_VADDR;
	while (1) {
		ipc_msg_t *ipc_msg = ipc_create_msg(&ipc_struct, req_size, 1);
		ipc_set_msg_cap(ipc_msg, 0, tmpfs_scan_pmo_cap);
		ipc_set_msg_data(ipc_msg, (char *)&req, 0, req_size);
		int ret = ipc_call(&ipc_struct, ipc_msg);
		ipc_destroy_msg(ipc_msg);
		if (ret < 0) {
			return ret;
		} else if (ret < req.count) {
			read_buf[ret] = '\0';
			printf("%s", read_buf);
			return 0;
		}
		req.offset += ret;
	}
	printf("\n");
	return 0;
}

int do_echo(char *cmdline)
{
	cmdline += 4;
	while (*cmdline == ' ')
		cmdline++;
	printf("%s", cmdline);
	return 0;
}

void do_clear(void)
{
	usys_putc(12);
	usys_putc(27);
	usys_putc('[');
	usys_putc('2');
	usys_putc('J');
}

int builtin_cmd(char *cmdline)
{
	int ret, i;
	char cmd[BUFLEN];
	for (i = 0; cmdline[i] != ' ' && cmdline[i] != '\0'; i++)
		cmd[i] = cmdline[i];
	cmd[i] = '\0';
	if (!strcmp(cmd, "quit") || !strcmp(cmd, "exit"))
		usys_exit(0);
	if (!strcmp(cmd, "cd")) {
		ret = do_cd(cmdline);
		return !ret ? 1 : -1;
	}
	if (!strcmp(cmd, "ls")) {
		ret = do_ls(cmdline);
		return !ret ? 1 : -1;
	}
	if (!strcmp(cmd, "echo")) {
		ret = do_echo(cmdline);
		return !ret ? 1 : -1;
	}
	if (!strcmp(cmd, "cat")) {
		ret = do_cat(cmdline);
		return !ret ? 1 : -1;
	}
	if (!strcmp(cmd, "clear")) {
		do_clear();
		return 1;
	}
	if (!strcmp(cmd, "top")) {
		ret = do_top();
		return !ret ? 1 : -1;
	}
	return 0;
}

int run_cmd(char *cmdline)
{
	char pathbuf[BUFLEN];
	struct user_elf user_elf;
	int ret;
	int caps[1];

	pathbuf[0] = '\0';
	while (*cmdline == ' ')
		cmdline++;
	if (*cmdline == '\0') {
		return -1;
	} else if (*cmdline != '/') {
		strcpy(pathbuf, "/");
	}
	strcat(pathbuf, cmdline);

	ret = readelf_from_fs(pathbuf, &user_elf);
	if (ret < 0) {
		printf("[Shell] No such binary\n");
		return ret;
	}

	caps[0] = fs_server_cap;
	return launch_process_with_pmos_caps(&user_elf, NULL, NULL,
					     NULL, 0, caps, 1, 0);
}

static int
run_cmd_from_kernel_cpio(const char *filename, int *new_thread_cap,
			 struct pmo_map_request *pmo_map_reqs,
			 int nr_pmo_map_reqs)
{
	struct user_elf user_elf;
	int ret;

	ret = readelf_from_kernel_cpio(filename, &user_elf);
	if (ret < 0) {
		printf("[Shell] No such binary in kernel cpio\n");
		return ret;
	}
	return launch_process_with_pmos_caps(&user_elf, NULL, new_thread_cap,
					     pmo_map_reqs, nr_pmo_map_reqs,
					     NULL, 0, 0);
}

void boot_fs(void)
{
	int ret = 0;
	int info_pmo_cap;
	int tmpfs_main_thread_cap;
	struct pmo_map_request pmo_map_requests[1];

	/* create a new process */
	printf("Booting fs...\n");
	/* prepare the info_page (transfer init info) for the new process */
	info_pmo_cap = usys_create_pmo(PAGE_SIZE, PMO_DATA);
	fail_cond(info_pmo_cap < 0, "usys_create_ret ret %d\n", info_pmo_cap);

	ret = usys_map_pmo(SELF_CAP,
			   info_pmo_cap, TMPFS_INFO_VADDR, VM_READ | VM_WRITE);
	fail_cond(ret < 0, "usys_map_pmo ret %d\n", ret);

	SERVER_READY_FLAG(TMPFS_INFO_VADDR) = 0;
	SERVER_EXIT_FLAG(TMPFS_INFO_VADDR) = 0;

	/* We also pass the info page to the new process  */
	pmo_map_requests[0].pmo_cap = info_pmo_cap;
	pmo_map_requests[0].addr = TMPFS_INFO_VADDR;
	pmo_map_requests[0].perm = VM_READ | VM_WRITE;
	ret = run_cmd_from_kernel_cpio("/tmpfs.srv", &tmpfs_main_thread_cap,
				       pmo_map_requests, 1);
	fail_cond(ret != 0, "create_process returns %d\n", ret);

	fs_server_cap = tmpfs_main_thread_cap;

	while (SERVER_READY_FLAG(TMPFS_INFO_VADDR) != 1)
		usys_yield();

	/* register IPC client */
	tmpfs_ipc_struct = &ipc_struct;
	ret = ipc_register_client(tmpfs_main_thread_cap, tmpfs_ipc_struct);
	fail_cond(ret < 0, "ipc_register_client failed\n");

	tmpfs_scan_pmo_cap = usys_create_pmo(PAGE_SIZE, PMO_DATA);
	fail_cond(tmpfs_scan_pmo_cap < 0, "usys create_ret ret %d\n",
		  tmpfs_scan_pmo_cap);

	ret = usys_map_pmo(SELF_CAP,
			   tmpfs_scan_pmo_cap,
			   TMPFS_SCAN_BUF_VADDR, VM_READ | VM_WRITE);
	fail_cond(ret < 0, "usys_map_pmo ret %d\n", ret);

	printf("fs is UP.\n");
}
