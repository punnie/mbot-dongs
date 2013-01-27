import sys

# sys.stdout.encoding is None when piping to a file.
encoding = sys.stdout.encoding
if encoding is None:
  encoding = sys.getfilesystemencoding()

def print_console(*args):
  print u' '.join(args).encode(encoding, 'replace')
