/*
 * lxc: linux Container library
 *
 * (C) Copyright IBM Corp. 2007, 2010
 *
 * Authors:
 * Daniel Lezcano <dlezcano at fr.ibm.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 */

#include <unistd.h>
#include <errno.h>
#include <sys/param.h>
#include <pwd.h>
#include "commands.h"
#include "arguments.h"
#include "namespace.h"
#include "log.h"

lxc_log_define(lxc_attach_ui, lxc);

static const struct option my_longopts[] = {
	LXC_COMMON_OPTIONS
};

static struct lxc_arguments my_args = {
	.progname = "lxc-attach",
	.help     = "\
--name=NAME\n\
\n\
Execute the specified command - enter the container NAME\n\
\n\
Options :\n\
  -n, --name=NAME   NAME for name of the container\n",
	.options  = my_longopts,
	.parser   = NULL,
	.checker  = NULL,
};

pid_t get_init_pid(const char *name)
{
	struct lxc_command command = {
		.request = { .type = LXC_COMMAND_PID },
	};

	int ret, stopped = 0;

	ret = lxc_command(name, &command, &stopped);
	if (ret < 0 && stopped) {
		INFO("'%s' is already stopped", name);
		return 0;
	}

	if (ret < 0) {
		ERROR("failed to send command");
		return -1;
	}

	if (command.answer.ret) {
		ERROR("failed to retrieve the init pid: %s",
		      strerror(command.answer.ret));
		return -1;
	}

	return command.answer.pid;
}

int main(int argc, char *argv[], char *envp[])
{
	int ret;
	pid_t pid;
	struct passwd *passwd;
	uid_t uid;

	ret = lxc_arguments_parse(&my_args, argc, argv);
	if (ret)
		return ret;

	ret = lxc_log_init(my_args.log_file, my_args.log_priority,
			   my_args.progname, my_args.quiet);
	if (ret)
		return ret;

	pid = get_init_pid(my_args.name);
	if (pid < 0) {
		ERROR("failed to get the init pid");
		return -1;
	}

	ret = lxc_attach(pid);
	if (ret < 0) {
		ERROR("failed to enter the namespace");
		return -1;
	}

	if (my_args.argc) {
		execve(my_args.argv[0], my_args.argv, envp);
		SYSERROR("failed to exec '%s'", my_args.argv[0]);
		return -1;
	}

	uid = getuid();

	passwd = getpwuid(uid);
	if (!passwd) {
		SYSERROR("failed to get passwd entry for uid '%d'", uid);
		return -1;
	}

	{
		char *const args[] = {
			passwd->pw_shell,
			NULL,
		};

		execve(args[0], args, envp);
		SYSERROR("failed to exec '%s'", args[0]);
		return -1;
	}

	return 0;
}