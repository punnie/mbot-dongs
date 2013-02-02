import socket
import sys
from mylib import print_console

if len(sys.argv) < 3:
  print_console("Usage: !dns <name> | !rdns <ip address>");
  exit(-1)
  
t = sys.argv[1]
p = sys.argv[2]

if t == "r":
  a = socket.gethostbyaddr(p)
  print "rDNS lookup for %s: %s" % (p, a[0])
elif t == "l":
  a = socket.gethostbyname(p)
  print "DNS lookup for %s: %s" % (p, a)

  
