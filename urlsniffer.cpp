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

#include <iostream>
#include <string>
#include <sstream>
#include <vector>
#include "easycurl/easycurl.h"

#define SOURCE s->script.source
#define DEST s->script.dest
#define SEND_TEXT s->script.send_text

#define MAX_URL_LENGTH 1024
#define MAX_URL_DETECT 3
#define TIME_BUFFER_SIZE 80

struct sniffing_channel_type {
  String* channel;
  bool tellall; // sniff all links or just text/html
  bool tellfail; // message on fail?
  bool tellold; // tell if the url was already seen by the channel

  sniffing_channel_type(String* s, bool th, bool tf, bool to) : 
    channel (s), 
    tellall (th),
    tellfail (tf),
    tellold (to) {}
    
  ~sniffing_channel_type() {
    delete channel;
  }
};

/* For telling on old urls */
struct url_type {
  string url;
  string channel;
  string nick;
  time_t timestamp;
  
  url_type(string u, string c, string n) :
    url (u), channel(c), nick(n), timestamp(time(0))
    { }
};

struct urlsniffer_type {
  NetServer *s;
  List* channels;
  vector<url_type> urls_seen;

  bool sniffing_channel(c_char channel);
  bool telling_fail_on_channel(c_char channel);
  bool telling_all_on_channel(c_char channel);
  bool telling_old_on_channel(c_char channel);
  time_t how_old_url(string url, string channel, string newnick, string& orignick);

  urlsniffer_type (NetServer *server) : 
    s (server) {}
  
  ~urlsniffer_type () {
    delete channels;
  }
};


List * urlsniffer_list;

///////////////
// Utils
///////////////


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

bool urlsniffer_type::telling_old_on_channel(c_char channel) {
  
  sniffing_channel_type *s;
  this->channels->rewind ();
  while ((s = (sniffing_channel_type *)this->channels->next()) != NULL)
  {
    if (*(s->channel) == channel)
      return s->tellold;
  }
  return false;
}

time_t urlsniffer_type::how_old_url(string url, string channel, string newnick, string& orignick) {
  time_t now = time(0);
  bool found = false;
  for (vector<url_type>::iterator it = this->urls_seen.begin() ; it != urls_seen.end(); ++it) {

    //cout << "d: " << it->url << ", " << it->nick << ", " << it->timestamp << ", " << url << ", " << channel << endl;

    if (it->url == url && it->channel == channel) {
      found = true;
      orignick = string(it->nick);
      return now - it->timestamp;
    }
  }
  
  if (!found) {
    url_type url_t(url, channel, newnick);
    this->urls_seen.push_back(url_t);
  }
  return (time_t)0;
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

    ostringstream oss;
    string url = buff[i];
    string nick = SOURCE;
    string channel(CMD[2]);
    
    //telling on old urls?
    string orignick;
    time_t age;
    
    bool tell_old = urlsniffer->telling_old_on_channel(CMD[2]);
    if (tell_old) {
      age = urlsniffer->how_old_url(url, channel, nick, orignick); 

      if (age > 0) {
        char buffer[TIME_BUFFER_SIZE];
        
        int days = age / 60 / 60 / 24;
        int hours = (age / 60 / 60) % 24;
        int minutes = (age / 60) % 60;

        char pl_days[2] = {0};
        if (days != 1)
          strcpy(pl_days, "s");
        char pl_hours[2] = {0};
        if (hours != 1)
          strcpy(pl_hours, "s");
        char pl_minutes[2] = {0};
        if (days != 1)
          strcpy(pl_hours, "s");
        
        if (days == 0 && hours == 0) {
          if (minutes == 0) {
            snprintf(buffer, TIME_BUFFER_SIZE, "just now", minutes, pl_minutes);
          } else {
            snprintf(buffer, TIME_BUFFER_SIZE, "%d minute%s ago", minutes, pl_minutes);
          }
        } else if (days == 0) {
          if (minutes > 0) {
            snprintf(buffer, TIME_BUFFER_SIZE, "%d hour%s, %d minute%s ago", hours, pl_hours, minutes, pl_minutes);
          } else {
            snprintf(buffer, TIME_BUFFER_SIZE, "%d hour%s ago", hours, pl_hours);
          }
        } else {
          if (hours > 0) {
            snprintf(buffer, TIME_BUFFER_SIZE, "%d day%s, %d hour%s ago", days, pl_days, hours, pl_hours);
          } else {
            snprintf(buffer, TIME_BUFFER_SIZE, "%d day%s ago", days, pl_days);
          }
        }
        
        SEND_TEXT(DEST, "BAH! %s's link is OLD! (shown %s by %s)", nick.c_str(), buffer, orignick.c_str());
        continue;
      }
    }
    
    //prevent highlights
    nick.insert(1, "0");
    
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

  char buf[5][MSG_SIZE+1];
  strsplit (bufread, buf, 4);
 
  if (strcasecmp (buf[0], "urlsniffer") != 0)
    return;
  
  //verificar se ja foi inicializado!
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
    (strncmp(buf[3], "tellfail", 8) == 0),
    (strncmp(buf[4], "tellold", 7) == 0)
  ));
}

// module termination
static void
urlsniffer_stop (void)
{
  sniffing_channel_type *channel;
  urlsniffer_type *urlsniffer;
  urlsniffer_list->rewind ();
  while ((urlsniffer = (urlsniffer_type *)urlsniffer_list->next ()) != NULL) {  
    urlsniffer->s->script.events.del ((void *)urlsniffer_event);
    urlsniffer->channels->rewind();
    while ((channel = (sniffing_channel_type*)urlsniffer->channels->next ()) != NULL) {
      delete channel;
    }
  
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

