import sys
import re
import htmlentitydefs

# sys.stdout.encoding is None when piping to a file.
encoding = sys.stdout.encoding
if encoding is None:
  encoding = sys.getfilesystemencoding()

def print_console(*args):
  print u' '.join(args).encode(encoding, 'replace')
  
def unescape(text):
  def fixup(m):
    text = m.group(0)
    if text[:2] == "&#":
      # character reference
      try:
        if text[:3] == "&#x":
          return unichr(int(text[3:-1], 16))
        else:
          return unichr(int(text[2:-1]))
      except ValueError:
        pass
    else:
      # named entity
      try:
        text = unichr(htmlentitydefs.name2codepoint[text[1:-1]])
      except KeyError:
        pass
    return text # leave as is
  return re.sub("&#?\w+;", fixup, text)
  
def strip(html):
  return re.sub('<[^<]+?>', '', html)
