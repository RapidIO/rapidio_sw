/* List utility CLI commands */
/*
****************************************************************************
Copyright (c) 2015, Integrated Device Technology Inc.
Copyright (c) 2015, RapidIO Trade Association
All rights reserved.

Redistribution and use in source and binary forms, with or without modification,
are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice, this
l of conditions and the following disclaimer.

2. Redistributions in binary form must reproduce the above copyright notice,
this l of conditions and the following disclaimer in the documentation
and/or other materials provided with the distribution.

3. Neither the name of the copyright holder nor the names of its contributors
may be used to endorse or promote products derived from this software without
specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*************************************************************************
*/

// #include "librlist_private.h"
#include "liblist.h"
#include "assert.h"
#include "libcli.h"

#ifdef __cplusplus
extern "C" {
#endif

struct l_head_t test_list;

void display_list(struct cli_env *env, struct l_head_t *l) {
	sprintf(env->output, "\nhead: %p tail: %p cnt: %d\n", 
			l->head, l->tail, l->cnt);
        logMsg(env);
	if (l->cnt) {
		struct l_item_t *li = l->head;
		int cnt = 0;

		sprintf(env->output, "Display forward:\n");
        	logMsg(env);
		while (NULL != li) {
			sprintf(env->output, 
				"%d %p: next %p prev %p  key %x item %p %x\n",
				cnt, li, li->next, li->prev, li->key, li->item,
				*(uint32_t *)(li->item));
			logMsg(env);
			li = li->next;
			cnt++;
		}
		if (cnt != l->cnt) {
			sprintf(env->output, "\nCount mismatch: %d\n", cnt);
        		logMsg(env);
		};
		cnt = l->cnt;
		li = l->tail;
		sprintf(env->output, "\nDisplay backward:\n");
        	logMsg(env);
		while (NULL != li) {
			sprintf(env->output, 
				"%d %p: next %p prev %p  key %x item %p %x\n",
				cnt, li, li->next, li->prev, li->key, li->item,
				*(int *)(li->item));
			logMsg(env);
			li = li->prev;
			cnt--;
		}
		if (cnt) {
			sprintf(env->output, "\nCount mismatch: %d\n", cnt);
        		logMsg(env);
		};
	};
};

extern struct cli_cmd ListDisplay;

int ListDisplayCmd(struct cli_env *env, int argc, char **argv)
{
	if (argc)
		goto show_help;

	display_list(env, &test_list);
        return 0;

show_help:
        sprintf(env->output, "\nFAILED: Extra parms or invalid values: %s\n",
			argv[0]);
        logMsg(env);
        cli_print_help(env, &ListDisplay);

        return 0;
};

struct cli_cmd ListDisplay = {
"listdisplay",
5,
0,
"Display test list .",
"No Parameters.\n",
ListDisplayCmd,
ATTR_NONE
};
	
extern struct cli_cmd ListInit;
int ListInitCmd(struct cli_env *env, int argc, char **argv)
{
	if (argc)
		goto show_help;

	l_init(&test_list);
	display_list(env, &test_list);
        return 0;

show_help:
        sprintf(env->output, "\nFAILED: Extra parms or invalid values: %s\n",
			argv[0]);
        logMsg(env);
        cli_print_help(env, &ListInit);

        return 0;
};

struct cli_cmd ListInit = {
"listinit",
5,
0,
"Initialize test list .",
"No Parameters.\n",
ListInitCmd,
ATTR_NONE
};
	
extern struct cli_cmd ListPush;
int ListPushCmd(struct cli_env *env, int argc, char **argv)
{
	int *val = (int *)malloc(sizeof(int));

	if (argc > 1)
		goto show_help;

	*val = getHex(argv[0], 0);
	
	l_push_tail(&test_list, val);
	display_list(env, &test_list);
        return 0;

show_help:
        sprintf(env->output, "\nFAILED: Extra parms or invalid values: %s\n",
			argv[0]);
        logMsg(env);
        cli_print_help(env, &ListPush);

        return 0;
};

struct cli_cmd ListPush = {
"listpush",
5,
1,
"Push value to end of list.",
"<val>.\n"
	"<val> is the integer value to add to the end of the list.\n",
ListPushCmd,
ATTR_NONE
};
	
extern struct cli_cmd ListPop;
int ListPopCmd(struct cli_env *env, int argc, char **argv)
{
	void *item;

	if (argc)
		goto show_help;

	item = l_pop_head(&test_list);

	if (item == NULL)  {
        	sprintf(env->output, "\n*Item is NULL\n");
        	logMsg(env);
	} else {
        	sprintf(env->output, "\n*Item: %p %d\n", item, *(int *)item);
        	logMsg(env);
		free(item);
	};
	display_list(env, &test_list);
        return 0;

show_help:
        sprintf(env->output, "\nFAILED: Extra parms or invalid values: %s\n",
			argv[0]);
        logMsg(env);
        cli_print_help(env, &ListPop);

        return 0;
};

struct cli_cmd ListPop = {
"listPop",
5,
0,
"Pop head.",
"No Parameters.\n",
ListPopCmd,
ATTR_NONE
};

extern struct cli_cmd ListAdd;
int ListAddCmd(struct cli_env *env, int argc, char **argv)
{
	uint32_t *val = (uint32_t *)malloc(sizeof(uint32_t));
	struct l_item_t *li;

	if (argc > 1)
		goto show_help;

	*val = getHex(argv[0], 0);
	
	sprintf(env->output, "\nNEW Value %s %d\n", argv[0], *val);
	logMsg(env);
	li = (struct l_item_t *)l_add(&test_list, *val, (void *)val);
	sprintf(env->output, "\nNEW %p: next %p prev %p  key %x item %p %x",
		li, li->next, li->prev, li->key, li->item, 
						*(uint32_t *)(li->item));
	logMsg(env);
	
	display_list(env, &test_list);
        return 0;

show_help:
        sprintf(env->output, "\nFAILED: Extra parms or invalid values: %s\n",
			argv[0]);
        logMsg(env);
        cli_print_help(env, &ListPush);

        return 0;
};

struct cli_cmd ListAdd = {
"listadd",
5,
1,
"Add value to ordered list.",
"<val>.\n"
	"<val> is the integer value to add to the list.\n",
ListAddCmd,
ATTR_NONE
};
	
struct l_item_t *found;

extern struct cli_cmd ListFind;
int ListFindCmd(struct cli_env *env, int argc, char **argv)
{
	void *item;
	uint32_t key;

	if (argc > 1)
		goto show_help;

	key = getHex(argv[0], 0);
	
	item = l_find(&test_list, key, &found);
	if (NULL == item)
		sprintf(env->output, "\nValue %x Not found: %p", key, item);
	else
		sprintf(env->output, "\nValue %x Found: %p %x", key, item, 
			*(uint32_t *)(item));
	logMsg(env);

	if (NULL == found)
		sprintf(env->output, "\nList Item Not found: %p", found);
	else
		sprintf(env->output, 
		"\nList Item Found: %p Next %p Prev %p key %x item %p %x",
				found, found->next, found->prev, found->key,
				found->item, *(int *)(found->item));
	logMsg(env);

	display_list(env, &test_list);
        return 0;

show_help:
        sprintf(env->output, "\nFAILED: Extra parms or invalid values: %s\n",
			argv[0]);
        logMsg(env);
        cli_print_help(env, &ListPush);

        return 0;
};

struct cli_cmd ListFind = {
"listfind",
5,
1,
"Find value to ordered list.",
"<val>.\n"
	"<val> is the integer value to find in the list.\n",
ListFindCmd,
ATTR_NONE
};
	
extern struct cli_cmd ListRemove;
int ListRemoveCmd(struct cli_env *env, int argc, char **argv)
{
	if (argc)
		goto show_help;

	l_remove(&test_list, found);
	display_list(env, &test_list);

        return 0;

show_help:
        sprintf(env->output, "\nFAILED: Extra parms or invalid values: %s\n",
			argv[0]);
        logMsg(env);
        cli_print_help(env, &ListPush);

        return 0;
};

struct cli_cmd ListRemove = {
"listremove",
5,
0,
"Remove last found ordered list entry.",
"No Parameters.\n",
ListRemoveCmd,
ATTR_NONE
};
	
extern struct cli_cmd LListRemove;
int LListRemoveCmd(struct cli_env *env, int argc, char **argv)
{
	int *l_val = NULL;

	if (argc)
		goto show_help;
       
	if (NULL != found)
		l_val = (int *)found->item;

	l_remove(&test_list, found);
	display_list(env, &test_list);

	if (NULL != l_val) {
        	sprintf(env->output, "\nCan still access list value: %p %d\n",
			l_val, *l_val);
        	logMsg(env);
		free(l_val);
	};

        return 0;

show_help:
        sprintf(env->output, "\nFAILED: Extra parms or invalid values: %s\n",
			argv[0]);
        logMsg(env);
        cli_print_help(env, &ListPush);

        return 0;
};

struct cli_cmd LListRemove = {
"llistremove",
5,
0,
"Remove last found ordered list entry.",
"No Parameters.\n",
LListRemoveCmd,
ATTR_NONE
};
	
extern struct cli_cmd ListSize;
int ListSizeCmd(struct cli_env *env, int argc, char **argv)
{
	if (argc)
		goto show_help;


        sprintf(env->output, "\nList size %d\n", l_size(&test_list) );
        logMsg(env);

	display_list(env, &test_list);

        return 0;

show_help:
        sprintf(env->output, "\nFAILED: Extra parms or invalid values: %s\n",
			argv[0]);
        logMsg(env);
        cli_print_help(env, &ListSize);

        return 0;
};

struct cli_cmd ListSize = {
"listsize",
5,
0,
"Size of test list.",
"No Parameters.\n",
ListSizeCmd,
ATTR_NONE
};
	
void *test_l;
struct l_item_t *test_li;
extern struct cli_cmd ListHead;
int ListHeadCmd(struct cli_env *env, int argc, char **argv)
{
	if (argc)
		goto show_help;

	test_l = l_head(&test_list, &test_li);


        sprintf(env->output, "\nHEAD %p", test_li);
        logMsg(env);
	if (NULL == test_li)
		sprintf(env->output, " EMPTY\n");
	else
		sprintf(env->output,
		"\nnext %p prev %p  key %x item %p %x rc %p %x\n",
			test_li->next, test_li->prev, test_li->key,
			test_li->item, *(uint32_t *)(test_li->item),
			test_l, *(uint32_t *)(test_l));
	logMsg(env);

        return 0;

show_help:
        sprintf(env->output, "\nFAILED: Extra parms or invalid values: %s\n",
			argv[0]);
        logMsg(env);
        cli_print_help(env, &ListHead);

        return 0;
};

struct cli_cmd ListHead = {
"listhead",
5,
0,
"Printe head of test list.",
"No Parameters.\n",
ListHeadCmd,
ATTR_NONE
};
	
extern struct cli_cmd ListNext;
int ListNextCmd(struct cli_env *env, int argc, char **argv)
{
	if (argc)
		goto show_help;

	test_l = l_next(&test_li);


        sprintf(env->output, "\nNext %p", test_li);
        logMsg(env);
	if (NULL == test_li)
		sprintf(env->output, " END\n");
	else
		sprintf(env->output,
		"\nnext %p prev %p  key %x item %p %x rc %p %x\n",
			test_li->next, test_li->prev, test_li->key,
			test_li->item, *(uint32_t *)(test_li->item),
			test_l, *(uint32_t *)(test_l));
	logMsg(env);

        return 0;

show_help:
        sprintf(env->output, "\nFAILED: Extra parms or invalid values: %s\n",
			argv[0]);
        logMsg(env);
        cli_print_help(env, &ListNext);

        return 0;
};

struct cli_cmd ListNext = {
"listnext",
5,
0,
"Printe next element of test list.",
"No Parameters.\n",
ListNextCmd,
ATTR_NONE
};
	
struct cli_cmd *rskt_lib_cmds[] =
        { &ListDisplay,
	&ListInit,
 	&ListPush,
	&ListPop,
	&ListAdd,
	&ListRemove,
	&LListRemove,
	&ListFind,
	&ListSize,
	&ListHead,
	&ListNext
        };

void liblist_bind_cli_cmds(void)
{
        add_commands_to_cmd_db(sizeof(rskt_lib_cmds)/sizeof(rskt_lib_cmds[0]),
                                rskt_lib_cmds);

        return;
};

#ifdef __cplusplus
}
#endif

