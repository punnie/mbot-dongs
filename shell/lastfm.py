import pylast 
import sys
import code

def usage():
  print(LEL + " Usage: ")
  print("!lastfm userinfo|topartists|topalbums|weeklyartists|weeklyalbums <username>")
  print("!lastfm compare <username1> <username2>")
  exit(-1);


LEL = "0,5last.fm"
if len(sys.argv) < 3:
  usage();
  
chart_length = 10
query = sys.argv[1]
user = sys.argv[2]
  
if query == "compare" and len(sys.argv) < 4:
  usage();
  
else:
  user2 = sys.argv[3]
  
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
  
else:
  usage();

  


