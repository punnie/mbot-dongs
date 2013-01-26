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

struct command_type {
  String* trigger;  // trigger
  int user_level;    // level required to run command
  int respond_with;  // response type, should change to an enum type TODO
  String* command;  // shell command
  
  command_type(String* t, int ul, int rw, String* c) : 
    trigger (t), 
    user_level (ul),
    respond_with (rw),
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
  char list[] = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
  int len = sizeof(list)/sizeof(list[1]);
  
  for (int i = 0; i < len; i++) {
    if (list[i] == c)
      return true;
  }
  return false;
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
void parse_command(char* buffer, command_type* command, char* params) {

  string params_s(params), 
    command_s(command->command->getstr()), 
    temp;
  stringstream ss(params_s);
  
  //Remove almost everything from PARAMS
  params_s.erase(remove_if(params_s.begin(), params_s.end(), is_invalid_char), params_s.end());
  
  int n = 1;
  while (ss >> temp) {
    stringstream temp_ss("");
    temp_ss << "{" << n++ << "}";
     
    //cout << "a: " << temp << endl;
    //cout << "b: " << temp_ss.str() << endl;
    //cout << "c: " << command_s << endl;
    replace(command_s, temp_ss.str(), temp);
    //cout << "d: " << command_s << endl;
  }
  
  strncpy(buffer, command_s.c_str(), COMMAND_SIZE);
  


  //deprecated
  //cout << "a: "<< command->trigger->getstr() << endl;
  //sprintf(buffer, command->command->getstr(), "~", "l");
  //cout << "b: \""<< params << "\"" << endl;
}

//returns the command for a given trigger
command_type* trigger2command(char* trigger, List* command_list) {
  command_type *command;
  command_list->rewind ();
  
  while ((command = (command_type *)command_list->next ()) != NULL) {
    if (*(command->trigger) == trigger) {      
      return command;
    }
  }
  return NULL;
}

// !shell cmd
static void
shell_cmd (NetServer *s)
{

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
  
    
    char cmdline[COMMAND_SIZE];
    parse_command(cmdline, command, BUF[1]);
    
    //debug
    cout << "Command: " << cmdline << endl;
        
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

      SEND_TEXT(DEST, "%s", buffer);
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

  cout << buf[4] << endl;
  shell_type *shell = server2shell(s);
  if (shell == NULL) {
    shell = new shell_type (s);
    
    if (shell == NULL)
      s->b->conf_error ("error initializing shell");
      
    shell_list->add ((void *)shell);
  }
  
    
  int cmdlevel = atoi(buf[2]);
  shell->commands->add((void*) new command_type(
    new String(buf[1], TRIGGER_SIZE),
     cmdlevel,
     atoi(buf[3]),
     new String(buf[4], COMMAND_SIZE)
  ));
  //s->script.events.add ((void *)shell_event);
  
  cout << " * binding " << buf[1] << " to " << buf[4] << endl;
  s->script.cmd_bind (shell_cmd, cmdlevel, buf[1], module.mod, HELP_shell);
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

