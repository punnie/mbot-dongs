import pylast 
import sys
import code

LEL = "0,4last.fm"
if len(sys.argv) < 2:
  print(LEL + "Usage: !lastfm <username>")
  exit(-1);
  
chart_length = 10
user = sys.argv[1]
  
(api_key, api_secret) = \
  ("fce0a7524bf2174e465cc2164029bf1f", "5ae9f52609c10a67d8b628f17fd69adc")
    
class LastFM:
  def __init__(self, proxy_host = None, proxy_port = None, proxy_enabled = False):
    api = pylast.get_lastfm_network(api_key, api_secret)
    if proxy_enabled:
      api.enable_proxy(host = proxy_host, port = proxy_port)
    
    self.api = api
    
  def get_weekly_charts(self):
    try:
      artist_list = self.api.get_user(user).get_weekly_artist_charts()
      
    except pylast.WSError as e:
      print(LEL + " WSError %s: %s" % (e.status,e.details))
      exit(-1)
      
    if len(artist_list) == 0:
      print(LEL + " No artists found in %s's weekly charts :(" % user)
      exit(-1)

    parsed_list = ["" + i.item.__str__() + " (" + str(i.weight) + ")" for i in artist_list[:10]]

    chart_text = ", ".join(parsed_list);
    print LEL + " Weekly Top %d for %s: %s" % (chart_length, user, chart_text)
  
lastfm = LastFM()


lastfm.get_weekly_charts()
#code.interact(local=locals())
  

