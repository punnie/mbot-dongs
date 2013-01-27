import feedparser
import re
from mylib import print_console

def strip(html):
  return re.sub('<[^<]+?>', '', html)
  
url = "http://www.priberam.pt/dlpo/DoDiaRSS.aspx"

f = feedparser.parse(url)
wotd_l = strip(f["items"][0]["summary"]).split("\n")

print_console("Palavra do dia: %s" % wotd_l[0])

for l in wotd_l[1:]:
  l = l.strip()
  if len(l) > 1:
    print_console(l)
