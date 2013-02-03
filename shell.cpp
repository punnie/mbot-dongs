/*

This file is part of mbot.

mbot is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or (at
your option) any later version.

mbot is distributed in the hope that it will be useful, but
WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
General Public License for more details.

You should have received a copy of the GNU General Public License
along with mbot; if not, write to the Free Software
Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

*/

#include "mbot.h"

#define LEVEL_shell 0

#define HELP_shell "!shell dong - shell the dong."

#define TIMESTR_SIZE 10
#define FIELD_SIZE (NICK_SIZE+TIMESTR_SIZE+1)

#define TRIGGER_SIZE 64
#define COMMAND_SIZE 256

#define RESULT_LINE_SIZE 1024
#define RESULT_LINE_MAX 25

#define SOURCE s->script.source
#define DEST s->script.dest
#define BUF s->script.buf
#define SEND_TEXT s->script.send_text

#define RESPONSE_USERMSG 0
#define RESPONSE_CHANMSG 1
#define RESPONSE_USERNOT 2
#define RESPONSE_CHANNOT 3

// /!\ ACHTUNG! CAUTION WITH THE LINE BELOW /!\
// /!\ INCLUDING CERTAIN CHARACTERS HERE WILL ALLOW /!\
// /!\ ARBITRARY SHELL COMMANDS TO BE EXECUTED /!\ 

#define VALID_CHARS "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789 _-.@!"

// /!\ CAUTION WITH THE LINE ABOVE. ACHTUNG! /!\ 

//The characters you want tabs replaced with.
#define TAB_CHAR "  "
struct command_type {
  String* trigger;  // trigger
  int user_level;    // level required to run command
  int cmd_type;  // command type (0=channel, 1=pvt, 2=both)
  String* command;  // shell command
  
  command_type(String* t, int ul, int ct, String* c) : 
    trigger (t),
    user_level (ul),
    cmd_type (ct),
    command (c) {}
    
  ~command_type() {
    delete trigger;
    delete command;
  }
};


// This class defines each instance of the module.
// NetServer *s represents the server the module belongs to.
// If you need additional data (like a list of nicks or channels)
// it is here where it belongs.

struct shell_type {
  NetServer *s;
  List *commands;

  shell_type (NetServer *server) : 
    s (server) {
      commands = new List();
    }
    
  ~shell_type() {
    delete commands;
  }
};

List * shell_list;

#include <stdio.h>
#include <ctype.h>
#include <string.h>

#include <string>
#include <sstream>
#include <algorithm>
using namespace std;

///////////////
// prototypes
///////////////

// This function is used to retrieve the Module instance in a context
// where you only have a server.

static shell_type *server2shell (NetServer *);

// This is a generic callback for server events
static void shell_event (NetServer *);

// This is the function where the module is initialized
static void shell_conf (NetServer *, c_char);

// This function runs when the module starts
static void shell_stop (void);

// This function runs when the module stops.
// This is where you delete stuff you created.
static void shell_start (void);

extern "C" {

EXPORT struct Module::module_type module = {
  MODULE_VERSION,
  "shell",
  shell_start,
  shell_stop,
  shell_conf,
  NULL
};

}

/////////////
// commands
/////////////

bool is_invalid_char(char c) {
  char list[] = VALID_CHARS;
  int len = sizeof(list)/sizeof(list[1]);
  
  for (int i = 0; i < len; i++) {
    if (list[i] == c)
      return false;
  }
  return true;
}

// http://stackoverflow.com/questions/3418231/c-replace-part-of-a-string-with-another-string

bool replace(std::string& str, const std::string& from, const std::string& to) {
    size_t start_pos = str.find(from);
    if(start_pos == std::string::npos)
        return false;
    str.replace(start_pos, from.length(), to);
    return true;
}

//builds the shell command based on user input
//if the return value is false, buffer contains
//an error message
bool parse_command(char* buffer, command_type* command, char* mask, char* params) {

  string mask_s(mask), params_s(params), command_s(command->command->getstr());

  //Remove almost everything from PARAMS
  
  // the line below is quite important
  mask_s.erase(remove_if(mask_s.begin(), mask_s.end(), is_invalid_char), mask_s.end());
  params_s.erase(remove_if(params_s.begin(), params_s.end(), is_invalid_char), params_s.end());
  // the line above is quite important

  replace(command_s, "{n}", mask_s);
  replace(command_s, "{}", params_s);

  strncpy(buffer, command_s.c_str(), COMMAND_SIZE);
  
  return true;
}

//returns the command for a given trigger
command_type* trigger2command(char* trigger, List* command_list) {
  command_type *command;
  command_list->rewind ();
  
  while ((command = (command_type *)command_list->next ()) != NULL) {
    String* trigger_lc = new String(trigger, TRIGGER_SIZE);
    trigger_lc->lower();
    if (*(command->trigger) == *trigger_lc) {
      delete trigger_lc;
      return command;
    } else {
      delete trigger_lc;
    }
  }
  return NULL;
}

