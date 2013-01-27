import feedparser
import re
import sys
import code

L = "0,11Twitter" 

def strip(html):
  return re.sub('<[^<]+?>', '', html)
  
def usage():
  print "%s Usage: !twitter username [n]" % L
  exit(-1)

if len(sys.argv) < 2:
  usage()

user = sys.argv[1]
try:
  n = int(sys.argv[2])
except IndexError, ValueError:
  n = 0

url = "https://api.twitter.com/1/statuses/user_timeline.rss?screen_name=%s"
profile_url = url % user

f = feedparser.parse(profile_url)

if f.bozo == 1:
  print "%s omg :( %s" % (f.bozo, f.bozo_exception)
  exit(-1)

if 'error' in f.feed.keys() or f.feed.keys() == []:
  print "%s Twitter feed for %s is private or doesn't exist" % (L, user)
  exit(-1)
  
try:
  entry = f.entries[n]
except IndexError:
  print "%s Tweet not available" % L
  exit(-1)
  
print "%s %s (%s)" % (L, entry.summary, entry.published.rsplit(' ', 1)[0])

