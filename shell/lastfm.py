import pylast 
import sys
import code
import re

def usage():
  print(LEL + " Usage: ")
  print("!lastfm compare <username1> <username2>")
  print("!lastfm artistinfo|artistevents <artist name>")
  print("!lastfm userinfo|topartists|topalbums|weeklyartists|weeklyalbums <username>")
  exit(-1);


LEL = "0,5last.fm"
NUM_EVENTS = 5

if len(sys.argv) < 3:
  usage();
  
chart_length = 10
query = sys.argv[1]

user = sys.argv[2]
  
if query == "compare":
  if len(sys.argv) < 4:
    usage();
  else:
    user2 = sys.argv[3]

  
if query == "artistinfo" or query == "artistevents":
  if len(sys.argv) < 3:
    usage();
  else:
    artist = " ".join(sys.argv[2:])
  
  
(api_key, api_secret) = \
  ("fce0a7524bf2174e465cc2164029bf1f", "5ae9f52609c10a67d8b628f17fd69adc")
    
class LastFM:
  def __init__(self, proxy_host = None, proxy_port = None, proxy_enabled = False):
    api = pylast.get_lastfm_network(api_key, api_secret)
    if proxy_enabled:
      api.enable_proxy(host = proxy_host, port = proxy_port)
    
    self.api = api
  
  def get_user_info(self):
    try:
      ui = self.api.get_user(user).get_info()
      
    except pylast.WSError as e:
      print(LEL + " WSError %s: %s" % (e.status,e.details))
      exit(-1)
    
    print LEL + " Profile info for %s (%s, %s, %s) - Country: %s - Registered: %s - Play count: %s -- %s" % (ui['name'], ui['realname'], ui['age'], ui['gender'], ui['country'], ui['registered'], ui['playcount'], ui['url'])

  def get_top_artists(self):
    try:
      artist_list = self.api.get_user(user).get_top_artists()
      
    except pylast.WSError as e:
      print(LEL + " WSError %s: %s" % (e.status,e.details))
      exit(-1)
      
    if len(artist_list) == 0:
      print(LEL + " No artists found in %s's weekly charts :(" % user)
      exit(-1)

    parsed_list = ["" + i.item.__str__() + " (" + str(i.weight) + ")" for i in artist_list[:chart_length]]

    chart_text = ", ".join(parsed_list);
    print LEL + " Top %d artists for %s: %s" % (chart_length, user, chart_text)

  def get_top_albums(self):
    try:
      album_list = self.api.get_user(user).get_top_albums()
      
    except pylast.WSError as e:
      print(LEL + " WSError %s: %s" % (e.status,e.details))
      exit(-1)
      
    if len(album_list) == 0:
      print(LEL + " No albums found in %s's weekly charts :(" % user)
      exit(-1)

    parsed_list = ["" + i.item.__str__() + " (" + str(i.weight) + ")" for i in album_list[:chart_length]]

    chart_text = ", ".join(parsed_list);
    print LEL + " Top %d albums for %s: %s" % (chart_length, user, chart_text)
  
  
  def get_weekly_artist_charts(self):
    try:
      artist_list = self.api.get_user(user).get_weekly_artist_charts()
      
    except pylast.WSError as e:
      print(LEL + " WSError %s: %s" % (e.status,e.details))
      exit(-1)
      
    if len(artist_list) == 0:
      print(LEL + " No artists found in %s's weekly charts :(" % user)
      exit(-1)

    parsed_list = ["" + i.item.__str__() + " (" + str(i.weight) + ")" for i in artist_list[:chart_length]]

    chart_text = ", ".join(parsed_list);
    print LEL + " Weekly Top %d artists for %s: %s" % (chart_length, user, chart_text)
    
  def get_weekly_album_charts(self):
    try:
      album_list = self.api.get_user(user).get_weekly_album_charts()
      
    except pylast.WSError as e:
      print(LEL + " WSError %s: %s" % (e.status,e.details))
      exit(-1)
      
    if len(album_list) == 0:
      print(LEL + " No albums found in %s's weekly charts :(" % user)
      exit(-1)

    parsed_list = ["" + i.item.__str__() + " (" + str(i.weight) + ")" for i in album_list[:chart_length]]

    chart_text = ", ".join(parsed_list);
    print LEL + " Weekly Top %d albums for %s: %s" % (chart_length, user, chart_text)
  
  def compare_users(self):
    try:
      comparison = self.api.get_user(user).compare_with_user(user2)
      
    except pylast.WSError as e:
      print(LEL + " WSError %s: %s" % (e.status,e.details))
      exit(-1)

    comparison_index = round(float(comparison[0]),2)
    artist_list = comparison[1]

    parsed_list = [item.__str__() for item in artist_list]

    chart_text = ", ".join(parsed_list);
    print LEL + " Comparison between %s and %s: Similarity Index: %.2f - Common Artists: %s" % (user, user2, comparison_index, chart_text)

  def get_artist_info(self):
    try:
      artist_info = self.api.get_artist(artist)

      bio = artist_info.get_bio_summary()
      if bio != None:
        bio = re.sub('<[^<]+?>', '', bio)
      
      name = artist_info.get_name()
      listener_count = artist_info.get_listener_count()
      
      
      tags = artist_info.get_top_tags()
      tag_text = ""
      if tags != []:
        tag_text = ", ".join([tag.item.__str__() for tag in tags[:10]])
        tag_text = "Tags: %s." % tag_text

      similars = artist_info.get_similar()
      similars_text = ", ".join([similar.item.__str__() for similar in similars[:10]])

      print LEL + " %s (%d listeners). %s" % (name, listener_count, tag_text)
      print "Similar Artists: %s" % (similars_text)
      
      if bio != None:
        print bio
    
    except pylast.WSError as e:
      print(LEL + " WSError %s: %s" % (e.status,e.details))
      exit(-1)

  def get_artist_events(self):
    try:
      artist_info = self.api.get_artist(artist)
      artist_events = artist_info.get_upcoming_events()

      
      #list comprehensiion wont do cause EXCEPTIONS
      events_str = ""
      n = 0
      for event in artist_events:
        try:
          e_date = event.get_start_date()
          e_name = event.get_title()
          e_url = event.get_url()
          
          events_str += " - %s: %s - %s\n" %(e_date[:-9], e_name, e_url)
          n = n + 1
          
          if n >= NUM_EVENTS:
            break
        except pylast.WSError:
          pass

      if n > 0:
        print LEL + " events for %s:" % artist_info.get_name()
        print events_str
      else:
        print LEL + " no events found for artist %s." % artist_info.get_name()

    except pylast.WSError as e:
      print(LEL + " WSError %s: %s" % (e.status,e.details))
      exit(-1)
  
lastfm = LastFM()

if query == "weeklyartists":
  LastFM().get_weekly_artist_charts()
elif query == "weeklyalbums":
  LastFM().get_weekly_album_charts()
elif query == "topalbums":
  LastFM().get_top_albums()
elif query == "topartists":
  LastFM().get_top_artists()
elif query == "userinfo":
  LastFM().get_user_info()
elif query == "compare":
  LastFM().compare_users()
elif query == "artistinfo":
  LastFM().get_artist_info()
elif query == "artistevents":
  LastFM().get_artist_events()  
  
else:
  usage();

  


