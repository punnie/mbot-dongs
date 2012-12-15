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

#define LEVEL_DONGS 0

#define HELP_DONGS "!dongs dong - Dongs the dong."

#define TIMESTR_SIZE 10
#define FIELD_SIZE (NICK_SIZE+TIMESTR_SIZE+1)

#define SOURCE s->script.source
#define DEST s->script.dest
#define BUF s->script.buf
#define SEND_TEXT s->script.send_text


// This class defines each instance of the module.
// NetServer *s represents the server the module belongs to.
// If you need additional data (like a list of nicks or channels)
// it is here where it belongs.

struct dongs_type {
  NetServer *s;

  dongs_type (NetServer *server) : 
    s (server) {}
};

List * dongs_list;

#include <stdio.h>
#include <ctype.h>
#include <string.h>


///////////////
// prototypes
///////////////

// This function is used to retrieve the Module instance in a context
// where you only have a server.

static dongs_type *server2dongs (NetServer *);

// This is a generic callback for server events
static void dongs_event (NetServer *);

// This is the function where the module is initialized
static void dongs_conf (NetServer *, c_char);

// This function runs when the module starts
static void dongs_stop (void);

// This function runs when the module stops.
// This is where you delete stuff you created.
static void dongs_start (void);

extern "C" {

EXPORT struct Module::module_type module = {
  MODULE_VERSION,
  "dongs",
  dongs_start,
  dongs_stop,
  dongs_conf,
  NULL
};

}

/////////////
// commands
/////////////

/////////////////////
// events
////////////////////



//dongs event
void
dongs_event (NetServer *s)
{

  if (EVENT != EVENT_PRIVMSG
      || CMD[3][0] == ''				// ctcp
      || (CMD[2][0] >= '0' && CMD[2][0] <= '9'))	// dcc chat
    return;

  dongs_type* dongs = server2dongs(s);


  if (!((CMD[2][0] == '#') || ((CMD[2][0] == '@') && (CMD[2][1] == '#'))))
    return;
	  

    
    //SEND_TEXT (DEST, "%s", oss.str().c_str());
}

// !dongs cmd
static void
dongs_cmd (NetServer *s)
{
  dongs_type *dongs = server2dongs (s);
  if (dongs == NULL)
    {
      SEND_TEXT (DEST, "This command is not available.");
      return;
    }
  strsplit (CMD[3], BUF, 2);
  if (BUF[1][0] == 0)
    {
      SEND_TEXT (DEST, "%s", HELP_DONGS);
      return;
    }
    
  SEND_TEXT (DEST, "lol dongs %s", BUF[1]);
}


////////////////////
// module managing
////////////////////

// returns the invite channel for a given server, NULL if nonexistant
static dongs_type *
server2dongs (NetServer *s)
{
  dongs_type *a;
  dongs_list->rewind ();
  while ((a = (dongs_type *)dongs_list->next ()) != NULL)
    if (a->s == s)
      return a;
  return NULL;
}

// configuration file's local parser
static void
dongs_conf (NetServer *s, c_char bufread)
{

  char buf[4][MSG_SIZE+1];
  strsplit (bufread, buf, 3);
 
  if (strcasecmp (buf[0], "dongs") != 0)
    return;
  
  //varifvarificar se ja inicializado!
  dongs_type *dongs = server2dongs(s);
  if (dongs == NULL) {
    dongs = new dongs_type (s);
    
    if (dongs == NULL)
      s->b->conf_error ("error initializing dongs");
      
    dongs_list->add ((void *)dongs);
    
    s->script.events.add ((void *)dongs_event);
    s->script.cmd_bind (dongs_cmd, LEVEL_DONGS, "!dongs", module.mod, HELP_DONGS);
  
  }
}

// module termination
static void
dongs_stop (void)
{
  dongs_type *dongs;
  dongs_list->rewind ();
  while ((dongs = (dongs_type *)dongs_list->next ()) != NULL)
    {

      dongs->s->script.events.del ((void *)dongs_event);
      delete dongs;
    }
  delete dongs_list;
}

// module initialization
static void
dongs_start (void)
{
  dongs_list = new List();
}

