/****************************************************************************
 * examples/hello/hello_main.c
 *
 * Licensed to the Apache Software Foundation (ASF) under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.  The
 * ASF licenses this file to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance with the
 * License.  You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
 * WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.  See the
 * License for the specific language governing permissions and limitations
 * under the License.
 *
 ****************************************************************************/

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <stdio.h>
#include <fcntl.h>

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * vrgate_main
 ****************************************************************************/

static int pipe_fd[2]={-1,-1};

static char ipc_buf[32];

int main(int argc, FAR char *argv[])
{
	if (pipe_fd[0]==-1){
		sprintf(ipc_buf,"testmesg\n");
		printf("create pipe returned %d\n",pipe(pipe_fd));
		char buf[1]={'a'};
		printf("write pipe returned %d\n",write(pipe_fd[1],buf,1));
	}else{
		printf("%s",ipc_buf);
		char buf[1]={0x00};
		printf("read pipe returned %d\n",read(pipe_fd[0],buf,1));
		printf("read pipe result %c\n",buf[0]);
		close(pipe_fd[0]);
		close(pipe_fd[1]);
	}
    return 0;
}

