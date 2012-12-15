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

#define SOURCE s->script.source
#define DEST s->script.dest
#define SEND_TEXT s->script.send_text

#define MAX_URL_LENGTH 1024
#define MAX_URL_DETECT 3
struct sniffing_channel_type {
  String* channel;
  bool tellall; // sniff all links or just text/html
  bool tellfail; // message on fail?
  
  sniffing_channel_type(String* s, bool th, bool tf) : 
    channel (s), 
    tellall (th),
    tellfail (tf) {}
    
  ~sniffing_channel_type() {
    delete channel;
  }
};

struct urlsniffer_type {
  NetServer *s;
  List* channels;
  bool sniffing_channel(c_char channel);
  bool telling_fail_on_channel(c_char channel);
  bool telling_all_on_channel(c_char channel);
  urlsniffer_type (NetServer *server) : 
    s (server) {}
};

List * urlsniffer_list;

///////////////
// Utils
///////////////


#include <iostream>
#include <string>
#include <sstream>
#include "easycurl/easycurl.h"


char* ircstrip(char* src, char* dst) {
  int dst_i = 0;
  bool strp_colors = false;
  for (int i = 0; src[i] != 0; i++) {
    switch (src[i]) {
      case 0x02:
      case 0x1F:
      case 0x16:
      case 0x0F:
        break;
      case 0x03:
        strp_colors = true;
        break;
      default:
        if (strp_colors and (isdigit(src[i]) or src[i] == ',')) {
          NULL; 
        } else {
          strp_colors = false;
          dst[dst_i++] = src[i];
        }

        break;
    }
  }
  dst[dst_i] = 0;
  return dst;
}

int geturls (char* src, char dst[][MSG_SIZE]) {
  int count = 0;
  char *r = NULL;
  r = strtok(src, " ");
  while (r != NULL) {
    char temp[MSG_SIZE];
    ircstrip(r, temp);
    if ((strncmp(temp, "http://", 7) == 0) 
    or  (strncmp(temp, "https://", 8) == 0) 
    or  (strncmp(temp, "www.", 4) == 0)) {
      if (count < MAX_URL_DETECT) {
       strcpy(dst[count], temp);
       count++;
      } else {
        break;
      }
    }
    r = strtok(NULL, " ");
  }
  return count;
}


///////////////
// prototypes
///////////////
static urlsniffer_type *server2urlsniffer (NetServer *);
static void urlsniffer_event (NetServer *);
static void urlsniffer_conf (NetServer *, c_char);
static void urlsniffer_stop (void);
static void urlsniffer_start (void);
extern size_t decode_html_entities_utf8(char *dest, const char *src);

extern "C" {

EXPORT struct Module::module_type module = {
  MODULE_VERSION,
  "urlsniffer",
  urlsniffer_start,
  urlsniffer_stop,
  urlsniffer_conf,
  NULL
};

}


//check to see if a certain channel is in the mod's sniff list
bool urlsniffer_type::sniffing_channel(c_char channel) {
  
  sniffing_channel_type *s;
  this->channels->rewind ();
  while ((s = (sniffing_channel_type *)this->channels->next()) != NULL)
  {
    if (*(s->channel) == channel)
      return true;
  }
  return false;
}

bool urlsniffer_type::telling_fail_on_channel(c_char channel) {
  
  sniffing_channel_type *s;
  this->channels->rewind ();
  while ((s = (sniffing_channel_type *)this->channels->next()) != NULL)
  { 
    if (*(s->channel) == channel)
      return s->tellfail;
  }
  return false;
}

bool urlsniffer_type::telling_all_on_channel(c_char channel) {
  
  sniffing_channel_type *s;
  this->channels->rewind ();
  while ((s = (sniffing_channel_type *)this->channels->next()) != NULL)
  {
    if (*(s->channel) == channel)
      return s->tellall;
  }
  return false;
}

/////////////
// commands
/////////////

// N/A

/////////////////////
// events
////////////////////



