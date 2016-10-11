/* Target info example */
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <liblightnvm.h>

int read(NVM_DEV dev, NVM_GEO geo, NVM_ADDR list[], int len)
{
	ssize_t err;
	char *buf;
	int buf_len, i;

	buf_len = len * geo.nbytes;
	buf = nvm_buf_alloc(geo, buf_len);
	if (!buf) {
		printf("Failed allocating buf\n");
		return -ENOMEM;
	}

	printf("** nvm_addr_read(...): \n");
	for (i = 0; i < len; ++i) {
		nvm_addr_pr(list[i]);
	}

	err = nvm_addr_read(dev, list, len, buf);

	printf("** DUMPING - BEGIN **\n");
	for(i = 0; i < buf_len; ++i) {
		printf("%d:%c\n", i, buf[i]);
	}
	printf("** DUMPING - END **\n");

	if (err) {
		printf("ERR: nvm_addr_read err(%ld)\n", err);
	}

	free(buf);

	return err;
}

int write(NVM_DEV dev, NVM_GEO geo, NVM_ADDR list[], int len)
{
	ssize_t err;

	char *buf;
	int buf_len, i;

	buf_len = len * geo.nbytes;		// Construct buffer
	buf = nvm_buf_alloc(geo, buf_len);
	if (!buf) {
		printf("Failed allocating buf\n");
		return -ENOMEM;
	}
	for(i = 0; i < buf_len; ++i)
		buf[i] = (i % 28) + 65;

	printf("** nvm_addr_write(...):\n");
	for (i = 0; i < len; ++i) {
		nvm_addr_pr(list[i]);
	}

	err = nvm_addr_write(dev, list, len, buf);
	if (err) {
		printf("Detected err(%ld)\n", err);
	}

	free(buf);

	return err;
}

int fmt_p(NVM_DEV dev, NVM_GEO geo, NVM_ADDR list[], int len)
{
	int i;
	
	for (i = 0; i < len; i++) {
		nvm_addr_pr(list[i]);
	}

	return 0;
}

int fmt_g(NVM_DEV dev, NVM_GEO geo, NVM_ADDR list[], int len)
{
	nvm_addr_pr(list[0]);

	return 0;
}

#define NVM_CLI_CMD_LEN 50

typedef struct {
	char name[NVM_CLI_CMD_LEN];
	int (*func)(NVM_DEV, NVM_GEO, NVM_ADDR[], int);
	int argc;
} NVM_CLI_ADDR_CMD;

static NVM_CLI_ADDR_CMD cmds[] = {
	{"read", read, -1},
	{"write", write, -1},
	{"fmt_p", fmt_p, -1},
	{"fmt_g", fmt_g, 9},
};

static int ncmds = sizeof(cmds) / sizeof(cmds[0]);

void _usage_pr(char *cli_name)
{
	int cmd;
	printf("Usage:\n");
	for (cmd = 0; cmd < ncmds; ++cmd) {
		if (cmds[cmd].argc < 0) {
			printf(" %s %6s dev_name ppa [ppa...]\n",
				cli_name, cmds[cmd].name);
		} else {
			printf(" %s %6s dev_name ch lun pl blk pg sec\n",
				cli_name, cmds[cmd].name);
		}
	}
}

int main(int argc, char **argv)
{
	char cmd_name[NVM_CLI_CMD_LEN];
	char dev_name[DISK_NAME_LEN+1];
	int ret;

	NVM_CLI_ADDR_CMD *cmd = NULL;

	NVM_DEV dev;
	NVM_GEO geo;
	NVM_ADDR list[1024];
	int i, len;

	if (argc < 4) {
		_usage_pr(argv[0]);
		return -EINVAL;
	}
							// Get `cmd_name`
	if (strlen(argv[1]) < 1 || strlen(argv[1]) > (NVM_CLI_CMD_LEN-1)) {
		printf("Invalid cmd\n");
		_usage_pr(argv[0]);
		return -EINVAL;
	}
	memset(cmd_name, 0, sizeof(cmd_name));
	strcpy(cmd_name, argv[1]);

	for(int i = 0; i < ncmds; ++i) {		// Get `cmd`
		if (strcmp(cmd_name, cmds[i].name) == 0) {
			cmd = &cmds[i];
			break;
		}
	}
	if (!cmd) {
		printf("Invalid cmd(%s)\n", cmd_name);
		_usage_pr(argv[0]);
		return -EINVAL;
	}
							// Get `dev_name`
	if (strlen(argv[2]) < 1 || strlen(argv[2]) > DISK_NAME_LEN) {
		printf("len(dev_name) > %d\n", DISK_NAME_LEN);
		return -EINVAL;
	}
	memset(dev_name, 0, sizeof(dev_name));
	strcpy(dev_name, argv[2]);

	switch(cmd->argc) {				// Get `list` and `len`
		case -1:				// ppa [ppa..]
			len = argc - 2;
			for (i = 0; i < len; ++i) {
				list[i].ppa = atol(argv[i+2]);
			}
			break;
		case 9:					// ch lun pl blk pg sec
			len = 1;
			list[0].g.ch = atoi(argv[3]);
			list[0].g.lun = atoi(argv[4]);
			list[0].g.pl = atoi(argv[5]);
			list[0].g.blk = atoi(argv[6]);
			list[0].g.pg = atoi(argv[7]);
			list[0].g.sec = atoi(argv[8]);
			break;
		default:
			printf("Invalid argc(%d) for cmd(%s)\n",
				cmd->argc, cmd_name);
			_usage_pr(argv[0]);
			return -EINVAL;
	}

	dev = nvm_dev_open(dev_name);
	if (!dev) {
		printf("Failed opening device, dev_name(%s)\n", dev_name);
		return -EINVAL;
	}
	geo = nvm_dev_attr_geo(dev);

	int has_invalid = 0;
	for (int i = 0; i < len; ++i) {			// Check `addr`
		int invalid_addr = 0;
		if (list[i].g.ch >= geo.nchannels) {
			printf("ERR: ppa(%lu), ch(%u) out of bounds\n",
				list[i].ppa, list[i].g.ch);
			invalid_addr = 1;
		}
		if (list[i].g.lun >= geo.nluns) {
			printf("ERR: ppa(%lu), lun(%u) out of bounds\n",
				list[i].ppa, list[i].g.lun);
			invalid_addr = 1;
		}
		if (list[i].g.pl >= geo.nplanes) {
			printf("ERR: ppa(%lu), pl(%u) out of bounds\n",
				list[i].ppa, list[i].g.pl);
			invalid_addr = 1;
		}
		if (list[i].g.blk >= geo.nblocks) {
			printf("ERR: ppa(%lu), blk(%u) out of bounds\n",
				list[i].ppa, list[i].g.blk);
			invalid_addr = 1;
		}
		if (list[i].g.pg >= geo.npages) {
			printf("ERR: ppa(%lu), pg(%u) out of bounds\n",
				list[i].ppa, list[i].g.pg);
			invalid_addr = 1;
		}
		if (list[i].g.sec >= geo.nsectors) {
			printf("ERR: ppa(%lu), sec(%u) out of bounds\n",
				list[i].ppa, list[i].g.sec);
			invalid_addr = 1;
		}
		has_invalid |= invalid_addr;
	}

	if (has_invalid) {
		printf("One of more addresses exceeds device boundaries\n");
		nvm_geo_pr(geo);
		ret = -EINVAL;
	} else {
		ret = cmd->func(dev, geo, list, len);
	}

	nvm_dev_close(dev);

	return ret;
}
