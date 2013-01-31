import feedparser
import re
import sys
import code
import os
from mylib import print_console, unescape

teh4chan = "8,34chan"

feeds = [
  {"id":"pplw", "logo": "11,2PPLWARE", "url":"http://pplware.sapo.pt/feed/"},
  {"id":"apod", "logo": "1,15APOD", "url":"http://apod.nasa.gov/apod.rss"},
  {"id":"tlks", "logo": "14,01TUGALEAKS", "url":"http://feeds.feedburner.com/tugaleaks"},
  {"id":"4chB", "logo": teh4chan, "url":"http://boards.4chan.org/b/index.rss"},
  {"id":"4chS", "logo": teh4chan, "url":"http://boards.4chan.org/s/index.rss"},
  {"id":"guns", "logo": "0,1Gun Show", "url":"http://www.rsspect.com/rss/gunshowcomic.xml"},
  {"id":"qcon", "logo": "10,12QC", "url":"http://www.questionablecontent.net/QCRSS.xml"},
  {"id":"xkcd", "logo": "1,0xkcd", "url":"http://xkcd.com/rss.xml"}
]


def strip(html):
  return re.sub('<[^<]+?>', '', html)
  
#def usage():
#  print "%s Usage: !feedname [n]" % L
#  exit(-1)

#if len(sys.argv) < 2:
#  usage()

arg1 = sys.argv[1]
try:
  n = int(sys.argv[2])
except IndexError:
  n = 0
except ValueError:
  n = 0


f = None
for feed in feeds:
  if feed["id"] == arg1:
    f = feed

if f is None:
  print_console("Feed %s not found!" % arg1)
  exit(-1);


url = f["url"]
logo = f["logo"]

f = feedparser.parse(url)
#code.interact(local=locals())
if f.bozo == 1:
  print_console("%s omg :( %s" % (f.bozo, f.bozo_exception))
  exit(-1)

try:
  entry = f.entries[n]
except IndexError:
  print_console("Entry not available")
  exit(-1)

title = entry.title
link = entry.link
summary = unescape(strip(entry.summary))

if 'published' in entry.keys():
  published = entry.published.rsplit(' ', 1)[0]
else:
  published = None

if published is not None:
  print_console("%s %s - %s (%s)" % (logo, title, link, published)) 
else:
  print_console("%s %s - %s" % (logo, title, link)) 

for l in summary.split("\n"):
  if len(l) > 0:
    print_console("%s %s" % (logo, l))