//urlsniffer event
void
urlsniffer_event (NetServer *s)
{

  if (EVENT != EVENT_PRIVMSG
      || CMD[3][0] == ''				// ctcp
      || (CMD[2][0] >= '0' && CMD[2][0] <= '9'))	// dcc chat
    return;

  urlsniffer_type* urlsniffer = server2urlsniffer(s);


  if (!((CMD[2][0] == '#') || ((CMD[2][0] == '@') && (CMD[2][1] == '#'))))
    return;
	  
  if (!urlsniffer->sniffing_channel(CMD[2]))
    return;
	
  char buff[MAX_URL_DETECT][MSG_SIZE];
  int count;
  char cmdcopy[MSG_SIZE];
  strcpy(cmdcopy, CMD[3]);
  count = geturls(cmdcopy, buff);
  bool haschecked = false;
  for (int i = 0; i < count; i++) {
    std::string message;
    for(int j = 0; j < i; j++) {
      if (strcmp(buff[i], buff[j]) == 0) {
      haschecked = true;
      }
    }
    
    if (haschecked) {
      haschecked = false;
      continue;
    }
    
    std::ostringstream oss;
    std::string url = buff[i];
    std::string nick = SOURCE;
    EasyCurl e(url);
    
    if (e.requestWentOk == 1) {
       if (e.isHtml) {
         oss << nick << "'s link title: " << e.html_title << " (";
       } else {
         oss << nick << "'s link title: N/A (";
       }
       if (e.response_code != "200") {
         oss << "HTTP " << e.response_code << ", ";
       }
       oss << e.response_content_type;
       if (e.response_content_length == "-1") {
         oss << ")";
       } else {
         oss << ", " << e.response_content_length << " bytes)";
       }
    } else {
      oss << nick << "'s link sucks. (" << e.error_message << ")";
    }
    

    
    if ((!e.requestWentOk) && !urlsniffer->telling_fail_on_channel(CMD[2]))
      return;
      
    if (!e.isHtml && !urlsniffer->telling_all_on_channel(CMD[2]))
      return;
    
    
    SEND_TEXT (DEST, "%s", oss.str().c_str());
  }
}


////////////////////
// module managing
////////////////////

// returns the invite channel for a given server, NULL if nonexistant
static urlsniffer_type *
server2urlsniffer (NetServer *s)
{
  urlsniffer_type *a;
  urlsniffer_list->rewind ();
  while ((a = (urlsniffer_type *)urlsniffer_list->next ()) != NULL)
    if (a->s == s)
      return a;
  return NULL;
}

// configuration file's local parser
static void
urlsniffer_conf (NetServer *s, c_char bufread)
{

  char buf[4][MSG_SIZE+1];
  strsplit (bufread, buf, 3);
 
  if (strcasecmp (buf[0], "urlsniffer") != 0)
    return;
  
  //varifvarificar se ja inicializado!
  urlsniffer_type *urlsniffer = server2urlsniffer(s);
  if (urlsniffer == NULL) {
    urlsniffer = new urlsniffer_type (s);
    if (urlsniffer == NULL)
      s->b->conf_error ("error initializing urlsniffer");
	urlsniffer_list->add ((void *)urlsniffer);
	urlsniffer->channels = new List();
	s->script.events.add ((void *)urlsniffer_event);
  }
  
  urlsniffer->channels->add ((void *) new sniffing_channel_type(
    new String(buf[1], CHANNEL_SIZE),
    (strncmp(buf[2], "tellall", 7) == 0),
    (strncmp(buf[3], "tellfail", 8) == 0)
  ));
}

// module termination
static void
urlsniffer_stop (void)
{
  urlsniffer_type *urlsniffer;
  urlsniffer_list->rewind ();
  while ((urlsniffer = (urlsniffer_type *)urlsniffer_list->next ()) != NULL)
    {
      delete urlsniffer->channels;
      
      urlsniffer->s->script.events.del ((void *)urlsniffer_event);
      delete urlsniffer;
    }
  delete urlsniffer_list;
}

// module initialization
static void
urlsniffer_start (void)
{
  urlsniffer_list = new List();
}

