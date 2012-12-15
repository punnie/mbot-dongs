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

#include <stdio.h>
#include <ctype.h>
#include <string.h>


#define MAX_URL_DT 3
#define USERAGENT_STR "Opera/12.80 (Windows NT 5.1; U; en) Presto/2.10.289 Version/12.02"
#define ISPRINT_LOCALE1 "pt_PT.UTF-8"
#define ISPRINT_LOCALE2 "pt_PT.ISO-8859-1"
#include <iostream>
#include <string>
#include <sstream>
#include <locale>
#include <algorithm>
#include "curl/curl.h"

bool is_not_printable(char c) {
  std::locale l1(ISPRINT_LOCALE1);
  //std::locale l2(ISPRINT_LOCALE2);
  //return !(std::isprint(c,l1) or std::isprint(c,l2));
  return !std::isprint(c,l1);
}

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
      if (count < MAX_URL_DT) {
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

///////////
// libcurl
//////////
#include <iostream>
#include <string>
#include <sstream>
#include <algorithm>
#include "curl/curl.h"

#define MAX_TITLE_LEN 256

// Limit buffer size to 500 KB
#define MAX_DL_SIZE (500*1024)

// tell fail?
class MyCurlRequest {
  
  public:
    int ok;
    std::string effective_url;
    std::string content_type;
    std::string content_length;
    std::string response_code;
    std::string redirect_count;
    std::string buffer; 
    std::string curlstatus;
    std::string htmltitle;

    MyCurlRequest() {
    }

    MyCurlRequest(CURLcode result) {
      this->ok = 0;
      this->curlstatus = curl_easy_strerror(result);
    }

    MyCurlRequest(CURLcode result, char* effective_url, string content_type, double content_length, long response_code, long redirect_count, std::string buffer) {

      std::ostringstream oss;
      std::string s;
      
      oss.precision(20);
      this->curlstatus = curl_easy_strerror(result);
      this->ok = 1;

      oss << effective_url;
      this->effective_url = oss.str();
      oss.str("");

      oss << content_type;
      s = oss.str();
      remove_if(s.begin(), s.end(), is_not_printable);
      this->content_type = s;
      oss.str("");
      
      oss << content_length;
      this->content_length = oss.str();
      oss.str("");

      oss << response_code;
      this->response_code = oss.str();
      oss.str("");

      oss << redirect_count;
      this->redirect_count = oss.str();
      oss.str("");
      
      oss << buffer;
      s = oss.str();
      remove_if(s.begin(), s.end(), is_not_printable);
      this->buffer = s;
      oss.str("");

      this->htmltitle = "N/A";
    }

    void printme() {
      ostringstream oss;
      oss << "url title: " << this->htmltitle << " (Content-Type " << this->content_type 
          <<  ", Content-Length " << this->content_length << ", HTTP Status " << this->response_code 
          << ", " << this->redirect_count << " redirects. " << this->buffer.length() << " bytes used.)";
      std::cout << oss.str() << std::endl;
    
    }
        
    bool is_html() {
      return this->content_type.compare(0, 9, "text/html") == 0;
    }
    
    void loadHTMLTitle() {
      char buff[MSG_SIZE];
      try {
        size_t pos1 = this->buffer.find("<title>");
        size_t pos2 = this->buffer.find("</title>");
        if (pos1 == -1)
          pos1 = this->buffer.find("<TITLE>");
        
        if (pos2 == -1)
          pos2 = this->buffer.find("</TITLE>");

        size_t tlen = pos2-(pos1+7);

        if (tlen > MAX_TITLE_LEN) {
          std::cout << pos1 << " " << pos2 << std::endl;
          this->htmltitle = "Failed to retrieve :(";
          return;
        }
        std::string title = this->buffer.substr(pos1+7, tlen);

        
        size_t wpos1 = title.find_first_not_of(" \t\n\r");
        size_t wpos2 = title.find_last_not_of(" \t\n\r");
        
        this->htmltitle = title.substr(wpos1, wpos2-wpos1+1);
        strncpy(buff, this->htmltitle.c_str(), MSG_SIZE);
        decode_html_entities_utf8(buff,(char*)NULL);
        this->htmltitle = buff;
      } catch (...) {
        this->htmltitle = "Failed to retrieve. :(";
      }
    }
};

class MyCurl
{
  private:

   static int writer(char *data, size_t size, size_t nmemb, std::string *buffer_in)
    {
      // Is there anything in the buffer?
      if (buffer_in != NULL)
      {
        // Append the data to the buffer
        buffer_in->append(data, size * nmemb);
        bufferTotal += size * nmemb;
        
        //Stop if max size exceeded by returning 0
        if (bufferTotal < MAX_DL_SIZE) {
          return size * nmemb;
        } else {
          return 0;
        }
      }

      return 0;
    }
   
  public:
    static int bufferTotal;
    static MyCurlRequest get(std::string url, bool getbody) {
      const char* c = url.c_str();
      return get(c, getbody);
    }

    static MyCurlRequest get(const char* url, bool getbody)
    {

      char* effective_url;
      char* content_type;
      char content_type_stripped[MSG_SIZE];
      double content_length;
      long response_code;
      long redirect_count;

      CURL *curl;
      CURLcode result;
      
      // Create our curl handle
      curl = curl_easy_init();
      //curl = curl_global_init(CURL_GLOBAL_ALL)
      char errorBuffer[CURL_ERROR_SIZE];
      
      // Write all expected data in here
      std::string buffer;

      if (curl)
      {
        curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, errorBuffer);
        curl_easy_setopt(curl, CURLOPT_URL, url);
        curl_easy_setopt(curl, CURLOPT_USERAGENT, USERAGENT_STR);
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, 5); //I'm not sure how this works
        curl_easy_setopt(curl, CURLOPT_MAXREDIRS, 10);
        curl_easy_setopt(curl, CURLOPT_HEADER, 0);
        curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1);
        //curl_easy_setopt(curl, CURLOPT_RANGE, DL_RANGE);
        curl_easy_setopt(curl, CURLOPT_ENCODING, "identity");
        curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1);
       if (getbody) {
          curl_easy_setopt(curl, CURLOPT_WRITEDATA, &buffer);
          curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, MyCurl::writer);
          curl_easy_setopt(curl, CURLOPT_NOBODY, 0);
        } else {
          curl_easy_setopt(curl, CURLOPT_WRITEHEADER, &buffer);
          curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, MyCurl::writer);
          curl_easy_setopt(curl, CURLOPT_NOBODY, 1);
        }
        // Attempt to retrieve the remote page
        result = curl_easy_perform(curl);

        // Along with the request get some information
        curl_easy_getinfo(curl, CURLINFO_EFFECTIVE_URL, &effective_url);
        curl_easy_getinfo(curl, CURLINFO_CONTENT_TYPE, &content_type);

        curl_easy_getinfo(curl, CURLINFO_CONTENT_LENGTH_DOWNLOAD, &content_length);
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response_code);
        curl_easy_getinfo(curl, CURLINFO_REDIRECT_COUNT, &redirect_count);
      }

      MyCurlRequest req;
      //std::cout << buffer <<std::endl;
      if(result == CURLE_OK) {
        req = MyCurlRequest(result, effective_url, content_type,
          content_length, response_code, redirect_count, buffer);
      } else {
        req = MyCurlRequest(result);
      }


      // Always cleanup
      curl_easy_cleanup(curl);
      return req;
    }
      // EAZY :-)
    static MyCurlRequest easyget(std::string url) {
      return easyget(url.c_str());
    }

    static MyCurlRequest easyget(const char* url) {
      bufferTotal = 0;
      MyCurlRequest h = MyCurl::get(url, false);
      if (h.ok == 1) {
        std::transform(h.content_type.begin(), h.content_type.end(), h.content_type.begin(), ::tolower);
        if (h.is_html()) {
          MyCurlRequest b = MyCurl::get(url, true); //if text/html then we are going to do the full req
          if ((b.ok) and (b.content_type.compare(0, 9, "text/html")==0)) {
            b.loadHTMLTitle();
          }
          return b; 
        } else {
          return h;
        }
      } else {
        return h; 
      }
    }
};
int MyCurl::bufferTotal;

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
   
    printf("tellfail is %d on %s\n", (int)s->tellfail, s->channel->getstr());
    
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
	
  char buff[MAX_URL_DT][MSG_SIZE];
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
    MyCurlRequest x = MyCurl::easyget(url);
    
    if (x.ok == 1) {

       oss << nick << "'s link title: " << x.htmltitle << " (";
       if (x.response_code != "200") {
         oss << "HTTP " << x.response_code << ", ";
       }
         oss << x.content_type;
       if (x.content_length == "-1") {
         oss << ")";
       } else {
         oss << ", " << x.content_length << " bytes)";
       }
    } else {
      oss << nick << "'s link sucks. (" << x.curlstatus << ")";
    }
    

    
    if ((x.ok == 0) && !urlsniffer->telling_fail_on_channel(CMD[2]))
      return;
      
    if (!x.is_html() && !urlsniffer->telling_all_on_channel(CMD[2]))
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