// !shell cmd
static void
shell_cmd (NetServer *s)
{

  char nick[NICK_SIZE];
  shell_type *shell = server2shell (s);

  if (shell == NULL) {
    SEND_TEXT (DEST, "This command is not available.");
    return;
  }

  strsplit (CMD[3], BUF, 1);

  command_type* command = trigger2command(BUF[0], shell->commands);
  

  
  if (command == NULL) {
  
    SEND_TEXT (DEST, "Failed retrieving :(");  
    
  } else {
    
    bool isChannel = (CMD[2][0] == '#') || ((CMD[2][0] == '@') && (CMD[2][1] == '#'));
    if ((isChannel && command->cmd_type == 1) || (!isChannel && command->cmd_type == 0)) {
      if (command->cmd_type == 0) {
        SEND_TEXT (DEST, "This command is only available in channels.");
      } else {
        SEND_TEXT (DEST, "This command is only available in private.");
      }
      return;
    }
  
    char cmdline[COMMAND_SIZE];
    if (!parse_command(cmdline, command, CMD[0], BUF[1])) {
      SEND_TEXT(DEST, cmdline); //in this case cmdline will contain an error msg
    }

        
    int lines_sent = 0;
    char buffer[RESULT_LINE_SIZE];
    FILE* p;

    if ((p = popen(cmdline, "r")) == NULL) {
    
      SEND_TEXT(DEST, "OMG HELP SOMETHING IS WRONG WITH 13%s", BUF[0]);
      return;
      
    }
    
    while(!feof(p)) {

      if (fgets(buffer, RESULT_LINE_SIZE, p) == NULL) {
        break;
      }
      
      if (lines_sent >= RESULT_LINE_MAX) {
        SEND_TEXT(DEST, "command output stopped at %d lines.", RESULT_LINE_MAX);
        break;
      }
      
      string temp(buffer);
      while (temp.find("\t") != string::npos)
        replace(temp, "\t", TAB_CHAR);

      SEND_TEXT(DEST, "%s", temp.c_str());
      lines_sent++;
    
    }
  }
}

/////////////////////
// events
////////////////////


//shell event
void
shell_event (NetServer *s)
{

  if (EVENT != EVENT_PRIVMSG
      || CMD[3][0] == ''				// ctcp
      || (CMD[2][0] >= '0' && CMD[2][0] <= '9'))	// dcc chat
    return;

  shell_type* shell = server2shell(s);


  if (!((CMD[2][0] == '#') || ((CMD[2][0] == '@') && (CMD[2][1] == '#'))))
    return;
	  


    //SEND_TEXT (DEST, "%s", oss.str().c_str());
}

////////////////////
// module managing
////////////////////

// returns the invite channel for a given server, NULL if nonexistant
static shell_type *
server2shell (NetServer *s)
{
  shell_type *a;
  shell_list->rewind ();
  while ((a = (shell_type *)shell_list->next ()) != NULL)
    if (a->s == s)
      return a;
  return NULL;
}

// configuration file's local parser
static void
shell_conf (NetServer *s, c_char bufread)
{

  char buf[5][MSG_SIZE+1];
  strsplit (bufread, buf, 4);
 
  if (strcasecmp (buf[0], "shell") != 0)
    return;

  shell_type *shell = server2shell(s);
  if (shell == NULL) {
    shell = new shell_type (s);
    
    if (shell == NULL)
      s->b->conf_error ("error initializing shell");

    shell_list->add ((void *)shell);
    
  }


  // Create a new command_type instance
  // 0     1        2          3           4
  // SHELL !trigger <cmdtype> <userlevel> <shell command>
  String *n_trigger = new String(buf[1], TRIGGER_SIZE);
  int n_cmdlevel = atoi(buf[3]);
  int n_cmdtype = atoi(buf[2]);
  String *n_command = new String(buf[4], COMMAND_SIZE);

  shell->commands->add(
    (void*) new command_type(
       n_trigger,
       n_cmdlevel,
       n_cmdtype,
       n_command
    )
  );

  cout << "  " << buf[1] << " is bound to \"" << buf[4] << "\" with acc lvl " << n_cmdlevel <<  endl;
  s->script.cmd_bind (shell_cmd, n_cmdlevel, buf[1], module.mod, HELP_shell);
}

// module termination
static void
shell_stop (void)
{
  shell_type *shell;
  command_type *command;
  shell_list->rewind ();
  while ((shell = (shell_type *)shell_list->next ()) != NULL) {
    shell->s->script.events.del ((void *)shell_event);
    
    shell->commands->rewind();
    while ((command = (command_type*)shell->commands->next ()) != NULL) {
      delete command;
    }
    delete shell;
    
  }
  delete shell_list;
}

// module initialization
static void
shell_start (void)
{
  shell_list = new List();
}

